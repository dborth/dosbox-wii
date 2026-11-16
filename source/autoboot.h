/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * autoboot.h
 *
 * Starting straight into a game: reading the launch arguments (the Homebrew
 * Channel's meta.xml <arguments>, or a WiiFlow plugin's arguments=) for the game to run.
 *
 * Plain C++ with no DOSBox, libgui or driver types, so the host tests can run
 * it. The file system is reached through AutobootHost.
 *
 * What is accepted (case does not matter for the switch or the extension):
 *
 *   -gameconf <conf or folder>      the explicit form
 *   <name>.conf                     a bare argument ending in .conf
 *   <folder>                        a bare argument that is a folder holding
 *                                   a game conf (autoboot.conf, or the only
 *                                   .conf other than dosbox.conf). A folder
 *                                   with no .conf is left for DOSBox, which
 *                                   mounts it as C: as it always did.
 *
 * A relative name is looked for as given, then beside the program that was
 * launched (argv[0], when the loader gave it with a device prefix), then in
 * the DOSBox folder. The game's own files are looked for beside its conf by
 * DOSBox, so a game keeps its conf, map and files in one folder.
 *
 * Both loaders give argv[0] as the path of the .dol they started, with a device
 * ("sd:/apps/dosbox-wii/boot.dol"), and one argv entry per argument, so a name
 * with spaces needs no quoting. WiiFlow's plugin line "arguments={device}:/{path}/{name}"
 * gives one argument, "sd:/DOSBox/Games/keen4.conf", which is the bare .conf form
 * above. WiiFlow inis written for older DOSBox Wii builds pass "-conf|<path>"; that
 * is DOSBox's own switch and is left alone (the conf then replaces dosbox.conf).
 *
 * Whatever is taken as the game is removed from the arguments DOSBox gets: the
 * app owns the game conf (SetGameConf()), and every start passes it itself.
 ***************************************************************************/

#ifndef _AUTOBOOT_H_
#define _AUTOBOOT_H_

#include <string>
#include <vector>

//! The file system and storage, as the parser needs them
class AutobootHost
{
	public:
		virtual ~AutobootHost() {}

		//! The path as the file system wants it (device spelling, waiting for a
		//! USB drive that is slow to show up). Called on every path before it is tested.
		virtual std::string resolve(const std::string & path) { return path; }

		virtual bool isFile(const std::string & path) = 0;
		virtual bool isFolder(const std::string & path) = 0;

		//! File names (not paths) in a folder; any that are not .conf may be left in
		virtual std::vector<std::string> namesIn(const std::string & folder) = 0;
};

struct LaunchArguments
{
	//! What DOSBox is given at every start, argv[0] first, with the game
	//! arguments taken out. Never empty.
	std::vector<std::string> args;

	//! The game conf: a path the file system accepts, as host.resolve() spelled it.
	//! Empty when none was named or it could not be used.
	std::string conf;

	//! What the arguments asked for, as written; empty when they asked for no game
	std::string requested;

	//! Why `requested` could not be used; empty when all is well
	std::string error;

	//! True when the arguments named a game (it may still have failed: see error)
	bool named() const { return !requested.empty() || !error.empty(); }
};

//! Reads the launch arguments (argv as main() got it; empty is fine).
//! dosboxFolder is the DOSBox folder ("sd:/DOSBox"), or empty if there is none.
LaunchArguments Autoboot_Parse(const std::vector<std::string> & argv, const std::string & dosboxFolder, AutobootHost & host);

//! A game bundled with the app: on the Wii U, a .wuhb's content folder, which Aroma
//! mounts at fs:/vol/content. It is read-only, so what it holds is the game and
//! what the game needs, not what DOSBox writes.
struct Bundle
{
	std::string folder;		//!< The content folder that has the game; empty when there is none
	std::string conf;		//!< Its game conf: autoboot.conf in that folder

	bool found() const { return !conf.empty(); }
};

//! The first of `folders` that holds an autoboot.conf (looked for by that name
//! only: the other confs in a bundle are for the Games list). Nothing when none
//! does, as on the Wii, or on the Wii U when the app was not started as a .wuhb.
Bundle Autoboot_FindBundle(const std::vector<std::string> & folders, AutobootHost & host);

//! The one DOS line that tells the player why the game did not start, for the
//! autoexec ("echo ..."): safe in a batch file and short enough for one screen
//! line. Empty when there is nothing to say.
std::string Autoboot_Notice(const std::string & error);

#endif
