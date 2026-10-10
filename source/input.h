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

#include <stddef.h>

class Section;

//! Starts the input HAL and counts the joysticks. Once, before the config is read.
void InitInput();

//! Init function of the [sdl] config section: video, mouse, key handlers.
void GUI_StartUp(Section * sec);

//! True while a HOME button is held. Reads the state InputHal_Update() scanned.
bool isMenuRequested();

//! What is connected on a player's channel (0-3), as text such as "Wiimote +
//! Nunchuk". Returns false, leaving buf alone, if nothing is connected there.
bool GetControllerSummary(int channel, char * buf, size_t size);

//! Leaves the app if the platform reports a shutdown request. Does not return then.
void CheckExit();

/* Key injection: a command typed on the on-screen keyboard is typed into DOS. */

//! The command the on-screen keyboard left, for QueueKeys() to take.
extern char dosboxCommand[1024];

//! Builds the shifted-key table. Once, at startup.
void InitKeyInjection();

//! Starts typing the command in dosboxCommand, and clears it. '\n' types Return.
void QueueKeys();

//! Types the next character of the command, if one is pending. Called once
//! per GFX_Events(), on the emulation thread.
void PumpKeys();

//! Cuts any typing short.
void AbortKeys();

#endif
