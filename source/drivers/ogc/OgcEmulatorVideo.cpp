/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2008-2026
 * OgcEmulatorVideo.cpp
 ***************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogc/timesupp.h>
#include <ogc/machine/processor.h>

#include "OgcEmulatorVideo.h"
#include "OgcVideoDriver.h"

static GXTexObj texobj;
static GXTexObj cursorObj;
static Mtx view;
static int vwidth, vheight;
static int updateScaling;

/* New texture based scaler */
typedef struct tagcamera
  {
    guVector pos;
    guVector up;
    guVector view;
  }
camera;

/*** Square Matrix
     This structure controls the size of the image on the screen.
***/
static s16 square[] ATTRIBUTE_ALIGN(32) = {
	/*
	* X,   Y,  Z
	* Values set are for roughly 4:3 aspect
	*/
	-200,  200, 0,	// 0
	 200,  200, 0,	// 1
	 200, -200, 0,	// 2
	-200, -200, 0	// 3
    };

// 96x96 static quad for the cursor (Centered at 0,0)
static s16 cursor_square[] ATTRIBUTE_ALIGN(32) = {
	-48,  48, 0,	// 0: Top Left
	 48,  48, 0,	// 1: Top Right
	 48, -48, 0,	// 2: Bottom Right
	-48, -48, 0 	// 3: Bottom Left
};

static camera cam = { {0.0F, 0.0F, 0.0F},
                      {0.0F, 0.5F, 0.0F},
                      {0.0F, 0.0F, -0.5F}
                    };

/****************************************************************************
 * Scaler Support Functions
 ****************************************************************************/
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
	Mtx m;			// model matrix.
	Mtx mv;			// modelview matrix.

	if (TiltScreen)
	{
		guMtxRotDeg(m, 'z', -TiltAngle);
		guMtxScaleApply(m, m, 0.8, 0.8, 1);
	}
	else
	{
		guMtxIdentity(m);
	}

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

void OgcEmulatorVideo::drawCursor()
{
	// --- 1. ISOLATE AND BIND THE CURSOR ARRAY ---
	// We reuse GX_VTXFMT0 to avoid crashing the menu, but we swap the positional array.
	// Flush the static array to main memory to guarantee the GPU reads it correctly.
	DCFlushRange(cursor_square, 32);
	GX_SetArray(GX_VA_POS, cursor_square, 3 * sizeof(s16));

	// --- 2. DISABLE TEX1 ---
	GX_SetVtxDesc(GX_VA_TEX1, GX_NONE);

	// --- 3. CONFIGURE TEV FOR UI ---
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	GX_SetNumChans(0);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);

	// --- 4. CONFIGURE ALPHA BLENDING ---
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_LoadTexObj(&cursorObj, GX_TEXMAP0);

	// --- 5. MATH & POSITIONING ---
	// Map the 0-640 / 0-480 IR coordinates into the -320 to 320 ortho space
	f32 cX = (f32)CursorX - 320.0f;
	f32 cY = 240.0f - (f32)CursorY;

	Mtx m, mv;
	guMtxIdentity(m);
	// Z MUST BE -100.0f
	guMtxTransApply(m, m, cX, cY, -100.0f);
	guMtxConcat(view, m, mv);
	GX_LoadPosMtxImm(mv, GX_PNMTX0);

	// --- 6. DRAW THE CURSOR ---
	// Using the exact same draw_vert logic the game loop uses
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		draw_vert(0, 0.0f, 0.0f);
		draw_vert(1, 1.0f, 0.0f);
		draw_vert(2, 1.0f, 1.0f);
		draw_vert(3, 0.0f, 1.0f);
	GX_End();

	// --- 7. CRITICAL STATE RESTORATION ---
	// Restore array pointer to the game's dynamic scaling square
	GX_SetArray(GX_VA_POS, square, 3 * sizeof(s16));

	// Rebind the game texture to MAP0
	GX_LoadTexObj(&texobj, GX_TEXMAP0);

	// Restore Blending
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	// Restore TEV pipeline exactly how the next frame's draw_square expects it
	configureTEV();
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

	videoDriver->setupVideoMode(rmode); // reconfigure VI

	// reconfigure GX
	GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);

	GX_SetDispCopyFrame2Field (rmode->copy_interlaced);
	GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyYScale (GX_GetYScaleFactor (rmode->efbHeight, rmode->xfbHeight));
	GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);

	u8* vfilter = {0,0,21,22,21,0,0};
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

	guOrtho(p, 480/2, -(480/2), -(640/2), 640/2, 100, 1000);	// matrix, t, b, l, r, n, f
	GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

	drawInit();
	// set aspect ratio
	updateScaling = 1;
}

