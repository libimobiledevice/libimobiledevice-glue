/*
 * collection.h
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

#ifndef __LIMD_COLLECTION_H
#define __LIMD_COLLECTION_H

#include <stddef.h>
#include <stdint.h>

typedef struct collection {
	void **list;
	int capacity;
} collection_t;

void collection_init(struct collection *col);
collection_t* collection_new(void);
void collection_add(struct collection *col, void *element);
int collection_remove(struct collection *col, void *element);
int collection_count(struct collection *col);
void collection_free(struct collection *col);
void collection_free_all(struct collection *col);
void collection_ensure_capacity(struct collection *col, size_t size);
void collection_copy(struct collection *dest, struct collection *src);

#define MERGE_(a,b) a ## _ ## b
#define LABEL_(a,b) MERGE_(a, b)
#define UNIQUE_VAR(a) LABEL_(a, __LINE__)

#define FOREACH(var, col) \
	do { \
		int UNIQUE_VAR(_iter); \
		for(UNIQUE_VAR(_iter)=0; UNIQUE_VAR(_iter)<(col)->capacity; UNIQUE_VAR(_iter)++) { \
			if(!(col)->list[UNIQUE_VAR(_iter)]) continue; \
			var = (col)->list[UNIQUE_VAR(_iter)];

#define ENDFOREACH \
		} \
	} while(0);

#endif // __LIMD_COLLECTION_H