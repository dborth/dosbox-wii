// Host tests for saving a game (source/menu/gamesave.cpp): the text work on a conf, the [autoexec] built from the mounts
// and the running program, and the save itself over a fake host. No console: the live host in gameslive.cpp is a thin
// wrapper over the core calls the fake stands in for.
#include "gamesave.h"
#include "games.h"
#include <map>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int checks = 0, failures = 0;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)
#define CHECK_EQ(a, b) do { checks++; const std::string x = (a), y = (b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n   got  [%s]\n   want [%s]\n", __FILE__, __LINE__, #a, x.c_str(), y.c_str()); } } while(0)

#define CHECK_N(a, b) do { checks++; const long x = (long)(a), y = (long)(b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n   got  %ld\n   want %ld\n", __FILE__, __LINE__, #a, x, y); } } while(0)

template<typename... A> static std::vector<std::string> V(A... a)
{
	return std::vector<std::string>{ std::string(a)... };
}

static std::string join(const std::vector<std::string>& v, const char * sep = "|")
{
	std::string s;
	for(size_t i = 0; i < v.size(); i++)
		s += (i ? sep : "") + v[i];
	return s;
}

static bool has(const std::string& text, const std::string& part)
{
	return text.find(part) != std::string::npos;
}

static int countOf(const std::string& text, const std::string& part)
{
	int n = 0;
	for(size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size()))
		n++;
	return n;
}

// ---- a fake host

class FakeHost : public GameSaveHost
{
	public:
		FakeHost() : mkdirOk(true), saveMapperOk(true), configOk(true)
		{
			files["sd:/DOSBox/dosbox.conf"] = "[autoexec]\nmount d sd:/CD\n";
			files["sd:/DOSBox/mapper-0.74.map"] = "key_a \"key 97\"\n";
			mapper = "sd:/DOSBox/mapper-0.74.map";
			joystick = "4axis";
			config = printed("fixed 3000", "mount d sd:/CD");
		}

		// What PrintConfig writes for this running config: dosbox.conf's autoexec lines, then the game's
		static std::string printed(const std::string& cycles, const std::string& autoexec, const std::string& mapperfile = "mapper-0.74.map")
		{
			return "# This is the configuration file for DOSBox 0.74. (Please use the latest version of DOSBox)\n"
				"# Lines starting with a # are comment lines and are ignored by DOSBox.\n"
				"\n"
				"[sdl]\n"
				"#   mapperfile: File used to load/save the key/event mappings from.\n"
				"mapperfile=" + mapperfile + "\n"
				"\n"
				"[cpu]\n"
				"# cycles: Amount of instructions DOSBox tries to emulate each millisecond.\n"
				"cycles=" + cycles + "\n"
				"\n"
				"[joystick]\n"
				"joysticktype=auto\n"
				"\n"
				"[autoexec]\n"
				"# Lines in this section will be run at startup.\n"
				"# You can put your MOUNT lines here.\n" + autoexec + "\n";
		}

		std::string dosboxDirectory() const override { return "sd:/DOSBox"; }
		bool makeDirectory(const std::string& path) override { made.insert(path); return mkdirOk; }
		bool exists(const std::string& path) const override { return files.count(path) > 0; }
		bool read(const std::string& path, std::string& text) const override
		{
			std::map<std::string, std::string>::const_iterator it = files.find(path);
			if(it == files.end())
				return false;
			text = it->second;
			return true;
		}
		bool write(const std::string& path, const std::string& text) override
		{
			for(std::set<std::string>::const_iterator f = failWrite.begin(); f != failWrite.end(); ++f)
				if(path.size() >= f->size() && path.compare(path.size() - f->size(), f->size(), *f) == 0)
					return false;
			files[path] = text;
			written.push_back(path);
			return true;
		}
		bool runningConfig(std::string& text, std::string& error) override
		{
			if(!configOk)
			{
				error = "The config could not be read.";
				return false;
			}
			text = config;
			return true;
		}
		std::string baseConfig() const override { return "sd:/DOSBox/dosbox.conf"; }
		bool saveMapper() override { return saveMapperOk; }
		std::string mapperFile() const override { return mapper; }
		std::string joystickType() const override { return joystick; }
		std::string runningProgramPath() const override { return program; }
		std::vector<GameDrive> drives() const override { return driveList; }

		std::map<std::string, std::string> files;
		std::set<std::string> made, failWrite;
		std::vector<std::string> written;
		bool mkdirOk, saveMapperOk, configOk;
		std::string config, mapper, joystick, program;
		std::vector<GameDrive> driveList;
};

static GameDrive D(char letter, const char * info)
{
	GameDrive d;
	d.letter = letter;
	d.info = info;
	return d;
}

// ---- names and paths

