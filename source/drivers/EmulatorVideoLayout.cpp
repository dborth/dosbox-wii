/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * EmulatorVideoLayout.cpp
 ***************************************************************************/
#include <math.h>
#include "EmulatorVideoLayout.h"

VideoRect ComputeVideoLayout(const EmulatorVideoSettings& s, const VideoLayoutInput& in)
{
	VideoRect r;

	if (in.frameWidth <= 0 || in.frameHeight <= 0 || in.targetWidth <= 0.0f || in.targetHeight <= 0.0f)
		return r;

	const float pa = in.outputPixelAspect > 0.0f ? in.outputPixelAspect : 1.0f;
	const float sx = (s.aspect == VideoAspect::Corrected && in.pixelAspectX > 0.0f) ? in.pixelAspectX : 1.0f;
	const float sy = (s.aspect == VideoAspect::Corrected && in.pixelAspectY > 0.0f) ? in.pixelAspectY : 1.0f;

	// The picture and the screen, both in square display units
	const float picW = in.frameWidth * sx;
	const float picH = in.frameHeight * sy;
	const float availW = in.targetWidth * pa;
	const float availH = in.targetHeight;

	float w, h;
	bool whole = false;

	if (s.fit == VideoFit::Fill)
	{
		w = availW * s.zoomX;
		h = availH * s.zoomY;
	}
	else
	{
		float scale = fminf(availW / picW, availH / picH);
		if (s.fit == VideoFit::Integer)
		{
			// Never below 1x by flooring: a frame bigger than the screen just fits
			if (scale >= 1.0f)
				scale = floorf(scale);
			whole = true;
			w = picW * scale;
			h = picH * scale;
		}
		else
		{
			w = picW * scale * s.zoomX;
			h = picH * scale * s.zoomY;
		}
	}

	w /= pa; // back to target pixels

	float x = (in.targetWidth - w) * 0.5f + s.shiftX * (in.targetWidth / EmulatorVideoSettings::SHIFT_UNITS_X);
	float y = (in.targetHeight - h) * 0.5f + s.shiftY * (in.targetHeight / EmulatorVideoSettings::SHIFT_UNITS_Y);

	if (whole)
	{
		// Pixel exact only if the picture also starts and ends on pixel boundaries
		const float x2 = floorf(x + w + 0.5f);
		const float y2 = floorf(y + h + 0.5f);
		x = floorf(x + 0.5f);
		y = floorf(y + 0.5f);
		w = x2 - x;
		h = y2 - y;
	}

	r.x = x; r.y = y; r.w = w; r.h = h;
	return r;
}
