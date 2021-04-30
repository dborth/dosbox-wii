// by retro100
// This file is dual licensed for you to choose from.
// public domain. no warranty implied; use at your own risk.
// also licensed under the zlib license.

#ifndef VCONSOLE_H
#define VCONSOLE_H

#include <stdarg.h>
#include <stdio.h>

#define VCONSOLE_WIDTH  73 // usable are 72
#define VCONSOLE_HEIGHT 33

/*
by retro100

If the macro SHOW_DEBUGVC is defined then
a virtual console with a size of 86x40 can
be used to print messages.
The output of the virtual console scrolls
if a new line is added.
\n and also \t are supported.
The font is NOT monospace. The effective length
of the rendered output depends on the used characters.
*/

#ifdef SHOW_DEBUGVC
#define DEBUG_vc_printf(fmt, ...) DEBUG_vc_printf__(fmt, ##__VA_ARGS__)
#define DEBUG_vc_print(text) DEBUG_vc_print__(text)
#define DEBUG_vc_render_console_with_init(r, g, b, a) DEBUG_vc_render_console_with_init__(r, g, b, a)
#define DEBUG_vc_render_console() DEBUG_vc_render_console__()
#else
#define DEBUG_vc_printf(fmt, ...)
#define DEBUG_vc_print(fmt, ...)
#define DEBUG_vc_render_console_with_init(r, g, b, a)
#define DEBUG_vc_render_console()
#endif


#ifdef __cplusplus
extern "C" {
#endif

typedef struct vconsole
{
	char vc_text_lines[VCONSOLE_HEIGHT][VCONSOLE_WIDTH];
	int vc_current_line_index;
	int vc_current_xpos_cursor_position;
	int explicit_new_line;
	char *vc_current_line;
	int line_count;
} vconsole_t;

void vconsole_init(vconsole_t* vc);
void vconsole_destroy(vconsole_t* vc);
int vconsole_printf(vconsole_t* vc, const char *format, ...)
	__attribute__ ((format(printf, 2, 3)));
int vconsole_vprintf(vconsole_t* vc, const char *format, va_list ap);
int vconsole_print(vconsole_t* vc, const char* text);
void vconsole_write(vconsole_t* vc, const void* buf, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif

