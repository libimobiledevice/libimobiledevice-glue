/*
 * collection.c
 *
 * Copyright (C) 2009 Hector Martin <hector@marcansoft.com>
 * Copyright (C) 2009 Nikias Bassen <nikias@gmx.li>
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
#include "libimobiledevice-glue/collection.h"

#undef NDEBUG // we need to make sure we still get assertions because we can't handle memory allocation errors
#include <assert.h>

#define INIT_NULL(addr, count) { unsigned int i_ = 0; for (i_ = 0; i_ < (count); i_++) ((void**)(addr))[i_] = NULL; }

#define CAPACITY_STEP 8

LIBIMOBILEDEVICE_GLUE_API void collection_init(struct collection *col)
{
	col->list = malloc(sizeof(void *) * CAPACITY_STEP);
	assert(col->list);
	INIT_NULL(col->list, CAPACITY_STEP);
	col->capacity = CAPACITY_STEP;
}

LIBIMOBILEDEVICE_GLUE_API collection_t* collection_new(void)
{
	collection_t *col = malloc(sizeof(collection_t));
	assert(col);
	collection_init(col);
	return col;
}

LIBIMOBILEDEVICE_GLUE_API void collection_free(struct collection *col)
{
	assert(col->list);

	free(col->list);
	col->list = NULL;
	col->capacity = 0;
}

LIBIMOBILEDEVICE_GLUE_API void collection_free_all(struct collection *col) 
{
	assert(col);
	assert(col->list);

	for (int i = 0; i < col->capacity; i++)
	{
		if (col->list[i] != NULL) 
		{
			free(col->list[i]);
			col->list[i] = NULL;
		}
	}
	collection_free(col);
}

LIBIMOBILEDEVICE_GLUE_API void collection_ensure_capacity(struct collection *col, size_t capacity)
{
	assert(col);
	assert(col->list);
	assert(capacity > 0);

	if (col->capacity < (int)capacity)
	{
		void **newlist = realloc(col->list, sizeof(void*) * capacity);
		assert(newlist);
		col->list = newlist;
		col->capacity = capacity;
	}

	return;
}

LIBIMOBILEDEVICE_GLUE_API void collection_add(struct collection *col, void *element)
{
	assert(col);
	assert(col->list);
	assert(element);

	int i;
	for(i=0; i<col->capacity; i++) {
		if(!col->list[i]) {
			col->list[i] = element;
			return;
		}
	}
	void **newlist = realloc(col->list, sizeof(void*) * (col->capacity + CAPACITY_STEP));
	assert(newlist);
	col->list = newlist;
	INIT_NULL(&col->list[col->capacity], CAPACITY_STEP);
	col->list[col->capacity] = element;
	col->capacity += CAPACITY_STEP;
}

LIBIMOBILEDEVICE_GLUE_API int collection_remove(struct collection *col, void *element)
{
	assert(col);
	assert(col->list);
	assert(element);

	int i;
	for(i=0; i<col->capacity; i++) {
		if(col->list[i] == element) {
			col->list[i] = NULL;
			return 0;
		}
	}
	fprintf(stderr, "%s: WARNING: element %p not present in collection %p (cap %d)", __func__, element, col, col->capacity);
	return -1;
}

LIBIMOBILEDEVICE_GLUE_API int collection_count(struct collection *col)
{
	assert(col);
	assert(col->list);

	int i, cnt = 0;
	for(i=0; i<col->capacity; i++) {
		if(col->list[i])
			cnt++;
	}
	return cnt;
}

LIBIMOBILEDEVICE_GLUE_API void collection_copy(struct collection *dest, struct collection *src)
{
	assert(src);
	assert(src->list);
	assert(dest);
	assert(dest->list);

	dest->capacity = src->capacity;
	dest->list = malloc(sizeof(void*) * src->capacity);

	memcpy(dest->list, src->list, sizeof(void*) * src->capacity);
}
