/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * WutEmulatorVideo.cpp
 ***************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include <coreinit/memdefaultheap.h>
#include <gx2/mem.h>
#include <gx2/surface.h>
#include <whb/gfx.h>

#include "WutEmulatorVideo.h"
#include "WutVideoDriver.h"
#include "WutOutputFilter.h"
#include "../EmulatorVideoLayout.h"
#include "shaders/Texture2DShader.h"

namespace
{
	// RGB565 channel -> 8 bit expansion (replicating the top bits into the bottom)
	struct Rgb565Tables
	{
		uint8_t r5[32];
		uint8_t g6[64];

		Rgb565Tables()
		{
			for (int i = 0; i < 32; i++)
				r5[i] = (uint8_t)((i << 3) | (i >> 2));
			for (int i = 0; i < 64; i++)
				g6[i] = (uint8_t)((i << 2) | (i >> 4));
		}
	};

	const Rgb565Tables tables;

	void PixelRectToNdc(float x, float y, float w, float h, int designWidth, int designHeight, float offset[3], float scale[3])
	{
		float centerPxX = x + w * 0.5f;
		float centerPxY = y + h * 0.5f;

		offset[0] = (centerPxX / designWidth) * 2.0f - 1.0f;
		offset[1] = 1.0f - (centerPxY / designHeight) * 2.0f;
		offset[2] = 0.0f;

		scale[0] = w / designWidth;
		scale[1] = h / designHeight;
		scale[2] = 1.0f;
	}
}

WutEmulatorVideo::WutEmulatorVideo()
{
	GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
}

WutEmulatorVideo::~WutEmulatorVideo()
{
	destroyTexture();
	free(screenshotSnapshot);
	screenshotSnapshot = nullptr;
}

void WutEmulatorVideo::init(VideoDriver* driver)
{
	videoDriver = static_cast<WutVideoDriver*>(driver);
}

/****************************************************************************
 * resetVideo
 *
 * The emulator takes (or takes back) the display. Nothing about the GX2
 * state needs reconfiguring - WutVideoDriver::prepareFrame() rebinds both
 * targets every frame - so only the placement is recomputed.
 ***************************************************************************/
void WutEmulatorVideo::resetVideo()
{
	placementDirty = true;
}

/****************************************************************************
 * stopVideo
 *
 * Waits for the GPU to retire the last submitted frame, so nothing of it is
 * still in flight when the menu starts drawing.
 ***************************************************************************/
void WutEmulatorVideo::stopVideo()
{
	videoDriver->waitForIdle();
}

void WutEmulatorVideo::setPixelAspect(float scaleX, float scaleY)
{
	if (scaleX <= 0.0f) scaleX = 1.0f;
	if (scaleY <= 0.0f) scaleY = 1.0f;
	if (scaleX == pixelAspectX && scaleY == pixelAspectY)
		return;

	pixelAspectX = scaleX;
	pixelAspectY = scaleY;
	placementDirty = true;
}

void WutEmulatorVideo::settingsChanged()
{
	samplerDirty = true;
	placementDirty = true;
}

EmulatorVideoCapabilities WutEmulatorVideo::getCapabilities() const
{
	EmulatorVideoCapabilities caps;
	caps.scanlines = true;
	caps.sharpFilter = true;
	// Each output's real shape is known, so there is nothing to tell it
	caps.widescreenSetting = false;
	return caps;
}

bool WutEmulatorVideo::getCanvasRect(float* x, float* y, float* w, float* h)
{
	if (frameWidth <= 0 || frameHeight <= 0)
		return false;

	if (placementDirty)
		recalculatePlacement();

	if (canvasW <= 0.0f || canvasH <= 0.0f)
		return false;

	if (x) *x = canvasX;
	if (y) *y = canvasY;
	if (w) *w = canvasW;
	if (h) *h = canvasH;
	return true;
}

/****************************************************************************
 * recalculatePlacement
 *
 * Places the frame inside each target's own physical pixels with the user's
 * settings (fit, aspect, zoom, shift). Every target's buffer is
 * square-pixel, so a target's aspect ratio is simply width/height and no
 * 16:9 setting is needed: a 16:9 TV and the 4:3-ish GamePad each get a
 * correctly shaped picture.
 ***************************************************************************/
void WutEmulatorVideo::recalculatePlacement()
{
	if (frameWidth <= 0 || frameHeight <= 0)
		return;

	for (int i = 0; i < OUTPUT_TARGET_COUNT; i++)
	{
		const OutputTarget target = static_cast<OutputTarget>(i);

		VideoLayoutInput in;
		in.frameWidth = frameWidth;
		in.frameHeight = frameHeight;
		in.pixelAspectX = pixelAspectX;
		in.pixelAspectY = pixelAspectY;
		in.targetWidth = (float)videoDriver->getTargetWidth(target);
		in.targetHeight = (float)videoDriver->getTargetHeight(target);

		const VideoRect r = ComputeVideoLayout(settings, in);
		placement[i].x = r.x;
		placement[i].y = r.y;
		placement[i].w = r.w;
		placement[i].h = r.h;
	}

	const TargetPlacement& tv = placement[static_cast<int>(OutputTarget::TV)];
	const float toCanvasX = (float)videoDriver->getScreenWidth()  / (float)videoDriver->getTargetWidth(OutputTarget::TV);
	const float toCanvasY = (float)videoDriver->getScreenHeight() / (float)videoDriver->getTargetHeight(OutputTarget::TV);

	canvasX = tv.x * toCanvasX;
	canvasY = tv.y * toCanvasY;
	canvasW = tv.w * toCanvasX;
	canvasH = tv.h * toCanvasY;

	placementDirty = false;
}

