// by retro100
// This file is dual licensed for you to choose from.
// public domain. no warranty implied; use at your own risk.
// also licensed under the zlib license.

#ifndef WIIIO_H
#define WIIIO_H

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vconsole vconsole_t;

void wiiio_init();

FILE* wiiio_fopen();
vconsole_t* wiiio_get_vconsole();

int wiiio_printf(const char *format, ...)
	__attribute__ ((format(printf, 1, 2)));
int wiiio_vprintf(const char *format, va_list ap);
int wiiio_print(const char* text);

// wiiio_puts() writes the string text and a trailing newline.
int wiiio_puts(const char* text);

#ifdef __cplusplus
}
#endif

#endif
