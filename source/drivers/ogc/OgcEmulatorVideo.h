/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2008-2026
 * OgcEmulatorVideo.h
 *
 * Presents DOSBox's RGB565 frame as a GX_TF_RGB565 textured quad
 ***************************************************************************/
#pragma once

#include <gccore.h>
#include <stdint.h>
#include <stdlib.h>
#include "../EmulatorVideoDriver.h"

class OgcVideoDriver;

class OgcEmulatorVideo : public EmulatorVideoDriver
{
	public:
		OgcEmulatorVideo() : videoDriver(nullptr) {}
		~OgcEmulatorVideo() override;

		void init(VideoDriver* videoDriver) override;
		void resetVideo() override;
		void presentFrame(const uint16_t* pixels, int width, int height, int pitch) override;
		void setPixelAspect(float scaleX, float scaleY) override;
		void setSmoothing(bool smooth) override;

		//! Copies the live (4x4-tiled GX_TF_RGB565) texture into a
		//! driver-owned buffer so it survives until readFrameRGB24()
		void snapshotFrame() override;
		//! Un-tiles the buffer snapshotFrame() captured into packed RGB24
		void readFrameRGB24(int width, int height, uint8_t* dst) override;

		void renderInit(int width, int height) override;
		bool mapPointerToUnit(float canvasX, float canvasY, bool onGamePad, float* u, float* v) override;

	private:
		//! GX can address at most 1024x1024 per texture
		static constexpr int MAX_TEX_DIM = 1024;

		void drawInit();
		void configureTEV();
		void drawSquare();
		void recalculateScaling();
		bool ensureTexture(int width, int height);
		void tileRGB565(const uint8_t* src, int pitch, int width, int height, uint8_t* dst);

		OgcVideoDriver* videoDriver;

		// Texture backing store (tiled RGB565, 32-byte aligned)
		void* texMem = nullptr;
		size_t texCapacity = 0;

		// Size of the frame currently in texMem
		int frameWidth = 0;
		int frameHeight = 0;
		int snapWidth = 0;
		int snapHeight = 0;

		float pixelAspectX = 1.0f;
		float pixelAspectY = 1.0f;
		bool smoothing = true;
		bool updateScaling = true;
		bool filterDirty = true;

		// One-shot: allocated by snapshotFrame(), freed by readFrameRGB24()
		uint8_t* screenshotSnapshot = nullptr;

		// The game quad's rect on the UI canvas (top-left x/y, size w/h),
		// recomputed by recalculateScaling(); used to map the pointer
		float frameX = 0, frameY = 0, frameW = 0, frameH = 0;
};