static void testNames()
{
	CHECK_EQ(Game_FileName("Commander Keen 4"), "Commander Keen 4");
	CHECK_EQ(Game_FileName("  Keen: 4?  "), "Keen 4");
	CHECK_EQ(Game_FileName("a/b\\c.d"), "abcd");
	CHECK_EQ(Game_FileName("..."), "");
	CHECK_EQ(Game_FileName(""), "");
	CHECK_EQ(Game_FileName("123456789012345678901234567890"), "123456789012345678901234");
	CHECK_EQ(Game_FileName("12345678901234567890123 5678"), "12345678901234567890123");	// no trailing space left by the cut

	FakeHost h;
	CHECK_EQ(Game_Directory(h), "sd:/DOSBox/Games");
	CHECK_EQ(Game_ConfPath(h, "Keen 4"), "sd:/DOSBox/Games/Keen 4.conf");
	CHECK_EQ(Game_ConfPath(h, "???"), "");

	CHECK_EQ(Game_ResolveConf(h, "sd:/DOSBox/Games/x.conf"), "sd:/DOSBox/Games/x.conf");
	CHECK_EQ(Game_ResolveConf(h, "usb1:/other/x.conf"), "usb1:/other/x.conf");
	CHECK_EQ(Game_ResolveConf(h, "/vol/external01/DOSBox/Games/x.conf"), "/vol/external01/DOSBox/Games/x.conf");
	CHECK_EQ(Game_ResolveConf(h, "Games/x.conf"), "sd:/DOSBox/Games/x.conf");
	CHECK_EQ(Game_ResolveConf(h, "x.conf"), "sd:/DOSBox/x.conf");
	CHECK_EQ(Game_ResolveConf(h, ""), "");
}

// ---- Conf_*

static void testSetValue()
{
	const std::string t = "[sdl]\n# mapperfile: the file\nmapperfile=old.map\nautolock=true\n\n[cpu]\ncycles=auto\n";

	// replaced in place
	CHECK_EQ(Conf_SetValue(t, "sdl", "mapperfile", "new.map"), "[sdl]\n# mapperfile: the file\nmapperfile=new.map\nautolock=true\n\n[cpu]\ncycles=auto\n");
	// section and key in any case
	CHECK_EQ(Conf_SetValue("[SDL]\nMapperFile = old.map\n", "sdl", "mapperfile", "n.map"), "[SDL]\nmapperfile=n.map\n");
	// a key of the same name in another section is left alone
	CHECK_EQ(Conf_SetValue("[cpu]\ncycles=auto\n[sdl]\ncycles=1\n", "sdl", "cycles", "2"), "[cpu]\ncycles=auto\n[sdl]\ncycles=2\n");
	// a comment that names the key is not the key
	CHECK_EQ(Conf_SetValue("[sdl]\n# mapperfile=zzz\n", "sdl", "mapperfile", "n.map"), "[sdl]\n# mapperfile=zzz\nmapperfile=n.map\n");
	// added to a section that lacks it: after its last line, before the blank that follows
	CHECK_EQ(Conf_SetValue("[sdl]\nautolock=true\n\n[cpu]\ncycles=auto\n", "sdl", "mapperfile", "n.map"), "[sdl]\nautolock=true\nmapperfile=n.map\n\n[cpu]\ncycles=auto\n");
	// added to the last section
	CHECK_EQ(Conf_SetValue("[sdl]\nautolock=true\n", "sdl", "mapperfile", "n.map"), "[sdl]\nautolock=true\nmapperfile=n.map\n");
	// a section that is not there is added
	CHECK_EQ(Conf_SetValue("[cpu]\ncycles=auto\n", "joystick", "joysticktype", "2axis"), "[cpu]\ncycles=auto\n\n[joystick]\njoysticktype=2axis\n");
	CHECK_EQ(Conf_SetValue("", "sdl", "mapperfile", "n.map"), "[sdl]\nmapperfile=n.map\n");
	// a value cannot start another line
	CHECK_EQ(Conf_SetValue("[sdl]\n", "sdl", "k", "a\nb=c"), "[sdl]\nk=a b=c\n");
}

static void testSections()
{
	const std::string t = "[a]\nx=1\n# c\n\n[Autoexec]\n# help\n  mount c .  \n\nc:\n[z]\ny=2\n";
	CHECK_EQ(join(Conf_SectionLines(t, "autoexec")), "mount c .|c:");
	CHECK_EQ(join(Conf_SectionLines(t, "a")), "x=1");
	CHECK(Conf_SectionLines(t, "missing").empty());

	// the section's help stays; its lines are replaced; the next section is still separated
	CHECK_EQ(Conf_ReplaceSectionLines(t, "autoexec", V("mount d x", "d:")), "[a]\nx=1\n# c\n\n[Autoexec]\n# help\nmount d x\nd:\n\n[z]\ny=2\n");
	// the last section
	CHECK_EQ(Conf_ReplaceSectionLines("[autoexec]\n# help\nold\n", "autoexec", V("new")), "[autoexec]\n# help\nnew\n");
	// emptied
	CHECK_EQ(Conf_ReplaceSectionLines("[autoexec]\n# help\nold\n\n[z]\ny=2\n", "autoexec", V()), "[autoexec]\n# help\n\n[z]\ny=2\n");
	// a section with only help
	CHECK_EQ(Conf_ReplaceSectionLines("[autoexec]\n# help\n\n", "autoexec", V("c:")), "[autoexec]\n# help\nc:\n");
	// missing: added at the end, and nothing added for nothing
	CHECK_EQ(Conf_ReplaceSectionLines("[a]\nx=1\n", "autoexec", V("c:")), "[a]\nx=1\n\n[autoexec]\nc:\n");
	CHECK_EQ(Conf_ReplaceSectionLines("[a]\nx=1\n", "autoexec", V()), "[a]\nx=1\n");
}

