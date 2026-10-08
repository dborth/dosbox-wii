/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2008-2026
 * OgcEmulatorVideo.cpp
 ***************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OgcEmulatorVideo.h"
#include "OgcVideoDriver.h"
#include "../EmulatorVideoLayout.h"

// The emulator view uses the same 640x480 design space as the menu
#define CANVAS_W 640
#define CANVAS_H 480

// The scanline texture: 8x4 I8, one bright and one dark EFB row, twice
#define SCANLINE_TEX_W 8
#define SCANLINE_TEX_H 4

static GXTexObj texobj;     // the emulator frame
static GXTexObj texobjPre;  // the prescaled frame (sharp filter)
static GXTexObj scanlineTexObj;
static Mtx view;
static u8 scanlineTexData[SCANLINE_TEX_W * SCANLINE_TEX_H] ATTRIBUTE_ALIGN(32);

/*** Square Matrix
     Vertices 0-3 are the picture on the canvas (centred origin, y up),
     rewritten by recalculateScaling() whenever the frame size or settings
     change. Vertices 4-7 are the prescale pass's quad, which covers its own
     viewport from the top-left (pixel units).
***/
static s16 square[24] ATTRIBUTE_ALIGN(32) = {
	-320,  240, 0,	// 0
	 320,  240, 0,	// 1
	 320, -240, 0,	// 2
	-320, -240, 0,	// 3
	   0,    0, 0,	// 4
	   0,    0, 0,	// 5
	   0,    0, 0,	// 6
	   0,    0, 0	// 7
};

struct Camera { guVector pos; guVector up; guVector view; };
static Camera cam = { {0.0F, 0.0F, 0.0F},
                      {0.0F, 0.5F, 0.0F},
                      {0.0F, 0.0F, -0.5F} };

static inline s16 toS16(float v)
{
	if (v > 32767.0f) return 32767;
	if (v < -32768.0f) return -32768;
	return (s16) v;
}

OgcEmulatorVideo::~OgcEmulatorVideo()
{
	free(screenshotSnapshot);
	free(texMem);
	free(prescaleMem);
}

/****************************************************************************
 * Settings
 ***************************************************************************/
void OgcEmulatorVideo::settingsChanged()
{
	rectDirty = true;
	updateScaling = true;
	filterDirty = true;
	drawDirty = true;
}

EmulatorVideoCapabilities OgcEmulatorVideo::getCapabilities() const
{
	EmulatorVideoCapabilities caps;
	caps.scanlines = scanlinesSupported();
	caps.sharpFilter = true;
	caps.widescreenSetting = true;
	return caps;
}

//! Scanlines are drawn one EFB row at a time, so they only make sense on a
//! 480+ line mode (not the 240p modes)
bool OgcEmulatorVideo::scanlinesSupported() const
{
	GXRModeObj* mode = videoDriver ? videoDriver->getVideoMode() : nullptr;
	return mode && mode->efbHeight > 300;
}

bool OgcEmulatorVideo::scanlinesActive() const
{
	return settings.scanlines > 0.0f && scanlinesSupported();
}

//! A 16:9 console stretches the 640-wide frame buffer sideways
bool OgcEmulatorVideo::widescreenCompensation() const
{
	switch (settings.widescreen)
	{
		case VideoWidescreen::Display16x9: return true;
		case VideoWidescreen::Display4x3: return false;
		default: return videoDriver && videoDriver->isWidescreen();
	}
}

/****************************************************************************
 * GX setup
 ***************************************************************************/
void OgcEmulatorVideo::configureTEV(bool scanlines)
{
	if (!scanlines)
	{
		GX_SetNumTexGens (1);
		GX_SetNumTevStages (1);
		GX_SetNumChans (0);

		GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

		GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
		GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
		return;
	}

	// Two textures, two stages: the frame, then multiplied by the scanline texture
	GX_SetNumTexGens(2);
	GX_SetNumTevStages(2);
	GX_SetNumChans(0);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLORNULL);
	// d + ((1 - c) * a + c * b) with a = 0, b = previous, c = scanline texel, d = 0
	GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_TEXA, GX_CA_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

//! Vertex description, TEV and matrices for drawing the quad, with or
//! without the scanline stage
void OgcEmulatorVideo::drawInit(bool scanlines)
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	if (scanlines)
	{
		GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
		GX_SetVtxDesc (GX_VA_TEX1, GX_DIRECT);
	}

	configureTEV(scanlines);

	GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

	memset (&view, 0, sizeof (Mtx));
	guLookAt(view, &cam.pos, &cam.up, &cam.view);
	GX_LoadPosMtxImm (view, GX_PNMTX0);

	GX_InvVtxCache ();	// update vertex cache
}

