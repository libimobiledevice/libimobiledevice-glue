/*
 * common.h
 *
 * Copyright (c) 2020 Nikias Bassen, All Rights Reserved.
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

#ifndef __COMMON_H
#define __COMMON_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#define LIBIMOBILEDEVICE_GLUE_API __declspec( dllexport )
#else
#ifdef HAVE_FVISIBILITY
#define LIBIMOBILEDEVICE_GLUE_API __attribute__((visibility("default")))
#else
#define LIBIMOBILEDEVICE_GLUE_API
#endif
#endif

#endif