// ---- Game_BuildStartup

static void testStartup()
{
	const std::string dosbox = "sd:/DOSBox";

	// the usual game: C: is the DOSBox folder, the program is under it
	GameStartup s = Game_BuildStartup(dosbox, { D('C', "sd:/DOSBox/"), D('Z', "Internal Virtual Drive") }, "C:\\GAMES\\KEEN4\\KEEN4E.EXE", V());
	CHECK_EQ(join(s.lines), "c:|cd \\GAMES\\KEEN4|KEEN4E.EXE");
	CHECK(s.notes.empty());

	// the program at the root needs no cd
	s = Game_BuildStartup(dosbox, { D('C', "sd:/DOSBox") }, "C:\\KEEN4E.EXE", V());
	CHECK_EQ(join(s.lines), "c:|KEEN4E.EXE");

	// other drives are mounted, in letter order, before the program
	s = Game_BuildStartup(dosbox, { D('E', "CDRom sd:/cd/"), D('D', "usb1:/My Games/"), D('C', "sd:/DOSBox/"), D('F', "isoDrive sd:/disc.iso") },
		"D:\\WC\\WC.EXE", V());
	CHECK_EQ(join(s.lines), "mount d \"usb1:/My Games\"|mount e sd:/cd -t cdrom|imgmount f sd:/disc.iso -t iso|d:|cd \\WC|WC.EXE");

	// a mount dosbox.conf's autoexec has is not repeated (spacing and case aside), and one it lacks is
	s = Game_BuildStartup(dosbox, { D('C', "sd:/DOSBox/"), D('D', "sd:/CD/"), D('E', "sd:/Other/") }, "", V("MOUNT  d  sd:/CD", "@echo off"));
	CHECK_EQ(join(s.lines), "mount e sd:/Other");

	// at the prompt: mounts only, or nothing
	s = Game_BuildStartup(dosbox, { D('C', "sd:/DOSBox/") }, "", V());
	CHECK(s.lines.empty());

	// a C: that is not the DOSBox folder, and a disk image, are said not to be saved
	s = Game_BuildStartup(dosbox, { D('C', "sd:/Elsewhere/"), D('B', "fatDrive sd:/disk.img") }, "C:\\X.EXE", V());
	CHECK_N(s.notes.size(), 2);
	CHECK(has(join(s.notes), "B: is a disk image"));
	CHECK(has(join(s.notes), "C: is not the DOSBox folder"));
	CHECK_EQ(join(s.lines), "c:|X.EXE");

	// the drive's separator, as GetDOSDriveInfo may give it, and a bare device
	s = Game_BuildStartup(dosbox, { D('D', "sd:/") }, "", V());
	CHECK_EQ(join(s.lines), "mount d sd:/");

	// a path with no drive keeps to the current one
	s = Game_BuildStartup(dosbox, {}, "GAMES\\X.EXE", V());
	CHECK_EQ(join(s.lines), "cd \\GAMES|X.EXE");
}

// ---- Game_Save

static void testNewGame()
{
	FakeHost h;
	h.program = "C:\\GAMES\\KEEN4\\KEEN4E.EXE";
	h.driveList = { D('C', "sd:/DOSBox/") };

	const GameSaveResult r = Game_SaveNew(h, "Commander Keen 4");
	CHECK(r.ok);
	CHECK(!r.exists);
	CHECK_EQ(r.path, "sd:/DOSBox/Games/Commander Keen 4.conf");
	CHECK(has(r.message, "Saved Commander Keen 4.conf and Commander Keen 4.map"));
	CHECK(has(r.message, "dosbox.conf was not changed"));
	CHECK(h.made.count("sd:/DOSBox/Games") > 0);

	const std::string conf = h.files["sd:/DOSBox/Games/Commander Keen 4.conf"];
	CHECK(conf.compare(0, 27, "# title=Commander Keen 4\n# ") == 0 || has(conf, "# title=Commander Keen 4\n"));
	CHECK(has(conf, "# program=KEEN4E.EXE\n"));
	CHECK(has(conf, "mapperfile=Commander Keen 4.map\n"));
	CHECK(!has(conf, "mapperfile=mapper-0.74.map"));
	CHECK(has(conf, "joysticktype=4axis\n"));		// "auto" was in the running config
	CHECK(has(conf, "cycles=fixed 3000\n"));
	// DOSBox's own header and help comments are there
	CHECK(has(conf, "# This is the configuration file for DOSBox"));
	CHECK(has(conf, "# You can put your MOUNT lines here."));
	// its own startup, not dosbox.conf's
	CHECK_EQ(join(Conf_SectionLines(conf, "autoexec")), "c:|cd \\GAMES\\KEEN4|KEEN4E.EXE");
	CHECK(!has(conf, "mount d sd:/CD"));

	// the map is a copy of the one in use, beside it
	CHECK_EQ(h.files["sd:/DOSBox/Games/Commander Keen 4.map"], "key_a \"key 97\"\n");
	// the map first, so a save cut short never lists a game with no map; and dosbox.conf is never touched
	CHECK_N(h.written.size(), 2);
	CHECK_EQ(h.written[0], "sd:/DOSBox/Games/Commander Keen 4.map");
	CHECK_EQ(h.written[1], "sd:/DOSBox/Games/Commander Keen 4.conf");
	CHECK_EQ(h.files["sd:/DOSBox/dosbox.conf"], "[autoexec]\nmount d sd:/CD\n");

	// it reads back as a game with that title and program
	GameRecord g;
	g.name = "Commander Keen 4";
	Game_ParseConf(conf, g);
	CHECK_EQ(g.title, "Commander Keen 4");
	CHECK_EQ(g.program, "KEEN4E.EXE");
	CHECK(!g.programInferred);
}

