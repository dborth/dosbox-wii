/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * EmulatorVideoLayout.h
 *
 * Where the emulator picture goes on a screen, given the user's display
 * settings. Pure arithmetic shared by every platform's EmulatorVideoDriver, so
 * they cannot drift apart. No platform or DOSBox dependencies.
 ***************************************************************************/
#pragma once

#include "EmulatorVideoSettings.h"

struct VideoRect
{
	float x = 0, y = 0, w = 0, h = 0; //!< top-left and size
};

struct VideoLayoutInput
{
	int frameWidth = 0;   //!< emulator frame, pixels
	int frameHeight = 0;
	float pixelAspectX = 1.0f; //!< the emulator's pixel aspect (scalex/scaley)
	float pixelAspectY = 1.0f;
	float targetWidth = 0;     //!< the screen, in its own pixels
	float targetHeight = 0;
	//! How much wider than tall one target pixel looks. 1.0 where the target
	//! is square-pixel; 4/3 for a 640x480 buffer shown on a 16:9 screen.
	float outputPixelAspect = 1.0f;
};

//! Returns the picture's rect in target pixels, or all zeros if the input has
//! no frame or no target. `settings` should already be clamp()ed.
VideoRect ComputeVideoLayout(const EmulatorVideoSettings& settings, const VideoLayoutInput& in);