static inline void draw_vert(u8 pos, f32 s, f32 t)
{
	GX_Position1x8(pos);
	GX_TexCoord2f32(s, t);
}

//! Draws the four vertices starting at `first`. With scanlines the quad also
//! carries the scanline texture's coordinates, tiled 1 texel per EFB pixel.
void OgcEmulatorVideo::drawSquare(u8 first, bool scanlines)
{
	Mtx m;		// model matrix.
	Mtx mv;		// modelview matrix.

	guMtxIdentity(m);
	guMtxTransApply(m, m, 0, 0, -100);
	guMtxConcat(view, m, mv);

	GX_LoadPosMtxImm(mv, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	if (scanlines)
	{
		GXRModeObj* mode = videoDriver->getVideoMode();

		// Picture size in EFB pixels (the canvas is stretched onto the EFB)
		const f32 efbW = frameW * ((f32) mode->fbWidth / CANVAS_W);
		const f32 efbH = frameH * ((f32) mode->efbHeight / CANVAS_H);
		const f32 uRepeat = efbW / SCANLINE_TEX_W;
		const f32 vRepeat = efbH / SCANLINE_TEX_H;

		// Half a texel in, so the sampler hits texel centres and no moire
		// comes from rounding at the edges
		const f32 uOff = 0.5f / SCANLINE_TEX_W;
		const f32 vOff = 0.5f / SCANLINE_TEX_H;

		draw_vert(first + 0, 0.0f, 0.0f); GX_TexCoord2f32(uOff, vOff);
		draw_vert(first + 1, 1.0f, 0.0f); GX_TexCoord2f32(uRepeat + uOff, vOff);
		draw_vert(first + 2, 1.0f, 1.0f); GX_TexCoord2f32(uRepeat + uOff, vRepeat + vOff);
		draw_vert(first + 3, 0.0f, 1.0f); GX_TexCoord2f32(uOff, vRepeat + vOff);
	}
	else
	{
		draw_vert(first + 0, 0.0f, 0.0f);
		draw_vert(first + 1, 1.0f, 0.0f);
		draw_vert(first + 2, 1.0f, 1.0f);
		draw_vert(first + 3, 0.0f, 1.0f);
	}
	GX_End();
}

//! Loads the scanline texture into TEXMAP1. Rows alternate full brightness
//! and (1 - strength) of it. Nearest sampling: linear would blur the lines
//! into an even grey.
void OgcEmulatorVideo::loadScanlineTexture()
{
	const u8 dark = (u8)(255.0f * (1.0f - settings.scanlines) + 0.5f);

	for (int y = 0; y < SCANLINE_TEX_H; y++)
		for (int x = 0; x < SCANLINE_TEX_W; x++)
			scanlineTexData[y * SCANLINE_TEX_W + x] = (y & 1) ? dark : 0xFF;

	DCFlushRange(scanlineTexData, sizeof(scanlineTexData));

	GX_InitTexObj(&scanlineTexObj, scanlineTexData, SCANLINE_TEX_W, SCANLINE_TEX_H, GX_TF_I8, GX_REPEAT, GX_REPEAT, GX_FALSE);
	GX_InitTexObjFilterMode(&scanlineTexObj, GX_NEAR, GX_NEAR);
	GX_LoadTexObj(&scanlineTexObj, GX_TEXMAP1);
}

//! The projection and viewport for drawing the picture on the canvas
void OgcEmulatorVideo::setCanvasProjection()
{
	Mtx44 p;
	GXRModeObj* rmode = videoDriver->getVideoMode();

	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	guOrtho(p, CANVAS_H/2, -(CANVAS_H/2), -(CANVAS_W/2), CANVAS_W/2, 100, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);
}

/****************************************************************************
 * resetVideo
 *
 * Reset the video/rendering mode for the emulator rendering
 ****************************************************************************/
void OgcEmulatorVideo::resetVideo()
{
	GXRModeObj * rmode = videoDriver->findVideoMode();
	u8 vfilter[7] = {0, 0, 21, 22, 21, 0, 0};

	videoDriver->setupVideoMode(rmode); // reconfigure VI

	// reconfigure GX
	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	GX_SetDispCopyFrame2Field (rmode->copy_interlaced);
	GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyYScale (GX_GetYScaleFactor (rmode->efbHeight, rmode->xfbHeight));
	GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);

	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_FALSE, vfilter);

	GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight / rmode->efbHeight == 2) ? GX_ENABLE : GX_DISABLE));

	if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	GX_SetCullMode (GX_CULL_NONE);
	GX_SetDispCopyGamma (GX_GM_1_0);

	// The bars around the frame are whatever the copy clears to, and the menu
	// may have changed that (VideoDriver::clearScreen)
	GXColor background = {0, 0, 0, 255};
	GX_SetCopyClear (background, GX_MAX_Z24);

	GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate (GX_TRUE);
	GX_SetBlendMode (GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	setCanvasProjection();

	scanlinesApplied = scanlinesActive();
	drawInit(scanlinesApplied);
	drawDirty = false;

	// the menu has used the texture units; reload everything on the next frame.
	// The frame size is kept so the placement can still be asked for.
	updateScaling = true;
	rectDirty = true;
	filterDirty = true;
}

