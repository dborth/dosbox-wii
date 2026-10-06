/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2008-2026
 * OgcEmulatorVideo.cpp
 ***************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OgcEmulatorVideo.h"
#include "OgcVideoDriver.h"

// The emulator view uses the same 640x480 design space as the menu
#define CANVAS_W 640
#define CANVAS_H 480

static GXTexObj texobj;
static Mtx view;

/*** Square Matrix
     Controls the size of the image on the screen. Rewritten by
     recalculateScaling() whenever the frame size or aspect changes.
***/
static s16 square[12] ATTRIBUTE_ALIGN(32) = {
	-320,  240, 0,	// 0
	 320,  240, 0,	// 1
	 320, -240, 0,	// 2
	-320, -240, 0	// 3
};

struct Camera { guVector pos; guVector up; guVector view; };
static Camera cam = { {0.0F, 0.0F, 0.0F},
                      {0.0F, 0.5F, 0.0F},
                      {0.0F, 0.0F, -0.5F} };

OgcEmulatorVideo::~OgcEmulatorVideo()
{
	free(screenshotSnapshot);
	free(texMem);
}

/****************************************************************************
 * GX setup
 ***************************************************************************/
void OgcEmulatorVideo::configureTEV()
{
	GX_SetNumTexGens (1);
	GX_SetNumTevStages (1);
	GX_SetNumChans (0);

	GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
}

void OgcEmulatorVideo::drawInit()
{
	GX_ClearVtxDesc ();
	GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	configureTEV();

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

void OgcEmulatorVideo::drawSquare()
{
	Mtx m;		// model matrix.
	Mtx mv;		// modelview matrix.

	guMtxIdentity(m);
	guMtxTransApply(m, m, 0, 0, -100);
	guMtxConcat(view, m, mv);

	GX_LoadPosMtxImm(mv, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	draw_vert(0, 0.0, 0.0);
	draw_vert(1, 1.0, 0.0);
	draw_vert(2, 1.0, 1.0);
	draw_vert(3, 0.0, 1.0);
	GX_End();
}

/****************************************************************************
 * resetVideo
 *
 * Reset the video/rendering mode for the emulator rendering
 ****************************************************************************/
void OgcEmulatorVideo::resetVideo()
{
	Mtx44 p;
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

	GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate (GX_TRUE);
	GX_SetBlendMode (GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	guOrtho(p, CANVAS_H/2, -(CANVAS_H/2), -(CANVAS_W/2), CANVAS_W/2, 100, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	drawInit();

	// force the texture object and quad to be rebuilt on the next frame
	frameWidth = 0;
	frameHeight = 0;
	updateScaling = true;
	filterDirty = true;
}

/****************************************************************************
 * recalculateScaling
 *
 * Fits the frame, with its pixel aspect applied, inside the 640x480 canvas
 ***************************************************************************/
void OgcEmulatorVideo::recalculateScaling()
{
	if (frameWidth <= 0 || frameHeight <= 0)
		return;

	float displayW = frameWidth * pixelAspectX;
	float displayH = frameHeight * pixelAspectY;

	float scale = (float)CANVAS_W / displayW;
	float scaleV = (float)CANVAS_H / displayH;
	if (scaleV < scale)
		scale = scaleV;

	frameW = displayW * scale;
	frameH = displayH * scale;
	frameX = (CANVAS_W - frameW) / 2.0f;
	frameY = (CANVAS_H - frameH) / 2.0f;

	s16 halfW = (s16)(frameW / 2.0f + 0.5f);
	s16 halfH = (s16)(frameH / 2.0f + 0.5f);

	square[0] = -halfW; square[1]  =  halfH;
	square[3] =  halfW; square[4]  =  halfH;
	square[6] =  halfW; square[7]  = -halfH;
	square[9] = -halfW; square[10] = -halfH;
	DCFlushRange(square, sizeof(square));
	GX_InvVtxCache();

	updateScaling = false;
}

bool OgcEmulatorVideo::mapPointerToUnit(float canvasX, float canvasY, bool, float* u, float* v)
{
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
	updateScaling = true;
}

void OgcEmulatorVideo::setSmoothing(bool smooth)
{
	if (smoothing != smooth) {
		smoothing = smooth;
		filterDirty = true;
	}
}

void OgcEmulatorVideo::renderInit(int width, int height)
{
	frameWidth = width;
	frameHeight = height;
	updateScaling = true;
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

	size_t size = (size_t)((frameWidth + 3) & ~3) * ((frameHeight + 3) & ~3) * 2;
	screenshotSnapshot = (uint8_t *) malloc(size);
	if (screenshotSnapshot) {
		memcpy(screenshotSnapshot, texMem, size);
		snapWidth = frameWidth;
		snapHeight = frameHeight;
	}
}

// Un-tiles the GX_TF_RGB565 buffer snapshotFrame() captured. The width and
// height must match the frame the snapshot was taken from.
void OgcEmulatorVideo::readFrameRGB24(int width, int height, uint8_t* dst)
{
	if (!screenshotSnapshot)
		return;

	if (width != snapWidth || height != snapHeight) {
		free(screenshotSnapshot);
		screenshotSnapshot = nullptr;
		return;
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
		updateScaling = true;
		filterDirty = true;
	}

	// Wait for the VI to finish displaying the previously submitted frame,
	// and for the GPU to finish rendering it, before touching texture memory.
	videoDriver->waitForBufferReady();

	if (updateScaling)
		recalculateScaling();

	if (filterDirty) {
		GX_InitTexObj(&texobj, texMem, frameWidth, frameHeight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObjFilterMode(&texobj, smoothing ? GX_LINEAR : GX_NEAR, smoothing ? GX_LINEAR : GX_NEAR);
		GX_LoadTexObj(&texobj, GX_TEXMAP0);
		filterDirty = false;
	}

	tileRGB565((const uint8_t*) pixels, pitch, frameWidth, frameHeight, (uint8_t*) texMem);
	DCFlushRange(texMem, (size_t)((frameWidth + 3) & ~3) * ((frameHeight + 3) & ~3) * 2);

	GX_InvalidateTexAll();

	drawSquare(); // render textured quad

	videoDriver->presentBuffer();
}
