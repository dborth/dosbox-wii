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

/* Line handler for one scaler block (SCALERNAME), source depth (SBPP) and
 * destination depth (DBPP). Compares the source line with the cached copy
 * and converts and writes only what changed. Unchanged runs are skipped.
 *
 * Each source pixel is written through SCALERFUNC (SCALERWIDTH x SCALERHEIGHT
 * output pixels). */
static void conc4d(SCALERNAME,SBPP,DBPP,R)(const void *s) {
	/* Clear the complete line marker */
	Bitu hadChange = 0;
	const SRCTYPE *src = (SRCTYPE*)s;
	SRCTYPE *cache = (SRCTYPE*)(render.scale.cacheRead);
	render.scale.cacheRead += render.scale.cachePitch;
	PTYPE * line0=(PTYPE *)(render.scale.outWrite);
#if (SBPP == 9)
	for (Bits x=render.src.width;x>0;) {
		if (*(Bit32u const*)src == *(Bit32u*)cache && !(
			render.pal.modified[src[0]] | 
			render.pal.modified[src[1]] | 
			render.pal.modified[src[2]] | 
			render.pal.modified[src[3]] )) {
			x-=4;
			src+=4;
			cache+=4;
			line0+=4*SCALERWIDTH;
#else 
	for (Bits x=render.src.width;x>0;) {
		if (*(Bitu const*)src == *(Bitu*)cache) {
			x-=(sizeof(Bitu)/sizeof(SRCTYPE));
			src+=(sizeof(Bitu)/sizeof(SRCTYPE));
			cache+=(sizeof(Bitu)/sizeof(SRCTYPE));
			line0+=(sizeof(Bitu)/sizeof(SRCTYPE))*SCALERWIDTH;
#endif
		} else {
#if (SCALERHEIGHT > 1) 
		PTYPE *line1 = (PTYPE *)(((Bit8u*)line0)+ render.scale.outPitch);
#endif
			hadChange = 1;
			for (Bitu i = x > 32 ? 32 : x;i>0;i--,x--) {
				const SRCTYPE S = *src;
				*cache = S;
				src++;cache++;
				const PTYPE P = PMAKE(S);
				SCALERFUNC;
				line0 += SCALERWIDTH;
#if (SCALERHEIGHT > 1) 
				line1 += SCALERWIDTH;
#endif
			}
		}
	}
	Bitu scaleLines = Scaler_Aspect[ render.scale.outLine++ ];
	if ( scaleLines - SCALERHEIGHT && hadChange ) {
		BituMove( render.scale.outWrite + render.scale.outPitch * SCALERHEIGHT,
			render.scale.outWrite + render.scale.outPitch * (SCALERHEIGHT-1),
			render.src.width * SCALERWIDTH * PSIZE);
	}
	ScalerAddLines( hadChange, scaleLines );
}