static void testNameTaken()
{
	FakeHost h;
	h.files["sd:/DOSBox/Games/Keen.conf"] = "# title=Keen\n";
	const GameSaveResult again = Game_SaveNew(h, "Keen");
	CHECK(!again.ok);
	CHECK(again.exists);
	CHECK_EQ(again.path, "sd:/DOSBox/Games/Keen.conf");
	CHECK(has(again.message, "already exists"));

	// nothing was written for the refusal
	const size_t before = h.written.size();
	Game_SaveNew(h, "Keen");
	CHECK_N(h.written.size(), before);

	// no usable name
	FakeHost h2;
	const GameSaveResult none = Game_SaveNew(h2, "???");
	CHECK(!none.ok && !none.exists);
	CHECK(has(none.message, "can't be used"));
	CHECK(h2.written.empty());
}

static void testUnchangedAndOneChange()
{
	FakeHost h;
	h.program = "C:\\KEEN4E.EXE";
	h.driveList = { D('C', "sd:/DOSBox/") };
	CHECK(Game_SaveNew(h, "Keen").ok);
	const std::string first = h.files["sd:/DOSBox/Games/Keen.conf"];

	// saved again with nothing changed (what a running game's Settings > Save does): the same file
	// (the running config now has the game's own startup after dosbox.conf's, as DOSBox appends them)
	h.config = FakeHost::printed("fixed 3000", "mount d sd:/CD\nc:\nKEEN4E.EXE", "Keen.map");
	CHECK(Game_SaveActive(h, "Games/Keen.conf").ok);
	CHECK_EQ(h.files["sd:/DOSBox/Games/Keen.conf"], first);

	// again, as a replace of the running game
	CHECK(Game_SaveReplace(h, "sd:/DOSBox/Games/Keen.conf", true).ok);
	CHECK_EQ(h.files["sd:/DOSBox/Games/Keen.conf"], first);

	// one setting changed: that line is the only one that differs
	h.config = FakeHost::printed("fixed 6000", "mount d sd:/CD\nc:\nKEEN4E.EXE", "Keen.map");
	CHECK(Game_SaveActive(h, "Games/Keen.conf").ok);
	std::string changed = h.files["sd:/DOSBox/Games/Keen.conf"];
	CHECK(has(changed, "cycles=fixed 6000\n"));
	CHECK(!has(changed, "cycles=fixed 3000"));
	const size_t at = changed.find("cycles=fixed 6000");
	changed.replace(at, strlen("cycles=fixed 6000"), "cycles=fixed 3000");
	CHECK_EQ(changed, first);

	// a multival that has spaces and a percent, and one with a comma
	h.config = FakeHost::printed("max 80%", "mount d sd:/CD\nc:\nKEEN4E.EXE", "Keen.map");
	CHECK(Game_SaveActive(h, "Games/Keen.conf").ok);
	CHECK(has(h.files["sd:/DOSBox/Games/Keen.conf"], "cycles=max 80%\n"));
	h.config = FakeHost::printed("auto", "c:", "Keen.map") + "[x]\nsensitivity=120,80\n";
	CHECK(Game_SaveActive(h, "Games/Keen.conf").ok);
	CHECK(has(h.files["sd:/DOSBox/Games/Keen.conf"], "cycles=auto\n"));
	CHECK(has(h.files["sd:/DOSBox/Games/Keen.conf"], "sensitivity=120,80\n"));
}

