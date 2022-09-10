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
 *  win32 api wrapper
 */

#ifndef COMPAT_W32WRAP_H
#define COMPAT_W32WRAP_H

#define WIN32_LEAN_AND_MEAN

#define STATIC_VAR static
#define STACK_VAR

#define DEF_LPWINAPI1(lpVarType, funcName, retType, paramType) \
    typedef retType (WINAPI* LPFN_##funcName##)(paramType); \
    lpVarType LPFN_##funcName lp##funcName = NULL;

#define DEF_LPWINAPI2(lpVarType, funcName, retType, paramType1, paramType2) \
    typedef retType (WINAPI* LPFN_##funcName##)(paramType1, paramType2); \
    lpVarType LPFN_##funcName lp##funcName = NULL;

#define DEF_LPWINAPI3(lpVarType, funcName, retType, paramType1, paramType2, paramType3) \
    typedef retType (WINAPI* LPFN_##funcName##)(paramType1, paramType2, paramType3); \
    lpVarType LPFN_##funcName lp##funcName = NULL;

#define DEF_LPWINAPI4(lpVarType, funcName, retType, paramType1, paramType2, paramType3, paramType4) \
    typedef retType (WINAPI* LPFN_##funcName##)(paramType1, paramType2, paramType3, paramType4); \
    lpVarType LPFN_##funcName lp##funcName = NULL;


static inline HMODULE loadKernel32dll()
{
    static HMODULE kernelHModule = NULL;
    if (kernelHModule == NULL)
        kernelHModule = GetModuleHandle("kernel32.dll");
    return kernelHModule;
}

#define GET_PROC_ADDRESS(lpFunc, funcName)                               \
    if (!lpFunc)                                                         \
    {                                                                    \
        HMODULE hModule = loadKernel32dll();                             \
        if (hModule == NULL)                                             \
            return 0;                                                    \
        lpFunc = (LPFN_##funcName##)GetProcAddress(hModule, #funcName);  \
        if (lpFunc == NULL)                                              \
            return 0;                                                    \
    }

#endif /* COMPAT_W32WRAP_H */
