/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * configsave.h
 *
 * Writing the running config back to dosbox.conf, without ever leaving the
 * player with a half-written or missing file. Plain stdio and DOSBox's own
 * config; no platform types, so it builds on the host for the settings test.
 ***************************************************************************/
#ifndef _CONFIGSAVE_H_
#define _CONFIGSAVE_H_

#include <string>

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

#endif
