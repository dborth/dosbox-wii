/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * input.h
 *
 * Input: event pump, menu and exit requests.
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

//! True while a controller is asking for the menu: HOME held, or L + R + Plus/Start
//! held on one device. Reads the state InputHal_Update() scanned.
bool isMenuRequested();

//! What is connected on a player's channel (0-3), as text such as "Wiimote +
//! Nunchuk". Returns false, leaving buf alone, if nothing is connected there.
bool GetControllerSummary(int channel, char * buf, size_t size);

//! Re-reads [sdl] sensitivity, so a changed value applies to the mouse at once.
void Mouse_SensitivityChanged(void);

//! Leaves the app if the platform reports a shutdown request. Does not return then.
void CheckExit();

#endif