/****************************************************************************
 * stopVideo
 *
 * presentFrame() only queues the copy to the XFB, which happens at the next
 * retrace. Drain that before the menu reconfigures GX and does its own copy.
 ***************************************************************************/
void OgcEmulatorVideo::stopVideo()
{
	videoDriver->waitForIdle();
}

/****************************************************************************
 * Placement
 *
 * ensureRect() works out the picture's rect on the 640x480 canvas from the
 * frame size, pixel aspect and settings, with no GX calls, so the menu can
 * ask for it at any time. recalculateScaling() (presentFrame only) then
 * writes it into the vertex array.
 ***************************************************************************/
void OgcEmulatorVideo::ensureRect()
{
	if (!rectDirty)
		return;

	VideoLayoutInput in;
	in.frameWidth = frameWidth;
	in.frameHeight = frameHeight;
	in.pixelAspectX = pixelAspectX;
	in.pixelAspectY = pixelAspectY;
	in.targetWidth = CANVAS_W;
	in.targetHeight = CANVAS_H;
	in.outputPixelAspect = widescreenCompensation() ? (4.0f / 3.0f) : 1.0f;

	VideoRect r = ComputeVideoLayout(settings, in);

	// Whole canvas units: the vertices are s16, and an edge between two EFB
	// pixels keeps nearest sampling even
	const float x0 = floorf(r.x + 0.5f);
	const float y0 = floorf(r.y + 0.5f);
	const float x1 = floorf(r.x + r.w + 0.5f);
	const float y1 = floorf(r.y + r.h + 0.5f);

	frameX = x0; frameY = y0;
	frameW = x1 - x0; frameH = y1 - y0;
	rectDirty = false;
}

void OgcEmulatorVideo::recalculateScaling()
{
	ensureRect();

	if (frameW <= 0.0f || frameH <= 0.0f)
		return;

	const s16 left = toS16(frameX - CANVAS_W / 2.0f);
	const s16 top = toS16(CANVAS_H / 2.0f - frameY);
	const s16 right = toS16(frameX + frameW - CANVAS_W / 2.0f);
	const s16 bottom = toS16(CANVAS_H / 2.0f - (frameY + frameH));

	square[0] = left;  square[1]  = top;
	square[3] = right; square[4]  = top;
	square[6] = right; square[7]  = bottom;
	square[9] = left;  square[10] = bottom;
	DCFlushRange(square, sizeof(square));
	GX_InvVtxCache();

	updateScaling = false;
}

bool OgcEmulatorVideo::getCanvasRect(float* x, float* y, float* w, float* h)
{
	if (frameWidth <= 0 || frameHeight <= 0)
		return false;

	ensureRect();
	if (frameW <= 0.0f || frameH <= 0.0f)
		return false;

	if (x) *x = frameX;
	if (y) *y = frameY;
	if (w) *w = frameW;
	if (h) *h = frameH;
	return true;
}

bool OgcEmulatorVideo::mapPointerToUnit(float canvasX, float canvasY, bool, float* u, float* v)
{
	ensureRect();

	if (!u || !v || frameW <= 0.0f || frameH <= 0.0f) // scaling hasn't been computed yet
		return false;

	float fx = (canvasX - frameX) / frameW;
	float fy = (canvasY - frameY) / frameH;
	*u = fx < 0.0f ? 0.0f : (fx > 1.0f ? 1.0f : fx);
	*v = fy < 0.0f ? 0.0f : (fy > 1.0f ? 1.0f : fy);
	return true;
}

