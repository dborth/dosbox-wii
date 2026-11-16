// Host tests for the launch arguments (source/autoboot.cpp): which argument is the game, where a name is looked for, what
// DOSBox is left with, and what is said when the game cannot be started. A fake file system stands in for the card.
#include "autoboot.h"
#include <map>
#include <set>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static int checks = 0, failures = 0;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)
#define CHECK_EQ(a, b) do { checks++; const std::string x = (a), y = (b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n   got  [%s]\n   want [%s]\n", __FILE__, __LINE__, #a, x.c_str(), y.c_str()); } } while(0)

template<typename... A> static std::vector<std::string> V(A... a)
{
	return std::vector<std::string>{ std::string(a)... };
}

// files and folders by exact path; "usb:/" is spelled "usb1:/" by resolve(), like the real one
class FakeHost : public AutobootHost
{
	public:
		std::set<std::string> files, folders;
		std::vector<std::string> asked;

		void file(const std::string & p) { files.insert(p); }
		void folder(const std::string & p) { folders.insert(p); }

		std::string resolve(const std::string & path) override
		{
			asked.push_back(path);

			if(path.compare(0, 5, "usb:/") == 0)
				return "usb1:/" + path.substr(5);

			return path;
		}

		bool isFile(const std::string & p) override { return files.count(p) != 0; }
		bool isFolder(const std::string & p) override { return folders.count(p) != 0; }

		std::vector<std::string> namesIn(const std::string & folder) override
		{
			std::vector<std::string> names;
			const std::string prefix = folder + "/";

			for(std::set<std::string>::const_iterator i = files.begin(); i != files.end(); ++i)
				if(i->compare(0, prefix.size(), prefix) == 0 && i->find('/', prefix.size()) == std::string::npos)
					names.push_back(i->substr(prefix.size()));

			return names;
		}
};

static const char * const DOSBOX = "sd:/DOSBox";

static void testNothing()
{
	FakeHost h;
	LaunchArguments a = Autoboot_Parse(V(), DOSBOX, h);

	CHECK_EQ(a.args.size() == 1 ? a.args[0] : "?", "dosbox");	// argc was 0
	CHECK(a.conf.empty() && a.error.empty() && !a.named());

	a = Autoboot_Parse(V("sd:/apps/dosbox-wii/boot.dol"), DOSBOX, h);
	CHECK(a.args == V("sd:/apps/dosbox-wii/boot.dol"));
	CHECK(!a.named());
	CHECK(h.asked.empty());		// nothing to look for, nothing touched
}

