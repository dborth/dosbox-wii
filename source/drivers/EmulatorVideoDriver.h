/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * EmulatorVideoDriver.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "VideoDriver.h"
#include "EmulatorVideoSettings.h"

//! Describes the frame captured by EmulatorVideoDriver::snapshotFrame()
struct FrameSnapshotInfo
{
	int width;     //!< snapshot width in pixels (pass to readFrameRGB24)
	int height;    //!< snapshot height in pixels (pass to readFrameRGB24)
	//! Where the frame was on the UI canvas (the 640x480 design space of the
	//! menu) when it was captured: aspect, scale and position already applied,
	//! so a background built from it lines up with what the player was looking at.
	float x, y, w, h;
};

class EmulatorVideoDriver
{
	public:
		virtual ~EmulatorVideoDriver() = default;

		virtual void init(VideoDriver* videoDriver) = 0;
		//! Takes the display for the emulator. Also used to take it back from the menu.
		virtual void resetVideo() = 0;

		//! The emulator gives up the display (eg. for the menu). Waits until the
		//! last frame handed to presentFrame() has actually been displayed, so
		//! nothing of it is still in flight when something else starts drawing.
		//! Safe at any time, including before the first frame. Pairs with
		//! resetVideo().
		virtual void stopVideo() {}

		//! Largest frame width or height, in pixels, that presentFrame() accepts.
		virtual int getMaxFrameDimension() const { return 1024; }

		//! Draws and presents one emulator frame.
		//! \param pixels   RGB565 (big-endian, native) row-major frame, 32-byte aligned
		//! \param width    frame width in pixels
		//! \param height   frame height in pixels
		//! \param pitch    bytes between the start of consecutive rows
		virtual void presentFrame(const uint16_t* pixels, int width, int height, int pitch) = 0;

		//! Pixel aspect of the frame (DOSBox's GFX_SetSize scalex/scaley), used
		//! to size the quad on screen. 1.0/1.0 is square pixels.
		virtual void setPixelAspect(float scaleX, float scaleY) { (void)scaleX; (void)scaleY; }

		//! The user's display options (fit, aspect, filter, scanlines, zoom, shift,
		//! 16:9). Safe to call at any time, from the menu too: the driver only
		//! records them and applies them on the next presentFrame(). Values are
		//! clamped. Everything not in here (pixel aspect, frame size) is
		//! emulator state, not a user option.
		void setSettings(const EmulatorVideoSettings& newSettings)
		{
			EmulatorVideoSettings next = newSettings;
			next.clamp();
			if (next == settings)
				return;
			settings = next;
			settingsChanged();
		}
		const EmulatorVideoSettings& getSettings() const { return settings; }

		//! What this driver can do, for hiding or disabling menu rows
		virtual EmulatorVideoCapabilities getCapabilities() const { return EmulatorVideoCapabilities(); }

		//! Where the picture is (or, before the next frame, will be) on the UI
		//! canvas with the current settings and frame size, for previewing a
		//! change over the menu background without presenting a frame. Same
		//! coordinate space as FrameSnapshotInfo::x/y/w/h. False until a frame size
		//! is known.
		virtual bool getCanvasRect(float* x, float* y, float* w, float* h) { (void)x; (void)y; (void)w; (void)h; return false; }

		//! Convenience for callers that only know the old on/off option. Off is
		//! Nearest; on is Bilinear unless a smoothing filter (Sharp) is already
		//! chosen, which it leaves alone.
		void setSmoothing(bool smooth)
		{
			EmulatorVideoSettings next = settings;
			if (!smooth)
				next.filter = VideoFilter::Nearest;
			else if (next.filter == VideoFilter::Nearest)
				next.filter = VideoFilter::Bilinear;
			setSettings(next);
		}

		//! Copies whatever this driver needs out of its live frame source,
		//! into storage it owns itself, so a later readFrameRGB24() call
		//! still has something valid to read even if the live source gets
		//! invalidated/repurposed in between.
		virtual void snapshotFrame() = 0;

		//! Describes the frame the last snapshotFrame() captured. Returns false
		//! if there isn't one (nothing was presented yet, or it was already
		//! consumed by readFrameRGB24()).
		virtual bool getSnapshotInfo(FrameSnapshotInfo* info) const { (void)info; return false; }

		//! Converts the width x height frame most recently captured by
		//! snapshotFrame() into packed RGB24, written to dst
		//! (width*height*3 bytes, tightly packed, no dst padding).
		//! One-shot: consumes the snapshot. Returns false, leaving dst
		//! untouched, if there is no snapshot or width/height don't match it.
		virtual bool readFrameRGB24(int width, int height, uint8_t* dst) = 0;

		//! Drops the snapshot if readFrameRGB24() has not consumed it. The
		//! snapshot lives in menu memory, which goes away when the menu does.
		virtual void releaseSnapshot() {}

		//! Sets the initial console dimensions, before the first presentFrame() call
		virtual void renderInit(int width, int height) { (void)width; (void)height; }

		//! Maps a UI-canvas pointer position (IR pointer / touch, in the same canvas
		//! coordinates as InputPadData::cursor_x/y) to a normalized position (0..1 on
		//! each axis, clamped) within the game picture, following its actual on-screen
		//! placement (aspect, zoom, fixed scale, shift). Returns false until the
		//! placement is known.
		//! onGamePad selects the output the pointer is on (touch is GamePad-only), since
		//! the game can be placed differently on the TV and the GamePad.
		virtual bool mapPointerToUnit(float canvasX, float canvasY, bool onGamePad, float* u, float* v)
		{
			(void)canvasX; (void)canvasY; (void)onGamePad; (void)u; (void)v;
			return false;
		}

	protected:
		//! Called when setSettings() changed something. Mark state dirty; do not
		//! touch the GPU here, the menu may own it.
		virtual void settingsChanged() {}

		EmulatorVideoSettings settings;
};