void OgcEmulatorVideo::setPixelAspect(float scaleX, float scaleY)
{
	if (scaleX <= 0.0f) scaleX = 1.0f;
	if (scaleY <= 0.0f) scaleY = 1.0f;
	if (scaleX == pixelAspectX && scaleY == pixelAspectY)
		return;
	pixelAspectX = scaleX;
	pixelAspectY = scaleY;
	rectDirty = true;
	updateScaling = true;
}

void OgcEmulatorVideo::renderInit(int width, int height)
{
	frameWidth = width;
	frameHeight = height;
	rectDirty = true;
	updateScaling = true;
}

/****************************************************************************
 * Sharp filter (prescale)
 *
 * Plain bilinear blurs every source pixel. Sharp bilinear first enlarges the
 * frame by the largest whole number that still fits the output, with no
 * blending, then lets bilinear do the rest: pixels stay crisp and only the
 * edges between them are blended. GX has no shaders, so the whole-number
 * enlargement is a first pass into the EFB that is copied back out as a
 * texture. Needs N >= 2; below that sharp and bilinear are the same thing.
 ***************************************************************************/
int OgcEmulatorVideo::choosePrescale()
{
	if (settings.filter != VideoFilter::Sharp || frameWidth <= 0 || frameHeight <= 0)
		return 0;

	GXRModeObj* mode = videoDriver->getVideoMode();
	ensureRect();
	if (frameW <= 0.0f || frameH <= 0.0f)
		return 0;

	// Output size in EFB pixels
	const float outW = frameW * ((float) mode->fbWidth / CANVAS_W);
	const float outH = frameH * ((float) mode->efbHeight / CANVAS_H);

	int n = (int) floorf(fminf(outW / frameWidth, outH / frameHeight));

	// The enlarged frame, padded to whole tiles, has to fit the EFB it is drawn in
	while (n >= 2)
	{
		const int w = (frameWidth * n + 3) & ~3;
		const int h = (frameHeight * n + 3) & ~3;
		if (w <= (int) mode->fbWidth && h <= (int) mode->efbHeight && w <= MAX_TEX_DIM && h <= MAX_TEX_DIM)
			return n;
		n--;
	}
	return 0;
}

bool OgcEmulatorVideo::ensurePrescaleBuffer(int width, int height)
{
	const size_t needed = GX_GetTexBufferSize(width, height, GX_TF_RGB565, GX_FALSE, 0);

	if (needed > prescaleCapacity)
	{
		free(prescaleMem);
		prescaleMem = memalign(32, needed);
		prescaleCapacity = prescaleMem ? needed : 0;
		if (!prescaleMem)
			return false;
		// the GPU writes it; make sure no stale cached line is ever written back over it
		DCInvalidateRange(prescaleMem, needed);
	}
	return true;
}

//! Pass 1: draws the frame, nearest-sampled, at N times its size into the
//! top-left of the EFB and copies it out to prescaleMem. Leaves GX as the
//! picture pass expects, with the enlarged frame in TEXMAP0.
void OgcEmulatorVideo::renderPrescale()
{
	const int w = frameWidth * prescaleN;
	const int h = frameHeight * prescaleN;
	const int w4 = (w + 3) & ~3;
	const int h4 = (h + 3) & ~3;

	// the prescale quad, in pixels from the top-left (vertices 4-7)
	square[12] = 0;           square[13] = 0;
	square[15] = (s16) w;     square[16] = 0;
	square[18] = (s16) w;     square[19] = (s16) -h;
	square[21] = 0;           square[22] = (s16) -h;
	DCFlushRange(square, sizeof(square));

	Mtx44 p;
	GX_SetViewport (0, 0, w, h, 0, 1);
	GX_SetScissor (0, 0, w, h);
	guOrtho(p, 0, -h, 0, w, 100, 1000);
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	drawInit(false);
	GX_LoadTexObj(&texobj, GX_TEXMAP0); // nearest
	drawSquare(4, false);

	GX_SetTexCopySrc(0, 0, w4, h4);
	GX_SetTexCopyDst(w4, h4, GX_TF_RGB565, GX_FALSE);
	GX_CopyTex(prescaleMem, GX_TRUE); // and clear, so none of this reaches the picture pass
	GX_PixModeSync();
	GX_InvalidateTexAll();

	// back to drawing the picture on the canvas
	setCanvasProjection();
	drawInit(scanlinesActive());
	GX_LoadTexObj(&texobjPre, GX_TEXMAP0); // linear
}

