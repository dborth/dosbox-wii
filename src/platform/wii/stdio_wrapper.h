// by retro100
// This file is dual licensed for you to choose from.
// public domain. no warranty implied; use at your own risk.
// also licensed under the zlib license.

#ifndef STDIO_WRAPPER_H
#define STDIO_WRAPPER_H

#include "wiiio.h"

#ifdef printf
#undef printf
#endif

#define printf(fmt, ...) wiiio_printf(fmt, ##__VA_ARGS__)

#endif
