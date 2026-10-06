/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * EmulatorVideoDriver.h
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "VideoDriver.h"

class EmulatorVideoDriver
{
	public:
		virtual ~EmulatorVideoDriver() = default;

		virtual void init(VideoDriver* videoDriver) = 0;
		virtual void resetVideo() = 0;

		//! Draws and presents one emulator frame.
		//! \param pixels   RGB565 (big-endian, native) row-major frame, 32-byte aligned
		//! \param width    frame width in pixels
		//! \param height   frame height in pixels
		//! \param pitch    bytes between the start of consecutive rows
		virtual void presentFrame(const uint16_t* pixels, int width, int height, int pitch) = 0;

		//! Pixel aspect of the frame (DOSBox's GFX_SetSize scalex/scaley), used
		//! to size the quad on screen. 1.0/1.0 is square pixels.
		virtual void setPixelAspect(float scaleX, float scaleY) { (void)scaleX; (void)scaleY; }

		//! Bilinear (true) or nearest-neighbour (false) magnification
		virtual void setSmoothing(bool smooth) { (void)smooth; }

		//! Copies whatever this driver needs out of its live frame source,
		//! into storage it owns itself, so a later readFrameRGB24() call
		//! still has something valid to read even if the live source gets
		//! invalidated/repurposed in between.
		virtual void snapshotFrame() = 0;

		//! Converts the width x height frame most recently captured by
		//! snapshotFrame() into packed RGB24, written to dst
		//! (width*height*3 bytes, tightly packed, no dst padding).
		virtual void readFrameRGB24(int width, int height, uint8_t* dst) = 0;

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
};