/****************************************************************************
 * Texture memory
 ***************************************************************************/
bool OgcEmulatorVideo::ensureTexture(int width, int height)
{
	if (width <= 0 || height <= 0 || width > MAX_TEX_DIM || height > MAX_TEX_DIM)
		return false;

	int padWidth = (width + 3) & ~3;
	int padHeight = (height + 3) & ~3;
	size_t needed = (size_t)padWidth * padHeight * 2;

	if (needed > texCapacity) {
		free(texMem);
		texMem = memalign(32, needed);
		texCapacity = texMem ? needed : 0;
		if (!texMem)
			return false;
	}
	return true;
}

// Converts a row-major RGB565 frame into GX's native 4x4-tiled
// GX_TF_RGB565 layout (32 bytes per tile). Partial tiles on the right and
// bottom edge are zero padded.
void OgcEmulatorVideo::tileRGB565(const uint8_t* src, int pitch, int width, int height, uint8_t* dst)
{
	const int tilesX = (width + 3) >> 2;
	const int tilesY = (height + 3) >> 2;
	const int fullX = width >> 2;
	const int fullY = height >> 2;

	uint32_t* out = (uint32_t*) dst;

	for (int ty = 0; ty < tilesY; ty++) {
		const uint8_t* row0 = src + (size_t)(ty * 4) * pitch;

		if (ty < fullY) {
			const uint8_t* row1 = row0 + pitch;
			const uint8_t* row2 = row1 + pitch;
			const uint8_t* row3 = row2 + pitch;

			for (int tx = 0; tx < fullX; tx++) {
				const uint32_t* a = (const uint32_t*)(row0 + tx * 8);
				const uint32_t* b = (const uint32_t*)(row1 + tx * 8);
				const uint32_t* c = (const uint32_t*)(row2 + tx * 8);
				const uint32_t* d = (const uint32_t*)(row3 + tx * 8);
				out[0] = a[0]; out[1] = a[1];
				out[2] = b[0]; out[3] = b[1];
				out[4] = c[0]; out[5] = c[1];
				out[6] = d[0]; out[7] = d[1];
				out += 8;
			}
			if (fullX < tilesX) { // partial tile at the right edge
				int validPx = width - fullX * 4;
				for (int r = 0; r < 4; r++) {
					uint16_t* o = (uint16_t*) out;
					const uint16_t* in = (const uint16_t*)(row0 + (size_t)r * pitch + fullX * 8);
					for (int x = 0; x < 4; x++)
						o[r * 4 + x] = (x < validPx) ? in[x] : 0;
				}
				out += 8;
			}
		}
		else { // partial row of tiles at the bottom edge
			int validRows = height - ty * 4;
			for (int tx = 0; tx < tilesX; tx++) {
				uint16_t* o = (uint16_t*) out;
				for (int r = 0; r < 4; r++) {
					const uint16_t* in = (const uint16_t*)(row0 + (size_t)r * pitch + tx * 8);
					for (int x = 0; x < 4; x++) {
						int px = tx * 4 + x;
						o[r * 4 + x] = (r < validRows && px < width) ? in[x] : 0;
					}
				}
				out += 8;
			}
		}
	}
}

/****************************************************************************
 * Screenshot support
 ***************************************************************************/
void OgcEmulatorVideo::snapshotFrame()
{
	free(screenshotSnapshot);
	screenshotSnapshot = nullptr;

	if (!texMem || frameWidth <= 0 || frameHeight <= 0)
		return;

	ensureRect();

	size_t size = (size_t)((frameWidth + 3) & ~3) * ((frameHeight + 3) & ~3) * 2;
	screenshotSnapshot = (uint8_t *) malloc(size);
	if (screenshotSnapshot) {
		memcpy(screenshotSnapshot, texMem, size);
		snapWidth = frameWidth;
		snapHeight = frameHeight;
		snapX = frameX; snapY = frameY; snapW = frameW; snapH = frameH;
	}
}

bool OgcEmulatorVideo::getSnapshotInfo(FrameSnapshotInfo* info) const
{
	if (!screenshotSnapshot || !info || snapW <= 0.0f || snapH <= 0.0f)
		return false;

	info->width = snapWidth;
	info->height = snapHeight;
	info->x = snapX; info->y = snapY; info->w = snapW; info->h = snapH;
	return true;
}

