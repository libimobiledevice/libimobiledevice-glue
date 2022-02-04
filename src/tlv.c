/*
 * tlv.c
 * Simple TLV implementation.
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
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "libimobiledevice-glue/tlv.h"
#include "endianness.h"

LIBIMOBILEDEVICE_GLUE_API tlv_buf_t tlv_buf_new()
{
	tlv_buf_t tlv = malloc(sizeof(struct tlv_buf));
	tlv->capacity = 1024;
	tlv->length = 0;
	tlv->data = malloc(tlv->capacity);
	return tlv;	
}

LIBIMOBILEDEVICE_GLUE_API void tlv_buf_free(tlv_buf_t tlv)
{
	if (tlv) {
		free(tlv->data);
		free(tlv);
	}
}

LIBIMOBILEDEVICE_GLUE_API void tlv_buf_append(tlv_buf_t tlv, uint8_t tag, unsigned int length, void* data)
{
	if (!tlv || !tlv->data) {
		return;
	}
	unsigned int req_len = (length > 255) ? (length / 255) * 257 + (2 + (length % 255)) : length;
	if (tlv->length + req_len > tlv->capacity) {
		unsigned int newcapacity = tlv->capacity + ((req_len / 1024) + 1) * 1024;
		unsigned char* newdata = realloc(tlv->data, newcapacity);
		if (!newdata) {
			fprintf(stderr, "%s: ERROR: Failed to realloc\n", __func__);
			return;
		}
		tlv->data = newdata;
		tlv->capacity = newcapacity;
	}
	unsigned char* p = tlv->data + tlv->length;
	unsigned int cur = 0;
	while (length - cur > 0) {
		if (length - cur > 255) {
			*(p++) = tag;
			*(p++) = 0xFF;
			memcpy(p, (unsigned char*)data + cur, 255);
			p += 255;
			cur += 255;
		} else {
			uint8_t rem = length - cur;
			*(p++) = tag;
			*(p++) = rem;
			memcpy(p, (unsigned char*)data + cur, rem);
			p += rem;
			cur += rem;
		}
	}
	tlv->length = p - tlv->data;
}

LIBIMOBILEDEVICE_GLUE_API unsigned char* tlv_get_data_ptr(const void* tlv_data, void* tlv_end, uint8_t tag, uint8_t* length)
{
	unsigned char* p = (unsigned char*)tlv_data;
	unsigned char* end = (unsigned char*)tlv_end;
	while (p < end) {
		uint8_t cur_tag = *(p++);
		uint8_t len = *(p++);
		if (cur_tag == tag) {
			*length = len;
			return p;
		}
		p+=len;
	}
	return NULL;
}

LIBIMOBILEDEVICE_GLUE_API int tlv_data_get_uint(const void* tlv_data, unsigned int tlv_length, uint8_t tag, uint64_t* value)
{
	if (!tlv_data || tlv_length < 2 || !value) {
		return 0;
	}
	uint8_t length = 0;
	unsigned char* ptr = tlv_get_data_ptr(tlv_data, (unsigned char*)tlv_data+tlv_length, tag, &length);
	if (!ptr) {
		return 0;
	}
	if (ptr + length > (unsigned char*)tlv_data + tlv_length) {
		return 0;
	}
	if (length == 1) {
		uint8_t val = *ptr;
		*value = val;
	} else if (length == 2) {
		uint16_t val = *(uint16_t*)ptr;
		val = le16toh(val);
		*value = val;
	} else if (length == 4) {
		uint32_t val = *(uint32_t*)ptr;
		val = le32toh(val);
		*value = val;
	} else if (length == 8) {
		uint64_t val = *(uint64_t*)ptr;
		val = le64toh(val);
		*value = val;
	} else {
		return 0;
	}
	return 1;
}

LIBIMOBILEDEVICE_GLUE_API int tlv_data_get_uint8(const void* tlv_data, unsigned int tlv_length, uint8_t tag, uint8_t* value)
{
	if (!tlv_data || tlv_length < 2 || !value) {
		return 0;
	}
	uint8_t length = 0;
	unsigned char* ptr = tlv_get_data_ptr(tlv_data, (unsigned char*)tlv_data+tlv_length, tag, &length);
	if (!ptr) {
		return 0;
	}
	if (length != 1) {
		return 0;
	}
	if (ptr + length > (unsigned char*)tlv_data + tlv_length) {
		return 0;
	}
	*value = *ptr;
	return 1;
}

LIBIMOBILEDEVICE_GLUE_API int tlv_data_copy_data(const void* tlv_data, unsigned int tlv_length, uint8_t tag, void** out, unsigned int* out_len)
{
	if (!tlv_data || tlv_length < 2 || !out || !out_len) {
		return 0;
	}
	*out = NULL;
	*out_len = 0;

	unsigned char* dest = NULL;
	unsigned int dest_len = 0;

	unsigned char* ptr = (unsigned char*)tlv_data;
	unsigned char* end = (unsigned char*)tlv_data + tlv_length;
	while (ptr < end) {
		uint8_t length = 0;
		ptr = tlv_get_data_ptr(ptr, end, tag, &length);
		if (!ptr) {
			break;
		}
		unsigned char* newdest = realloc(dest, dest_len + length);
		if (!newdest) {
			free(dest);
			return 0;
		}
		dest = newdest;
		memcpy(dest + dest_len, ptr, length);
		dest_len += length;
		ptr += length;
	}
	if (!dest) {
		return 0;
	}

	*out = (void*)dest;
	*out_len = dest_len;

	return 1;
}
