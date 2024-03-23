/*
 * nskeyedarchive.c
 * Helper code to work with plist files containing NSKeyedArchiver data.
 *
 * Copyright (c) 2019 Nikias Bassen, All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <plist/plist.h>
#include "common.h"
#include "libimobiledevice-glue/nskeyedarchive.h"

#define NS_KEYED_ARCHIVER_NAME "NSKeyedArchiver"
#define NS_KEYED_ARCHIVER_VERSION 100000

struct nskeyedarchive_st {
	plist_t dict;
	uint64_t uid;
};

nskeyedarchive_t nskeyedarchive_new(void)
{
	plist_t dict = plist_new_dict();
	plist_dict_set_item(dict, "$version", plist_new_uint(NS_KEYED_ARCHIVER_VERSION));
	plist_t objects = plist_new_array();
	plist_array_append_item(objects, plist_new_string("$null"));
	plist_dict_set_item(dict, "$objects", objects);
	plist_dict_set_item(dict, "$archiver", plist_new_string(NS_KEYED_ARCHIVER_NAME));

	nskeyedarchive_t nskeyed = (nskeyedarchive_t)malloc(sizeof(struct nskeyedarchive_st));
	nskeyed->dict = dict;
	nskeyed->uid = 1;
	return nskeyed;
}

void nskeyedarchive_free(nskeyedarchive_t ka)
{
	if (ka) {
		if (ka->dict) {
			plist_free(ka->dict);
		}
		free(ka);
	}
}

void nskeyedarchive_set_top_ref_key_name(nskeyedarchive_t ka, const char* keyname)
{
	if (!ka) {
		return;
	}
	plist_t top = plist_dict_get_item(ka->dict, "$top");
	if (top) {
		plist_dict_iter iter = NULL;
		plist_dict_new_iter(top, &iter);
		if (iter) {
			plist_t node = NULL;
			char* keyn = NULL;
			plist_dict_next_item(top, iter, &keyn, &node);
			plist_t keynode = plist_dict_item_get_key(node);
			plist_set_key_val(keynode, keyname);
			free(keyn);
			free(iter);
		}
	}
}

plist_t nskeyedarchive_get_objects(nskeyedarchive_t ka)
{
	plist_t objects = plist_dict_get_item(ka->dict, "$objects");
	if (!objects || (plist_get_node_type(objects) != PLIST_ARRAY)) {
		fprintf(stderr, "ERROR: $objects node not found!\n");
		return NULL;
	}

	return objects;
}

plist_t nskeyedarchive_get_object_by_uid(nskeyedarchive_t ka, uint64_t uid)
{
	plist_t objects = nskeyedarchive_get_objects(ka);
	if (!objects) {
		return NULL;
	}

	plist_t obj = plist_array_get_item(objects, (uint32_t)uid);
	if (!obj) {
		fprintf(stderr, "ERROR: unable to get object node with uid %llu\n", (long long)uid);
		return NULL;
	}

	return obj;
}

plist_t nskeyedarchive_get_class_by_uid(nskeyedarchive_t ka, uint64_t uid)
{
	plist_t ret = nskeyedarchive_get_object_by_uid(ka, uid);
	if (ret && (plist_get_node_type(ret) != PLIST_DICT)) {
		fprintf(stderr, "ERROR: the uid %llu does not reference a valid class with node type PLIST_DICT!\n", (long long)uid);
		return NULL;
	}

	return ret;
}

void nskeyedarchive_append_object(nskeyedarchive_t ka, plist_t object)
{
	plist_t objects = nskeyedarchive_get_objects(ka);
	if (objects && plist_get_node_type(objects) != PLIST_ARRAY) {
		fprintf(stderr, "ERROR: unable to append object\n");
		return;
	}

	plist_array_append_item(objects, object);
}

static void nskeyedarchive_append_class_v(nskeyedarchive_t ka, const char* classname, va_list* va)
{
	if (!ka) {
		fprintf(stderr, "%s: ERROR: invalid keyed archive!\n", __func__);
		return;
	} else if (!classname) {
		fprintf(stderr, "%s: ERROR: missing classname!\n", __func__);
		return;
	}

	char* arg = NULL;
	plist_t classes = NULL;
	do {
		arg = (char*)va_arg(*va, char*);
		if (!arg) {
			break;
		}
		if (!classes) {
			classes = plist_new_array();
			plist_array_append_item(classes, plist_new_string(classname));
		}
		plist_array_append_item(classes, plist_new_string(arg));
	} while (arg);

	plist_t cls = plist_new_dict();
	plist_dict_set_item(cls, "$class", plist_new_uid(++ka->uid));

	nskeyedarchive_append_object(ka, cls);

	cls = plist_new_dict();
	if (classes) {
		plist_dict_set_item(cls, "$classes", classes);
	}
	plist_dict_set_item(cls, "$classname", plist_new_string(classname));

	nskeyedarchive_append_object(ka, cls);
}

void nskeyedarchive_append_class(nskeyedarchive_t ka, const char* classname, ...)
{
	if (!ka) {
		fprintf(stderr, "%s: ERROR: invalid keyed archive!\n", __func__);
		return;
	} else if (!classname) {
		fprintf(stderr, "%s: ERROR: missing classname!\n", __func__);
		return;
	}

	va_list va;
	va_start(va, classname);
	nskeyedarchive_append_class_v(ka, classname, &va);
	va_end(va);
}

void nskeyedarchive_add_top_class_uid(nskeyedarchive_t ka, uint64_t uid)
{
	plist_t top = plist_dict_get_item(ka->dict, "$top");
	if (!top) {
		top = plist_new_dict();
		plist_dict_set_item(top, "$0", plist_new_uid(uid));
		plist_dict_set_item(ka->dict, "$top", top);
	} else {
		uint32_t num = plist_dict_get_size(top);
		char newkey[8];
		sprintf(newkey, "$%d", num);
		plist_dict_set_item(top, newkey, plist_new_uid(uid));
	}
}

uint64_t nskeyedarchive_add_top_class(nskeyedarchive_t ka, const char* classname, ...)
{
	if (!ka) {
		fprintf(stderr, "%s: ERROR: invalid keyed archive!\n", __func__);
		return 0;
	} else if (!classname) {
		fprintf(stderr, "%s: ERROR: missing classname!\n", __func__);
		return 0;
	}

	uint64_t uid = ka->uid;

	va_list va;
	va_start(va, classname);
	nskeyedarchive_append_class_v(ka, classname, &va);
	va_end(va);

	nskeyedarchive_add_top_class_uid(ka, uid);
	return uid;
}

static void nskeyedarchive_set_class_property_v(nskeyedarchive_t ka, uint64_t uid, const char* propname, enum nskeyedarchive_class_type_t proptype, va_list* va);

static void nskeyedarchive_nsarray_append(nskeyedarchive_t ka, plist_t array, enum nskeyedarchive_class_type_t type, ...);

static void nskeyedarchive_nsarray_append_v(nskeyedarchive_t ka, plist_t array, enum nskeyedarchive_class_type_t type, va_list* va)
{
	if (!ka) {
		fprintf(stderr, "%s: ERROR: invalid keyed archive!\n", __func__);
		return;
	} else if (!array) {
		fprintf(stderr, "%s: ERROR: missing plist!\n", __func__);
		return;
	}

	uint64_t newuid;

	switch (type) {
	case NSTYPE_INTEGER:
		plist_array_append_item(array, plist_new_uint(va_arg(*va, int)));
		break;
	case NSTYPE_INTREF:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		nskeyedarchive_append_object(ka, plist_new_uint(va_arg(*va, int)));
		break;
	case NSTYPE_BOOLEAN:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		nskeyedarchive_append_object(ka, plist_new_bool(va_arg(*va, int)));
		break;
	case NSTYPE_CHARS:
		plist_array_append_item(array, plist_new_string(va_arg(*va, char*)));
		break;
	case NSTYPE_STRING:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		nskeyedarchive_append_object(ka, plist_new_string(va_arg(*va, char*)));
		break;
	case NSTYPE_REAL:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		nskeyedarchive_append_object(ka, plist_new_real(va_arg(*va, double)));
		break;
	case NSTYPE_NSMUTABLESTRING:
	case NSTYPE_NSSTRING:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		newuid = ka->uid;
		if (type == NSTYPE_NSMUTABLESTRING) {
			nskeyedarchive_append_class(ka, "NSMutableString", "NSString", "NSObject", NULL);
		} else {
			nskeyedarchive_append_class(ka, "NSString", "NSObject", NULL);
		}
		nskeyedarchive_set_class_property(ka, newuid, "NS.string", NSTYPE_CHARS, va_arg(*va, char*));
		break;
	case NSTYPE_NSMUTABLEARRAY:
	case NSTYPE_NSARRAY:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		newuid = ka->uid;
		if (type == NSTYPE_NSMUTABLEARRAY) {
			nskeyedarchive_append_class(ka, "NSMutableArray", "NSArray", "NSObject", NULL);
		} else {
			nskeyedarchive_append_class(ka, "NSArray", "NSObject", NULL);
		}
		{
		plist_t arr = plist_new_array();
		do {
			int ptype = va_arg(*va, int);
			if (ptype == 0)
				 break;
			nskeyedarchive_nsarray_append_v(ka, arr, ptype, va);
		} while (1);
		nskeyedarchive_set_class_property(ka, newuid, "NS.objects", NSTYPE_ARRAY, arr);
		plist_free(arr);
		}
		break;
	case NSTYPE_NSMUTABLEDICTIONARY:
	case NSTYPE_NSDICTIONARY:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		newuid = ka->uid;
		if (type == NSTYPE_NSMUTABLEDICTIONARY) {
			nskeyedarchive_append_class(ka, "NSMutableDictionary", "NSDictionary", "NSObject", NULL);
		} else {
			nskeyedarchive_append_class(ka, "NSDictionary", "NSObject", NULL);
		}
		{
		plist_t keyarr = plist_new_array();
		plist_t valarr = plist_new_array();
		do {
			char* key = va_arg(*va, char*);
			if (!key)
				break;
			int ptype = va_arg(*va, int);
			if (ptype == 0)
				break;
			nskeyedarchive_nsarray_append(ka, keyarr, NSTYPE_STRING, key);
			nskeyedarchive_nsarray_append_v(ka, valarr, ptype, va);
		} while (1);
		nskeyedarchive_set_class_property(ka, newuid, "NS.keys", NSTYPE_ARRAY, keyarr);
		nskeyedarchive_set_class_property(ka, newuid, "NS.objects", NSTYPE_ARRAY, valarr);
		plist_free(keyarr);
		plist_free(valarr);
		}
		break;
	case NSTYPE_NSDATE:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		newuid = ka->uid;
		nskeyedarchive_append_class(ka, "NSDate", "NSObject", NULL);
		nskeyedarchive_set_class_property(ka, newuid, "NS.time", NSTYPE_REAL, va_arg(*va, double));
		break;
	case NSTYPE_NSMUTABLEDATA:
	case NSTYPE_NSDATA:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		newuid = ka->uid;
		nskeyedarchive_append_class(ka, "NSMutableData", "NSData", "NSObject", NULL);
		nskeyedarchive_set_class_property(ka, newuid, "NS.data", NSTYPE_DATA, va_arg(*va, char*));
		break;
	case NSTYPE_NSURL:
		plist_array_append_item(array, plist_new_uid(++ka->uid));
		newuid = ka->uid;
		nskeyedarchive_append_class(ka, "NSURL", "NSObject", NULL);
		{
		int ptype = va_arg(*va, int);
		if (ptype == 0)
			break;
		nskeyedarchive_set_class_property_v(ka, newuid, "NS.base", ptype, va);
		ptype = va_arg(*va, int);
		if (ptype == 0)
			break;
		nskeyedarchive_set_class_property_v(ka, newuid, "NS.relative",  ptype, va);
		}
		break;
	case NSTYPE_NSKEYEDARCHIVE:
		{
		nskeyedarchive_t pka = va_arg(*va, nskeyedarchive_t);
		if (!pka) {
			fprintf(stderr, "%s: ERROR: no nskeyedarchive argument given for type NSTYPE_NSKEYEDARCHIVE\n", __func__);
			return;
		}
		uint64_t top = nskeyedarchive_get_class_uid(pka, NULL);
		if (top != 0) {
			plist_array_append_item(array, plist_new_uid(++ka->uid));

			plist_t object = nskeyedarchive_get_object_by_uid(pka, top);
			if (!object) {
				fprintf(stderr, "%s: ERROR: can't get object for uid %lld\n", __func__, (long long)top);
				return;
			}

			plist_t objcopy = plist_copy(object);
			nskeyedarchive_append_object(ka, objcopy);
			nskeyedarchive_merge_object(ka, pka, objcopy);
		}
		}
		break;
	case NSTYPE_FROM_PLIST:
		{
		plist_t plist = va_arg(*va, plist_t);
		if (!plist) {
			fprintf(stderr, "%s: ERROR: no plist argument given for NSTYPE_PLIST\n", __func__);
			return;
		}
		switch (plist_get_node_type(plist)) {
		case PLIST_STRING:
		{
			char* str = NULL;
			plist_get_string_val(plist, &str);
			nskeyedarchive_nsarray_append(ka, array, NSTYPE_NSMUTABLESTRING, str, NULL);
		} break;
		case PLIST_DICT:
		{
			plist_array_append_item(array, plist_new_uid(++ka->uid));
			newuid = ka->uid;
			nskeyedarchive_append_class(ka, "NSDictionary", "NSObject", NULL);

			plist_t keyarr = plist_new_array();
			plist_t valarr = plist_new_array();

			plist_dict_iter iter = NULL;
			plist_dict_new_iter(plist, &iter);

			char *key = NULL;
			plist_t node = NULL;

			do {
				plist_dict_next_item(plist, iter, &key, &node);
				if (key) {
					int ptype = 0;
					uint8_t bv = 0;
					uint64_t u64val = 0;
					int intval = 0;
					char *str = NULL;
					void *val = NULL;
					nskeyedarchive_nsarray_append(ka, keyarr, NSTYPE_STRING, key);
					switch (plist_get_node_type(node)) {
					case PLIST_BOOLEAN: {
						ptype = NSTYPE_BOOLEAN;
						plist_get_bool_val(node, &bv);
						intval = bv;
						val = &intval;
						} break;
					case PLIST_UINT:
						ptype = NSTYPE_INTEGER;
						plist_get_uint_val(node, &u64val);
						val = &u64val;
						break;
					case PLIST_STRING:
						ptype = NSTYPE_STRING;
						plist_get_string_val(node, &str);
						val = str;
						break;
					default:
						fprintf(stderr, "Unhandled plist type when parsing plist_dict\n");
						break;
					}
					nskeyedarchive_nsarray_append(ka, valarr, ptype, val);
				}
				free(key);
			} while (node);

			free(iter);

			nskeyedarchive_set_class_property(ka, newuid, "NS.keys", NSTYPE_ARRAY, keyarr);
			nskeyedarchive_set_class_property(ka, newuid, "NS.objects", NSTYPE_ARRAY, valarr);
			plist_free(keyarr);
			plist_free(valarr);
		} break;
		default:
			fprintf(stderr, "%s: ERROR: unhandled plist type %d\n", __func__, plist_get_node_type(plist));
			return;
		}
		}
		break;
	default:
		fprintf(stderr, "%s: unexpected type %d\n", __func__, type);
		break;
	}
}

static void nskeyedarchive_nsarray_append(nskeyedarchive_t ka, plist_t array, enum nskeyedarchive_class_type_t type, ...)
{
	va_list va;
	va_start(va, type);
	nskeyedarchive_nsarray_append_v(ka, array, type, &va);
	va_end(va);
}

void nskeyedarchive_nsarray_append_item(nskeyedarchive_t ka, uint64_t uid, enum nskeyedarchive_class_type_t type, ...)
{
	if (!ka) {
		return;
	}
	plist_t objects = NULL;
	nskeyedarchive_get_class_property(ka, uid, "NS.objects", &objects);
	if (!objects) {
		fprintf(stderr, "ERROR: invalid NSArray object in archive: missing NS.objects property\n");
		return;
	}
	va_list va;
	va_start(va, type);
	nskeyedarchive_nsarray_append_v(ka, objects, type, &va);
	va_end(va);
}

void nskeyedarchive_nsdictionary_add_item(nskeyedarchive_t ka, uint64_t uid, const char* key, enum nskeyedarchive_class_type_t type, ...)
{
	if (!ka) {
		return;
	}
	plist_t keys = NULL;
	nskeyedarchive_get_class_property(ka, uid, "NS.keys", &keys);
	if (!keys) {
		fprintf(stderr, "ERROR: invalid NSDictionary object in archive: missing NS.keys property\n");
		return;
	}
	plist_t objects = NULL;
	nskeyedarchive_get_class_property(ka, uid, "NS.objects", &objects);
	if (!objects) {
		fprintf(stderr, "ERROR: invalid NSDictionary object in archive: missing NS.objects property\n");
		return;
	}
	va_list va;
	va_start(va, type);
	nskeyedarchive_nsarray_append(ka, keys, NSTYPE_STRING, key);
	nskeyedarchive_nsarray_append_v(ka, objects, type, &va);
	va_end(va);
}

void nskeyedarchive_append_class_type_v(nskeyedarchive_t ka, enum nskeyedarchive_class_type_t type, va_list* va)
{
	uint64_t newuid = 0;

	switch (type) {
	case NSTYPE_INTEGER:
		fprintf(stderr, "%s: ERROR: NSTYPE_INTEGER is not an object type, can't add it as class!.\n", __func__);
		break;
	case NSTYPE_INTREF:
		nskeyedarchive_append_object(ka, plist_new_uint(va_arg(*va, int)));
		break;
	case NSTYPE_BOOLEAN:
		nskeyedarchive_append_object(ka, plist_new_bool(va_arg(*va, int)));
		break;
	case NSTYPE_CHARS:
		fprintf(stderr, "%s: ERROR: NSTYPE_CHARS is not an object type, can't add it as class!\n", __func__);
		break;
	case NSTYPE_STRING:
		{
		char* strval = va_arg(*va, char*);
		if (strval) {
			if (strcmp(strval, "$null") != 0) {
				nskeyedarchive_append_object(ka, plist_new_string(strval));
			} else {
				// make $null the top class if required
				plist_t top = plist_dict_get_item(ka->dict, "$top");
				if (!top) {
					top = plist_new_dict();
					plist_dict_set_item(top, "$0", plist_new_uid(0));
					plist_dict_set_item(ka->dict, "$top", top);
				}
			}
		}
		}
		break;
	case NSTYPE_REAL:
		nskeyedarchive_append_object(ka, plist_new_real(va_arg(*va, double)));
		break;
	case NSTYPE_ARRAY:
		fprintf(stderr, "%s: ERROR: NSTYPE_ARRAY is not an object type, can't add it as class!\n", __func__);
		return;
	case NSTYPE_NSMUTABLESTRING:
	case NSTYPE_NSSTRING:
		newuid = ka->uid;
		if (type == NSTYPE_NSMUTABLESTRING) {
			nskeyedarchive_append_class(ka, "NSMutableString", "NSString", "NSObject", NULL);
		} else {
			nskeyedarchive_append_class(ka, "NSString", "NSObject", NULL);
		}
		nskeyedarchive_set_class_property(ka, newuid, "NS.string", NSTYPE_STRING, va_arg(*va, char*));
		break;
	case NSTYPE_NSMUTABLEARRAY:
	case NSTYPE_NSARRAY:
		newuid = ka->uid;
		if (type == NSTYPE_NSMUTABLEARRAY) {
			nskeyedarchive_append_class(ka, "NSMutableArray", "NSArray", "NSObject", NULL);
		} else {
			nskeyedarchive_append_class(ka, "NSArray", "NSObject", NULL);
		}
		{
		plist_t arr = plist_new_array();
		do {
			int ptype = va_arg(*va, int);
			if (ptype == 0)
				 break;
			nskeyedarchive_nsarray_append_v(ka, arr, ptype, va);
		} while (1);
		nskeyedarchive_set_class_property(ka, newuid, "NS.objects", NSTYPE_ARRAY, arr);
		plist_free(arr);
		}
		break;
	case NSTYPE_NSMUTABLEDICTIONARY:
	case NSTYPE_NSDICTIONARY:
		newuid = ka->uid;
		if (type == NSTYPE_NSMUTABLEDICTIONARY) {
			nskeyedarchive_append_class(ka, "NSMutableDictionary", "NSDictionary", "NSObject", NULL);
		} else {
			nskeyedarchive_append_class(ka, "NSDictionary", "NSObject", NULL);
		}
		{
		plist_t keyarr = plist_new_array();
		plist_t valarr = plist_new_array();
		do {
			char* key = va_arg(*va, char*);
			if (!key)
				break;
			int ptype = va_arg(*va, int);
			if (ptype == 0)
				break;
			nskeyedarchive_nsarray_append(ka, keyarr, NSTYPE_STRING, key);
			nskeyedarchive_nsarray_append_v(ka, valarr, ptype, va);
		} while (1);
		nskeyedarchive_set_class_property(ka, newuid, "NS.keys", NSTYPE_ARRAY, keyarr);
		nskeyedarchive_set_class_property(ka, newuid, "NS.objects", NSTYPE_ARRAY, valarr);
		plist_free(keyarr);
		plist_free(valarr);
		}
		break;
	case NSTYPE_NSDATE:
		newuid = ka->uid;
		nskeyedarchive_append_class(ka, "NSDate", "NSObject", NULL);
		nskeyedarchive_set_class_property(ka, newuid, "NS.time", NSTYPE_REAL, va_arg(*va, double));
		break;
	case NSTYPE_NSMUTABLEDATA:
	case NSTYPE_NSDATA:
		newuid = ka->uid;
		nskeyedarchive_append_class(ka, "NSMutableData", "NSData", "NSObject", NULL);
		nskeyedarchive_set_class_property(ka, newuid, "NS.data", NSTYPE_DATA, va_arg(*va, char*));
		break;
	case NSTYPE_NSURL:
		newuid = ka->uid;
		nskeyedarchive_append_class(ka, "NSURL", "NSObject", NULL);
		{
		int ptype = va_arg(*va, int);
		if (ptype == 0)
			break;
		nskeyedarchive_set_class_property_v(ka, newuid, "NS.base", ptype, va);
		ptype = va_arg(*va, int);
		if (ptype == 0)
			break;
		nskeyedarchive_set_class_property_v(ka, newuid, "NS.relative",  ptype, va);
		}
		break;
	default:
		fprintf(stderr, "unexpected class type %d\n", type);
		return;
	}

	plist_t top = plist_dict_get_item(ka->dict, "$top");
	if (!top) {
		top = plist_new_dict();
		plist_dict_set_item(top, "$0", plist_new_uid(1));
		plist_dict_set_item(ka->dict, "$top", top);
	}
}

void nskeyedarchive_append_class_type(nskeyedarchive_t ka, enum nskeyedarchive_class_type_t type, ...)
{
	if (!ka) {
		fprintf(stderr, "%s: ERROR: invalid keyed archive!\n", __func__);
		return;
	} else if (!type) {
		fprintf(stderr, "%s: ERROR: invalid class type!\n", __func__);
		return;
	}

	va_list va;
	va_start(va, type);
	nskeyedarchive_append_class_type_v(ka, type, &va);
	va_end(va);
}

void nskeyedarchive_merge_object(nskeyedarchive_t ka, nskeyedarchive_t pka, plist_t object)
{
	if (!ka || !pka || !object) {
		return;
	}

	switch (plist_get_node_type(object)) {
	case PLIST_DICT:
		{
		plist_dict_iter iter = NULL;
		plist_dict_new_iter(object, &iter);
		if (iter) {
			plist_t val;
			char* key;
			do {
				key = NULL;
				val = NULL;
				plist_dict_next_item(object, iter, &key, &val);
				if (key) {
					switch (plist_get_node_type(val)) {
					case PLIST_UID:
						{
						uint64_t thisuid = 0;
						plist_get_uid_val(val, &thisuid);
						if (thisuid > 0) {
							// remap thisuid to ka->uid+1
							plist_t nextobj = nskeyedarchive_get_object_by_uid(pka, thisuid);
							plist_set_uid_val(val, ++ka->uid);
							plist_t nextcopy = plist_copy(nextobj);
							nskeyedarchive_append_object(ka, nextcopy);
							nskeyedarchive_merge_object(ka, pka, nextcopy);
						}
						}
						break;
					case PLIST_DICT:
					case PLIST_ARRAY:
						nskeyedarchive_merge_object(ka, pka, val);
						break;
					default:
						break;
					}
					free(key);
				}
			} while (val);
			free(iter);
		}
		}
		break;
	case PLIST_ARRAY:
		{
		uint32_t i;
		for (i = 0; i < plist_array_get_size(object); i++) {
			plist_t val = plist_array_get_item(object, i);
			switch (plist_get_node_type(val)) {
			case PLIST_UID:
				{
				uint64_t thisuid = 0;
				plist_get_uid_val(val, &thisuid);
				if (thisuid > 0) {
					// remap thisuid to ka->uid+1
					plist_t nextobj = nskeyedarchive_get_object_by_uid(pka, thisuid);
					plist_set_uid_val(val, ++ka->uid);
					plist_t nextcopy = plist_copy(nextobj);
					nskeyedarchive_append_object(ka, nextcopy);
					nskeyedarchive_merge_object(ka, pka, nextcopy);
				}
				}
				break;
			case PLIST_ARRAY:
			case PLIST_DICT:
				nskeyedarchive_merge_object(ka, pka, val);
				break;
			default:
				break;
			}
		}
		}
		break;
	default:
		break;
	}

}

static void nskeyedarchive_set_class_property_v(nskeyedarchive_t ka, uint64_t uid, const char* propname, enum nskeyedarchive_class_type_t proptype, va_list* va)
{
	plist_t a = nskeyedarchive_get_class_by_uid(ka, uid);
	if (a == NULL)
		return;

	uint64_t newuid;

	switch (proptype) {
	case NSTYPE_INTEGER:
		plist_dict_set_item(a, propname, plist_new_uint(va_arg(*va, int)));
		break;
	case NSTYPE_INTREF:
		plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
		nskeyedarchive_append_object(ka, plist_new_uint(va_arg(*va, int)));
		break;
	case NSTYPE_BOOLEAN:
		plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
		nskeyedarchive_append_object(ka, plist_new_bool(va_arg(*va, int)));
		break;
	case NSTYPE_CHARS:
		plist_dict_set_item(a, propname, plist_new_string(va_arg(*va, char*)));
		break;
	case NSTYPE_STRING:
		{
		char* strval = va_arg(*va, char*);
		if (strval && (strcmp(strval, "$null") == 0)) {
			plist_dict_set_item(a, propname, plist_new_uid(0));
		} else {
			plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
			nskeyedarchive_append_object(ka, plist_new_string(strval));
		}
		}
		break;
	case NSTYPE_REAL:
		plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
		nskeyedarchive_append_class_type_v(ka, proptype, va);
		break;
	case NSTYPE_ARRAY:
		plist_dict_set_item(a, propname, plist_copy(va_arg(*va, plist_t)));
		break;
	case NSTYPE_NSMUTABLESTRING:
	case NSTYPE_NSSTRING:
		plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
		nskeyedarchive_append_class_type_v(ka, proptype, va);
		break;
	case NSTYPE_NSMUTABLEARRAY:
	case NSTYPE_NSARRAY:
		plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
		nskeyedarchive_append_class_type_v(ka, proptype, va);
		break;
	case NSTYPE_NSMUTABLEDICTIONARY:
	case NSTYPE_NSDICTIONARY:
		plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
		nskeyedarchive_append_class_type_v(ka, proptype, va);
		break;
	case NSTYPE_NSDATE:
		plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
		nskeyedarchive_append_class_type_v(ka, proptype, va);
		break;
	case NSTYPE_NSMUTABLEDATA:
	case NSTYPE_NSDATA:
		plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
		nskeyedarchive_append_class_type_v(ka, proptype, va);
		break;
	case NSTYPE_NSURL:
		plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
		nskeyedarchive_append_class_type_v(ka, proptype, va);
		break;
	case NSTYPE_NSKEYEDARCHIVE:
		{
		nskeyedarchive_t pka = va_arg(*va, nskeyedarchive_t);
		if (!pka) {
			fprintf(stderr, "%s: ERROR: no nskeyedarchive argument given for type NSTYPE_NSKEYEDARCHIVE\n", __func__);
			return;
		}
		uint64_t top = nskeyedarchive_get_class_uid(pka, NULL);
		if (top != 0) {
			plist_t object = nskeyedarchive_get_object_by_uid(pka, top);
			if (!object) {
				fprintf(stderr, "%s: ERROR: can't get object for uid %lld\n", __func__, (long long)top);
				return;
			}

			plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
			plist_t objcopy = plist_copy(object);
			nskeyedarchive_append_object(ka, objcopy);
			nskeyedarchive_merge_object(ka, pka, objcopy);
		} else {
			plist_dict_set_item(a, propname, plist_new_uid(0));
		}
		}
		break;
	case NSTYPE_FROM_PLIST:
		{
		plist_t plist = va_arg(*va, plist_t);
		if (!plist) {
			fprintf(stderr, "%s: ERROR: no plist argument given for NSTYPE_PLIST\n", __func__);
			return;
		}
		switch (plist_get_node_type(plist)) {
		case PLIST_ARRAY:
			plist_dict_set_item(a, propname, plist_new_uid(++ka->uid));
			newuid = ka->uid;
			nskeyedarchive_append_class(ka, "NSMutableArray", "NSArray", "NSObject", NULL);
			{
			plist_t arr = plist_new_array();
			uint32_t ai;
			for (ai = 0; ai < plist_array_get_size(plist); ai++) {
				plist_t ae = plist_array_get_item(plist, ai);
				nskeyedarchive_nsarray_append(ka, arr, NSTYPE_FROM_PLIST, ae);
			}
			nskeyedarchive_set_class_property(ka, newuid, "NS.objects", NSTYPE_ARRAY, arr);
			}
			break;
		default:
			fprintf(stderr, "%s: sorry, plist type %d is not implemented for conversion.\n", __func__, plist_get_node_type(plist));
			break;
		}
		}
		break;
	default:
		fprintf(stderr, "unexpected property type %d\n", proptype);
		break;
	}
}

void nskeyedarchive_set_class_property(nskeyedarchive_t ka, uint64_t uid, const char* propname, enum nskeyedarchive_class_type_t proptype, ...)
{
	plist_t a = nskeyedarchive_get_class_by_uid(ka, uid);
	if (a == NULL) {
		return;
	}

	va_list va;
	va_start(va, proptype);
	nskeyedarchive_set_class_property_v(ka, uid, propname, proptype, &va);
	va_end(va);
}


void nskeyedarchive_print(nskeyedarchive_t ka)
{
	char* xml = NULL;
	uint32_t xlen = 0;
	plist_to_xml(ka->dict, &xml, &xlen);
	puts(xml);
	free(xml);
}

plist_t nskeyedarchive_get_plist_ref(nskeyedarchive_t ka)
{
	return ka->dict;
}

nskeyedarchive_t nskeyedarchive_new_from_plist(plist_t plist)
{
	if (!plist || (plist_get_node_type(plist) != PLIST_DICT)) {
		fprintf(stderr, "%s: ERROR: invalid parameter, PLIST_DICT expected\n", __func__);
		return NULL;
	}

	plist_t node;
	char* strval = NULL;
	uint64_t uintval = 0;

	// check for $archiver key
	node = plist_dict_get_item(plist, "$archiver");
	if (node && (plist_get_node_type(node) == PLIST_STRING)) {
		plist_get_string_val(node, &strval);
	}
	if (!strval || (strcmp(strval, "NSKeyedArchiver") != 0)) {
		fprintf(stderr, "%s: ERROR: plist is not in NSKeyedArchiver format ($archiver key not found or invalid)!\n", __func__);
		if (strval)
			free(strval);
		return NULL;
	}
	if (strval)
		free(strval);
	strval = NULL;

	// check for $version key
	node = plist_dict_get_item(plist, "$version");
	if (node && (plist_get_node_type(node) == PLIST_UINT)) {
		plist_get_uint_val(node, &uintval);
	}
	if (uintval != 100000) {
		fprintf(stderr, "%s: ERROR: unexpected NSKeyedArchiver version encountered (%lld != 100000)!\n", __func__, (long long int)uintval);
		return NULL;
	}
	uintval = 0;

	// check for $top key
	node = plist_dict_get_item(plist, "$top");
	if (!node || (plist_get_node_type(node) != PLIST_DICT)) {
		fprintf(stderr, "%s: ERROR: $top node not found\n", __func__);
		return NULL;
	}
	plist_t topuid = plist_dict_get_item(node, "$0");
	if (!topuid) {
		topuid = plist_dict_get_item(node, "root");
	}
	if (!topuid || (plist_get_node_type(topuid) != PLIST_UID)) {
		fprintf(stderr, "%s: ERROR: uid '$0' or 'root' not found in $top dict!\n", __func__);
		return NULL;
	}

	uintval = -1LL;
	plist_get_uid_val(topuid, (uint64_t*)&uintval);

	if (uintval == (uint64_t)-1LL) {
		fprintf(stderr, "%s: ERROR: could not get UID value.\n", __func__);
		return NULL;
	}

	// check for $objects key
	plist_t objects = plist_dict_get_item(plist, "$objects");
	if (!objects || (plist_get_node_type(objects) != PLIST_ARRAY)) {
		fprintf(stderr, "%s: ERROR: $objects node not found!\n", __func__);
		return NULL;
	}
	plist_t obj = plist_array_get_item(objects, (uint32_t)uintval);
	if (!obj) {
		fprintf(stderr, "%s: ERROR: can't get object node\n", __func__);
		return NULL;
	}

	nskeyedarchive_t archive = (nskeyedarchive_t)malloc(sizeof(struct nskeyedarchive_st));
	archive->dict = plist_copy(plist);
	archive->uid = plist_array_get_size(objects) - 1;

	return archive;
}

nskeyedarchive_t nskeyedarchive_new_from_data(const void* data, uint32_t size)
{
	if (!data || (size < 8)) {
		fprintf(stderr, "%s: ERROR: invalid parameter\n", __func__);
		return NULL;
	}


	plist_t plist = NULL;
	if (memcmp(data, "bplist00", 8) == 0) {
		plist_from_bin(data, size, &plist);
	} else if ((memcmp(data, "<?xml", 5) == 0) || (memcmp(data, "<plist", 6) == 0)) {
		plist_from_xml(data, size, &plist);
	} else {
		// fail silently
		return NULL;
	}
	if (!plist) {
		fprintf(stderr, "%s: ERROR: Can't parse plist from data\n", __func__);
		return NULL;
	}

	nskeyedarchive_t ka = nskeyedarchive_new_from_plist(plist);
	plist_free(plist);
	return ka;
}

uint64_t nskeyedarchive_get_class_uid(nskeyedarchive_t ka, const char* classref)
{
	uint64_t uintval = -1LL;
	if (!ka) {
		return uintval;
	}
	if (!ka->dict) {
		return uintval;
	}

	plist_t node;

	node = plist_dict_get_item(ka->dict, "$top");
	if (!node || (plist_get_node_type(node) != PLIST_DICT)) {
		fprintf(stderr, "%s: ERROR: $top node not found\n", __func__);
		return 0;
	}
	plist_t topuid = plist_dict_get_item(node, (classref) ? classref : "$0");
	if (!topuid && !classref) {
		topuid = plist_dict_get_item(node, "root");
	}
	if (!topuid || (plist_get_node_type(topuid) != PLIST_UID)) {
		fprintf(stderr, "%s: ERROR: uid for '%s' not found in $top dict!\n", __func__, classref);
		return 0;
	}

	plist_get_uid_val(topuid, (uint64_t*)&uintval);

	return uintval;
}

const char* nskeyedarchive_get_classname(nskeyedarchive_t ka, uint64_t uid)
{
	if (!ka) {
		return NULL;
	}
	if (!ka->dict) {
		return NULL;
	}

	plist_t obj = nskeyedarchive_get_object_by_uid(ka, uid);
	if (!obj) {
		return NULL;
	}

	plist_t classuid = plist_dict_get_item(obj, "$class");
	if (plist_get_node_type(classuid) != PLIST_UID) {
		fprintf(stderr, "ERROR: $class is not a uid node\n");
		return NULL;
	}

	uint64_t uintval = 0;
	plist_get_uid_val(classuid, &uintval);
	if (uintval == 0) {
		fprintf(stderr, "ERROR: can't get $class uid val\n");
		return NULL;
	}

	plist_t cls = nskeyedarchive_get_class_by_uid(ka, uintval);
	if (!cls) {
		return NULL;
	}

	plist_t classname = plist_dict_get_item(cls, "$classname");
	if (!classname || (plist_get_node_type(classname) != PLIST_STRING)) {
		fprintf(stderr, "ERROR: invalid $classname in class dict\n");
		return NULL;
	}

	return plist_get_string_ptr(classname, NULL);
}

int nskeyedarchive_get_class_property(nskeyedarchive_t ka, uint64_t uid, const char* propname, plist_t* value)
{
	if (!ka || !ka->dict || !value) {
		return -1;
	}

	plist_t obj = nskeyedarchive_get_class_by_uid(ka, uid);
	if (!obj) {
		return -1;
	}

	*value = plist_dict_get_item(obj, propname);
	if (!*value) {
		return -1;
	}

	return 0;
}

int nskeyedarchive_get_class_uint64_property(nskeyedarchive_t ka, uint64_t uid, const char* propname, uint64_t* value)
{
	plist_t prop = NULL;

	nskeyedarchive_get_class_property(ka, uid, propname, &prop);
	if (!prop) {
		fprintf(stderr, "%s: ERROR: no such property '%s'\n", __func__, propname);
		return -1;
	}

	if (plist_get_node_type(prop) == PLIST_UID) {
		uint64_t uintval = 0;
		plist_get_uid_val(prop, &uintval);
		prop = nskeyedarchive_get_object_by_uid(ka, uintval);
	}

	if (plist_get_node_type(prop) != PLIST_UINT) {
		fprintf(stderr, "%s: ERROR: property '%s' is not of type integer.\n", __func__, propname);
		return -1;
	}

	plist_get_uint_val(prop, value);

	return 0;
}

int nskeyedarchive_get_class_int_property(nskeyedarchive_t ka, uint64_t uid, const char* propname, int* value)
{
	uint64_t uintval = 0;
	int res = nskeyedarchive_get_class_uint64_property(ka, uid, propname, &uintval);
	if (res < 0) {
		return res;
	}
	*value = (int)uintval;
	return 0;
}

int nskeyedarchive_get_class_string_property(nskeyedarchive_t ka, uint64_t uid, const char* propname, char** value)
{
	plist_t node = NULL;

	nskeyedarchive_get_class_property(ka, uid, propname, &node);
	if (!node || (plist_get_node_type(node) != PLIST_UID)) {
		return -1;
	}

	uint64_t uintval = 0;
	plist_get_uid_val(node, &uintval);

	plist_t prop = nskeyedarchive_get_object_by_uid(ka, uintval);
	if (!prop || (plist_get_node_type(prop) != PLIST_STRING)) {
		fprintf(stderr, "%s: ERROR: property '%s' is not a string\n", __func__, propname);
		return -1;
	}

	plist_get_string_val(prop, value);

	return 0;
}

static plist_t _nska_parse_object(nskeyedarchive_t ka, uint64_t uid)
{
	plist_t obj = nskeyedarchive_get_object_by_uid(ka, uid);
	switch (plist_get_node_type(obj)) {
		case PLIST_BOOLEAN:
		case PLIST_INT:
		case PLIST_STRING:
			return plist_copy(obj);
		default:
			break;
	}

	const char* clsn = nskeyedarchive_get_classname(ka, uid);
	plist_t pl = NULL;
	if (!strcmp(clsn, "NSMutableDictionary") || !strcmp(clsn, "NSDictionary")) {
		plist_t keys = NULL;
		nskeyedarchive_get_class_property(ka, uid, "NS.keys", &keys);
		plist_t vals = NULL;
		nskeyedarchive_get_class_property(ka, uid, "NS.objects", &vals);

		uint32_t count = plist_array_get_size(keys);
		if (count != plist_array_get_size(vals)) {
			printf("ERROR: %s: inconsistent number of keys vs. values in dictionary object\n", __func__);
			return NULL;
		}
		pl = plist_new_dict();
		uint32_t i = 0;
		for (i = 0; i < count; i++) {
			plist_t knode = plist_array_get_item(keys, i);
			plist_t vnode = plist_array_get_item(vals, i);

			uint64_t subuid = 0;
			plist_get_uid_val(knode, &subuid);
			plist_t newkey = _nska_parse_object(ka, subuid);

			subuid = 0;
			plist_get_uid_val(vnode, &subuid);
			plist_t newval = _nska_parse_object(ka, subuid);

			if (!PLIST_IS_STRING(newkey)) {
				printf("ERROR: %s: key node is not of string type.\n", __func__);
				return NULL;
			}

			plist_dict_set_item(pl, plist_get_string_ptr(newkey, NULL), newval);
			plist_free(newkey);
		}
	} else if (!strcmp(clsn, "NSMutableArray") || !strcmp(clsn, "NSArray")) {
		plist_t vals = NULL;
		nskeyedarchive_get_class_property(ka, uid, "NS.objects", &vals);

		uint32_t count = plist_array_get_size(vals);
		pl = plist_new_array();
		uint32_t i = 0;
		for (i = 0; i < count; i++) {
			plist_t vnode = plist_array_get_item(vals, i);
			uint64_t subuid = 0;
			plist_get_uid_val(vnode, &subuid);
			plist_t newval = _nska_parse_object(ka, subuid);
			plist_array_append_item(pl, newval);
		}	
	} else {
		printf("ERROR: %s: unhandled class type '%s'\n", __func__, clsn);
	}
	return pl;
}

plist_t nskeyedarchive_to_plist(nskeyedarchive_t ka)
{
	uint64_t obj_uid = nskeyedarchive_get_class_uid(ka, NULL);
	return _nska_parse_object(ka, obj_uid);
}
