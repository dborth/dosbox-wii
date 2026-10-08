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
		void stopVideo() override;
		int getMaxFrameDimension() const override { return MAX_TEX_DIM; }
		void presentFrame(const uint16_t* pixels, int width, int height, int pitch) override;
		void setPixelAspect(float scaleX, float scaleY) override;
		EmulatorVideoCapabilities getCapabilities() const override;
		bool getCanvasRect(float* x, float* y, float* w, float* h) override;

		//! Copies the live (4x4-tiled GX_TF_RGB565) texture into a
		//! driver-owned buffer so it survives until readFrameRGB24()
		void snapshotFrame() override;
		bool getSnapshotInfo(FrameSnapshotInfo* info) const override;
		//! Un-tiles the buffer snapshotFrame() captured into packed RGB24
		bool readFrameRGB24(int width, int height, uint8_t* dst) override;

		void renderInit(int width, int height) override;
		bool mapPointerToUnit(float canvasX, float canvasY, bool onGamePad, float* u, float* v) override;

	protected:
		void settingsChanged() override;

	private:
		//! GX can address at most 1024x1024 per texture
		static constexpr int MAX_TEX_DIM = 1024;

		void drawInit(bool scanlines);
		void configureTEV(bool scanlines);
		void drawSquare(u8 first, bool scanlines);
		void loadScanlineTexture();
		void setCanvasProjection();
		void recalculateScaling();
		void ensureRect();
		bool scanlinesSupported() const;
		bool scanlinesActive() const;
		bool widescreenCompensation() const;
		int choosePrescale();
		bool ensurePrescaleBuffer(int width, int height);
		void renderPrescale();
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
		float snapX = 0, snapY = 0, snapW = 0, snapH = 0; // frame rect at capture

		float pixelAspectX = 1.0f;
		float pixelAspectY = 1.0f;
		//! rectDirty: the canvas rect needs recomputing (no GX). updateScaling: the
		//! vertex array needs rewriting. filterDirty: texture objects need
		//! rebuilding. drawDirty: vertex description / TEV need setting up again.
		bool rectDirty = true;
		bool updateScaling = true;
		bool filterDirty = true;
		bool drawDirty = true;
		bool scanlinesApplied = false;

		// Sharp filter: the whole-number enlargement of the frame (0 = none), and
		// the buffer the first pass copies it into
		int prescaleN = 0;
		void* prescaleMem = nullptr;
		size_t prescaleCapacity = 0;

		// One-shot: allocated by snapshotFrame(), freed by readFrameRGB24()
		uint8_t* screenshotSnapshot = nullptr;

		// The game quad's rect on the UI canvas (top-left x/y, size w/h),
		// recomputed by recalculateScaling(); used to map the pointer
		float frameX = 0, frameY = 0, frameW = 0, frameH = 0;
};
