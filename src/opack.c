/*
 * opack.c
 * "opack" format encoder/decoder implementation.
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
#include "libimobiledevice-glue/cbuf.h"
#include "libimobiledevice-glue/opack.h"
#include "endianness.h"

#define MAC_EPOCH 978307200

static void opack_encode_node(plist_t node, struct char_buf* cbuf)
{
	plist_type type = plist_get_node_type(node);
	switch (type) {
		case PLIST_DICT: {
			uint32_t count = plist_dict_get_size(node);
			uint8_t blen = 0xEF;
			if (count < 15)
				blen = (uint8_t)count-32;	
			char_buf_append(cbuf, 1, &blen);
			plist_dict_iter iter = NULL;
			plist_dict_new_iter(node, &iter);
			if (iter) {
				plist_t sub = NULL;
				do {
					sub = NULL;
					plist_dict_next_item(node, iter, NULL, &sub);
					if (sub) {
						plist_t key = plist_dict_item_get_key(sub);
						opack_encode_node(key, cbuf);
						opack_encode_node(sub, cbuf);
					}
				} while (sub);
				free(iter);
				if (count > 14) {
					uint8_t term = 0x03;
					char_buf_append(cbuf, 1, &term);
				}
			}
		}	break;
		case PLIST_ARRAY: {
			uint32_t count = plist_array_get_size(node);
			uint8_t blen = 0xDF;
			if (count < 15)
				blen = (uint8_t)(count-48);
			char_buf_append(cbuf, 1, &blen);
			plist_array_iter iter = NULL;
			plist_array_new_iter(node, &iter);
			if (iter) {
				plist_t sub = NULL;
				do {
					sub = NULL;
					plist_array_next_item(node, iter, &sub);
					if (sub) {
						opack_encode_node(sub, cbuf);
					}
				} while (sub);
				free(iter);
				if (count > 14) {
					uint8_t term = 0x03;
					char_buf_append(cbuf, 1, &term);
				}
			}
		}	break;
		case PLIST_BOOLEAN: {
			uint8_t bval = 2 - plist_bool_val_is_true(node);
			char_buf_append(cbuf, 1, &bval);
		}	break;
		case PLIST_UINT: {
			uint64_t u64val = 0;
			plist_get_uint_val(node, &u64val);
			if ((uint8_t)u64val == u64val) {
				uint8_t u8val = (uint8_t)u64val;
				if (u8val > 0x27) {
					uint8_t blen = 0x30;
					char_buf_append(cbuf, 1, &blen);
					char_buf_append(cbuf, 1, &u8val);
				} else {
					u8val += 8;
					char_buf_append(cbuf, 1, &u8val);
				}
			} else if ((uint32_t)u64val == u64val) {
				uint8_t blen = 0x32;
				char_buf_append(cbuf, 1, &blen);
				uint32_t u32val = (uint32_t)u64val;
				u32val = htole32(u32val);
				char_buf_append(cbuf, 4, (unsigned char*)&u32val);
			} else {
				uint8_t blen = 0x33;
				char_buf_append(cbuf, 1, &blen);
				u64val = htole64(u64val);
				char_buf_append(cbuf, 8, (unsigned char*)&u64val);
			}
		}	break;
		case PLIST_REAL: {
			double dval = 0;
			plist_get_real_val(node, &dval);
			if ((float)dval == dval) {
				float fval = (float)dval;
				uint32_t u32val = 0;
				memcpy(&u32val, &fval, 4);
				u32val = float_bswap32(u32val);
				uint8_t blen = 0x35;
				char_buf_append(cbuf, 1, &blen);
				char_buf_append(cbuf, 4, (unsigned char*)&u32val);
			} else {
				uint64_t u64val = 0;
				memcpy(&u64val, &dval, 8);
				u64val = float_bswap64(u64val);
				uint8_t blen = 0x36;
				char_buf_append(cbuf, 1, &blen);
				char_buf_append(cbuf, 8, (unsigned char*)&u64val);
			}
		}	break;
		case PLIST_DATE: {
			int32_t sec = 0;
			int32_t usec = 0;	
			plist_get_date_val(node, &sec, &usec);
			time_t tsec = sec;
			tsec -= MAC_EPOCH;
			double dval = (double)tsec + ((double)usec / 1000000);
			uint8_t blen = 0x06;
			char_buf_append(cbuf, 1, &blen);
			uint64_t u64val = 0;
			memcpy(&u64val, &dval, 8);
			u64val = float_bswap64(u64val);
			char_buf_append(cbuf, 8, (unsigned char*)&u64val);
		}	break;
		case PLIST_STRING:
		case PLIST_KEY: {
			uint64_t len = 0;
			char* str = NULL;
			if (type == PLIST_KEY) {
				plist_get_key_val(node, &str);
				len = strlen(str);
			} else {
				str = (char*)plist_get_string_ptr(node, &len);
			}
			if (len > 0x20) {
				if (len > 0xFF) {
					if (len > 0xFFFF) {
						if (len >> 32) {
							uint8_t blen = 0x64;
							char_buf_append(cbuf, 1, &blen);
							uint64_t u64val = htole64(len);
							char_buf_append(cbuf, 8, (unsigned char*)&u64val);
						} else {
							uint8_t blen = 0x63;
							char_buf_append(cbuf, 1, &blen);
							uint32_t u32val = htole32((uint32_t)len);
							char_buf_append(cbuf, 4, (unsigned char*)&u32val);
						}
					} else {
						uint8_t blen = 0x62;
						char_buf_append(cbuf, 1, &blen);
						uint16_t u16val = htole16((uint16_t)len);
						char_buf_append(cbuf, 2, (unsigned char*)&u16val);
					}
				} else {
					uint8_t blen = 0x61;
					char_buf_append(cbuf, 1, &blen);
					char_buf_append(cbuf, 1, (unsigned char*)&len);
				}
			} else {
				uint8_t blen = 0x40 + len;
				char_buf_append(cbuf, 1, &blen);
			}
			char_buf_append(cbuf, len, (unsigned char*)str);
			if (type == PLIST_KEY) {
				free(str);
			}
		}	break;
		case PLIST_DATA: {
			uint64_t len = 0;
			const char* data = plist_get_data_ptr(node, &len);
			if (len > 0x20) {
				if (len > 0xFF) {
					if (len > 0xFFFF) {
						if (len >> 32) {
							uint8_t blen = 0x94;
							char_buf_append(cbuf, 1, &blen);
							uint64_t u64val = htole64(len);
							char_buf_append(cbuf, 8, (unsigned char*)&u64val);
						} else {
							uint8_t blen = 0x93;
							char_buf_append(cbuf, 1, &blen);
							uint32_t u32val = htole32((uint32_t)len);
							char_buf_append(cbuf, 4, (unsigned char*)&u32val);
						}
					} else {
						uint8_t blen = 0x92;
						char_buf_append(cbuf, 1, &blen);
						uint16_t u16val = htole16((uint16_t)len);
						char_buf_append(cbuf, 2, (unsigned char*)&u16val);
					}
				} else {
					uint8_t blen = 0x91;
					char_buf_append(cbuf, 1, &blen);
					char_buf_append(cbuf, 1, (unsigned char*)&len);
				}
			} else {
				uint8_t blen = 0x70 + len;
				char_buf_append(cbuf, 1, &blen);
			}
			char_buf_append(cbuf, len, (unsigned char*)data);
		}	break;
		default:
			fprintf(stderr, "%s: ERROR: Unsupported data type in plist\n", __func__);
			break;
	}
}

LIBIMOBILEDEVICE_GLUE_API void opack_encode_from_plist(plist_t plist, unsigned char** out, unsigned int* out_len)
{
	if (!plist || !out || !out_len) {
		return;
	}
	struct char_buf* cbuf = char_buf_new();
	opack_encode_node(plist, cbuf);
	*out = cbuf->data;
	*out_len = cbuf->length;
	cbuf->data = NULL;
	char_buf_free(cbuf);
}

static int opack_decode_obj(unsigned char** p, unsigned char* end, plist_t* plist_out, uint32_t level)
{
	uint8_t type = **p;
	if (type == 0x02) {
		/* bool: false */
		*plist_out = plist_new_bool(0);
		(*p)++;
		return 0;
	} else if (type == 0x01) {
		/* bool: true */
		*plist_out = plist_new_bool(1);
		(*p)++;
		return 0;
	} else if (type == 0x03) {
		/* NULL / structured type child node terminator */
		(*p)++;
		return -2;
	} else if (type == 0x06) {
		/* date type */
		(*p)++;
		double value = *(double*)*p;
		time_t sec = (time_t)value;
		value -= sec;
		uint32_t usec = value * 1000000;
		(*p)+=8;
		*plist_out = plist_new_date(sec, usec);
	} else if (type >= 0x08 && type <= 0x36) {
		/* numerical type */
		(*p)++;
		uint64_t value = 0;
		if (type == 0x36) {
			/* double */
			uint64_t u64val = float_bswap64(*(uint64_t*)(*p));
			(*p)+=8;
			double dval = 0;
			memcpy(&dval, &u64val, 8);
			*plist_out = plist_new_real(dval);
			return 0;
		} else if (type == 0x35) {
			/* float */
			uint32_t u32val = float_bswap32(*(uint32_t*)(*p));
			(*p)+=4;
			float fval = 0;
			memcpy(&fval, &u32val, 4);
			*plist_out = plist_new_real((double)fval);
			return 0;
		} else if (type < 0x30) {
			value = type - 8;
		} else if (type == 0x30) {
			value = (int8_t)**p;
			(*p)++;
		} else if (type == 0x32) {
			uint32_t u32val = *(uint32_t*)*p;
			value = (int32_t)le32toh(u32val);
			(*p)+=4;
		} else if (type == 0x33) {
			uint64_t u64val = *(uint64_t*)*p;
			value = le64toh(u64val);
			(*p)+=8;
		} else {
			fprintf(stderr, "%s: ERROR: Invalid encoded byte '%02x'\n", __func__, type);
			*p = end;
			return -1;
		}
		*plist_out = plist_new_uint(value);
	} else if (type >= 0x40 && type <= 0x64) {
		/* string */
		(*p)++;
		size_t slen = 0;
		if (type < 0x61) {
			slen = type - 0x40;
		} else {
			if (type == 0x61) {
				slen = **p;
				(*p)++;
			} else if (type == 0x62) {
				uint16_t u16val = *(uint16_t*)*p;
				slen = le16toh(u16val);
				(*p)+=2;
			} else if (type == 0x63) {
				uint32_t u32val = *(uint32_t*)*p;
				slen = le32toh(u32val);
				(*p)+=4;
			} else if (type == 0x64) {
				uint64_t u64val = *(uint64_t*)*p;
				slen = le64toh(u64val);
				(*p)+=8;
			}
		}
		if (*p + slen > end) {
			fprintf(stderr, "%s: ERROR: Size points past end of data\n", __func__);
			*p = end;
			return -1;
		}
		char* str = malloc(slen+1);
		strncpy(str, (const char*)*p, slen);
		str[slen] = '\0';
		*plist_out = plist_new_string(str);
		(*p)+=slen;
		free(str);
	} else if (type >= 0x70 && type <= 0x94) {
		/* data */
		(*p)++;
		size_t dlen = 0;
		if (type < 0x91) {
			dlen = type - 0x70;
		} else {
			if (type == 0x91) {
				dlen = **p;
				(*p)++;
			} else if (type == 0x92) {
				uint16_t u16val = *(uint16_t*)*p;
				dlen = le16toh(u16val);
				(*p)+=2;
			} else if (type == 0x93) {
				uint32_t u32val = *(uint32_t*)*p;
				dlen = le32toh(u32val);
				(*p)+=4;
			} else if (type == 0x94) {
				uint64_t u64val = *(uint64_t*)*p;
				dlen = le64toh(u64val);
				(*p)+=8;
			}
		}
		if (*p + dlen > end) {
			fprintf(stderr, "%s: ERROR: Size points past end of data\n", __func__);
			*p = end;
			return -1;
		}
		*plist_out = plist_new_data((const char*)*p, dlen);
		(*p)+=dlen;
	} else if (type >= 0xE0 && type <= 0xEF) {
		/* dictionary */
		(*p)++;
		plist_t dict = plist_new_dict();
		uint32_t num_children = 0xFFFFFFFF;
		if (type < 0xEF) {
			/* explicit number of children */
			num_children = type - 0xE0;
		}
		if (!*plist_out) {
			*plist_out = dict;	
		}
		uint32_t i = 0;
		while (i++ < num_children) {
			plist_t keynode = NULL;
			int res = opack_decode_obj(p, end, &keynode, level+1);
			if (res == -2) {
				break;
			} else if (res < 0) {
				return -1;
			}
			if (!PLIST_IS_STRING(keynode)) {
				plist_free(keynode);
				fprintf(stderr, "%s: ERROR: Invalid node type for dictionary key node\n", __func__);
				*p = end;
				return -1;
			}
			char* key = NULL;
			plist_get_string_val(keynode, &key);
			plist_free(keynode);
			plist_t valnode = NULL;
			if (opack_decode_obj(p, end, &valnode, level+1) < 0) {
				free(key);
				return -1;
			}
			plist_dict_set_item(dict, key, valnode);
		}
		if (level == 0) {
			*p = end;
			return 0;
		}
	} else if (type >= 0xD0 && type <= 0xDF) {
		/* array */
		(*p)++;
		plist_t array = plist_new_array();
		if (!*plist_out) {
			*plist_out = array;
		}
		uint32_t num_children = 0xFFFFFFFF;
		if (type < 0xDF) {
			/* explicit number of children */
			num_children = type - 0xD0;
		}
		uint32_t i = 0;
		while (i++ < num_children) {
			plist_t child = NULL;
			int res = opack_decode_obj(p, end, &child, level+1);
			if (res == -2) {
				if (type < 0xDF) {
					fprintf(stderr, "%s: ERROR: Expected child node, found terminator\n", __func__);
					*p = end;
					return -1;
				}
				break;
			} else if (res < 0) {
				return -1;
			}
			plist_array_append_item(array, child);
		}
		if (level == 0) {
			*p = end;
			return 0;
		}
	} else {
		fprintf(stderr, "%s: ERROR: Unexpected character '%02x encountered\n", __func__, type);
		*p = end;
		return -1;
	}
	return 0;
}

LIBIMOBILEDEVICE_GLUE_API int opack_decode_to_plist(unsigned char* buf, unsigned int buf_len, plist_t* plist_out)
{
	if (!buf || buf_len == 0 || !plist_out) {
		return -1;
	}
	unsigned char* p = buf;
	unsigned char* end = buf + buf_len;
	while (p < end) {
		opack_decode_obj(&p, end, plist_out, 0);
	}
	return 0;
}
