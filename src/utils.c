/*
 * utils.c
 * Miscellaneous utilities for string manipulation,
 * file I/O and plist helper.
 *
 * Copyright (c) 2014-2023 Nikias Bassen, All Rights Reserved.
 * Copyright (c) 2013-2014 Martin Szulecki, All Rights Reserved.
 * Copyright (c) 2013 Federico Mena Quintero
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>

#include "common.h"
#include "libimobiledevice-glue/utils.h"

#ifndef HAVE_STPCPY
#undef stpcpy
char *stpcpy(char *s1, const char *s2);

/**
 * Copy characters from one string into another
 *
 * @note: The strings should not overlap, as the behavior is undefined.
 *
 * @s1: The source string.
 * @s2: The destination string.
 *
 * @return a pointer to the terminating `\0' character of @s1,
 * or NULL if @s1 or @s2 is NULL.
 */
char *stpcpy(char *s1, const char *s2)
{
	if (s1 == NULL || s2 == NULL)
		return NULL;

	strcpy(s1, s2);

	return s1 + strlen(s2);
}
#endif

/**
 * Concatenate strings into a newly allocated string
 *
 * @note: Specify NULL for the last string in the varargs list
 *
 * @str: The first string in the list
 * @...: Subsequent strings.  Use NULL for the last item.
 *
 * @return a newly allocated string, or NULL if @str is NULL.  This will also
 * return NULL and set errno to ENOMEM if memory is exhausted.
 */
LIBIMOBILEDEVICE_GLUE_API char *string_concat(const char *str, ...)
{
	size_t len;
	va_list args;
	char *s;
	char *result;
	char *dest;

	if (!str)
		return NULL;

	/* Compute final length */

	len = strlen(str) + 1; /* plus 1 for the null terminator */

	va_start(args, str);
	s = va_arg(args, char *);
	while (s) {
		len += strlen(s);
		s = va_arg(args, char*);
	}
	va_end(args);

	/* Concat each string */

	result = malloc(len);
	if (!result)
		return NULL; /* errno remains set */

	dest = result;

	dest = stpcpy(dest, str);

	va_start(args, str);
	s = va_arg(args, char *);
	while (s) {
		dest = stpcpy(dest, s);
		s = va_arg(args, char *);
	}
	va_end(args);

	return result;
}

LIBIMOBILEDEVICE_GLUE_API char *string_append(char* str, ...)
{
	size_t len = 0;
	size_t slen;
	va_list args;
	char *s;
	char *result;
	char *dest;

	/* Compute final length */

	if (str) {
		len = strlen(str);
	}
	slen = len;
	len++; /* plus 1 for the null terminator */

	va_start(args, str);
	s = va_arg(args, char *);
	while (s) {
		len += strlen(s);
		s = va_arg(args, char*);
	}
	va_end(args);

	result = realloc(str, len);
	if (!result)
		return NULL; /* errno remains set */

	dest = result + slen;

	/* Concat additional strings */

	va_start(args, str);
	s = va_arg(args, char *);
	while (s) {
		dest = stpcpy(dest, s);
		s = va_arg(args, char *);
	}
	va_end(args);

	return result;
}

LIBIMOBILEDEVICE_GLUE_API char *string_build_path(const char *elem, ...)
{
	if (!elem)
		return NULL;
	va_list args;
	int len = strlen(elem)+1;
	va_start(args, elem);
	char *arg = va_arg(args, char*);
	while (arg) {
		len += strlen(arg)+1;
		arg = va_arg(args, char*);
	}
	va_end(args);

	char* out = (char*)malloc(len);
	strcpy(out, elem);

	va_start(args, elem);
	arg = va_arg(args, char*);
	while (arg) {
		strcat(out, "/");
		strcat(out, arg);
		arg = va_arg(args, char*);
	}
	va_end(args);
	return out;
}

LIBIMOBILEDEVICE_GLUE_API char *string_format_size(uint64_t size)
{
	char buf[80];
	double sz;
	if (size >= 1000000000000LL) {
		sz = ((double)size / 1000000000000.0F);
		sprintf(buf, "%0.1f TB", sz);
	} else if (size >= 1000000000LL) {
		sz = ((double)size / 1000000000.0F);
		sprintf(buf, "%0.1f GB", sz);
	} else if (size >= 1000000LL) {
		sz = ((double)size / 1000000.0F);
		sprintf(buf, "%0.1f MB", sz);
	} else if (size >= 1000LL) {
		sz = ((double)size / 1000.0F);
		sprintf(buf, "%0.1f KB", sz);
	} else {
		sprintf(buf, "%d Bytes", (int)size);
	}
	return strdup(buf);
}

LIBIMOBILEDEVICE_GLUE_API char *string_toupper(char* str)
{
	char *res = strdup(str);
	size_t i;
	for (i = 0; i < strlen(res); i++) {
		res[i] = toupper(res[i]);
	}
	return res;
}

static int get_rand(int min, int max)
{
	int retval = (rand() % (max - min)) + min;
	return retval;
}

LIBIMOBILEDEVICE_GLUE_API char *generate_uuid()
{
	const char *chars = "ABCDEF0123456789";
	int i = 0;
	char *uuid = (char *) malloc(sizeof(char) * 37);

	srand(time(NULL));

	for (i = 0; i < 36; i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23) {
			uuid[i] = '-';
			continue;
		}
		uuid[i] = chars[get_rand(0, 16)];
	}

	/* make it a real string */
	uuid[36] = '\0';

	return uuid;
}

LIBIMOBILEDEVICE_GLUE_API int buffer_read_from_filename(const char *filename, char **buffer, uint64_t *length)
{
	FILE *f;
	uint64_t size;

	if (!filename || !buffer || !length) {
		return 0;
	}

	*length = 0;

	f = fopen(filename, "rb");
	if (!f) {
		return 0;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	rewind(f);

	if (size == 0) {
		fclose(f);
		return 0;
	}

	*buffer = (char*)malloc(sizeof(char)*(size+1));

	if (*buffer == NULL) {
		fclose(f);
		return 0;
	}

	int ret = 1;
	if (fread(*buffer, sizeof(char), size, f) != size) {
		free(*buffer);
		ret = 0;
		errno = EIO;
	}
	fclose(f);

	*length = size;
	return ret;
}

LIBIMOBILEDEVICE_GLUE_API int buffer_write_to_filename(const char *filename, const char *buffer, uint64_t length)
{
	FILE *f;

	f = fopen(filename, "wb");
	if (f) {
		size_t written = fwrite(buffer, sizeof(char), length, f);
		fclose(f);

		if (written == length) {
			return 1;
		}
		else {
			// Not all data could be written.
			errno = EIO;
			return 0;
		}
	}
	else {
		// Failed to open the file, let the caller know.
		return 0;
	}
}
