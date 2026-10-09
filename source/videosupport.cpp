/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * The DOSBox GFX_* video backend, on the platform HAL.
 *
 * DOSBox renders into a plain RGB565 frame buffer that this file owns, and
 * every finished frame is handed to EmulatorVideoDriver::presentFrame(). The
 * driver does everything display related: tiling into a GX texture, scaling
 * to the screen, pixel aspect, filtering and vsync. There is no SDL surface,
 * no overlay and no scaler on the CPU beyond what the DOSBox config asks for.
 *
 * Only one output exists: RGB565, with GFX_SCALING, which tells render.cpp
 * to hand over native-resolution frames and leave aspect correction and
 * enlargement to us, exactly as it does for the OpenGL/overlay outputs.
 ***************************************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "dosbox.h"
#include "video.h"
#include "cpu.h"
#include "logging.h"
#include "drivers/Platform.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "videosupport.h"
#include "osk.h"

static struct {
	bool active;			// GFX_Start()/GFX_Stop(): DOSBox may draw
	bool updating;			// between GFX_StartUpdate() and GFX_EndUpdate()
	bool suspended;			// the menu owns the display

	Bitu width, height;		// size of the frame DOSBox renders
	Bitu flags;
	double scalex, scaley;	// pixel aspect asked for by the core
	GFX_CallBack_t callback;

	// Persistent RGB565 frame. DOSBox only rewrites the lines that changed,
	// so this must survive from one frame to the next.
	uint16_t * frame;
	size_t frameCapacity;
	Bitu pitch;				// bytes per row
} gfx;

static EmulatorVideoDriver * EmulatorVideo(void) {
	return platform->getVideo()->getEmulatorVideo();
}

static void PresentFrame(void) {
	EmulatorVideo()->presentFrame(gfx.frame, (int)gfx.width, (int)gfx.height, (int)gfx.pitch);
}

/****************************************************************************
 * Entry / exit
 ***************************************************************************/
void GFX_HalInit(void) {
	memset(&gfx, 0, sizeof(gfx));

	EmulatorVideoDriver * video = EmulatorVideo();
	video->resetVideo();
	video->setSmoothing(true);
}

void GFX_HalShutdown(void) {
	GFX_Stop();
	if (gfx.callback) {
		GFX_CallBack_t callback = gfx.callback;
		gfx.callback = NULL;
		callback(GFX_CallBackStop);		// RENDER_Halt()
	}

	EmulatorVideo()->stopVideo();

	free(gfx.frame);
	gfx.frame = NULL;
	gfx.frameCapacity = 0;
}

void GFX_Suspend(void) {
	if (gfx.suspended)
		return;

	OSK_Close();		// the menu is about to take over the GamePad too

	// A frame DOSBox is part way through drawing is left alone. Nothing runs
	// while the menu is up, and the rest of it is drawn after GFX_Resume().
	EmulatorVideoDriver * video = EmulatorVideo();
	video->snapshotFrame();		// reads the last presented frame, so first
	video->stopVideo();
	gfx.suspended = true;
}

void GFX_Resume(void) {
	if (!gfx.suspended)
		return;

	gfx.suspended = false;
	EmulatorVideo()->resetVideo();

	// The screen still shows the menu. DOSBox will not present again until
	// something changes, which may be a long time at a quiet prompt.
	if (gfx.frame)
		PresentFrame();
}

void GFX_Refresh(void) {
	if (!gfx.frame || gfx.width == 0 || gfx.height == 0 || gfx.suspended || gfx.updating)
		return;

	PresentFrame();
}

void GFX_ShowScreen(const unsigned short * pixels, int width, int height, int pitch) {
	if (gfx.updating)
		GFX_EndUpdate(0);

	EmulatorVideoDriver * video = EmulatorVideo();
	video->setPixelAspect(1.0f, 1.0f);
	video->presentFrame((const uint16_t *)pixels, width, height, pitch);
}

bool GFX_IsFullscreen(void) {
	return true;
}

/****************************************************************************
 * Mode
 ***************************************************************************/
