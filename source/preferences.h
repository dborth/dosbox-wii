/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * preferences.h
 *
 * Preferences: the [sdl] config section, and finding, loading and generating
 * dosbox.conf. Call these once the config system (control) exists.
 ***************************************************************************/

#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

//! Declares the [sdl] section. Before DOSBOX_Init().
void Config_Add_SDL();

//! Handles the -eraseconf/-resetconf and -erasemapper/-resetmapper switches.
//! Exits after erasing.
void ResetPrefs();

//! Parses the config files, generating a default dosbox.conf if there is none.
void LoadPrefs();

#endif
