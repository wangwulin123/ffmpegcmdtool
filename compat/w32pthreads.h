/*
 * Copyright (C) 2010-2011 x264 project
 *
 * Authors: Steven Walters <kemuri9@gmail.com>
 *          Pegasys Inc. <http://www.pegasys-inc.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * w32threads to pthreads wrapper
 */

#ifndef COMPAT_W32PTHREADS_H
#define COMPAT_W32PTHREADS_H

/* Build up a pthread-like API using underlying Windows API. Have only static
 * methods so as to not conflict with a potentially linked in pthread-win32
 * library.
 * As most functions here are used without checking return values,
 * only implement return values as necessary. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include "w32wrap.h"

#include "libavutil/attributes.h"
#include "libavutil/common.h"
#include "libavutil/internal.h"
#include "libavutil/mem.h"

typedef struct pthread_t {
    void *handle;
    void *(*func)(void* arg);
    void *arg;
    void *ret;
} pthread_t;

/* use light weight mutex/condition variable API for Windows Vista and later */
typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT

#define InitializeCriticalSection(x) InitializeCriticalSectionEx(x, 0, 0)
#define WaitForSingleObject(a, b) WaitForSingleObjectEx(a, b, FALSE)

static av_unused unsigned __stdcall attribute_align_arg win32thread_worker(void *arg)
{
    pthread_t *h = (pthread_t*)arg;
    h->ret = h->func(h->arg);
    return 0;
}

static av_unused int pthread_create(pthread_t *thread, const void *unused_attr,
                                    void *(*start_routine)(void*), void *arg)
{
    thread->func   = start_routine;
    thread->arg    = arg;
#if HAVE_WINRT
    thread->handle = (void*)CreateThread(NULL, 0, win32thread_worker, thread,
                                           0, NULL);
#else
    thread->handle = (void*)_beginthreadex(NULL, 0, win32thread_worker, thread,
                                           0, NULL);
#endif
    return !thread->handle;
}

static av_unused int pthread_join(pthread_t thread, void **value_ptr)
{
    DWORD ret = WaitForSingleObject(thread.handle, INFINITE);
    if (ret != WAIT_OBJECT_0) {
        if (ret == WAIT_ABANDONED)
            return EINVAL;
        else
            return EDEADLK;
    }
    if (value_ptr)
        *value_ptr = thread.ret;
    CloseHandle(thread.handle);
    return 0;
}

static inline int pthread_mutex_init(pthread_mutex_t *m, void* attr)
{
    DEF_LPWINAPI1(STATIC_VAR, InitializeSRWLock, void, PSRWLOCK);
    GET_PROC_ADDRESS(lpInitializeSRWLock, InitializeSRWLock);
    lpInitializeSRWLock(m);
    return 0;
}
static inline int pthread_mutex_destroy(pthread_mutex_t *m)
{
    /* Unlocked SWR locks use no resources */
    return 0;
}
static inline int pthread_mutex_lock(pthread_mutex_t *m)
{
    DEF_LPWINAPI1(STATIC_VAR, AcquireSRWLockExclusive, void, PSRWLOCK);
    GET_PROC_ADDRESS(lpAcquireSRWLockExclusive, AcquireSRWLockExclusive);
    lpAcquireSRWLockExclusive(m);
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *m)
{
    DEF_LPWINAPI1(STATIC_VAR, ReleaseSRWLockExclusive, void, PSRWLOCK);
    GET_PROC_ADDRESS(lpReleaseSRWLockExclusive, ReleaseSRWLockExclusive);
    lpReleaseSRWLockExclusive(m);
    return 0;
}

#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT


/* atomic init state of dynamically loaded functions */
static LONG w32thread_init_state = 0;
/* for pre-Windows 6.0 platforms, define INIT_ONCE struct,
 * compatible to the one used in the native API */

typedef union pthread_once_t {
    void* Ptr;    ///< For the Windows 6.0+ native functions
    LONG state;    ///< For the pre-Windows 6.0 compat code
} pthread_once_t;

static BOOL(WINAPI* initonce_begin)(pthread_once_t* lpInitOnce, DWORD dwFlags, BOOL* fPending, void** lpContext);
static BOOL(WINAPI* initonce_complete)(pthread_once_t* lpInitOnce, DWORD dwFlags, void* lpContext);

/* pre-Windows 6.0 compat using a spin-lock */
static inline void w32thread_once_fallback(LONG volatile* state, void (*init_routine)(void))
{
    switch (InterlockedCompareExchange(state, 1, 0)) {
        /* Initial run */
    case 0:
        init_routine();
        InterlockedExchange(state, 2);
        break;
        /* Another thread is running init */
    case 1:
        while (1) {
            MemoryBarrier();
            if (*state == 2)
                break;
            Sleep(0);
        }
        break;
        /* Initialization complete */
    case 2:
        break;
    }
}

static av_unused void w32thread_init(void)
{
    HMODULE kernel_dll = GetModuleHandle(TEXT("kernel32.dll"));
    /* if one is available, then they should all be available */
    initonce_begin = (BOOL(WINAPI*)(pthread_once_t*, DWORD, BOOL*, void**))
        GetProcAddress(kernel_dll, "InitOnceBeginInitialize");
    initonce_complete = (BOOL(WINAPI*)(pthread_once_t*, DWORD, void*))
    GetProcAddress(kernel_dll, "InitOnceComplete");
}

static av_unused int pthread_once(pthread_once_t* once_control, void (*init_routine)(void))
{
    w32thread_once_fallback(&w32thread_init_state, w32thread_init);

    /* Use native functions on Windows 6.0+ */
    if (initonce_begin && initonce_complete) {
        BOOL pending = FALSE;
        initonce_begin(once_control, 0, &pending, NULL);
        if (pending)
            init_routine();
        initonce_complete(once_control, 0, NULL);
        return 0;
    }

    w32thread_once_fallback(&once_control->state, init_routine);
    return 0;
}

static inline int pthread_cond_init(pthread_cond_t *cond, const void *unused_attr)
{
    DEF_LPWINAPI1(STATIC_VAR, InitializeConditionVariable, void, PCONDITION_VARIABLE);
    GET_PROC_ADDRESS(lpInitializeConditionVariable, InitializeConditionVariable);
    lpInitializeConditionVariable(cond);
    return 0;
}

/* native condition variables do not destroy */
static inline int pthread_cond_destroy(pthread_cond_t *cond)
{
    return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t *cond)
{
    DEF_LPWINAPI1(STATIC_VAR, WakeAllConditionVariable, void, PCONDITION_VARIABLE);
    GET_PROC_ADDRESS(lpWakeAllConditionVariable, WakeAllConditionVariable);
    lpWakeAllConditionVariable(cond);
    return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    DEF_LPWINAPI4(STATIC_VAR, SleepConditionVariableSRW, BOOL, PCONDITION_VARIABLE, PSRWLOCK, DWORD, ULONG);
    GET_PROC_ADDRESS(lpSleepConditionVariableSRW, SleepConditionVariableSRW);
    lpSleepConditionVariableSRW(cond, mutex, INFINITE, 0);
    return 0;
}

static inline int pthread_cond_signal(pthread_cond_t *cond)
{
    DEF_LPWINAPI1(STATIC_VAR, WakeConditionVariable, void, PCONDITION_VARIABLE);
    GET_PROC_ADDRESS(lpWakeConditionVariable, WakeConditionVariable);
    lpWakeConditionVariable(cond);
    return 0;
}

#endif /* COMPAT_W32PTHREADS_H */