/****************************************************************************
 * mapPointerToUnit
 *
 * The pointer is reported in UI-canvas coordinates, which span the whole
 * screen on every output, so they are a fraction of the target the pointer
 * is on. That is mapped through that target's own placement.
 ***************************************************************************/
bool WutEmulatorVideo::mapPointerToUnit(float canvasXPos, float canvasYPos, bool onGamePad, float* u, float* v)
{
	if (!u || !v)
		return false;

	if (placementDirty)
		recalculatePlacement();

	const OutputTarget target = onGamePad ? OutputTarget::DRC : OutputTarget::TV;
	const TargetPlacement& p = placement[static_cast<int>(target)];
	if (p.w <= 0.0f || p.h <= 0.0f) // no frame yet
		return false;

	const float px = (canvasXPos / (float)videoDriver->getScreenWidth())  * (float)videoDriver->getTargetWidth(target);
	const float py = (canvasYPos / (float)videoDriver->getScreenHeight()) * (float)videoDriver->getTargetHeight(target);

	const float fx = (px - p.x) / p.w;
	const float fy = (py - p.y) / p.h;
	*u = fx < 0.0f ? 0.0f : (fx > 1.0f ? 1.0f : fx);
	*v = fy < 0.0f ? 0.0f : (fy > 1.0f ? 1.0f : fy);
	return true;
}

/****************************************************************************
 * rebuildTexture / destroyTexture
 *
 * The texture is recreated only when the frame's width/height changes (see
 * presentFrame), not every frame. Callers must have waited for the GPU to
 * retire any frame that sampled the old one (presentFrame() does, through
 * prepareFrame()).
 ***************************************************************************/
void WutEmulatorVideo::destroyTexture()
{
	if (!texture)
		return;

	if (texture->surface.image)
		MEMFreeToDefaultHeap(texture->surface.image);

	delete texture;
	texture = nullptr;
}

void WutEmulatorVideo::rebuildTexture(int width, int height)
{
	destroyTexture();

	if (width <= 0 || height <= 0)
		return;

	texture = new GX2Texture();
	GX2InitTexture(texture, width, height, 1, 0, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_SURFACE_DIM_TEXTURE_2D, GX2_TILE_MODE_LINEAR_ALIGNED);

	GX2CalcSurfaceSizeAndAlignment(&texture->surface);
	GX2InitTextureRegs(texture);

	texture->surface.image = MEMAllocFromDefaultHeapEx(texture->surface.imageSize, texture->surface.alignment);
	if (!texture->surface.image)
	{
		delete texture;
		texture = nullptr;
	}
}

/****************************************************************************
 * uploadFrame
 *
 * Converts the RGB565 frame (R in the top 5 bits of each host-endian
 * uint16_t) into the RGBA8 texture, a row at a time. `pitch` is bytes
 * between source rows.
 ***************************************************************************/
void WutEmulatorVideo::uploadFrame(const uint16_t* pixels, int width, int height, int pitch)
{
	if (!texture || !texture->surface.image)
		return;

	uint8_t* dst = static_cast<uint8_t*>(texture->surface.image);
	const uint32_t dstStride = texture->surface.pitch * 4; // bytes/row, RGBA8

	for (int y = 0; y < height; y++)
	{
		const uint16_t* srcRow = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(pixels) + (size_t)y * pitch);
		uint8_t* out = dst + (size_t)y * dstStride;

		for (int x = 0; x < width; x++, out += 4)
		{
			const uint16_t px = srcRow[x];

			out[0] = tables.r5[(px >> 11) & 0x1F];
			out[1] = tables.g6[(px >> 5)  & 0x3F];
			out[2] = tables.r5[px         & 0x1F];
			out[3] = 0xFF;
		}
	}

	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture->surface.image, texture->surface.imageSize);
}

/****************************************************************************
 * drawQuad
 *
 * Draws the frame into the TV target, then the GamePad target, sized and
 * positioned per target by placement[].
 ***************************************************************************/