static void testAutoexecAppends()
{
	// DOSBox appends every file's [autoexec] lines, base first. What a game saves is its own lines, so that
	// dosbox.conf's followed by the game's gives back what was running, with nothing run twice.
	FakeHost h;
	h.program = "C:\\GAMES\\WC\\WC.EXE";
	h.driveList = { D('C', "sd:/DOSBox/"), D('D', "sd:/CD/") };
	h.config = FakeHost::printed("auto", "mount d sd:/CD\nc:\ncd \\GAMES\\WC\nWC.EXE");

	CHECK(Game_SaveNew(h, "Wing Commander").ok);
	const std::string conf = h.files["sd:/DOSBox/Games/Wing Commander.conf"];

	std::vector<std::string> together = Conf_SectionLines(h.files["sd:/DOSBox/dosbox.conf"], "autoexec");
	const std::vector<std::string> own = Conf_SectionLines(conf, "autoexec");
	together.insert(together.end(), own.begin(), own.end());
	CHECK_EQ(join(together), "mount d sd:/CD|c:|cd \\GAMES\\WC|WC.EXE");
	CHECK_N(countOf(conf, "mount d sd:/CD"), 0);

	// saved again as it is, the same (no growth)
	const std::string first = conf;
	h.config = FakeHost::printed("auto", "mount d sd:/CD\nc:\ncd \\GAMES\\WC\nWC.EXE", "Wing Commander.map");
	CHECK(Game_SaveActive(h, "Games/Wing Commander.conf").ok);
	CHECK_EQ(h.files["sd:/DOSBox/Games/Wing Commander.conf"], first);
}

static void testNoConfYet()
{
	// a game DOSBox was given a name for whose conf is not there: Settings > Save makes it
	FakeHost h;
	h.program = "C:\\DOOM\\DOOM.EXE";
	h.driveList = { D('C', "sd:/DOSBox/") };
	const GameSaveResult r = Game_SaveActive(h, "doom.conf");
	CHECK(r.ok);
	CHECK_EQ(r.path, "sd:/DOSBox/doom.conf");
	const std::string conf = h.files["sd:/DOSBox/doom.conf"];
	CHECK(has(conf, "# title=doom\n"));
	CHECK(has(conf, "mapperfile=doom.map\n"));
	CHECK_EQ(join(Conf_SectionLines(conf, "autoexec")), "c:|cd \\DOOM|DOOM.EXE");
	CHECK(has(conf, "# program=DOOM.EXE\n"));
	CHECK(h.files.count("sd:/DOSBox/doom.map") > 0);

	// nothing given: nothing to save
	FakeHost h2;
	CHECK(!Game_SaveActive(h2, "").ok);
	CHECK(h2.written.empty());
}

static void testReplace()
{
	FakeHost h;
	h.files["sd:/DOSBox/Games/Prince.conf"] =
		"# my notes about this one\n"
		"# title=Prince of Persia\n"
		"# program=PRINCE.EXE\n"
		"[cpu]\ncycles=fixed 3000\n"
		"[autoexec]\nmount c sd:/DOSBox\nc:\ncd \\GAMES\\PRINCE\nPRINCE.EXE megahit\n";
	h.program = "C:\\GAMES\\KEEN4\\KEEN4E.EXE";
	h.driveList = { D('C', "sd:/DOSBox/") };

	// the running game replaced: its hand-made startup and its comments stay, its title too
	CHECK(Game_SaveReplace(h, "sd:/DOSBox/Games/Prince.conf", true).ok);
	std::string conf = h.files["sd:/DOSBox/Games/Prince.conf"];
	CHECK(has(conf, "# my notes about this one\n"));
	CHECK(has(conf, "# title=Prince of Persia\n"));
	CHECK(has(conf, "# program=PRINCE.EXE\n"));
	CHECK(!has(conf, "KEEN4E"));
	CHECK_EQ(join(Conf_SectionLines(conf, "autoexec")), "mount c sd:/DOSBox|c:|cd \\GAMES\\PRINCE|PRINCE.EXE megahit");
	// the hand-written comment is kept once, however often it is saved
	CHECK(Game_SaveReplace(h, "sd:/DOSBox/Games/Prince.conf", true).ok);
	CHECK_N(countOf(h.files["sd:/DOSBox/Games/Prince.conf"], "# my notes about this one"), 1);
	CHECK_N(countOf(h.files["sd:/DOSBox/Games/Prince.conf"], "# title="), 1);
	CHECK_N(countOf(h.files["sd:/DOSBox/Games/Prince.conf"], "# This is the configuration file"), 1);

	// another game replaced with what is running: its startup is worked out again, its title stays
	CHECK(Game_SaveReplace(h, "sd:/DOSBox/Games/Prince.conf", false).ok);
	conf = h.files["sd:/DOSBox/Games/Prince.conf"];
	CHECK_EQ(join(Conf_SectionLines(conf, "autoexec")), "c:|cd \\GAMES\\KEEN4|KEEN4E.EXE");
	CHECK(has(conf, "# title=Prince of Persia\n"));
	CHECK(has(conf, "# program=KEEN4E.EXE\n"));
	CHECK(has(conf, "# my notes about this one\n"));

	// at the prompt with nothing mounted to go on, the startup it had is kept rather than emptied
	h.program.clear();
	CHECK(Game_SaveReplace(h, "sd:/DOSBox/Games/Prince.conf", false).ok);
	CHECK_EQ(join(Conf_SectionLines(h.files["sd:/DOSBox/Games/Prince.conf"], "autoexec")), "c:|cd \\GAMES\\KEEN4|KEEN4E.EXE");
}

