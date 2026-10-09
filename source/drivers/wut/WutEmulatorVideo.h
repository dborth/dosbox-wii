/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * WutEmulatorVideo.h
 *
 * EmulatorVideoDriver implementation for Wii U: converts DOSBox's RGB565
 * frame into a linear RGBA8 GX2 texture and draws it with the shared
 * Texture2DShader, to both the TV and the GamePad.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include <gx2/sampler.h>
#include <gx2/texture.h>
#include "../EmulatorVideoDriver.h"
#include "WutVideoDriver.h"

class WutEmulatorVideo : public EmulatorVideoDriver
{
	public:
		WutEmulatorVideo();
		~WutEmulatorVideo() override;

		void init(VideoDriver* videoDriver) override;
		void resetVideo() override;
		void stopVideo() override;
		void presentFrame(const uint16_t* pixels, int width, int height, int pitch) override;
		void setPixelAspect(float scaleX, float scaleY) override;
		EmulatorVideoCapabilities getCapabilities() const override;
		bool getCanvasRect(float* x, float* y, float* w, float* h) override;

		//! Copies the live RGBA8 texture into a driver-owned buffer so it
		//! survives until releaseSnapshot()
		void snapshotFrame() override;
		bool getSnapshotInfo(FrameSnapshotInfo* info) const override;
		//! Packs the RGBA8 buffer snapshotFrame() captured into RGB24
		bool readFrameRowRGB24(int y, uint8_t* dst) override;
		void releaseSnapshot() override;

		bool mapPointerToUnit(float canvasX, float canvasY, bool onGamePad, float* u, float* v) override;

	protected:
		void settingsChanged() override;

	private:
		void rebuildTexture(int width, int height);
		void destroyTexture();
		//! Converts the RGB565 frame into the live linear GX2 texture
		void uploadFrame(const uint16_t* pixels, int width, int height, int pitch);
		void drawQuad();
		//! Recomputes where the frame goes on each output target, and the
		//! same placement in UI-canvas pixels
		void recalculatePlacement();

		WutVideoDriver* videoDriver = nullptr;

		GX2Texture* texture = nullptr;
		GX2Sampler sampler;

		// Size of the frame currently in the texture
		int frameWidth = 0;
		int frameHeight = 0;

		float pixelAspectX = 1.0f;
		float pixelAspectY = 1.0f;
		bool samplerDirty = true;
		bool placementDirty = true;

		// Where the frame is drawn: top-left x/y and size w/h in the physical
		// pixels of each render target. The TV and the GamePad are fitted
		// independently, so a 16:9 TV and the GamePad each get a correctly
		// shaped picture.
		struct TargetPlacement { float x, y, w, h; };
		TargetPlacement placement[OUTPUT_TARGET_COUNT] = { {0, 0, 0, 0}, {0, 0, 0, 0} };

		// The TV placement expressed in UI-canvas pixels, for the menu's
		// screenshot background (the canvas is stretched onto each target
		// per axis, so this lines up with what the player was looking at)
		float canvasX = 0, canvasY = 0, canvasW = 0, canvasH = 0;

		// Allocated by snapshotFrame(), freed by releaseSnapshot()
		uint8_t* screenshotSnapshot = nullptr;
		int snapWidth = 0, snapHeight = 0;
		uint32_t snapPitch = 0; // texels/row, RGBA8, as snapshotted
		float snapX = 0, snapY = 0, snapW = 0, snapH = 0; // canvas rect at capture
};
