/*
 * termcolors.c
 *
 * Copyright (c) 2020-2021 Nikias Bassen, All Rights Reserved.
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
#include "config.h"
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "libimobiledevice-glue/termcolors.h"

static int use_colors = 0;

#ifdef WIN32
static int WIN32_LEGACY_MODE = 1;
static WORD COLOR_RESET_ATTR = 0;
static WORD DEFAULT_FG_ATTR = 0;
static WORD DEFAULT_BG_ATTR = 0;
static HANDLE h_stdout = INVALID_HANDLE_VALUE;
static HANDLE h_stderr = INVALID_HANDLE_VALUE;

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#define STYLE_DIM (1 << 16)

#define FG_COLOR_MASK (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define BG_COLOR_MASK (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE)
#define FG_COLOR_ATTR_MASK (FG_COLOR_MASK | FOREGROUND_INTENSITY)
#define BG_COLOR_ATTR_MASK (BG_COLOR_MASK | BACKGROUND_INTENSITY)

static int style_map[9] = {
	/* RESET ALL   */ 0,
	/* BRIGHT      */ FOREGROUND_INTENSITY,
	/* DIM         */ STYLE_DIM,
	/* (n/a)       */ 0,
        /* UNDERLINED  */ 0, // COMMON_LVB_UNDERSCORE ?
	/* BLINK       */ 0, // NOT SUPPORTED
	/* (n/a)       */ 0,
	/* REVERSE CLR */ 0, // COMMON_LVB_REVERSE_VIDEO ?
	/* HIDDEN      */ 0  // NOT SUPPORTED
};

static int fgcolor_map[8] = {
	/* BLACK   */ 0,
	/* RED     */ FOREGROUND_RED,
	/* GREEN   */ FOREGROUND_GREEN,
	/* YELLOW  */ FOREGROUND_GREEN | FOREGROUND_RED,
	/* BLUE    */ FOREGROUND_BLUE,
	/* MAGENTA */ FOREGROUND_BLUE | FOREGROUND_RED,
	/* CYAN    */ FOREGROUND_BLUE | FOREGROUND_GREEN,
	/* WHITE   */ FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
};
static int bgcolor_map[8] = {
	/* BLACK   */ 0,
	/* RED     */ BACKGROUND_RED,
	/* GREEN   */ BACKGROUND_GREEN,
	/* YELLOW  */ BACKGROUND_GREEN | BACKGROUND_RED,
	/* BLUE    */ BACKGROUND_BLUE,
	/* MAGENTA */ BACKGROUND_BLUE | BACKGROUND_RED,
	/* CYAN    */ BACKGROUND_BLUE | BACKGROUND_GREEN,
	/* WHITE   */ BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE
};
#else
static int WIN32_LEGACY_MODE = 0;
#endif

LIBIMOBILEDEVICE_GLUE_API void term_colors_init()
{	
#ifdef WIN32
	DWORD conmode = 0;
	h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleMode(h_stdout, &conmode);
	if (conmode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
		WIN32_LEGACY_MODE = 0;
	} else {
		conmode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (SetConsoleMode(h_stdout, conmode)) {
			WIN32_LEGACY_MODE = 0;
		} else {
			WIN32_LEGACY_MODE = 1;
		}
	}
	if (WIN32_LEGACY_MODE) {
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (GetConsoleScreenBufferInfo(h_stdout, &csbi)) {
			COLOR_RESET_ATTR = csbi.wAttributes;
			DEFAULT_FG_ATTR = csbi.wAttributes & FG_COLOR_ATTR_MASK;
			DEFAULT_BG_ATTR = csbi.wAttributes & BG_COLOR_ATTR_MASK;
		}
		h_stderr = GetStdHandle(STD_ERROR_HANDLE);
	}
#endif
	use_colors = isatty(1);
	const char* color_env = getenv("COLOR");
	if (color_env) {
		long val = strtol(color_env, NULL, 10);
		use_colors = (val != 0);
	}
}

LIBIMOBILEDEVICE_GLUE_API void term_colors_set_enabled(int en)
{
	use_colors = en;
}