static void testMapper()
{
	// the mapper file in use is a game's own (DOSBox was started with it): saved in place, not copied onto itself
	FakeHost h;
	h.files["sd:/DOSBox/Games/Keen.conf"] = "# title=Keen\n[sdl]\nmapperfile=Keen.map\n";
	h.files["sd:/DOSBox/Games/Keen.map"] = "key_b \"key 98\"\n";
	h.mapper = "sd:/DOSBox/Games/Keen.map";
	h.config = FakeHost::printed("auto", "", "Keen.map");
	CHECK(Game_SaveActive(h, "Games/Keen.conf").ok);
	CHECK_N(h.written.size(), 1);
	CHECK_EQ(h.written[0], "sd:/DOSBox/Games/Keen.conf");
	CHECK_EQ(h.files["sd:/DOSBox/Games/Keen.map"], "key_b \"key 98\"\n");

	// the one in use is somebody else's (dosbox.conf's, or a hand-made game that has none): copied beside the conf
	FakeHost g;
	g.mapper = "sd:/DOSBox/mapper-0.74.map";
	CHECK(Game_SaveNew(g, "Doom").ok);
	CHECK_EQ(g.files["sd:/DOSBox/Games/Doom.map"], "key_a \"key 97\"\n");
	CHECK(has(g.files["sd:/DOSBox/Games/Doom.conf"], "mapperfile=Doom.map\n"));
	CHECK_EQ(g.files["sd:/DOSBox/mapper-0.74.map"], "key_a \"key 97\"\n");	// the base's own is not changed

	// a mapper file that cannot be written or read stops the save before any file is replaced
	FakeHost bad;
	bad.saveMapperOk = false;
	GameSaveResult r = Game_SaveNew(bad, "Doom");
	CHECK(!r.ok && has(r.message, "Not saved") && has(r.message, "mapper"));
	CHECK(bad.written.empty());
	FakeHost gone;
	gone.mapper = "sd:/DOSBox/missing.map";
	r = Game_SaveNew(gone, "Doom");
	CHECK(!r.ok && has(r.message, "mapper"));
	CHECK(gone.written.empty());
}

static void testFailures()
{
	// a folder that cannot be made (a locked or missing card)
	FakeHost a;
	a.mkdirOk = false;
	GameSaveResult r = Game_SaveNew(a, "Doom");
	CHECK(!r.ok);
	CHECK(has(r.message, "Could not make the folder sd:/DOSBox/Games"));
	CHECK(a.written.empty());

	// the map cannot be written: no conf is written either
	FakeHost b;
	b.failWrite.insert(".map");
	r = Game_SaveNew(b, "Doom");
	CHECK(!r.ok && has(r.message, "Doom.map"));
	CHECK(b.written.empty());
	CHECK(!b.exists("sd:/DOSBox/Games/Doom.conf"));

	// the conf cannot be written: an old one is as it was (the host keeps it until the new one is in place)
	FakeHost c;
	c.files["sd:/DOSBox/Games/Doom.conf"] = "# title=Old\n[autoexec]\nold.exe\n";
	c.failWrite.insert(".conf");
	r = Game_SaveReplace(c, "sd:/DOSBox/Games/Doom.conf", true);
	CHECK(!r.ok && has(r.message, "Doom.conf"));
	CHECK_EQ(c.files["sd:/DOSBox/Games/Doom.conf"], "# title=Old\n[autoexec]\nold.exe\n");

	// the running config could not be had
	FakeHost d;
	d.configOk = false;
	r = Game_SaveNew(d, "Doom");
	CHECK(!r.ok && has(r.message, "The config could not be read"));
	CHECK(d.written.empty());

	// no storage
	class NoCard : public FakeHost { public: std::string dosboxDirectory() const override { return std::string(); } };
	NoCard e;
	r = Game_SaveNew(e, "Doom");
	CHECK(!r.ok && !r.exists);
	CHECK(e.written.empty());
}

// ---- the files half, on a real folder

static std::string readFile(const std::string& path)
{
	FILE * f = fopen(path.c_str(), "rb");
	std::string s;
	if(!f)
		return s;
	char buf[256];
	size_t n;
	while((n = fread(buf, 1, sizeof(buf), f)) > 0)
		s.append(buf, n);
	fclose(f);
	return s;
}

// the files half is abstract (the core's half is in gameslive.cpp): this gives the rest as nothing
class BareFiles : public FileGameSaveHost
{
	public:
		explicit BareFiles(const std::string& folder) : FileGameSaveHost(folder) {}
		bool runningConfig(std::string&, std::string&) override { return false; }
		std::string baseConfig() const override { return std::string(); }
		bool saveMapper() override { return false; }
		std::string mapperFile() const override { return std::string(); }
		std::string joystickType() const override { return std::string(); }
		std::string runningProgramPath() const override { return std::string(); }
		std::vector<GameDrive> drives() const override { return std::vector<GameDrive>(); }
};

static FoundGame foundGame(const char * folder, std::vector<std::string> programs)
{
	FoundGame f;
	f.folder = folder;
	f.title = Game_SuggestTitle(folder);
	f.programs = programs;
	return f;
}

