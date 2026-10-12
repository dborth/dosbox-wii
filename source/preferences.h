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

class Property;
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

/* ---- Reading the running config ---- */

//! The section, or NULL if there is none (or no config system yet).
Section_prop * Config_GetSection(const char * name);

//! The property by name, ignoring case, or NULL. A NULL section has none.
Property * Config_FindProp(Section_prop * sec, const char * name);

//! The property's value as it stands in the config, or "" if there isn't one.
std::string Config_Get(const char * section, const char * prop);

enum CyclesMode { CYCLES_AUTO = 0, CYCLES_FIXED, CYCLES_MAX };

//! cpu.cycles as configured: the one multival property ("type" plus
//! "parameters", see CPU::Change_Config) taken apart. "auto", "fixed N" and
//! "max P%" are the forms; a bare number is a fixed amount.
struct CyclesConfig
{
	CyclesMode mode;
	int amount;		//!< cycles for fixed, percent for max; 0 if not given
	bool limited;	//!< max with a cycle limit after the percentage ("max 80% 20000")
};

CyclesConfig Config_ReadCycles();

#endif