LIBIMOBILEDEVICE_GLUE_API int cvfprintf(FILE* stream, const char* fmt, va_list vargs)
{
	int res = 0;
	int colorize = use_colors;
#ifdef WIN32
	struct esc_item {
		int pos;
		WORD attr;
	};
	HANDLE h_stream = h_stdout;

	if (WIN32_LEGACY_MODE) {
		if (stream == stdout) {
			h_stream = h_stdout;
		} else if (stream == stderr) {
			h_stream = h_stderr;
		} else {
			colorize = 0;
		}
	}
#endif
	if (!colorize || WIN32_LEGACY_MODE) {
		// first, we need to print the string WITH escape sequences
		va_list vargs_copy;
		va_copy(vargs_copy, vargs);
		int len = vsnprintf(NULL, 0, fmt, vargs);
		char* newbuf = (char*)malloc(len+1);
		res = vsnprintf(newbuf, len+1, fmt, vargs_copy);
		va_end(vargs_copy);

		// then, we need to remove the escape sequences, if any
#ifdef WIN32
		// if colorize is on, we need to keep their positions for later
		struct esc_item* esc_items = NULL;
		int attr = 0;
		if (colorize) {
			esc_items = (struct esc_item*)malloc(sizeof(struct esc_item) * ((len/3)+1));
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			if (GetConsoleScreenBufferInfo(h_stream, &csbi)) {
				attr = csbi.wAttributes;
			}
		}
#endif
		int num_esc = 0;
		char* start = &newbuf[0];
		char* p = start;
		char* end = start + len + 1;
		while (p < end-1) {
			char* cur = p;
			if (*p == '\e' && end-p > 2 && *(p+1) == '[') {
				p+=2;
				if (*p == 'm') {
#ifdef WIN32
					attr = COLOR_RESET_ATTR;
#endif
					int move_by = (p+1)-cur;
					int move_amount = end-(p+1);
					memmove(cur, p+1, move_amount);
					end -= move_by;
					p = cur;
				} else {
					char* endp = NULL;
					do {
						long lval = strtol(p, &endp, 10);
						if (!endp || (*endp != ';' && *endp != 'm')) {
							fprintf(stderr, "\n*** %s: Invalid escape sequence in format string, expected ';' or 'm' ***\n", __func__);
#ifdef WIN32
							free(esc_items);
#endif
							free(newbuf);
							return -1;
						}
#ifdef WIN32
						if (colorize) {
							if (lval >= 0 && lval <= 8) {
								/* style attributes */
								attr &= ~FOREGROUND_INTENSITY;
								attr |= style_map[lval];
							} else if (lval >= 30 && lval <= 37) {
								/* foreground color */
								attr &= ~FG_COLOR_MASK;
								attr |= fgcolor_map[lval-30];
							} else if (lval == 39) {
								/* default foreground color */
								attr &= ~FG_COLOR_ATTR_MASK;
								attr |= DEFAULT_FG_ATTR;
							} else if (lval >= 40 && lval <= 47) {
								/* background color */
								attr &= ~BG_COLOR_MASK;
								attr |= bgcolor_map[lval-40];
							} else if (lval == 49) {
								/* default background color */
								attr &= ~BG_COLOR_ATTR_MASK;
								attr |= DEFAULT_BG_ATTR;
							} else if (lval >= 90 && lval <= 97) {
								/* foreground color bright */
								attr &= ~FG_COLOR_ATTR_MASK;
								attr |= fgcolor_map[lval-90];
								attr |= FOREGROUND_INTENSITY;
							} else if (lval >= 100 && lval <= 107) {
								/* background color bright */
								attr &= ~BG_COLOR_MASK;
								attr |= bgcolor_map[lval-100];
								attr |= BACKGROUND_INTENSITY;
							}
						}
#else
						(void)lval; // suppress compiler warning about unused variable
#endif
						p = endp+1;
					} while (p < end && *endp == ';');

					int move_by = (endp+1)-cur;
					int move_amount = end-(endp+1);
					memmove(cur, endp+1, move_amount);
					end -= move_by;
					p = cur;
				}
#ifdef WIN32
				if (colorize) {
					esc_items[num_esc].pos = p-start;
					if (attr & STYLE_DIM) {
						attr &= ~STYLE_DIM;
						attr &= ~FOREGROUND_INTENSITY;
					}
					esc_items[num_esc].attr = (WORD)attr;
					num_esc++;
				}
#endif
			} else {
				p++;
			}
		}

		if (num_esc == 0) {
			res = fputs(newbuf, stream);
			free(newbuf);
#ifdef WIN32
			free(esc_items);
#endif
			return res;
		}
#ifdef WIN32
		else {
			p = &newbuf[0];
			char* lastp = &newbuf[0];
			int i;
			for (i = 0; i < num_esc; i++) {
				p = &newbuf[esc_items[i].pos];
				if (lastp < p) {
					fprintf(stream, "%.*s", (int)(p-lastp), lastp);
					lastp = p;
				}
				SetConsoleTextAttribute(h_stream, esc_items[i].attr);
			}
			if (lastp < end) {
				fprintf(stream, "%.*s", (int)(end-lastp), lastp);
			}
			return res;
		}
#endif
	} else {
		res = vfprintf(stream, fmt, vargs);
	}
	return res;
}

LIBIMOBILEDEVICE_GLUE_API int cfprintf(FILE* stream, const char* fmt, ...)
{
	int res = 0;
	va_list va;
	va_start(va, fmt);
	res = cvfprintf(stream, fmt, va);
	va_end(va);
	return res;
}

LIBIMOBILEDEVICE_GLUE_API int cprintf(const char* fmt, ...)
{
	int res = 0;
	va_list va;
	va_start(va, fmt);
	res = cvfprintf(stdout, fmt, va);
	va_end(va);
	return res;
}
