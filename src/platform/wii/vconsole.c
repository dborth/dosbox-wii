// by retro100
// This file is dual licensed for you to choose from.
// public domain. no warranty implied; use at your own risk.
// also licensed under the zlib license.

#include "vconsole.h"

#include <gccore.h>
#include <string.h>

#define MAX_PRINTF_LEN 512

static void vc_jump_to_next_line(vconsole_t* vc) {
	// terminate the current line
	vc->vc_current_line[vc->vc_current_xpos_cursor_position] = '\0';

	// jump to next line
	vc->vc_current_xpos_cursor_position = 0;
	vc->explicit_new_line = 0;
	// increase vc_current_line_index to a tmp variable and
	// only update to the increased value if it is allowed
	// --> Avoids a momentary invalid value at vc->vc_current_line_index.
	// --> Race conditions not really a problem (no sync is needed).
	int next_line_index = vc->vc_current_line_index + 1;
	if (next_line_index >= VCONSOLE_HEIGHT) {
		vc->vc_current_line_index = 0;
	}
	else {
		vc->vc_current_line_index = next_line_index;
	}
	vc->vc_current_line = vc->vc_text_lines[vc->vc_current_line_index];

	// clear the line content of the new line.
	// Using memset instead of simple
	// vc_current_line[vc_current_xpos_cursor_position] = '\0';
	// because if there are race conditions of different threads
	// at the current line then it always has a termination
	// at any position.
	memset(vc->vc_current_line, 0, VCONSOLE_WIDTH);

	++vc->line_count;
}

void vconsole_init(vconsole_t* vc)
{
	memset(vc->vc_text_lines, 0, sizeof(vc->vc_text_lines));
	//memset(vc->vc_text_lines, 0, VCONSOLE_HEIGHT * VCONSOLE_WIDTH);
	vc->vc_current_line_index = 0;
	vc->vc_current_xpos_cursor_position = 0;
	vc->explicit_new_line = 1;
	vc->vc_current_line = vc->vc_text_lines[0];
	vc->line_count = 0;
}

void vconsole_destroy(vconsole_t* vc)
{
	// currently nothing to to
}

int vconsole_printf(vconsole_t* vc, const char *format, ...)
{
	va_list args;
	va_start (args, format);
	int rv = vconsole_vprintf(vc, format, args);
	va_end (args);
	return rv;
}

int vconsole_vprintf(vconsole_t* vc, const char *format, va_list ap)
{
	char msg[MAX_PRINTF_LEN];
	vsnprintf(msg, MAX_PRINTF_LEN, format, ap);
	msg[MAX_PRINTF_LEN - 1] = '\0';
	return vconsole_print(vc, msg);
}

int vconsole_print(vconsole_t* vc, const char* text)
{
	size_t len = strlen(text);
	vconsole_write(vc, text, len);
	return len;
}

void vconsole_write(vconsole_t* vc, const void* buf, unsigned int len)
{
	const char* text = buf;
	unsigned int i;
	for (i = 0; i < len; ++i) {
		char ch = text[i];
		if (ch == '\0') {
			// ignore it
		}
		else if (ch == '\n') {
			if (vc->vc_current_xpos_cursor_position > 0 || vc->explicit_new_line) {
				vc_jump_to_next_line(vc);
				vc->explicit_new_line = 1;
			}
		}
		else if (ch == '\r') {
			// ignore it.
			// --> For a new line \r\n or \n must be used.
			// Only \r makes no new line.
		}
		else if (ch == '\t') {
			if (vc->vc_current_xpos_cursor_position >= VCONSOLE_WIDTH - 4) {
				vc_jump_to_next_line(vc);
			}
			vc->vc_current_line[vc->vc_current_xpos_cursor_position] += ' ';
			vc->vc_current_line[vc->vc_current_xpos_cursor_position + 1] += ' ';
			vc->vc_current_line[vc->vc_current_xpos_cursor_position + 2] += ' ';
			vc->vc_current_line[vc->vc_current_xpos_cursor_position + 3] += ' ';

			vc->vc_current_xpos_cursor_position += 4;
			if (vc->vc_current_xpos_cursor_position >= VCONSOLE_WIDTH - 1) {
				vc_jump_to_next_line(vc);
			}
		}
		else {
			if (ch < 0x20 || ch >= 0x7f) {
				ch = '?';
			}
			vc->vc_current_line[vc->vc_current_xpos_cursor_position] = ch;
			++vc->vc_current_xpos_cursor_position;
			if (vc->vc_current_xpos_cursor_position >= VCONSOLE_WIDTH - 1) {
				vc_jump_to_next_line(vc);
			}
		}
	}
	// terminate the new added content
	vc->vc_current_line[vc->vc_current_xpos_cursor_position] = '\0';
}

