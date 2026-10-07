/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * dosboxwii.h
 *
 * Program flow, shared between main.cpp and the rest of the app.
 *
 * NOTE: this header is included by DOSBox core files, so it must not pull
 * in drivers/ or libgui/ headers (their LOG() macro collides with DOSBox's).
 ***************************************************************************/

#ifndef _DOSBOXWII_H_
#define _DOSBOXWII_H_

//! Emulation -> home menu -> emulation. Returns when the menu is closed.
void EnterMenu();

//! Shuts every driver down and leaves the app (power off, or back to the
//! loader). Does not return.
void ExitApp();

#endif
