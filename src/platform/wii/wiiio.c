// by retro100
// This file is dual licensed for you to choose from.
// public domain. no warranty implied; use at your own risk.
// also licensed under the zlib license.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "wiiio.h"
#include "vconsole.h"
#include <stdlib.h>

static vconsole_t* vc = NULL;

static ssize_t vc_memfile_write(void *c, const char *buf, size_t size)
{
	vconsole_write(vc, buf, size);
	return size;
}

static ssize_t vc_memfile_read(void *c, char *buf, size_t size)
{
	return 0;
}

static int vc_memfile_seek(void *c, off_t *offset, int whence)
{
	return 0; // or maybe -1 should be returned?
}

static int vc_memfile_close(void *c)
{
	return 0;
}

void wiiio_init()
{
	if (!vc) {
		vc = malloc(sizeof(vconsole_t));
		vconsole_init(vc);
	}
}

FILE* wiiio_fopen()
{
	cookie_io_functions_t vc_memfile_func = {
		.read  = vc_memfile_read,
		.write = vc_memfile_write,
		.seek  = vc_memfile_seek,
		.close = vc_memfile_close
	};
	FILE* fp = fopencookie(vc, "w+", vc_memfile_func);
	if (!fp) {
		vconsole_print(vc, "open failed!\n");
	}
	return fp;
}

vconsole_t* wiiio_get_vconsole()
{
	return vc;
}

int wiiio_printf(const char *format, ...)
{
	va_list args;
	va_start (args, format);
	int rv = wiiio_vprintf(format, args);
	va_end (args);
	return rv;
}

int wiiio_vprintf(const char *format, va_list ap)
{
	return vconsole_vprintf(vc, format, ap);
}

int wiiio_print(const char* text)
{
	return vconsole_print(vc, text);
}

int wiiio_puts(const char* text)
{
	int rv = vconsole_print(vc, text);
	rv += vconsole_print(vc, "\n");
	return rv;
}

