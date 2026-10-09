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


#include "dosbox.h"
#include "render.h"
#include <string.h>
#include <stdlib.h>

Bit8u Scaler_Aspect[SCALER_MAXHEIGHT];
Bit16u Scaler_ChangedLines[SCALER_MAXHEIGHT];
Bitu Scaler_ChangedLineIndex;

Bit8u *Scaler_SourceCache = 0;
static size_t scalerCacheSize = 0;

/* The line handlers compare whole words, so the last line can be read a few
 * bytes past its end when the width is not a multiple of the word size */
#define SCALER_CACHE_SLACK	64

bool Scaler_SizeCache(size_t bytes) {
	bytes += SCALER_CACHE_SLACK;
	/* Keep a block that is the right size to within a factor of two. A mode
	 * change is rare, a heap with no spare room does not like churn, but a
	 * 640x480 cache left behind by a 320x200 mode is 240 KB wasted */
	if (Scaler_SourceCache && bytes <= scalerCacheSize && bytes * 2 > scalerCacheSize) {
		memset(Scaler_SourceCache, 0, scalerCacheSize);
		return true;
	}
	/* Free first: the old block and the new one are never both needed */
	free(Scaler_SourceCache);
	Scaler_SourceCache = 0;
	scalerCacheSize = 0;
	Scaler_SourceCache = (Bit8u *)malloc(bytes);
	if (!Scaler_SourceCache)
		return false;
	scalerCacheSize = bytes;
	memset(Scaler_SourceCache, 0, bytes);
	return true;
}

#define _conc7(A,B,C,D,E,F,G) A ## B ## C ## D ## E ## F ## G
#define conc4d(A,B,C,D) _conc7(A,_,B,_,C,_,D)

static INLINE void BituMove( void *_dst, const void * _src, Bitu size) {
	Bitu * dst=(Bitu *)(_dst);
	const Bitu * src=(Bitu *)(_src);
	size/=sizeof(Bitu);
	for (Bitu x=0; x<size;x++)
		dst[x] = src[x];
}

static INLINE void ScalerAddLines( Bitu changed, Bitu count ) {
	if ((Scaler_ChangedLineIndex & 1) == changed ) {
		Scaler_ChangedLines[Scaler_ChangedLineIndex] += count;
	} else {
		Scaler_ChangedLines[++Scaler_ChangedLineIndex] = count;
	}
	render.scale.outWrite += render.scale.outPitch * count;
}

/* Line handlers for each source depth, all writing RGB565 */
#define DBPP 16
#define SBPP 8
#include "render_templates.h"
#undef SBPP
/* SBPP 9 is a special case with palette check support */
#define SBPP 9
#include "render_templates.h"
#undef SBPP
#define SBPP 15
#include "render_templates.h"
#undef SBPP
#define SBPP 16
#include "render_templates.h"
#undef SBPP
#define SBPP 32
#include "render_templates.h"
#undef SBPP
#undef DBPP

ScalerSimpleBlock_t ScaleNormal1x = {
	"Normal",
	GFX_CAN_16,
	1,1,{
{	0,	0,	Normal1x_8_16_R,	0 },
{	0,	0,	Normal1x_15_16_R,	0 },
{	0,	0,	Normal1x_16_16_R,	0 },
{	0,	0,	Normal1x_32_16_R,	0 },
{	0,	0,	Normal1x_9_16_R,	0 }
}};

ScalerSimpleBlock_t ScaleNormalDw = {
	"Normal",
	GFX_CAN_16,
	2,1,{
{	0,	0,	NormalDw_8_16_R,	0 },
{	0,	0,	NormalDw_15_16_R,	0 },
{	0,	0,	NormalDw_16_16_R,	0 },
{	0,	0,	NormalDw_32_16_R,	0 },
{	0,	0,	NormalDw_9_16_R,	0 }
}};

ScalerSimpleBlock_t ScaleNormalDh = {
	"Normal",
	GFX_CAN_16,
	1,2,{
{	0,	0,	NormalDh_8_16_R,	0 },
{	0,	0,	NormalDh_15_16_R,	0 },
{	0,	0,	NormalDh_16_16_R,	0 },
{	0,	0,	NormalDh_32_16_R,	0 },
{	0,	0,	NormalDh_9_16_R,	0 }
}};
