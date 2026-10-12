/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * preferences.h
 *
 * Preferences: everything about dosbox.conf. The config files (finding,
 * loading, generating, saving), and the two config sections the app owns:
 * [sdl] (mouse) and [display] (video output options, which become the
 * EmulatorVideoSettings the video driver consumes).
 *
 * Call these once the config system (control) exists.
 ***************************************************************************/

#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include <string>
#include "drivers/EmulatorVideoSettings.h"

class Section_prop;

//! Declares the [sdl] section. Before DOSBOX_Init().
void Config_Add_SDL();

//! Handles the -eraseconf/-resetconf and -erasemapper/-resetmapper switches.
//! Exits after erasing.
void ResetPrefs();

//! Parses the config files, generating a default dosbox.conf if there is none.
void LoadPrefs();

/* ---- Saving ---- */

//! The file a save goes to: the primary config file DOSBox read at start, or
//! the default one in the DOSBox folder if none was read.
std::string ConfigSavePath();

/**
 * Writes the whole running config to `path`.
 *
 * Config::PrintConfig() empties its file as soon as it opens it and never
 * says if a write failed, so it is pointed at path + ".tmp" first. Only a
 * file that is there, not empty and ends with the last section replaces the
 * old one, which is kept as path + ".bak" (the one before that is dropped).
 * If the new file cannot be moved into place the old one is put back.
 *
 * @param error  on failure, one line saying why. May be NULL.
 * @return true if the new file is in place
 */
bool ConfigSave(const char * path, std::string * error);

/* ---- The [display] section ---- */

//! Declares the [display] section. After Config_Add_SDL(), before DOSBOX_Init().
void Config_Add_Display();

//! What the section currently holds, as the driver's settings (clamped).
EmulatorVideoSettings DisplaySettingsFromConfig(Section_prop * section);

#endif
