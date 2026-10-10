/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * mousecapture.h
 *
 * Whether mouse motion and clicks reach DOS. Pure logic, no includes, so it
 * is checked by the host tests in tests/.
 *
 * The mouse is captured from startup (see GUI_StartUp): there is no desktop
 * to hand it back to, and a capturing click used up the first click. Motion
 * is then relative, as it was after that first click.
 ***************************************************************************/

#ifndef DOSBOX_MOUSECAPTURE_H
#define DOSBOX_MOUSECAPTURE_H

//! Motion reaches DOS while captured, or when capture is not in use at all.
inline bool MouseMotionForwarded(bool locked, bool autoenable)
{
	return locked || !autoenable;
}

//! A click is used up capturing the mouse when DOS has asked for a lock and
//! the mouse is not locked yet. Never the case once it is captured at startup.
inline bool MouseClickSwallowed(bool requestlock, bool locked)
{
	return requestlock && !locked;
}

#endif
