/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * displayconfig.h
 *
 * The [display] config section: the user's video output options (fit,
 * aspect, filter, scanlines, zoom, shift, widescreen), and how they become the
 * EmulatorVideoSettings the video driver consumes. The section is registered
 * as changeable, so the menu applies a change the way it applies any other:
 * the section's init function runs again (GFX_DisplayInit, videosupport.cpp).
 *
 * No platform types here, so it builds on the host for the settings test.
 ***************************************************************************/
#ifndef _DISPLAYCONFIG_H_
#define _DISPLAYCONFIG_H_

#include "drivers/EmulatorVideoSettings.h"

class Section_prop;

//! Declares the [display] section. After Config_Add_SDL(), before DOSBOX_Init().
void Config_Add_Display();

//! What the section currently holds, as the driver's settings (clamped).
EmulatorVideoSettings DisplaySettingsFromConfig(Section_prop * section);

#endif
