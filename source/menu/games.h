/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * games.h
 *
 * Games on disk. A game is a .conf that DOSBox layers over dosbox.conf (see
 * SetGameConf()), so it only holds what differs. Nothing has to be registered:
 * the scan picks up every .conf in the DOSBox folder, whoever wrote it, by
 * hand or from the menu (our own controls setups are games too).
 *
 * Where it looks, and no further, so it stays quick on a slow card:
 *
 *     <app drive>/DOSBox/                 every .conf in it, except dosbox.conf
 *     <app drive>/DOSBox/Setups/          the same
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
	bool isSetup;				/**< One of our controls setups (it starts with our marker line) */
	std::string path;			/**< The whole path, which is what SetGameConf() takes ("sd:/DOSBox/Setups/keen.conf") */
	std::string where;			/**< The folder below the DOSBox folder it was found in; empty for the DOSBox folder itself ("Setups", "GAMES") */
	std::vector<std::string> autoexec;	/**< The first lines of [autoexec], for showing what it will run */

	GameRecord() : programInferred(false), isSetup(false) {}
};

/** How many [autoexec] lines are kept per game. */
#define GAME_AUTOEXEC_LINES 6

/** Fills title, program, programInferred, isSetup and autoexec from a conf's text. `out.name` is used for the title when the conf has none. */
void Game_ParseConf(const std::string& text, GameRecord& out);

/**
 * The program an [autoexec] runs: the last line that is not a DOS built-in or a drive change ("c:"), looking past
 * loadfix / lh / loadhigh / call. "KEEN4E.EXE", upper case with the extension as written; empty when there is none.
 */
std::string Game_InferProgram(const std::vector<std::string>& autoexec);

/** The most games the scan returns, and the most folders it opens below the DOSBox folder. */
#define GAMES_MAX 300
#define GAMES_MAX_FOLDERS 48

/** Every game under the host's folder, by title ignoring case, then path. Empty when there is no storage. */
std::vector<GameRecord> Games_Scan(const GamesHost& host);

#endif