static void testAddFound()
{
	FakeHost h;
	const FoundGame wolf = foundGame("WOLF3D", V("WOLF3D.EXE", "SETUP.EXE"));

	// the file name comes from the folder, the title is whatever was typed
	CHECK_EQ(Game_FoundConfPath(h, wolf, "Wolfenstein 3D"), "sd:/DOSBox/Games/WOLF3D.conf");

	const GameSaveResult r = Game_AddFound(h, wolf, "Wolfenstein 3D", "WOLF3D.EXE");
	CHECK(r.ok);
	CHECK(!r.exists);
	CHECK_EQ(r.path, "sd:/DOSBox/Games/WOLF3D.conf");
	CHECK(has(r.message, "Added WOLF3D.conf"));
	CHECK(has(r.message, "defaults"));
	CHECK(h.made.count("sd:/DOSBox/Games") > 0);

	// a small conf: title, program, startup; no map, no settings; only that one file written
	const std::string conf = h.files["sd:/DOSBox/Games/WOLF3D.conf"];
	CHECK_EQ(conf, "# title=Wolfenstein 3D\n# program=WOLF3D.EXE\n\n[autoexec]\nc:\ncd \\WOLF3D\nWOLF3D.EXE\n");
	CHECK_N(h.written.size(), 1);
	CHECK(!has(conf, "mapperfile"));
	CHECK(!has(conf, "[cpu]"));
	CHECK(h.files.count("sd:/DOSBox/Games/WOLF3D.map") == 0);
	CHECK_EQ(h.files["sd:/DOSBox/dosbox.conf"], "[autoexec]\nmount d sd:/CD\n");

	// the scan reads it back as that game, covering the folder: it is no longer offered
	GameRecord g;
	g.name = "WOLF3D";
	Game_ParseConf(conf, g);
	CHECK_EQ(g.title, "Wolfenstein 3D");
	CHECK_EQ(g.program, "WOLF3D.EXE");
	CHECK(!g.programInferred);
	CHECK_EQ(join(Game_AutoexecFolders(Conf_SectionLines(conf, "autoexec"))), "wolf3d");
	CHECK_EQ(Game_InferProgram(Conf_SectionLines(conf, "autoexec")), "WOLF3D.EXE");

	// the program chosen, the first by default, a BAT run by its name
	CHECK_EQ(Game_FoundConfText(wolf, "W", ""), "# title=W\n# program=WOLF3D.EXE\n\n[autoexec]\nc:\ncd \\WOLF3D\nWOLF3D.EXE\n");
	CHECK(has(Game_FoundConfText(wolf, "W", "setup.exe"), "# program=SETUP.EXE\n"));
	CHECK(has(Game_FoundConfText(foundGame("KEEN", V("KEEN.BAT")), "Keen", "KEEN.BAT"), "cd \\KEEN\nKEEN.BAT\n"));
	// the folder as it is on the card, in lower case, is written the DOS way
	CHECK(has(Game_FoundConfText(foundGame("keen", V("keen4e.exe")), "K", ""), "cd \\KEEN\nKEEN4E.EXE\n"));
}

static void testAddFoundNames()
{
	FakeHost h;
	const FoundGame doom = foundGame("DOOM", V("DOOM.EXE"));

	// the folder's name is taken (a conf by hand with no program): the title's name, then the folder numbered
	h.files["sd:/DOSBox/Games/DOOM.conf"] = "[cpu]\n";
	CHECK_EQ(Game_FoundConfPath(h, doom, "Doom Shareware"), "sd:/DOSBox/Games/Doom Shareware.conf");
	h.files["sd:/DOSBox/Games/Doom Shareware.conf"] = "";
	CHECK_EQ(Game_FoundConfPath(h, doom, "Doom Shareware"), "sd:/DOSBox/Games/DOOM 2.conf");
	for(int n = 2; n <= 8; n++)
		h.files["sd:/DOSBox/Games/DOOM " + std::to_string(n) + ".conf"] = "";
	CHECK_EQ(Game_FoundConfPath(h, doom, "Doom Shareware"), "sd:/DOSBox/Games/DOOM 9.conf");

	const GameSaveResult r = Game_AddFound(h, doom, "Doom Shareware", "DOOM.EXE");
	CHECK(r.ok);
	CHECK_EQ(r.path, "sd:/DOSBox/Games/DOOM 9.conf");
	// what was there is untouched
	CHECK_EQ(h.files["sd:/DOSBox/Games/DOOM.conf"], "[cpu]\n");

	// every name taken: nothing is written
	h.files["sd:/DOSBox/Games/DOOM 9.conf"] = "keep";
	const size_t before = h.written.size();
	const GameSaveResult none = Game_AddFound(h, doom, "Doom Shareware", "DOOM.EXE");
	CHECK(!none.ok);
	CHECK(none.exists);
	CHECK(has(none.message, "Not added"));
	CHECK_N(h.written.size(), before);
	CHECK_EQ(h.files["sd:/DOSBox/Games/DOOM 9.conf"], "keep");

	// a title with nothing the card can hold uses the folder; the file name never has a character the card cannot
	FakeHost g;
	CHECK_EQ(Game_FoundConfPath(g, doom, "???"), "sd:/DOSBox/Games/DOOM.conf");
	CHECK_EQ(Game_FoundConfText(doom, "  \t ", ""), "# title=DOOM\n# program=DOOM.EXE\n\n[autoexec]\nc:\ncd \\DOOM\nDOOM.EXE\n");
	// a title cannot start a new setting, and is kept to the length a title has
	CHECK(has(Game_FoundConfText(doom, "A\n[cpu]\ncycles=1", ""), "# title=A [cpu] cycles=1\n"));
	CHECK(!has(Game_FoundConfText(doom, "A\n[cpu]", ""), "\n[cpu]"));
	CHECK(has(Game_FoundConfText(doom, std::string(80, 'x'), ""), "# title=" + std::string(40, 'x') + "\n"));
}

