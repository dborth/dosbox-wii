/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlslive.h
 *
 * The console-backed implementations behind the controller screens (see
 * controlslive.cpp). Forward declarations only, so a file that sees DOSBox's
 * headers (settings.cpp) can include this one.
 ***************************************************************************/
#ifndef _CONTROLSLIVE_H_
#define _CONTROLSLIVE_H_

#include <string>

class ControlsHardware;
class PadMapper;

/** The HAL-backed hardware (controllers, keyboard, mouse). Needs the platform up. */
ControlsHardware& Controls_Hal();

/** The real mapper (needs the DOSBox core, so the host tests use a fake). */
PadMapper& Mapper_Live();

/** The DOS program running now, as the PSP records it (empty at the prompt). */
std::string Controls_RunningProgram();

/** The joystick type in effect changed behind the model's back (the Settings row): moves the pad binds to it and saves. Called by the settings apply path. */
void Controls_JoystickTypeChanged();

#endif
