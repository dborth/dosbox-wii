/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * videosupport.h
 *
 * The DOSBox GFX_* video backend, implemented on the platform HAL
 * (EmulatorVideoDriver). Replaces the SDL video code that used to live in
 * sdlmain.cpp.
 *
 * Deliberately includes nothing: this header is used by both DOSBox core
 * files and platform files, whose headers collide (see fileop.h).
 ***************************************************************************/
#ifndef DOSBOX_VIDEOSUPPORT_H
#define DOSBOX_VIDEOSUPPORT_H

/* Entry/exit of emulator video as a whole. DOSBox's own GFX_Start() and
 * GFX_Stop() (video.h) only gate drawing and are unrelated to these. */

//! Takes the display for the emulator. Called once, from GUI_StartUp().
void GFX_HalInit(void);

//! Releases emulator video. Called once, from GUI_ShutDown(). The platform
//! itself is shut down afterwards, by ExitApp().
void GFX_HalShutdown(void);

/* Handoff to and from the menu, while the emulator keeps running. */

//! The emulator gives up the display. Waits for the last presented frame to
//! reach the screen, and keeps a copy of it for the menu background. Call
//! before switching the video hardware to menu mode.
void GFX_Suspend(void);

//! The emulator takes the display back and repaints the last frame, since
//! DOSBox only presents a frame when something on screen changed.
void GFX_Resume(void);

//! Presents the last frame again. DOSBox only presents when the DOS screen
//! changes; this is for things drawn with the frame (the GamePad overlay) that
//! must update when it has not. Does nothing while the menu owns the display,
//! while DOSBox is part way through a frame, or before any mode is set.
void GFX_Refresh(void);
//! Shows a finished RGB565 picture that is not emulator output (the mapper
//! screen), square pixels, in place of the emulator frame. pixels must be 32
//! byte aligned. GFX_ResetScreen() brings the emulator picture back.
void GFX_ShowScreen(const unsigned short * pixels, int width, int height, int pitch);

//! The Wii has no windowed mode, so this is always true.
bool GFX_IsFullscreen(void);

#endif
