/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlssetups.h
 *
 * Setups on disk. A setup is two files in one folder, the way DOSBox keeps its
 * own settings:
 *
 *     <name>.conf   what DOSBox reads: [sdl] mapperfile (pointing at the .map
 *                   beside it, by file name, which DOSBox resolves against the
 *                   conf's folder) and [joystick] joysticktype. Comment lines
 *                   at the top carry what only we need: the program it is for
 *                   and the preset each player started from.
 *     <name>.map    DOSBox's mapper file, a copy of the one in use when saved.
 *
 * Everything here is plain text work over the SetupHost seam, so it builds and
 * tests on a PC with a fake. FileSetupHost (below) is the stdio one;
 * controlslive.cpp is the one that knows the core.
 ***************************************************************************/
#ifndef _CONTROLSSETUPS_H_
#define _CONTROLSSETUPS_H_

#include "controlsmodel.h"
#include <stddef.h>
#include <string>
#include <vector>

/** What setups need of the app: the folder they live in, reading and writing files, the mapper's file, the running program. */
class SetupHost
{
	public:
		virtual ~SetupHost() {}
		/** The folder setups are kept in, without a trailing separator ("sd:/DOSBox/Setups"). Empty when there is no storage to use. */
		virtual std::string directory() const = 0;
		/** Makes the folder if it is not there. True when it exists afterwards. */
		virtual bool makeDirectory() = 0;
		/** The names (no folder) of the files in the folder; empty when it cannot be read. */
		virtual std::vector<std::string> files() const = 0;
		/** The whole of a file. */
		virtual bool read(const std::string& path, std::string& text) const = 0;
		/** Replaces a file with this text: the old one is kept until the new one is in place. */
		virtual bool write(const std::string& path, const std::string& text) = 0;
		/** The mapper file DOSBox is using (where MAPPER_SaveBindsNow() writes); empty when unknown. */
		virtual std::string mapperFile() const = 0;
		/** The DOS program running now, as DOS recorded it ("KEEN4E.EXE", or just "KEEN4E" when only the short name is known); empty at the prompt. */
		virtual std::string runningProgram() const = 0;
};

/**
 * The files half of a SetupHost, over stdio and dirent: the setups folder, listing it, reading and writing. Still
 * abstract: the mapper's file and the running program need the core (controlslive.cpp), and a PC test gives them as it likes.
 */
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

/** One setup as its .conf describes it. */
struct SetupRecord
{
	std::string name;			/**< The file name without ".conf" */
	std::string program;		/**< "KEEN4E.EXE", or empty */
	std::string joystickType;	/**< The [joystick] joysticktype it was saved with; empty when the conf has none */
	std::string mapperFile;		/**< [sdl] mapperfile as written (a file name beside the conf, or a full path) */
	std::string presets[4];		/**< The preset name each player started from; empty for a player that was not connected (or unknown) */
};

/** The .conf text for a record. */
std::string Setup_FormatConf(const SetupRecord& record);
/** Reads a .conf of ours. False for any other file (it has to carry our first line), leaving `out` as it was. */
bool Setup_ParseConf(const std::string& text, SetupRecord& out);

/** Whether a path is absolute the way DOSBox decides it on this build (Cross::IsPathAbsolute): it starts with a device prefix ("sd:", "usb2:"). */
bool Setup_IsAbsolute(const std::string& path);
/** Where a mapperfile value points: as is when absolute, else inside the conf's folder. */
std::string Setup_ResolveFile(const std::string& directory, const std::string& value);

/**
 * What identifies a program in the index: the file name alone, upper case, without the extension and cut to the 8
 * characters DOS keeps ("c:\\games\\keen4e.exe", "KEEN4E.EXE" and "Keen4E" are all "KEEN4E"). Empty when there is none.
 */
std::string Setup_ProgramKey(const std::string& program);

/**
 * The program name in a DOS environment block: after the variables' closing empty string there is a word (1) and the
 * program's full path, which is where DOS_Execute puts it. Returns the file name alone, upper case, with its extension
 * ("KEEN4E.EXE"); empty when the block is not in that shape. `size` is the bytes in the block.
 */
std::string Setup_ProgramFromEnvironment(const unsigned char * env, size_t size);

/** Every pad source's events found in a mapper file, by player: `pad_N button B` and `pad_N hat 0 M`, and the stick group's `stick_N button B` and `stick_N hat 0 M` for the same player, in file order. */
struct MapPadBinds
{
	std::vector<std::string> events[4][PAD_SOURCE_COUNT];
};
void Setup_ParsePadBinds(const std::string& mapText, MapPadBinds& out);

/** The setups in the host's folder: each .conf that starts with our first line (any other .conf is left alone), by name ignoring case. Whether its .map is there is not checked. */
std::vector<SetupRecord> Setups_List(const SetupHost& host);

/**
 * The program index: the setup that loads for a program, by Setup_ProgramKey(). When several are tied to the
 * same program the first by name wins. -1 when there is none (or the program is empty).
 */
int Setups_IndexForProgram(const std::vector<SetupRecord>& setups, const std::string& program);

#endif