// Un-tiles the GX_TF_RGB565 buffer snapshotFrame() captured. The width and
// height must match the frame the snapshot was taken from.
bool OgcEmulatorVideo::readFrameRGB24(int width, int height, uint8_t* dst)
{
	if (!screenshotSnapshot || !dst)
		return false;

	if (width != snapWidth || height != snapHeight) {
		free(screenshotSnapshot);
		screenshotSnapshot = nullptr;
		return false;
	}

	int paddedWidth = (width + 3) & ~3;
	const uint16_t* tex16 = (const uint16_t*) screenshotSnapshot;

	for (int y = 0; y < height; y++) {
		int tileY = y >> 2;
		int inTileY = y & 3;
		for (int x = 0; x < width; x++) {
			int tileX = x >> 2;
			int inTileX = x & 3;
			uint16_t c = tex16[(tileY * (paddedWidth >> 2) + tileX) * 16 + (inTileY * 4 + inTileX)];

			uint8_t r = (c >> 11) & 0x1F;
			uint8_t g = (c >> 5) & 0x3F;
			uint8_t b = c & 0x1F;

			uint8_t* o = dst + (y * width + x) * 3;
			o[0] = (r << 3) | (r >> 2);
			o[1] = (g << 2) | (g >> 4);
			o[2] = (b << 3) | (b >> 2);
		}
	}

	free(screenshotSnapshot);
	screenshotSnapshot = nullptr;
	return true;
}

void OgcEmulatorVideo::init(VideoDriver* driver)
{
	videoDriver = static_cast<OgcVideoDriver*>(driver);
}

/****************************************************************************
 * presentFrame
 *
 * Uploads the RGB565 frame as a tiled texture and draws it as a quad.
 ****************************************************************************/
void OgcEmulatorVideo::presentFrame(const uint16_t* pixels, int width, int height, int pitch)
{
	if (!pixels || !ensureTexture(width, height))
		return;

	if (width != frameWidth || height != frameHeight) {
		frameWidth = width;
		frameHeight = height;
		rectDirty = true;
		updateScaling = true;
		filterDirty = true;
	}

	// Wait for the VI to finish displaying the previously submitted frame,
	// and for the GPU to finish rendering it, before touching texture memory.
	videoDriver->waitForBufferReady();

	// A settings change can turn the scanline stage on or off
	const bool scanlines = scanlinesActive();
	if (drawDirty || scanlines != scanlinesApplied) {
		drawInit(scanlines);
		scanlinesApplied = scanlines;
		drawDirty = false;
	}

	if (updateScaling)
		recalculateScaling();

	if (filterDirty) {
		const int n = choosePrescale();
		prescaleN = (n >= 2 && ensurePrescaleBuffer((frameWidth * n + 3) & ~3, (frameHeight * n + 3) & ~3)) ? n : 0;

		// With a prescale pass the frame is only ever sampled nearest, to be enlarged
		const bool linear = prescaleN == 0 && settings.filter != VideoFilter::Nearest;
		GX_InitTexObj(&texobj, texMem, frameWidth, frameHeight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObjFilterMode(&texobj, linear ? GX_LINEAR : GX_NEAR, linear ? GX_LINEAR : GX_NEAR);
		GX_LoadTexObj(&texobj, GX_TEXMAP0);

		if (prescaleN) {
			GX_InitTexObj(&texobjPre, prescaleMem, frameWidth * prescaleN, frameHeight * prescaleN, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
			GX_InitTexObjFilterMode(&texobjPre, GX_LINEAR, GX_LINEAR);
		}

		if (scanlines)
			loadScanlineTexture();

		filterDirty = false;
	}

	tileRGB565((const uint8_t*) pixels, pitch, frameWidth, frameHeight, (uint8_t*) texMem);
	DCFlushRange(texMem, (size_t)((frameWidth + 3) & ~3) * ((frameHeight + 3) & ~3) * 2);

	GX_InvalidateTexAll();

	if (prescaleN)
		renderPrescale();

	drawSquare(0, scanlines); // render textured quad

	// The prescale pass loaded its own texture; the next frame's upload needs the source back
	if (prescaleN)
		GX_LoadTexObj(&texobj, GX_TEXMAP0);

	videoDriver->presentBuffer();
}