static void testAddFoundFailures()
{
	const FoundGame wolf = foundGame("WOLF3D", V("WOLF3D.EXE"));

	// no program
	FoundGame bare = wolf;
	bare.programs.clear();
	FakeHost a;
	GameSaveResult r = Game_AddFound(a, bare, "W", "");
	CHECK(!r.ok);
	CHECK(a.written.empty());
	// ...unless one is given
	CHECK(Game_AddFound(a, bare, "W", "X.EXE").ok);
	CHECK(has(a.files["sd:/DOSBox/Games/WOLF3D.conf"], "# program=X.EXE\n"));

	// no folder to make
	FakeHost b;
	b.mkdirOk = false;
	r = Game_AddFound(b, wolf, "W", "");
	CHECK(!r.ok);
	CHECK(!r.exists);
	CHECK(has(r.message, "Not saved"));
	CHECK(b.written.empty());

	// the write fails
	FakeHost c;
	c.failWrite.insert("WOLF3D.conf");
	r = Game_AddFound(c, wolf, "W", "");
	CHECK(!r.ok);
	CHECK(has(r.message, "Could not write WOLF3D.conf"));
	CHECK(c.files.count("sd:/DOSBox/Games/WOLF3D.conf") == 0);

	// nothing is asked of the running core: a found game is added with nothing running (no config, no mapper, no program)
	FakeHost d;
	d.configOk = false;
	d.saveMapperOk = false;
	CHECK(Game_AddFound(d, wolf, "W", "").ok);
}

static void testFileHost()
{
	char tmpl[] = "/tmp/gamesaveXXXXXX";
	if(!mkdtemp(tmpl))
	{
		printf("no temp folder, skipping the file host tests\n");
		return;
	}
	const std::string base = tmpl;
	BareFiles h(base);

	CHECK_EQ(h.dosboxDirectory(), base);
	CHECK(h.makeDirectory(base + "/Games"));
	CHECK(h.makeDirectory(base + "/Games"));			// already there
	CHECK(h.makeDirectory(base + "/Deep/Er"));		// the parent is made too
	CHECK(!h.makeDirectory(base + "/a/b/c"));			// not further than that
	CHECK(!h.makeDirectory(""));

	const std::string file = base + "/Games/x.conf";
	CHECK(!h.exists(file));
	CHECK(h.write(file, "one\n"));
	CHECK_EQ(readFile(file), "one\n");
	CHECK(h.write(file, "two\n"));
	CHECK_EQ(readFile(file), "two\n");
	// no .tmp or .bak left behind
	CHECK(!h.exists(file + ".tmp"));
	CHECK(!h.exists(file + ".bak"));
	std::string text;
	CHECK(h.read(file, text));
	CHECK_EQ(text, "two\n");
	CHECK(!h.read(base + "/none", text));

	// a folder that is not there: the write fails and nothing is left
	CHECK(!h.write(base + "/missing/x.conf", "x"));
	CHECK(!h.exists(base + "/missing/x.conf.tmp"));

	// a leftover .tmp from a cut short write does not get in the way, and the old file survives a failed swap's retry
	FILE * f = fopen((file + ".tmp").c_str(), "wb");
	if(f) { fputs("half", f); fclose(f); }
	CHECK(h.write(file, "three\n"));
	CHECK_EQ(readFile(file), "three\n");
	CHECK(!h.exists(file + ".tmp"));

	std::string cmd = "rm -rf '" + base + "'";
	if(system(cmd.c_str()) != 0)
		printf("could not remove %s\n", base.c_str());
}

int main()
{
	testNames();
	testSetValue();
	testSections();
	testStartup();
	testNewGame();
	testNameTaken();
	testUnchangedAndOneChange();
	testAutoexecAppends();
	testNoConfYet();
	testReplace();
	testMapper();
	testFailures();
	testAddFound();
	testAddFoundNames();
	testAddFoundFailures();
	testFileHost();

	if(failures)
	{
		printf("gamesave tests: %d of %d checks FAILED\n", failures, checks);
		return 1;
	}
	printf("gamesave tests: %d checks passed\n", checks);
	return 0;
}
