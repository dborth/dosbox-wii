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

#ifndef _RENDER_SCALERS_H
#define _RENDER_SCALERS_H

#include <stddef.h>
#include "video.h"

#define SCALER_MAXWIDTH		640
#define SCALER_MAXHEIGHT	480

typedef enum {
	scalerMode8, scalerMode15, scalerMode16, scalerMode32
} scalerMode_t;

typedef void (*ScalerLineHandler_t)(const void *src);

extern Bit8u Scaler_Aspect[];
extern Bitu Scaler_ChangedLineIndex;
extern Bit16u Scaler_ChangedLines[];

/* The source frame as it was last drawn, one line per source line
 * (render.scale.cachePitch bytes apart). Kept to find which lines changed.
 * Sized for the current mode by Scaler_SizeCache(): a 320x200 8 bit mode needs
 * 64 KB, where a cache for the largest mode (640x480 32 bit) is 1.2 MB. NULL
 * until a mode has sized it. */
extern Bit8u *Scaler_SourceCache;

/* Makes the cache hold at least `bytes`. Its contents are not kept. Returns
 * false if there is no memory for it, with no cache left. */
bool Scaler_SizeCache(size_t bytes);

//! Line handlers by source mode (8, 15, 16, 32 bit, then 8 bit with palette
//! change detection) and destination mode (scalerMode_t). Only the
//! scalerMode16 (RGB565) column is filled in.
typedef ScalerLineHandler_t ScalerLineBlock_t[5][4];

typedef struct {
	const char *name;
	Bitu gfxFlags;
	Bitu xscale,yscale;
	ScalerLineBlock_t	Handlers;
} ScalerSimpleBlock_t;

/* The source line by line, with the width or height doubled for the video
 * modes that need it. No enlarging or filtering: that is up to the display. */
extern ScalerSimpleBlock_t ScaleNormal1x;
extern ScalerSimpleBlock_t ScaleNormalDw;
extern ScalerSimpleBlock_t ScaleNormalDh;

#endif
