/****************************************************************************
 * DOSBox Wii
 * gfx_hal.h
 *
 * The DOSBox GFX_* video backend, implemented on the platform HAL
 * (EmulatorVideoDriver). Replaces the SDL video code that used to live in
 * sdlmain.cpp.
 *
 * Deliberately includes nothing: this header is used by both DOSBox core
 * files and platform files, whose headers collide (see wiihardware.h).
 ***************************************************************************/
#ifndef DOSBOX_GFX_HAL_H
#define DOSBOX_GFX_HAL_H

/* Entry/exit of emulator video as a whole. DOSBox's own GFX_Start() and
 * GFX_Stop() (video.h) only gate drawing and are unrelated to these. */

//! Takes the display for the emulator. Called once, from GUI_StartUp().
void GFX_HalInit(void);

//! Releases emulator video. Called once, from GUI_ShutDown(). The platform
//! itself is shut down afterwards, by WiiFinished().
void GFX_HalShutdown(void);

/* Handoff to and from the menu, while the emulator keeps running. */

//! The emulator gives up the display. Waits for the last presented frame to
//! reach the screen, and keeps a copy of it for the menu background. Call
//! before switching the video hardware to menu mode.
void GFX_Suspend(void);

//! The emulator takes the display back and repaints the last frame, since
//! DOSBox only presents a frame when something on screen changed.
void GFX_Resume(void);

//! The Wii has no windowed mode, so this is always true.
bool GFX_IsFullscreen(void);

#endif
