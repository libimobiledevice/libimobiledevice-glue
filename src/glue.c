/*
 * glue.c
 *
 * Copyright (c) 2021 Nikias Bassen, All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include "common.h"
#include "libimobiledevice-glue/thread.h"

extern void term_colors_init();

static void internal_glue_init(void)
{
	term_colors_init();
}

static void internal_glue_deinit(void)
{

}

static thread_once_t init_once = THREAD_ONCE_INIT;
static thread_once_t deinit_once = THREAD_ONCE_INIT;

#ifndef HAVE_ATTRIBUTE_CONSTRUCTOR
  #if defined(__llvm__) || defined(__GNUC__)
    #define HAVE_ATTRIBUTE_CONSTRUCTOR
  #endif
#endif

#ifdef HAVE_ATTRIBUTE_CONSTRUCTOR
static void __attribute__((constructor)) limd_glue_initialize(void)
{
    thread_once(&init_once, internal_glue_init);
}

static void __attribute__((destructor)) limd_glue_deinitialize(void)
{
    thread_once(&deinit_once, internal_glue_deinit);
}
#elif defined(WIN32)
BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        thread_once(&init_once, internal_glue_init);
        break;
    case DLL_PROCESS_DETACH:
        thread_once(&deinit_once, internal_glue_deinit);
        break;
    default:
        break;
    }
    return 1;
}
#else
#warning No compiler support for constructor/destructor attributes, some features might not be available.
#endif