static void testExplicitSwitch()
{
	FakeHost h;
	h.file("sd:/DOSBox/Games/keen.conf");

	LaunchArguments a = Autoboot_Parse(V("sd:/apps/dosbox-wii/boot.dol", "-gameconf", "sd:/DOSBox/Games/keen.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/DOSBox/Games/keen.conf");
	CHECK(a.error.empty() && a.named());
	CHECK(a.args == V("sd:/apps/dosbox-wii/boot.dol"));		// the game is the app's now, not left in DOSBox's arguments

	// case of the switch does not matter
	a = Autoboot_Parse(V("boot.dol", "-GameConf", "sd:/DOSBox/Games/keen.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/DOSBox/Games/keen.conf");

	// relative: the DOSBox folder
	a = Autoboot_Parse(V("boot.dol", "-gameconf", "Games/keen.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/DOSBox/Games/keen.conf");
	CHECK_EQ(a.requested, "Games/keen.conf");

	// no value
	a = Autoboot_Parse(V("boot.dol", "-gameconf"), DOSBOX, h);
	CHECK(a.conf.empty() && a.named());
	CHECK_EQ(a.error, "-gameconf needs a file");
}

static void testBareConf()
{
	FakeHost h;
	h.file("sd:/DOSBox/Games/keen.conf");
	h.file("sd:/DOSBox/other.conf");

	LaunchArguments a = Autoboot_Parse(V("boot.dol", "other.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/DOSBox/other.conf");
	CHECK(a.args == V("boot.dol"));

	a = Autoboot_Parse(V("boot.dol", "OTHER.CONF"), DOSBOX, h);		// extension case does not matter; the file system's does
	CHECK(a.conf.empty());
	CHECK_EQ(a.error, "Can't find OTHER.CONF");

	a = Autoboot_Parse(V("boot.dol", "Games/keen.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/DOSBox/Games/keen.conf");
}

static void testLookupOrder()
{
	FakeHost h;

	// as given beats the loader's folder beats the DOSBox folder
	h.file("keen.conf");
	h.file("sd:/apps/keen/keen.conf");
	h.file("sd:/DOSBox/keen.conf");

	LaunchArguments a = Autoboot_Parse(V("sd:/apps/keen/boot.dol", "keen.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "keen.conf");

	h.files.erase("keen.conf");
	a = Autoboot_Parse(V("sd:/apps/keen/boot.dol", "keen.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/apps/keen/keen.conf");

	h.files.erase("sd:/apps/keen/keen.conf");
	a = Autoboot_Parse(V("sd:/apps/keen/boot.dol", "keen.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/DOSBox/keen.conf");

	// argv[0] with no device (a loader may give just a name) is no folder to look in
	h.files.clear();
	h.file("/keen.conf");
	h.file("sd:/apps/keen/keen.conf");
	a = Autoboot_Parse(V("boot.dol", "keen.conf"), DOSBOX, h);
	CHECK(a.conf.empty());
	a = Autoboot_Parse(V("keen/boot.dol", "keen.conf"), DOSBOX, h);
	CHECK(a.conf.empty());

	// the loader's folder when the program is at the card's root
	h.files.clear();
	h.file("sd:/keen.conf");
	a = Autoboot_Parse(V("sd:/boot.dol", "keen.conf"), "", h);
	CHECK_EQ(a.conf, "sd:/keen.conf");

	// no DOSBox folder known: only the other two places
	h.files.clear();
	h.file("sd:/DOSBox/keen.conf");
	a = Autoboot_Parse(V("boot.dol", "keen.conf"), "", h);
	CHECK(a.conf.empty());
}

static void testResolveHook()
{
	FakeHost h;
	h.file("usb1:/games/keen/keen.conf");

	// the path comes back as the file system spells it, so that is what DOSBox is given
	LaunchArguments a = Autoboot_Parse(V("boot.dol", "-gameconf", "usb:/games/keen/keen.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "usb1:/games/keen/keen.conf");
	CHECK_EQ(a.requested, "usb:/games/keen/keen.conf");

	// an absolute path is looked for once, not under the other places as well
	h.asked.clear();
	a = Autoboot_Parse(V("boot.dol", "-gameconf", "sd:/nothing.conf"), "sd:/DOSBox", h);
	CHECK(h.asked.size() == 1);
}

static void testFolders()
{
	FakeHost h;

	// autoboot.conf wins, whatever else is there
	h.folder("sd:/Games/keen");
	h.file("sd:/Games/keen/autoboot.conf");
	h.file("sd:/Games/keen/other.conf");
	LaunchArguments a = Autoboot_Parse(V("boot.dol", "sd:/Games/keen"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/Games/keen/autoboot.conf");
	CHECK(a.args == V("boot.dol"));
	CHECK_EQ(a.requested, "sd:/Games/keen");

	// a trailing slash is fine
	a = Autoboot_Parse(V("boot.dol", "-gameconf", "sd:/Games/keen/"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/Games/keen/autoboot.conf");

	// the only conf, dosbox.conf and other files not counting
	h.folder("sd:/Games/doom");
	h.file("sd:/Games/doom/doom.conf");
	h.file("sd:/Games/doom/dosbox.conf");
	h.file("sd:/Games/doom/doom.map");
	h.file("sd:/Games/doom/DOOM.EXE");
	a = Autoboot_Parse(V("boot.dol", "-gameconf", "sd:/Games/doom"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/Games/doom/doom.conf");

	// a conf of upper-case name
	h.folder("sd:/Games/up");
	h.file("sd:/Games/up/GAME.CONF");
	a = Autoboot_Parse(V("boot.dol", "sd:/Games/up"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/Games/up/GAME.CONF");

	// several, and none of them autoboot.conf: not guessed, said
	h.folder("sd:/Games/many");
	h.file("sd:/Games/many/a.conf");
	h.file("sd:/Games/many/b.conf");
	a = Autoboot_Parse(V("boot.dol", "sd:/Games/many"), DOSBOX, h);
	CHECK(a.conf.empty());
	CHECK_EQ(a.error, "sd:/Games/many has several .conf files");
	CHECK(a.args == V("boot.dol"));

	// a folder with no conf, given bare, is for DOSBox to mount as it always did
	h.folder("sd:/Games/plain");
	h.file("sd:/Games/plain/RUN.EXE");
	a = Autoboot_Parse(V("boot.dol", "sd:/Games/plain"), DOSBOX, h);
	CHECK(a.conf.empty() && a.error.empty() && !a.named());
	CHECK(a.args == V("boot.dol", "sd:/Games/plain"));

	// ... but named with -gameconf it is an error
	a = Autoboot_Parse(V("boot.dol", "-gameconf", "sd:/Games/plain"), DOSBOX, h);
	CHECK_EQ(a.error, "sd:/Games/plain has no .conf file");
	CHECK(a.args == V("boot.dol"));

	// only the first bare argument is looked at as a folder
	a = Autoboot_Parse(V("boot.dol", "sd:/Games/plain", "sd:/Games/keen"), DOSBOX, h);
	CHECK(a.conf.empty());
	CHECK(a.args == V("boot.dol", "sd:/Games/plain", "sd:/Games/keen"));

	// the card's root is a folder too, and keeps its slash
	h.folder("sd:/");
	h.file("sd:/autoboot.conf");
	a = Autoboot_Parse(V("boot.dol", "-gameconf", "sd:/"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/autoboot.conf");

	// a bare program is left alone
	h.file("sd:/Games/plain/RUN.EXE");
	a = Autoboot_Parse(V("boot.dol", "sd:/Games/plain/RUN.EXE"), DOSBOX, h);
	CHECK(!a.named());
	CHECK(a.args == V("boot.dol", "sd:/Games/plain/RUN.EXE"));
}

static void testOtherArguments()
{
	FakeHost h;
	h.file("sd:/DOSBox/keen.conf");
	h.file("sd:/DOSBox/mine.conf");
	h.folder("sd:/DOSBox/Games");
	h.file("sd:/DOSBox/Games/g.conf");

	// DOSBox's own switches pass through in order; a value that looks like a conf or a folder is theirs, not a game
	LaunchArguments a = Autoboot_Parse(V("boot.dol", "-exit", "-c", "keen.conf", "-conf", "mine.conf", "keen.conf", "-noautoexec"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/DOSBox/keen.conf");
	CHECK(a.args == V("boot.dol", "-exit", "-c", "keen.conf", "-conf", "mine.conf", "-noautoexec"));

	a = Autoboot_Parse(V("boot.dol", "-c", "Games", "-machine", "Games"), DOSBOX, h);
	CHECK(!a.named());
	CHECK(a.args == V("boot.dol", "-c", "Games", "-machine", "Games"));

	// a switch with its value missing is passed on as it is
	a = Autoboot_Parse(V("boot.dol", "-c"), DOSBOX, h);
	CHECK(a.args == V("boot.dol", "-c"));

	// the game anywhere among them
	a = Autoboot_Parse(V("boot.dol", "-exit", "KEEN4E.EXE", "keen.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/DOSBox/keen.conf");
	CHECK(a.args == V("boot.dol", "-exit", "KEEN4E.EXE"));
}

static void testTwoGames()
{
	FakeHost h;
	h.file("sd:/DOSBox/a.conf");
	h.file("sd:/DOSBox/b.conf");

	// not guessed: neither starts
	LaunchArguments a = Autoboot_Parse(V("boot.dol", "a.conf", "b.conf"), DOSBOX, h);
	CHECK(a.conf.empty());
	CHECK_EQ(a.error, "More than one game was named");
	CHECK(a.args == V("boot.dol"));

	a = Autoboot_Parse(V("boot.dol", "-gameconf", "a.conf", "b.conf"), DOSBOX, h);
	CHECK(a.conf.empty() && a.error == "More than one game was named");

	a = Autoboot_Parse(V("boot.dol", "-gameconf", "a.conf", "-gameconf", "b.conf"), DOSBOX, h);
	CHECK(a.conf.empty() && a.error == "More than one game was named");
}

static void testMissing()
{
	FakeHost h;

	LaunchArguments a = Autoboot_Parse(V("boot.dol", "nothing.conf", "-exit"), DOSBOX, h);
	CHECK(a.conf.empty() && a.named());
	CHECK_EQ(a.error, "Can't find nothing.conf");
	CHECK_EQ(a.requested, "nothing.conf");
	CHECK(a.args == V("boot.dol", "-exit"));		// not left for the shell to try to run
}

static void testNotice()
{
	CHECK_EQ(Autoboot_Notice(""), "");
	CHECK_EQ(Autoboot_Notice("Can't find keen.conf"), "echo Autoboot: Can't find keen.conf");

	// nothing a batch line would act on
	CHECK_EQ(Autoboot_Notice("Can't find a%b|c>d<e&f^g\"h"), "echo Autoboot: Can't find a?b?c?d?e?f?g?h");

	// one screen line: the end is cut
	const std::string line = Autoboot_Notice("Can't find sd:/a/very/long/path/to/a/game/folder/that/goes/on/and/on/game.conf");
	CHECK(line.size() <= 5 + 72);
	CHECK(line.compare(line.size() - 3, 3, "...") == 0);
	CHECK(line.compare(0, 5, "echo ") == 0);

	// bytes that are not printable ASCII (a name in another encoding)
	CHECK_EQ(Autoboot_Notice(std::string("x\xc3\xa9y")), "echo Autoboot: x??y");
}

// The arguments WiiFlow builds (menu_game_boot.cpp _launchHomebrew / Plugin::CreateArgs): argv[0] is the .dol it started,
// then one entry per "|" piece of the plugin ini's arguments=, with {device}:/{path}/{name} filled in
static void testWiiFlow()
{
	FakeHost h;
	h.file("sd:/DOSBox/Games/Commander Keen 4.conf");
	h.file("usb1:/DOSBox/Games/keen4.conf");

	// arguments={device}:/{path}/{name}, a name with spaces stays one argument
	LaunchArguments a = Autoboot_Parse(V("sd:/apps/dosbox-wii/boot.dol", "sd:/DOSBox/Games/Commander Keen 4.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/DOSBox/Games/Commander Keen 4.conf");
	CHECK(a.error.empty() && a.named());
	CHECK(a.args == V("sd:/apps/dosbox-wii/boot.dol"));

	// WiiFlow says "usb" for a FAT USB partition; the file system's spelling is usb1
	a = Autoboot_Parse(V("usb1:/apps/dosbox-wii/boot.dol", "usb:/DOSBox/Games/keen4.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "usb1:/DOSBox/Games/keen4.conf");
	CHECK(a.args == V("usb1:/apps/dosbox-wii/boot.dol"));

	// the old plugin ini (arguments=-conf|{device}:/{path}/{name}) is DOSBox's own switch: no game, passed on as it was
	a = Autoboot_Parse(V("sd:/wiiflow/plugins/dosbox-wii.dol", "-conf", "sd:/DOSBox/Games/Commander Keen 4.conf"), DOSBOX, h);
	CHECK(a.conf.empty() && a.error.empty() && !a.named());
	CHECK(a.args == V("sd:/wiiflow/plugins/dosbox-wii.dol", "-conf", "sd:/DOSBox/Games/Commander Keen 4.conf"));

	// a plugin .dol kept in wiiflow/plugins: a bare name is looked for beside it
	h.file("sd:/wiiflow/plugins/keen4.conf");
	a = Autoboot_Parse(V("sd:/wiiflow/plugins/dosbox-wii.dol", "keen4.conf"), DOSBOX, h);
	CHECK_EQ(a.conf, "sd:/wiiflow/plugins/keen4.conf");

	// a drive WiiFlow can read and DOSBox Wii cannot (ntfs:) is a clear miss, not a guess
	a = Autoboot_Parse(V("sd:/apps/dosbox-wii/boot.dol", "ntfs:/DOSBox/Games/keen4.conf"), DOSBOX, h);
	CHECK(a.conf.empty() && a.named());
	CHECK_EQ(a.error, "Can't find ntfs:/DOSBox/Games/keen4.conf");
}

int main()
{
	testNothing();
	testExplicitSwitch();
	testBareConf();
	testLookupOrder();
	testResolveHook();
	testFolders();
	testOtherArguments();
	testTwoGames();
	testMissing();
	testWiiFlow();
	testNotice();

	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