/****************************************************************************
 * recalculateScaling
 *
 * Recomputes the on-screen quad and gameScreenPng scale/offset whenever the
 * console resolution changes.
 ***************************************************************************/
void OgcEmulatorVideo::recalculateScaling()
{
	// TODO
	updateScaling = 0;
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

// Converts flat, row-major RGBA8 pixels into GX's native 4x4-tiled
// GX_TF_RGB5A3 layout, opaque/RGB555-mode (bit15 set), writing directly into
// a caller-supplied destination (the live GX texture memory).

void OgcEmulatorVideo::tileRGBA8ToGxTexture(const uint8_t *rgba, int width, int height, void *dst)
{
	int padWidth = (width + 3) & ~3;
	int padHeight = (height + 3) & ~3;

	uint16_t *tiled = (uint16_t *) dst;

	for (int y = 0; y < padHeight; y++) {
		int tile_y = y / 4;
		int in_tile_y = y % 4;
		for (int x = 0; x < padWidth; x++) {
			int tile_x = x / 4;
			int in_tile_x = x % 4;
			int idx = (tile_y * (padWidth / 4) + tile_x) * 16 + (in_tile_y * 4 + in_tile_x);

			uint16_t color = 0x8000; // RGB555 mode, opaque
			if (x < width && y < height) {
				const uint8_t *px = rgba + (y * width + x) * 4;
				uint8_t r5 = px[0] >> 3;
				uint8_t g5 = px[1] >> 3;
				uint8_t b5 = px[2] >> 3;
				color |= (r5 << 10) | (g5 << 5) | b5;
			}
			tiled[idx] = color;
		}
	}
}

void OgcEmulatorVideo::snapshotFrame()
{
	if(screenshotSnapshot)
	{
		free(screenshotSnapshot);
		screenshotSnapshot = nullptr;
	}

	if(!texturemem)
		return;

	screenshotSnapshot = (uint8_t *)malloc(TEXTUREMEM_SIZE);
	if(screenshotSnapshot)
		memcpy(screenshotSnapshot, texturemem, TEXTUREMEM_SIZE);
}

// Un-swizzles the 4x4-tiled GX_TF_RGB5A3 buffer snapshotFrame() captured
void OgcEmulatorVideo::readFrameRGB24(int width, int height, uint8_t* dst)
{
	if(!screenshotSnapshot)
		return;

	int padded_width = (width + 3) & ~3;

	const uint16_t * tex16 = (const uint16_t *) screenshotSnapshot;

	for(int y = 0; y < height; y++) {
		int tile_y = y / 4;
		int in_tile_y = y % 4;
		for(int x = 0; x < width; x++) {
			int tile_x = x / 4;
			int in_tile_x = x % 4;

			int tex_pixel_idx = (tile_y * (padded_width / 4) + tile_x) * 16 + (in_tile_y * 4 + in_tile_x);
			uint16_t color = tex16[tex_pixel_idx];

			// RGB555 format
			u8 r = (color >> 10) & 0x1F;
			u8 g = (color >> 5) & 0x1F;
			u8 b = color & 0x1F;

			int out_idx = (y * width + x) * 3;
			dst[out_idx]     = (r << 3) | (r >> 2);
			dst[out_idx + 1] = (g << 3) | (g >> 2);
			dst[out_idx + 2] = (b << 3) | (b >> 2);
		}
	}

	free(screenshotSnapshot);
	screenshotSnapshot = nullptr;
}

static void MakeTexture(const void *src, void *dst, s32 width, s32 height, s32 pitch, s32 dst_gap_bytes)
{
    u32 src_row_stride = pitch * 4;
    u32 r_src_row, row_ptr, mask;
    u32 tmpA, tmpB, tmpC, tmpD;

    __asm__ __volatile__ (
        "lis    %[mask], 0x8000\n"
        "ori    %[mask], %[mask], 0x8000\n"

        "srwi   %[width], %[width], 2\n"       // num_tiles_x = width / 4
        "srwi   %[height], %[height], 2\n"     // num_tiles_y = height / 4

    "2: mtctr   %[width]\n"                    // Set inner loop counter (X)
        "mr     %[r_src_row], %[src]\n"        // Save the start of the current source 4-row block

    "1: dcbz    0, %[dst]\n"                   // ZERO L1 CACHE: Dest is perfectly 32-byte aligned
        "mr     %[row_ptr], %[src]\n"

        // Load Row 0
        "lwz    %[tmpA], 0(%[row_ptr])\n"
        "lwz    %[tmpB], 4(%[row_ptr])\n"
        "add    %[row_ptr], %[row_ptr], %[pitch]\n"

        // Load Row 1, Store Row 0
        // Interleaving hides the 3-cycle load latency
        "lwz    %[tmpC], 0(%[row_ptr])\n"
        "or     %[tmpA], %[tmpA], %[mask]\n"
        "stw    %[tmpA], 0(%[dst])\n"

        "lwz    %[tmpD], 4(%[row_ptr])\n"
        "or     %[tmpB], %[tmpB], %[mask]\n"
        "stw    %[tmpB], 4(%[dst])\n"
        "add    %[row_ptr], %[row_ptr], %[pitch]\n"

        // Load Row 2, Store Row 1
        "lwz    %[tmpA], 0(%[row_ptr])\n"      // Recycle tmpA and tmpB
        "or     %[tmpC], %[tmpC], %[mask]\n"
        "stw    %[tmpC], 8(%[dst])\n"

        "lwz    %[tmpB], 4(%[row_ptr])\n"
        "or     %[tmpD], %[tmpD], %[mask]\n"
        "stw    %[tmpD], 12(%[dst])\n"
        "add    %[row_ptr], %[row_ptr], %[pitch]\n"

        // Load Row 3, Store Row 2
        "lwz    %[tmpC], 0(%[row_ptr])\n"
        "or     %[tmpA], %[tmpA], %[mask]\n"
        "stw    %[tmpA], 16(%[dst])\n"

        "lwz    %[tmpD], 4(%[row_ptr])\n"
        "or     %[tmpB], %[tmpB], %[mask]\n"
        "stw    %[tmpB], 20(%[dst])\n"

        // Store Row 3
        "or     %[tmpC], %[tmpC], %[mask]\n"
        "stw    %[tmpC], 24(%[dst])\n"

        "or     %[tmpD], %[tmpD], %[mask]\n"
        "stw    %[tmpD], 28(%[dst])\n"

        // Advance pointers for the next tile in the row
        "addi   %[src], %[src], 8\n"           // Advance src X by 4 pixels (8 bytes)
        "addi   %[dst], %[dst], 32\n"          // Advance dst by 1 full tile (32 bytes)
        "bdnz   1b\n"                          // Decrement CTR, loop inner if > 0

        // Advance pointers to the next row of tiles
        "add    %[src], %[r_src_row], %[src_row_stride]\n" // Jump down 4 source rows
        "add    %[dst], %[dst], %[dst_gap_bytes]\n"        // Skip right/left borders in dest

        "subic. %[height], %[height], 1\n"     // Decrement height counter (Y)
        "bne    2b\n"                          // Loop outer if > 0

        : [r_src_row] "=&b" (r_src_row),
          [row_ptr] "=&b" (row_ptr),
          [mask] "=&r" (mask),
          [tmpA] "=&r" (tmpA),
          [tmpB] "=&r" (tmpB),
          [tmpC] "=&r" (tmpC),
          [tmpD] "=&r" (tmpD),
          [src] "+b" (src),
          [dst] "+b" (dst),
          [width] "+r" (width),
          [height] "+r" (height)
        : [pitch] "r" (pitch),
          [src_row_stride] "r" (src_row_stride),
          [dst_gap_bytes] "r" (dst_gap_bytes)
        : "memory", "cc"
    );
}

/****************************************************************************
 * writeFrameToTextureMemory
 ****************************************************************************/
void OgcEmulatorVideo::writeFrameToTextureMemory(u8* srcBuffer, void* textureBase, int width, int height)
{
	long long int* dst_ptr = processFrameAndGetDest(textureBase, (const uint16_t*)srcBuffer, width, height);

	int targetWidth  = gameBorder.hasBorder() ? gameBorder.getWidth()  : width;
	int targetHeight = gameBorder.hasBorder() ? gameBorder.getHeight() : height;

	int pitch = width * 2 + 4;
	int dst_gap_bytes = ((targetWidth - width) / 4) * 32;

	MakeTexture(srcBuffer, dst_ptr, width, height, pitch, dst_gap_bytes);

	// High-efficiency targeted data cache flushing
	if (targetWidth > width && !updateScaling) {
		// Normal Frame: Flush ONLY the game screen cache lines
		u8* flush_ptr = (u8*)dst_ptr;
		u32 row_bytes = width * 8; // bytes per tile row for game screen
		u32 stride_bytes = targetWidth * 8; // full texture pitch stride bytes
		int tile_rows = height / 4;
		for (int i = 0; i < tile_rows; i++) {
			DCStoreRange(flush_ptr, row_bytes);
			flush_ptr += stride_bytes;
		}
	} else {
		// Flush everything if borderless, OR if the border was just copied this frame
		DCStoreRange(textureBase, targetWidth * targetHeight * 2);
	}
}

void OgcEmulatorVideo::init(VideoDriver* driver)
{
	videoDriver = static_cast<OgcVideoDriver*>(driver);
}

void OgcEmulatorVideo::renderInit(int width, int height)
{
	// Setup for first call to scaler
	vwidth = width;
	vheight = height;
}

/****************************************************************************
 * presentFrame
 *
 * Pass in the console's width/height to update as a tiled RGB555 texture
 * (2 bytes per pixel). Reads directly from the emulator core's shared
 * display buffer ('pix'), matching what GX_Render used to receive as its
 * explicit buffer argument.
 ****************************************************************************/
void OgcEmulatorVideo::presentFrame(int targetWidth, int targetHeight)
{
	u8* buffer = pix;

	if (vwidth != targetWidth || vheight != targetHeight) {
		vwidth = targetWidth;
		vheight = targetHeight;
		updateScaling = 1;
	}

	// Wait for the VI to finish displaying the previously submitted frame,
	// and for the GPU to finish rendering it, before touching texture memory.
	videoDriver->waitForBufferReady();

	if (updateScaling) {
		recalculateScaling();

		GX_InitTexObj(&texobj, texturemem, vwidth * fscale, vheight * fscale, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObjFilterMode(&texobj,GX_NEAR,GX_NEAR);
		GX_LoadTexObj(&texobj, GX_TEXMAP0);
		GX_InitTexObj(&cursorObj, pointer[0]->getTexture(), 96, 96, GX_TF_RGBA8,GX_CLAMP, GX_CLAMP,GX_FALSE);
	}

	writeFrameToTextureMemory(buffer, texturemem, consoleWidth, consoleHeight);

	GX_InvalidateTexAll();

	drawSquare(); // render textured quad
	drawCursor(); // render cursor

	videoDriver->presentBuffer();
}
