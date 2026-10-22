/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * gamesave.h
 *
 * Saving a game. A game is a normal DOSBox conf (see games.h) that DOSBox reads
 * after dosbox.conf, plus the mapper file beside it:
 *
 *     <app drive>/DOSBox/Games/Commander Keen 4.conf
 *     <app drive>/DOSBox/Games/Commander Keen 4.map
 *
 * What is written is the whole running config, the way Settings > Save writes
 * dosbox.conf (Config::PrintConfig), with five things changed in the text:
 *
 *   - [sdl] mapperfile    <name>.map, beside the conf (DOSBox resolves a relative
 *                         mapperfile against the conf's folder)
 *   - [joystick] joysticktype   the type in effect, so "auto" is not left to guess
 *   - [autoexec]          the game's own lines only. The running config's
 *                         [autoexec] is dosbox.conf's followed by the game's
 *                         (DOSBox appends every file's lines), so printing it as it
 *                         is would run dosbox.conf's lines twice on the next start
 *   - "# title=" and "# program=" comment lines at the top, which DOSBox ignores
 *   - comment lines at the top that were already in the file are kept
 *
 * Everything else DOSBox reads dosbox.conf for first and the game's file wins,
 * so a hand-made game conf can be as small as it likes.
 *
 * A game found on the card (games.h, FoundGame) is added the other way: nothing is running to save, so what is
 * written is a small conf of its own with a title, the program and an [autoexec] that goes there and runs it. It has
 * no map, so its controls start from the defaults, and nothing else is set, so dosbox.conf's settings apply.
 *
 * This is plain text work over the GameSaveHost seam, so it builds and tests on a
 * PC with a fake. FileGameSaveHost (below) is the files half; gameslive.cpp adds
 * what needs the core (the running config, the mapper, the drives, the program).
 ***************************************************************************/
#ifndef _GAMESAVE_H_
#define _GAMESAVE_H_

#include "games.h"
#include <string>
#include <vector>

/** A mounted DOS drive as GetDOSDriveInfo() describes it: "sd:/DOSBox/GAMES/", "CDRom sd:/cd/", "isoDrive sd:/x.iso", "fatDrive ...". */
struct GameDrive
{
	char letter;
	std::string info;
};

/** What saving a game needs of the app. */
class GameSaveHost
{
	public:
		virtual ~GameSaveHost() {}

		/** The DOSBox folder, without a trailing separator ("sd:/DOSBox"). Empty when there is no storage to use. */
		virtual std::string dosboxDirectory() const = 0;
		/** Makes a folder if it is not there (and its parent, not further). True when it exists afterwards. */
		virtual bool makeDirectory(const std::string& path) = 0;
		virtual bool exists(const std::string& path) const = 0;
		/** The whole of a file. */
		virtual bool read(const std::string& path, std::string& text) const = 0;
		/** Replaces a file with this text: the old one is kept until the new one is in place. */
		virtual bool write(const std::string& path, const std::string& text) = 0;
		/** Deletes a file. True when it is gone (a file that was not there counts). */
		virtual bool erase(const std::string& path) = 0;

		/** The running config as dosbox.conf would be written (Config::PrintConfig). False with a reason when it could not be had. */
		virtual bool runningConfig(std::string& text, std::string& error) = 0;
		/** dosbox.conf: the file the games are layered over. Empty when there is none. */
		virtual std::string baseConfig() const = 0;
		/** Writes the mapper's binds to the mapper file DOSBox is using (MAPPER_SaveBindsNow). */
		virtual bool saveMapper() = 0;
		/** The mapper file DOSBox is using; empty when it is not known. */
		virtual std::string mapperFile() const = 0;
		/** The joystick type in effect ("4axis"); empty when not known. */
		virtual std::string joystickType() const = 0;
		/** The DOS path of the program running now, as DOS recorded it ("C:\GAMES\KEEN4\KEEN4E.EXE"); empty at the prompt. */
		virtual std::string runningProgramPath() const = 0;
		/** The DOS drives now mounted. */
		virtual std::vector<GameDrive> drives() const = 0;
};

/** The files half of a GameSaveHost, over stdio: the folder, existence, reading and writing. Still abstract. */
class FileGameSaveHost : public GameSaveHost
{
	public:
		explicit FileGameSaveHost(const std::string& folder = std::string()) : folderPath(folder) {}

		std::string dosboxDirectory() const override { return folderPath; }
		bool makeDirectory(const std::string& path) override;
		bool exists(const std::string& path) const override;
		bool read(const std::string& path, std::string& text) const override;
		/** Writes path + ".tmp", then swaps it in keeping the old file as path + ".bak" until the swap is done (put back if it fails). */
		bool write(const std::string& path, const std::string& text) override;
		bool erase(const std::string& path) override;

	private:
		std::string folderPath;
};

/** The console's host (gameslive.cpp). */
GameSaveHost& GameSave_Live();

// ---- names and paths

/** The file name (no folder, no ".conf") a title is saved under: no characters a card cannot hold, no dots, at most 24 characters; empty when nothing is left. */
std::string Game_FileName(const std::string& title);

/** Where new games are written: <DOSBox folder>/Games. Empty when there is no storage. */
std::string Game_Directory(const GameSaveHost& host);

/** The conf a title would be saved as; empty when the title gives no usable name. */
std::string Game_ConfPath(const GameSaveHost& host, const std::string& title);

/**
 * The file the game conf DOSBox was given is. It is tried as given, then in the DOSBox folder (as LoadPrefs() does); one
 * that is not there yet is made where it was asked for, which for a name without a device prefix is the DOSBox folder.
 */
std::string Game_ResolveConf(const GameSaveHost& host, const std::string& given);

// ---- text work on a conf (public so the tests can reach them)

/** Sets `key` in `section` to `value`, replacing the line that has it or adding one to the end of the section (or the section to the end of the text). */
std::string Conf_SetValue(const std::string& text, const std::string& section, const std::string& key, const std::string& value);

/** The lines of a section that are not comments or blank, trimmed. Empty when there is no such section. */
std::vector<std::string> Conf_SectionLines(const std::string& text, const std::string& section);

/** Gives a section these lines in place of the ones it has, keeping the comment lines at its start. Adds the section when it is missing. */
std::string Conf_ReplaceSectionLines(const std::string& text, const std::string& section, const std::vector<std::string>& lines);

/** Sets the "# title=" comment of a conf: the one at the top is replaced, or a line is added at the top. Nothing else is touched. */
std::string Conf_SetTitle(const std::string& text, const std::string& title);

/**
 * The [autoexec] a game should start with: the drives it needs mounted, then the program. C: is the DOSBox folder (the app
 * mounts it before the shell starts) so it is not mounted again, and a mount line dosbox.conf's autoexec already has is
 * not repeated. `notes` says what could not be written (a disk image, a C: that is not the DOSBox folder).
 */
struct GameStartup
{
	std::vector<std::string> lines;
	std::vector<std::string> notes;
};
GameStartup Game_BuildStartup(const std::string& dosboxDirectory, const std::vector<GameDrive>& drives,
	const std::string& programPath, const std::vector<std::string>& baseAutoexec);

// ---- saving

/** How a save went. */
struct GameSaveResult
{
	bool ok = false;
	bool exists = false;		/**< Not saved because the file is there and a replace was not asked for */
	std::string path;			/**< The conf (written, or the one that exists) */
	std::string message;		/**< For the help line: what was written, or why not */
};

/** What to save and where. */
struct GameSave
{
	std::string path;			/**< The conf to write */
	std::string title;			/**< "# title="; empty keeps the file's own (or its name) */
	bool keepStartup = false;	/**< Keep the file's [autoexec] and "# program=" instead of working them out from what is running */
};

/**
 * Writes a game: the mapper file's binds to <name>.map beside the conf, then the conf. The map goes first, so a save
 * cut short never leaves a conf with no map; either file is only replaced once the new one is complete (host.write).
 * An existing conf has its [autoexec] kept when `keepStartup` is set, and also whenever there is nothing running to
 * build one from. Header comments in it are kept.
 */
GameSaveResult Game_Save(GameSaveHost& host, const GameSave& what);

/** A new game under a title: not saved when that name is taken (result.exists), so the caller can ask to replace it. */
GameSaveResult Game_SaveNew(GameSaveHost& host, const std::string& title);

/**
 * Replaces the game at `path` with what is running. `keepStartup` for the game that is running (its [autoexec] is
 * what started it, and may have been edited by hand); another game's startup is worked out again from what is running.
 */
GameSaveResult Game_SaveReplace(GameSaveHost& host, const std::string& path, bool keepStartup);

// ---- renaming and deleting
/**
 * Gives the game at `path` another name in the Games list: only its "# title=" comment changes (so its file names, its
 * map and the path DOSBox was given if it is running all stay as they are). Everything else in the conf is left as it is.
 */
GameSaveResult Game_Rename(GameSaveHost& host, const std::string& path, const std::string& title);
/**
 * Deletes the game at `path`: its conf, and its map when the conf names a "<conf name>.map" beside it (a map under any
 * other name may be shared with another game, so it stays). The caller does not offer this for the game that is running.
 */
GameSaveResult Game_Delete(GameSaveHost& host, const std::string& path);

// ---- adding a found game

/**
 * The conf a found game is added as: named after its folder ("WOLF3D.conf"), else after its title, else the folder with
 * a number (up to 9), in the Games folder; the first that is not taken. Empty when none is free or there is no storage.
 */
std::string Game_FoundConfPath(const GameSaveHost& host, const FoundGame& found, const std::string& title);

/** What the conf says: "# title=", "# program=", and an [autoexec] with c:, cd \FOLDER and the program. */
std::string Game_FoundConfText(const FoundGame& found, const std::string& title, const std::string& program);

/**
 * Writes the conf of a found game. `program` is one of found.programs (the first when empty). An existing conf is never
 * touched: the name is taken (result.exists) only when every name Game_FoundConfPath() tries is.
 */
GameSaveResult Game_AddFound(GameSaveHost& host, const FoundGame& found, const std::string& title, const std::string& program);

/** Settings > Save while a game is active: the game DOSBox was given (`given`, as GetGameConf() has it) is written, its startup and title kept. */
GameSaveResult Game_SaveActive(GameSaveHost& host, const std::string& given);

#endif