Bitu GFX_GetBestMode(Bitu flags) {
	// RGB565 is the only format the display path takes. Every scaler block
	// can produce it (render_scalers.cpp), and sources that are 8, 15 or 32
	// bits per pixel are converted by the scaler's own line handlers.
	if (!(flags & GFX_CAN_16))
		return 0;

	return GFX_CAN_16 | GFX_SCALING;
}

static bool AllocateFrame(Bitu width, Bitu height) {
	// 32-byte rows keep every row 4-byte aligned for the tiler, whatever the width
	Bitu pitch = (width * 2 + 31) & ~(Bitu)31;
	size_t needed = (size_t)pitch * height;

	if (needed > gfx.frameCapacity) {
		free(gfx.frame);
		gfx.frame = (uint16_t *)memalign(32, needed);
		gfx.frameCapacity = gfx.frame ? needed : 0;
		if (!gfx.frame)
			return false;
	}

	gfx.pitch = pitch;
	memset(gfx.frame, 0, needed);		// black
	return true;
}

Bitu GFX_SetSize(Bitu width, Bitu height, Bitu flags, double scalex, double scaley, GFX_CallBack_t callback) {
	if (gfx.updating)
		GFX_EndUpdate(0);

	gfx.width = width;
	gfx.height = height;
	gfx.flags = flags;
	gfx.scalex = scalex;
	gfx.scaley = scaley;
	gfx.callback = callback;

	// GFX_GetBestMode() only ever offers RGB565
	if (!(flags & GFX_CAN_16))
		return 0;

	EmulatorVideoDriver * video = EmulatorVideo();
	int maxDimension = video->getMaxFrameDimension();
	if ((int)width > maxDimension || (int)height > maxDimension)
		E_Exit("Video mode %ix%i is larger than the %ix%i the display can present", (int)width, (int)height, maxDimension, maxDimension);

	if (!AllocateFrame(width, height))
		E_Exit("Out of memory for a %ix%i video frame", (int)width, (int)height);

	video->setPixelAspect((float)scalex, (float)scaley);

	GFX_Start();
	return GFX_CAN_16 | GFX_SCALING;
}

void GFX_ResetScreen(void) {
	GFX_Stop();
	if (gfx.callback)
		(gfx.callback)(GFX_CallBackReset);
	GFX_Start();
	CPU_Reset_AutoAdjust();
}

void GFX_GetSize(int &width, int &height, bool &fullscreen) {
	width = (int)gfx.width;
	height = (int)gfx.height;
	fullscreen = true;
}

/****************************************************************************
 * Frame
 ***************************************************************************/
bool GFX_StartUpdate(Bit8u * & pixels, Bitu & pitch) {
	if (!gfx.active || gfx.updating || !gfx.frame)
		return false;

	pixels = (Bit8u *)gfx.frame;
	pitch = gfx.pitch;
	gfx.updating = true;
	return true;
}

void GFX_EndUpdate(const Bit16u * changedLines) {
	if (!gfx.updating)
		return;
	gfx.updating = false;

	// No line list means the frame was abandoned (frame skip, mode change, halt)
	if (!changedLines)
		return;

	PresentFrame();
}

void GFX_Start(void) {
	gfx.active = true;
}

void GFX_Stop(void) {
	if (gfx.updating)
		GFX_EndUpdate(0);
	gfx.active = false;
}

/****************************************************************************
 * Colour
 ***************************************************************************/
Bitu GFX_GetRGB(Bit8u red, Bit8u green, Bit8u blue) {
	// RGB565, the same layout the 16 bit scalers write (render_templates.h)
	return ((Bitu)(red >> 3) << 11) | ((Bitu)(green >> 2) << 5) | (Bitu)(blue >> 3);
}

void GFX_SetPalette(Bitu start, Bitu count, GFX_PalEntry * entries) {
	// Only called for 8 bit output (scalerMode8), which GFX_GetBestMode()
	// never offers. Palettes reach the screen through GFX_GetRGB() instead.
	(void)start; (void)count; (void)entries;
}
