/*
 * sha1.h
 *
 * Copyright (c) 2012-2019 Nikias Bassen, All Rights Reserved.
 * Copyright (c) 2012 Martin Szulecki, All Rights Reserved.
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
 *
 *   SHA-1 in C
 *   By Steve Reid <steve@edmweb.com>
 *   100% Public Domain
 * 
 * Adapted for use in libimobiledevice-glue by Rick Mark
 */

#ifndef __LIMD_GLUE_SHA1_H
#define __LIMD_GLUE_SHA1_H

#include <stddef.h>
#include <stdint.h>

#define SHA1_HASH_LENGTH_BYTES 20
#define SHA1_HASH_LENGTH_BITS 160
#define SHA1_HASH_BLOCK_SIZE_BYTES 64
#define SHA1_HASH_STATE_SIZE_BYTES 5

typedef struct
{
    uint32_t state[SHA1_HASH_STATE_SIZE_BYTES];
    uint32_t count[2];
    unsigned char buffer[SHA1_HASH_BLOCK_SIZE_BYTES];
} LIMD_SHA1_CTX;

void LIMD_SHA1Transform(
    uint32_t state[SHA1_HASH_STATE_SIZE_BYTES],
    const unsigned char buffer[SHA1_HASH_BLOCK_SIZE_BYTES]
    );

void LIMD_SHA1Init(
    LIMD_SHA1_CTX * context
    );

void LIMD_SHA1Update(
    LIMD_SHA1_CTX * context,
    const unsigned char *data,
    size_t len
    );

void LIMD_SHA1Final(
    unsigned char digest[SHA1_HASH_LENGTH_BYTES],
    LIMD_SHA1_CTX * context
    );

void LIMD_SHA1(
    const unsigned char *str,
    size_t len,
    unsigned char *hash_out);

#endif /* __LIMD_GLUE_SHA1_H */
