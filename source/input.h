/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * input.h
 *
 * Input: event pump, menu and exit requests, key injection.
 *
 * NOTE: this header is included by DOSBox core files, so it must not pull
 * in drivers/ or libgui/ headers (their LOG() macro collides with DOSBox's).
 ***************************************************************************/

#ifndef _INPUT_H_
#define _INPUT_H_

class Section;

//! Starts the input HAL and counts the joysticks. Once, before the config is read.
void InitInput();

//! Init function of the [sdl] config section: video, mouse, key handlers.
void GUI_StartUp(Section * sec);

//! True while a HOME button is held. Reads the state InputHal_Update() scanned.
bool isMenuRequested();

//! Leaves the app if the platform reports a shutdown request. Does not return then.
void CheckExit();

/* Key injection: a command typed on the on-screen keyboard is typed into DOS. */

//! The command the on-screen keyboard left, for QueueKeys() to take.
extern char dosboxCommand[1024];

//! Starts the typing thread. Once, at startup.
void InitKeyInjection();

//! Hands the command in dosboxCommand to the typing thread, and clears it.
void QueueKeys();

//! Cuts any typing short and waits for the typing thread to go idle.
void AbortKeys();

#endif