void WutEmulatorVideo::drawQuad()
{
	videoDriver->flushDrawQueue();

	if (!texture || !videoDriver->isForeground())
		return;

	if (samplerDirty)
	{
		// Sharp samples linearly too: the output filter does the sharpening
		GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP,
			settings.filter == VideoFilter::Nearest ? GX2_TEX_XY_FILTER_MODE_POINT : GX2_TEX_XY_FILTER_MODE_LINEAR);
		samplerDirty = false;
	}

	const float colorIntensity[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	Texture2DShader* shader = Texture2DShader::instance();

	auto placementNdc = [&](OutputTarget target, float offset[3], float scale[3]) {
		const TargetPlacement& p = placement[static_cast<int>(target)];
		PixelRectToNdc(p.x, p.y, p.w, p.h, videoDriver->getTargetWidth(target), videoDriver->getTargetHeight(target), offset, scale);
	};

	auto drawPass = [&](OutputTarget target) {
		float offset[3];
		float scale[3];
		placementNdc(target, offset, scale);

		shader->setShaders();
		shader->setAttributeBuffer();
		shader->setAngle(0.0f);
		shader->setOffset(offset);
		shader->setScale(scale);
		shader->setColorIntensity(colorIntensity);
		shader->clearBlur();
		shader->setTextureAndSampler(texture, &sampler);
		shader->draw(GX2_PRIMITIVE_MODE_QUADS, 4);
	};

	const bool sharp = settings.filter == VideoFilter::Sharp;
	const float scanlines = settings.scanlines;

	// Sharp bilinear and/or scanlines go through the output filter; if it is
	// unavailable (shader failed to set up) the plain textured quad is drawn
	auto drawGame = [&](OutputTarget target) {
		if (sharp || scanlines > 0.0f)
		{
			const TargetPlacement& p = placement[static_cast<int>(target)];

			WutOutputFilter::Params pp;
			pp.texture = texture;
			placementNdc(target, pp.offset, pp.scale);
			pp.outWidth = p.w;
			pp.outHeight = p.h;
			pp.linear = settings.filter != VideoFilter::Nearest;
			pp.sharp = sharp;
			pp.scanlineStrength = scanlines;
			// the emulated lines the quad covers, so the gaps follow the picture's own lines
			pp.sourceLines = (float)frameHeight;
			if (WutOutputFilter::instance()->draw(pp))
				return;
		}
		drawPass(target);
	};

	WHBGfxBeginRenderTV();
	drawGame(OutputTarget::TV);
	WHBGfxBeginRenderDRC();
	drawGame(OutputTarget::DRC);
}

/****************************************************************************
 * presentFrame
 *
 * prepareFrame() first: it waits for the GPU to retire the previous frame
 * (so the texture can be rewritten or replaced) and rebinds/clears both
 * targets.
 ***************************************************************************/
void WutEmulatorVideo::presentFrame(const uint16_t* pixels, int width, int height, int pitch)
{
	if (!pixels || width <= 0 || height <= 0)
		return;

	videoDriver->prepareFrame();

	if (!texture || width != frameWidth || height != frameHeight)
	{
		rebuildTexture(width, height);
		frameWidth = texture ? width : 0;
		frameHeight = texture ? height : 0;
		placementDirty = true;
	}

	if (texture)
	{
		if (placementDirty)
			recalculatePlacement();

		uploadFrame(pixels, width, height, pitch);
		drawQuad();
	}

	videoDriver->presentBuffer();
}

/****************************************************************************
 * snapshotFrame / getSnapshotInfo / readFrameRowRGB24
 ***************************************************************************/
void WutEmulatorVideo::snapshotFrame()
{
	free(screenshotSnapshot);
	screenshotSnapshot = nullptr;

	if (!texture || !texture->surface.image || frameWidth <= 0 || frameHeight <= 0)
		return;

	screenshotSnapshot = (uint8_t*)malloc(texture->surface.imageSize);
	if (!screenshotSnapshot)
		return;

	memcpy(screenshotSnapshot, texture->surface.image, texture->surface.imageSize);
	snapWidth  = frameWidth;
	snapHeight = frameHeight;
	snapPitch  = texture->surface.pitch;

	if (placementDirty)
		recalculatePlacement();
	snapX = canvasX; snapY = canvasY; snapW = canvasW; snapH = canvasH;
}

bool WutEmulatorVideo::getSnapshotInfo(FrameSnapshotInfo* info) const
{
	if (!screenshotSnapshot || !info || snapW <= 0.0f || snapH <= 0.0f)
		return false;

	info->width  = snapWidth;
	info->height = snapHeight;
	info->x = snapX; info->y = snapY; info->w = snapW; info->h = snapH;
	return true;
}

bool WutEmulatorVideo::readFrameRowRGB24(int y, uint8_t* dst)
{
	if (!screenshotSnapshot || !dst || y < 0 || y >= snapHeight)
		return false;

	const uint32_t srcStride = snapPitch * 4; // bytes/row, RGBA8
	const uint8_t* srcRow = screenshotSnapshot + (size_t)y * srcStride;

	for (int x = 0; x < snapWidth; x++)
	{
		dst[x * 3 + 0] = srcRow[x * 4 + 0];
		dst[x * 3 + 1] = srcRow[x * 4 + 1];
		dst[x * 3 + 2] = srcRow[x * 4 + 2];
	}
	return true;
}

void WutEmulatorVideo::releaseSnapshot()
{
	free(screenshotSnapshot);
	screenshotSnapshot = nullptr;
}
