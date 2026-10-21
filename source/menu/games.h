/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * games.h
 *
 * Games on disk. A game is a .conf that DOSBox layers over dosbox.conf (see
 * SetGameConf()), so it only has to hold what differs: a hand-made one can be
 * as small as it likes. Nothing has to be registered: the scan picks up every
 * .conf in the DOSBox folder, whoever wrote it, by hand or from the menu
 * (gamesave.h writes them, into DOSBox/Games).
 *
 * Where it looks, and no further, so it stays quick on a slow card:
 *
 *     <app drive>/DOSBox/                 every .conf in it, except dosbox.conf
 *     <app drive>/DOSBox/Games/           the same: where the menu saves games
 *     <app drive>/DOSBox/<any folder>/    the same, one level down
 *
 * What a conf can say about itself is read from its comment lines, which DOSBox
 * ignores, so a hand-made conf can carry them too:
 *
 *     # title=Commander Keen 4
 *     # program=KEEN4E.EXE
 *
 * Without a title the file name is used. Without a program it is worked out from
 * the last line of [autoexec] that runs something.
 *
 * Found games: the same scan also looks at what is in the folders one level below the DOSBox folder (never
 * deeper) for .EXE, .COM and .BAT files, and offers each folder that holds a program and that no game covers yet
 * (FoundGame, below). gamesave.h writes the small conf that adds one.
 *
 * Plain text work over the GamesHost seam, so it builds and tests on a PC with a
 * fake. FileGamesHost is the stdio one; gameslive.cpp knows where the app drive is.
 ***************************************************************************/
#ifndef _GAMES_H_
#define _GAMES_H_

#include <stddef.h>
#include <string>
#include <vector>

/** What the scan needs of the app: a folder to look in, listing folders, and reading files. */
class GamesHost
{
	public:
		virtual ~GamesHost() {}
		/** The DOSBox folder, without a trailing separator ("sd:/DOSBox"). Empty when there is no storage to use. */
		virtual std::string directory() const = 0;
		/** The names (no folder) of everything in a folder; empty when it cannot be read. */
		virtual std::vector<std::string> list(const std::string& folder) const = 0;
		virtual bool isDirectory(const std::string& path) const = 0;
		/** The start of a file: at most maxBytes of it. */
		virtual bool read(const std::string& path, std::string& text, size_t maxBytes) const = 0;
};

/** The GamesHost over stdio and dirent, for a folder given at construction. */
class FileGamesHost : public GamesHost
{
	public:
		explicit FileGamesHost(const std::string& folder = std::string()) : folderPath(folder) {}

		std::string directory() const override { return folderPath; }
		std::vector<std::string> list(const std::string& folder) const override;
		bool isDirectory(const std::string& path) const override;
		bool read(const std::string& path, std::string& text, size_t maxBytes) const override;

	private:
		std::string folderPath;
};

/** One game as its .conf describes it. */
struct GameRecord
{
	std::string name;			/**< The file name without ".conf" */
	std::string title;			/**< "# title=", or the name */
	std::string program;			/**< "KEEN4E.EXE" (as written, upper case): "# program=", else what [autoexec] runs; empty when neither says */
	bool programInferred;			/**< program came from [autoexec], not from a comment */
	bool ownControls;			/**< [sdl] mapperfile names a map of its own (not the shared mapper-<version>.map) */
	std::string cycles;			/**< [cpu] cycles as written ("fixed 6000", "max"); empty when the conf does not say */
	std::string path;			/**< The whole path, which is what SetGameConf() takes ("sd:/DOSBox/Games/keen.conf") */
	std::string where;			/**< The folder below the DOSBox folder it was found in; empty for the DOSBox folder itself ("Games", "GAMES") */
	std::vector<std::string> autoexec;	/**< The first lines of [autoexec], for showing what it will run */
	std::vector<std::string> folders;	/**< Lower-case names of the folders its [autoexec] goes into (cd) or mounts; see Game_AutoexecFolders() */

	GameRecord() : programInferred(false), ownControls(false) {}
};

/** A short phrase for a cycles setting ("6000 cycles", "max cycles"); empty for "auto" or none, which is nothing to say. */
std::string Game_CyclesTag(const std::string& cycles);

/** How many [autoexec] lines are kept per game. */
#define GAME_AUTOEXEC_LINES 6

/** Fills title, program, programInferred and autoexec from a conf's text. `out.name` is used for the title when the conf has none. */
void Game_ParseConf(const std::string& text, GameRecord& out);

/**
 * The program an [autoexec] runs: the last line that is not a DOS built-in or a drive change ("c:"), looking past
 * loadfix / lh / loadhigh / call. "KEEN4E.EXE", upper case with the extension as written; empty when there is none.
 */
std::string Game_InferProgram(const std::vector<std::string>& autoexec);

/**
 * The folder names an [autoexec] goes into: the first part of each `cd` path ("cd \\KEEN\\LEVELS" -> "keen") and every
 * part of a `mount` path that is a host folder ("mount d sd:/DOSBox/KEEN" -> "dosbox", "keen"). Lower case, no repeats.
 */
std::vector<std::string> Game_AutoexecFolders(const std::vector<std::string>& autoexec);

/** A name DOS can type as it is: 8.3, no spaces, none of the characters that make a long name (which DOSBox shows as KEEN~1). */
bool Game_IsDosName(const std::string& name);

/** A folder that holds a game nothing has set up yet. */
struct FoundGame
{
	std::string folder;				/**< As on the card: "WOLF3D" */
	std::string title;				/**< A suggestion for its name: "Wolf3d" (see Game_SuggestTitle) */
	std::vector<std::string> programs;	/**< Upper case, best first (Game_RankPrograms): "WOLF3D.EXE", "SETUP.EXE" */
};

/** A title from a folder name: an all upper or all lower case name is capitalised ("KEEN" -> "Keen", "my_game" -> "My Game"), a mixed case one is left. */
std::string Game_SuggestTitle(const std::string& folder);

/**
 * The programs of a folder, best first: a .BAT named like the folder, then a .EXE or .COM named like it (the same, or one
 * starting with the other: KEEN4E.EXE in KEEN), then the other .BAT, .EXE and .COM files; utilities (SETUP, INSTALL,
 * UNINST, sound setup, DOS extenders...) last. Names that are not DOS names, and other kinds of file, are dropped.
 * Upper case, no repeats, at most FOUND_PROGRAMS_MAX.
 */
std::vector<std::string> Game_RankPrograms(const std::string& folder, const std::vector<std::string>& names);

/** The most games the scan returns, and the most folders it opens below the DOSBox folder. */
#define GAMES_MAX 300
#define GAMES_MAX_FOLDERS 48

/** The most folders offered as found, and the most programs listed for one. */
#define FOUND_MAX 100
#define FOUND_PROGRAMS_MAX 24

/** What a scan finds: the games, and the folders that could be games. */
struct GamesScan
{
	std::vector<GameRecord> games;	/**< By title ignoring case, then path */
	std::vector<FoundGame> found;		/**< By folder name ignoring case */
};

/**
 * One pass over the DOSBox folder and the folders in it (each listed once). A folder is not offered when it has no
 * program, is where the menu saves games ("Games"), is not a DOS name, or a game already covers it: its [autoexec] goes
 * into it (cd) or mounts it, or (having no cd of its own) its program is one of the folder's, or it is a conf in that folder.
 */
GamesScan Games_ScanAll(const GamesHost& host);

/** Every game under the host's folder, by title ignoring case, then path. Empty when there is no storage. */
std::vector<GameRecord> Games_Scan(const GamesHost& host);

#endif
