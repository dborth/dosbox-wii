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
		FakeHost() : bindsOk(true), configOk(true)
		{
			files["sd:/DOSBox/dosbox.conf"] = "[autoexec]\nmount d sd:/CD\n";
			files["sd:/DOSBox/mapper-0.74.map"] = "key_a \"key 97\"\n";	// the shared mapper file: a save neither reads nor writes it
			binds = "key_a \"key 97\"\n";
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
		bool erase(const std::string& path) override
		{
			erased.push_back(path);
			if(eraseFails)
				return false;
			return files.erase(path) > 0;		// like remove(): a file that is not there is not deleted
		}
		std::vector<std::string> erased;
		bool eraseFails = false;
		bool read(const std::string& path, std::string& text) const override
		{
			reads.push_back(path);
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
		bool mapperText(std::string& text) override
		{
			if(!bindsOk)
				return false;
			text = binds;
			return true;
		}
		std::string joystickType() const override { return joystick; }
		std::string runningProgramPath() const override { return program; }
		std::vector<GameDrive> drives() const override { return driveList; }

		std::map<std::string, std::string> files;
		std::set<std::string> failWrite;
		std::vector<std::string> written;
		mutable std::vector<std::string> reads;	// every read, to show what a save looks at
		bool bindsOk, configOk;
		std::string config, binds, joystick, program;
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
	CHECK_EQ(r.text, h.files["sd:/DOSBox/Games/Commander Keen 4.conf"]);	// what the games list is told it is

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

	// the map is the controls as they are in memory, beside the conf
	CHECK_EQ(h.files["sd:/DOSBox/Games/Commander Keen 4.map"], "key_a \"key 97\"\n");
	// all a save reads is the conf it may replace (its absence is how a new name is known) and dosbox.conf
	CHECK_N(h.reads.size(), 2);
	CHECK_EQ(h.reads[0], "sd:/DOSBox/Games/Commander Keen 4.conf");
	CHECK_EQ(h.reads[1], "sd:/DOSBox/dosbox.conf");
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

	// the games list is told what was written, and has the game as a scan of the file would make it
	const GameRecord listed = Game_RecordFor(r.path, "sd:/DOSBox", r.text);
	CHECK_EQ(listed.name, "Commander Keen 4");
	CHECK_EQ(listed.title, "Commander Keen 4");
	CHECK_EQ(listed.program, "KEEN4E.EXE");
	CHECK_EQ(listed.where, "Games");
	CHECK_EQ(listed.mapFile, "Commander Keen 4.map");
	CHECK(listed.ownControls);
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

	// nothing was written for the refusal, and the only thing looked at was the file that is there
	const size_t before = h.written.size();
	h.reads.clear();
	Game_SaveNew(h, "Keen");
	CHECK_N(h.written.size(), before);
	CHECK_N(h.reads.size(), 1);

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

static void testControls()
{
	// the map holds the controls as they are in memory: whatever was changed in the menu and not saved yet
	FakeHost h;
	h.binds = "key_b \"key 98\"\njbutton_0_0 \"pad_0 button 0\"\n";
	CHECK(Game_SaveNew(h, "Doom").ok);
	CHECK_EQ(h.files["sd:/DOSBox/Games/Doom.map"], "key_b \"key 98\"\njbutton_0_0 \"pad_0 button 0\"\n");
	CHECK(has(h.files["sd:/DOSBox/Games/Doom.conf"], "mapperfile=Doom.map\n"));

	// the shared mapper file is neither read nor written: its place is not looked at
	CHECK_EQ(h.files["sd:/DOSBox/mapper-0.74.map"], "key_a \"key 97\"\n");
	for(size_t i = 0; i < h.written.size(); i++)
		CHECK(!has(h.written[i], "mapper-0.74.map"));
	for(size_t i = 0; i < h.reads.size(); i++)
		CHECK(!has(h.reads[i], "mapper-0.74.map"));

	// the game that is running, whose own map DOSBox was started with: map and conf written, once each, map first
	FakeHost g;
	g.files["sd:/DOSBox/Games/Keen.conf"] = "# title=Keen\n[sdl]\nmapperfile=Keen.map\n";
	g.files["sd:/DOSBox/Games/Keen.map"] = "key_b \"key 98\"\n";
	g.binds = "key_c \"key 99\"\n";
	g.config = FakeHost::printed("auto", "", "Keen.map");
	CHECK(Game_SaveActive(g, "Games/Keen.conf").ok);
	CHECK_N(g.written.size(), 2);
	CHECK_EQ(g.written[0], "sd:/DOSBox/Games/Keen.map");
	CHECK_EQ(g.written[1], "sd:/DOSBox/Games/Keen.conf");
	CHECK_EQ(g.files["sd:/DOSBox/Games/Keen.map"], "key_c \"key 99\"\n");

	// controls that cannot be had stop the save before any file is written
	FakeHost bad;
	bad.bindsOk = false;
	const GameSaveResult r = Game_SaveNew(bad, "Doom");
	CHECK(!r.ok && has(r.message, "Not saved") && has(r.message, "controls"));
	CHECK(bad.written.empty());
}

static void testFailures()
{
	// the map cannot be written (a locked or missing card, or a folder that could not be made): no conf is written either
	FakeHost b;
	b.failWrite.insert(".map");
	GameSaveResult r = Game_SaveNew(b, "Doom");
	CHECK(!r.ok && has(r.message, "Doom.map"));
	CHECK(b.written.empty());
	CHECK(b.files.count("sd:/DOSBox/Games/Doom.conf") == 0);

	// the conf cannot be written: it is said, and the game is not reported as saved
	FakeHost c;
	c.files["sd:/DOSBox/Games/Doom.conf"] = "# title=Old\n[autoexec]\nold.exe\n";
	c.failWrite.insert(".conf");
	r = Game_SaveReplace(c, "sd:/DOSBox/Games/Doom.conf", true);
	CHECK(!r.ok && has(r.message, "Doom.conf"));
	CHECK(r.text.empty());
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
		bool mapperText(std::string&) override { return false; }
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

	// a small conf: title, program, startup; no map, no settings; only that one file written
	const std::string conf = h.files["sd:/DOSBox/Games/WOLF3D.conf"];
	CHECK_EQ(conf, "# title=Wolfenstein 3D\n# program=WOLF3D.EXE\n\n[autoexec]\nc:\ncd \\WOLF3D\nWOLF3D.EXE\n");
	CHECK_N(h.written.size(), 1);
	CHECK_EQ(r.text, conf);
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
	d.bindsOk = false;
	CHECK(Game_AddFound(d, wolf, "W", "").ok);
}

static void testRename()
{
	// the title line at the top is replaced; nothing else is touched
	const std::string conf = "# title=Old name\n# program=KEEN4E.EXE\n[sdl]\nmapperfile=keen.map\n[cpu]\ncycles=fixed 6000\n[autoexec]\ncd KEEN\nKEEN4E.EXE\n";
	CHECK_EQ(Conf_SetTitle(conf, "Commander Keen 4"),
		"# title=Commander Keen 4\n# program=KEEN4E.EXE\n[sdl]\nmapperfile=keen.map\n[cpu]\ncycles=fixed 6000\n[autoexec]\ncd KEEN\nKEEN4E.EXE\n");
	// no title yet: one is put at the top
	CHECK_EQ(Conf_SetTitle("[cpu]\ncycles=max\n", "Doom"), "# title=Doom\n[cpu]\ncycles=max\n");
	// a title that is not in the comments at the top (a key inside a section) is not the one
	CHECK_EQ(Conf_SetTitle("[x]\n# title=inside\n", "T"), "# title=T\n[x]\n# title=inside\n");
	// any case, and no space after the hash
	CHECK_EQ(Conf_SetTitle("#TITLE = a\n[x]\n", "b"), "# title=b\n[x]\n");

	FakeHost h;
	h.files["sd:/DOSBox/Games/keen.conf"] = conf;
	GameSaveResult r = Game_Rename(h, "sd:/DOSBox/Games/keen.conf", "  Commander Keen 4 ");
	CHECK(r.ok);
	CHECK_EQ(r.message, "Renamed to Commander Keen 4.");
	CHECK(h.files["sd:/DOSBox/Games/keen.conf"].find("# title=Commander Keen 4\n# program=KEEN4E.EXE\n[sdl]") == 0);
	CHECK(h.files["sd:/DOSBox/Games/keen.conf"].find("cycles=fixed 6000") != std::string::npos);
	CHECK(h.erased.empty());				// no file was removed or renamed: the paths stay as they are
	CHECK_EQ(r.text, h.files["sd:/DOSBox/Games/keen.conf"]);	// the games list is told the new text, and so has the new title
	CHECK_EQ(Game_RecordFor(r.path, "sd:/DOSBox", r.text).title, "Commander Keen 4");
	CHECK_N(h.written.size(), 1);							// one read, one write

	// a long title is cut; an empty one, a missing file and a write that fails are not renames
	r = Game_Rename(h, "sd:/DOSBox/Games/keen.conf", std::string(80, 'x'));
	CHECK(r.ok);
	CHECK(h.files["sd:/DOSBox/Games/keen.conf"].find("# title=" + std::string(40, 'x') + "\n") == 0);
	const std::string before = h.files["sd:/DOSBox/Games/keen.conf"];
	CHECK(!Game_Rename(h, "sd:/DOSBox/Games/keen.conf", "   ").ok);
	CHECK(!Game_Rename(h, "sd:/DOSBox/Games/nothere.conf", "x").ok);
	CHECK_EQ(h.files["sd:/DOSBox/Games/keen.conf"], before);
}

static void testDelete()
{
	// the conf and the map it names beside it: two deletes, and nothing is read
	FakeHost h;
	h.files["sd:/DOSBox/Games/keen.conf"] = "# title=Commander Keen 4\n[sdl]\nmapperfile=keen.map\n";
	h.files["sd:/DOSBox/Games/keen.map"] = "key_a \"key 97\"\n";
	h.files["sd:/DOSBox/Games/other.conf"] = "# title=Other\n";
	GameSaveResult r = Game_Delete(h, "sd:/DOSBox/Games/keen.conf", "keen.map", "Commander Keen 4");
	CHECK(r.ok);
	CHECK_EQ(r.message, "Deleted Commander Keen 4.");
	CHECK(h.files.count("sd:/DOSBox/Games/keen.conf") == 0);
	CHECK(h.files.count("sd:/DOSBox/Games/keen.map") == 0);
	CHECK(h.files.count("sd:/DOSBox/Games/other.conf") == 1);		// nothing else
	CHECK_N(h.erased.size(), 2);
	CHECK(h.reads.empty());
	CHECK(h.written.empty());

	// a map under another name may belong to other games: it stays, with the shared mapper file
	FakeHost g;
	g.files["sd:/DOSBox/Games/a.conf"] = "[sdl]\nmapperfile=shared.map\n";
	g.files["sd:/DOSBox/Games/shared.map"] = "x\n";
	g.files["sd:/DOSBox/Games/b.conf"] = "[sdl]\nmapperfile=mapper-0.74.map\n";
	g.files["sd:/DOSBox/Games/mapper-0.74.map"] = "y\n";
	CHECK(Game_Delete(g, "sd:/DOSBox/Games/a.conf", "shared.map", "").ok);
	CHECK(Game_Delete(g, "sd:/DOSBox/Games/b.conf", "mapper-0.74.map", "").ok);
	CHECK(g.files.count("sd:/DOSBox/Games/shared.map") == 1);
	CHECK(g.files.count("sd:/DOSBox/Games/mapper-0.74.map") == 1);

	// the map's name is compared the way a card does, ignoring case
	FakeHost c;
	c.files["sd:/DOSBox/Games/Doom.conf"] = "x\n";
	c.files["sd:/DOSBox/Games/Doom.map"] = "x\n";
	CHECK(Game_Delete(c, "sd:/DOSBox/Games/Doom.conf", "DOOM.MAP", "Doom").ok);
	CHECK_N(c.erased.size(), 2);								// the map is asked for as the conf wrote it (a card finds it either way)
	CHECK_EQ(c.erased[1], "sd:/DOSBox/Games/DOOM.MAP");

	// no title given: it is named by its file; a conf with no map just goes
	FakeHost n;
	n.files["sd:/DOSBox/Games/plain.conf"] = "[cpu]\ncycles=max\n";
	r = Game_Delete(n, "sd:/DOSBox/Games/plain.conf", "", "");
	CHECK(r.ok);
	CHECK_EQ(r.message, "Deleted plain.");
	CHECK_N(n.erased.size(), 1);

	// a conf that cannot be deleted leaves its map alone; one that is not there is not a success (Rescan clears it from the list)
	FakeHost f;
	f.files["sd:/DOSBox/Games/k.conf"] = "[sdl]\nmapperfile=k.map\n";
	f.files["sd:/DOSBox/Games/k.map"] = "z\n";
	f.eraseFails = true;
	CHECK(!Game_Delete(f, "sd:/DOSBox/Games/k.conf", "k.map", "K").ok);
	CHECK(f.files.count("sd:/DOSBox/Games/k.map") == 1);
	r = Game_Delete(n, "sd:/DOSBox/Games/gone.conf", "", "");
	CHECK(!r.ok && has(r.message, "Rescan"));
}

static bool onDisk(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
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

	// the folder is not there yet: the write makes it, and the file is the whole of what was asked
	const std::string file = base + "/Games/x.conf";
	CHECK(!onDisk(base + "/Games"));
	CHECK(h.write(file, "one\n"));
	CHECK_EQ(readFile(file), "one\n");
	CHECK(onDisk(base + "/Games"));

	// written again, in place: the old text is gone, and nothing else is made beside it
	CHECK(h.write(file, "two\n"));
	CHECK_EQ(readFile(file), "two\n");
	CHECK(!onDisk(file + ".tmp"));
	CHECK(!onDisk(file + ".bak"));
	CHECK(h.write(file, "a\n"));						// shorter than before: none of the old text is left
	CHECK_EQ(readFile(file), "a\n");
	CHECK(h.write(file, ""));							// an empty file is a file
	CHECK(onDisk(file) && readFile(file).empty());

	std::string text;
	CHECK(h.write(file, "two\n"));
	CHECK(h.read(file, text));
	CHECK_EQ(text, "two\n");
	CHECK(!h.read(base + "/none", text));

	// erase: gone afterwards; a file that was not there is not a success
	CHECK(h.write(base + "/Games/y.conf", "y\n"));
	CHECK(h.erase(base + "/Games/y.conf"));
	CHECK(!onDisk(base + "/Games/y.conf"));
	CHECK(!h.erase(base + "/Games/y.conf"));

	// only the folder the file is in is made, not the ones above it
	CHECK(!h.write(base + "/a/b/x.conf", "x"));
	CHECK(!onDisk(base + "/a"));

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
	testControls();
	testFailures();
	testAddFound();
	testAddFoundNames();
	testAddFoundFailures();
	testRename();
	testDelete();
	testFileHost();

	if(failures)
	{
		printf("gamesave tests: %d of %d checks FAILED\n", failures, checks);
		return 1;
	}
	printf("gamesave tests: %d checks passed\n", checks);
	return 0;
}
