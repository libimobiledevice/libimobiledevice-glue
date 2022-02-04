/*
 * cbuf.c
 * Simple char buffer implementation.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "libimobiledevice-glue/cbuf.h"

LIBIMOBILEDEVICE_GLUE_API struct char_buf* char_buf_new()
{
	struct char_buf* cbuf = (struct char_buf*)malloc(sizeof(struct char_buf));
	cbuf->capacity = 256;
	cbuf->length = 0;
	cbuf->data = (unsigned char*)malloc(cbuf->capacity);
	return cbuf;
}

LIBIMOBILEDEVICE_GLUE_API void char_buf_free(struct char_buf* cbuf)
{
	if (cbuf) {
		free(cbuf->data);
		free(cbuf);
	}
}

LIBIMOBILEDEVICE_GLUE_API void char_buf_append(struct char_buf* cbuf, unsigned int length, unsigned char* data)
{
	if (!cbuf || !cbuf->data) {
		return;
	}
	if (cbuf->length + length > cbuf->capacity) {
		unsigned int newcapacity = cbuf->capacity + ((length / 256) + 1) * 256;
		unsigned char* newdata = realloc(cbuf->data, newcapacity);
		if (!newdata) {
			fprintf(stderr, "%s: ERROR: Failed to realloc\n", __func__);
			return;
		}
		cbuf->data = newdata;
		cbuf->capacity = newcapacity;
	}
	memcpy(cbuf->data + cbuf->length, data, length);
	cbuf->length += length;
}
