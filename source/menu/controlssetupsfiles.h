/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlssetupsfiles.h
 *
 * The files half of a SetupHost, over stdio and dirent: the setups folder,
 * listing it, reading and writing. Still abstract: the mapper's file and the
 * running program need the core (controlssetupslive.cpp), and a PC test
 * gives them as it likes.
 ***************************************************************************/
#ifndef _CONTROLSSETUPSFILES_H_
#define _CONTROLSSETUPSFILES_H_

#include "controlssetups.h"

class FileSetupHost : public SetupHost
{
	public:
		explicit FileSetupHost(const std::string& folder = std::string()) : folderPath(folder) {}

		/** The folder given at construction; the live host works it out when asked, as the app drive is not known until the platform is up. */
		std::string directory() const override { return folderPath; }
		/** Makes the folder, and its parent when that is missing too (not further). */
		bool makeDirectory() override;
		std::vector<std::string> files() const override;
		bool read(const std::string& path, std::string& text) const override;
		/** Writes path + ".tmp", then swaps it in keeping the old file as path + ".bak" until the swap is done (put back if it fails). */
		bool write(const std::string& path, const std::string& text) override;

	private:
		std::string folderPath;
};

#endif
