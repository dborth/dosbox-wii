/*
 *  Copyright (C) 2002-2019  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Everything is converted to RGB565 (DBPP 16), the only format the display
 * path takes (see GFX_GetBestMode in videosupport.cpp). */
#define PSIZE 2
#define PTYPE Bit16u

#if SBPP == 8 || SBPP == 9
#define PMAKE(_VAL) render.pal.lut.b16[_VAL]
#define SRCTYPE Bit8u
#endif

#if SBPP == 15
#define PMAKE(_VAL) (((_VAL) & 31) | ((_VAL) & ~31) << 1)
#define SRCTYPE Bit16u
#endif

#if SBPP == 16
#define PMAKE(_VAL) (_VAL)
#define SRCTYPE Bit16u
#endif

#if SBPP == 32
#define PMAKE(_VAL) (PTYPE)(((_VAL&(31<<19))>>8)|((_VAL&(63<<10))>>4)|((_VAL&(31<<3))>>3))
#define SRCTYPE Bit32u
#endif

/* Simple scalers. The source is only ever written 1:1, with the width or the
 * height doubled for the video modes that need it. Anything else (enlarging,
 * filtering) is left to the display. */
#define SCALERNAME		Normal1x
#define SCALERWIDTH		1
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	line0[0] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		NormalDw
#define SCALERWIDTH		2
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	line0[0] = P;								\
	line0[1] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		NormalDh
#define SCALERWIDTH		1
#define SCALERHEIGHT	2
#define SCALERFUNC								\
	line0[0] = P;								\
	line1[0] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#undef PSIZE
#undef PTYPE
#undef PMAKE
#undef SRCTYPE
