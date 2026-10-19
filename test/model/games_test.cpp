// Host tests for the games scan (source/menu/games.cpp): parsing a conf, working out the program, and finding games
// under the DOSBox folder. No console, no threads: the scanner in gameslive.cpp is a thin wrapper over Games_Scan().
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

template<typename... A> static std::vector<std::string> V(A... a)
{
	return std::vector<std::string>{ std::string(a)... };
}

// ---- Game_InferProgram

static void testInfer()
{
	CHECK_EQ(Game_InferProgram(V("mount c .", "c:", "cd games\\keen4", "keen4e.exe", "exit")), "KEEN4E.EXE");
	CHECK_EQ(Game_InferProgram(V("mount c .", "c:", "cd keen", "keen4e")), "KEEN4E");
	CHECK_EQ(Game_InferProgram(V("@echo off", "mount c .", "c:", "loadfix doom.exe -warp 1")), "DOOM.EXE");
	CHECK_EQ(Game_InferProgram(V("c:", "lh   game.com")), "GAME.COM");
	CHECK_EQ(Game_InferProgram(V("c:", "call start.bat")), "START.BAT");
	CHECK_EQ(Game_InferProgram(V("c:\\games\\prince\\prince.exe megahit")), "PRINCE.EXE");
	CHECK_EQ(Game_InferProgram(V("c:/games/prince/prince.exe")), "PRINCE.EXE");
	CHECK_EQ(Game_InferProgram(V("@keen4e.exe")), "KEEN4E.EXE");
	// built-ins and drive changes alone are not a program
	CHECK_EQ(Game_InferProgram(V("mount c .", "c:", "cd..", "cd\\games", "cls", "echo hello", "exit")), "");
	CHECK(Game_InferProgram(V()).empty());
	CHECK_EQ(Game_InferProgram(V("# a comment", "keen4e.exe")), "KEEN4E.EXE");
	CHECK_EQ(Game_InferProgram(V("keen4e.exe", "# trailing comment")), "KEEN4E.EXE");
	CHECK_EQ(Game_InferProgram(V("keen4e.exe", "", "   ")), "KEEN4E.EXE");
	// the last one wins: the one that was run last
	CHECK_EQ(Game_InferProgram(V("setup.exe", "game.exe")), "GAME.EXE");
	// a prefix with nothing after it is skipped
	CHECK_EQ(Game_InferProgram(V("game.exe", "loadfix")), "GAME.EXE");
}

// ---- Game_ParseConf

static void testParse()
{
	GameRecord g;
	g.name = "keen4";
	Game_ParseConf("# title=Commander Keen 4\n# program=keen4e.exe\n[cpu]\ncycles=fixed 3000\n", g);
	CHECK_EQ(g.title, "Commander Keen 4");
	CHECK_EQ(g.program, "KEEN4E.EXE");
	CHECK(!g.programInferred);
	CHECK(!g.isSetup);
	CHECK(g.autoexec.empty());

	// no title: the name; program from [autoexec]
	GameRecord h;
	h.name = "doom";
	Game_ParseConf("[autoexec]\nmount c .\nc:\ncd doom\ndoom.exe\nexit\n", h);
	CHECK_EQ(h.title, "doom");
	CHECK_EQ(h.program, "DOOM.EXE");
	CHECK(h.programInferred);
	CHECK(h.autoexec.size() == 5);
	CHECK_EQ(h.autoexec[3], "doom.exe");

	// a comment says the program, and wins over the autoexec
	GameRecord i;
	i.name = "x";
	Game_ParseConf("# program = real.exe\n[autoexec]\nother.exe\n", i);
	CHECK_EQ(i.program, "REAL.EXE");
	CHECK(!i.programInferred);

	// spaces and case in the comment; the first title wins
	GameRecord j;
	j.name = "x";
	Game_ParseConf("#   TITLE =  Spaced Out  \n# title=Second\n", j);
	CHECK_EQ(j.title, "Spaced Out");

	// '%' is a comment to DOSBox too; a comment that is not key=value is ignored
	GameRecord k;
	k.name = "x";
	Game_ParseConf("% title=Percent\n# just a note\n", k);
	CHECK_EQ(k.title, "Percent");

	// section names in any case; only [autoexec] is read for lines; a later section ends it
	GameRecord l;
	l.name = "x";
	Game_ParseConf("[AutoExec]\ngame.exe\n[cpu]\ncycles=max\n", l);
	CHECK(l.autoexec.size() == 1);
	CHECK_EQ(l.program, "GAME.EXE");

	// only the first lines are kept, all of them are looked at
	GameRecord m;
	m.name = "x";
	std::string text = "[autoexec]\n";
	for(int n = 0; n < 12; n++)
		text += "echo line\n";
	text += "last.exe\n";
	Game_ParseConf(text, m);
	CHECK(m.autoexec.size() == GAME_AUTOEXEC_LINES);
	CHECK_EQ(m.program, "LAST.EXE");

	// our setups are recognised, and take their program from the comment they carry
	GameRecord s;
	s.name = "keen";
	Game_ParseConf("\n# DOSBox Wii setup\n# program=KEEN4E.EXE\n[sdl]\nmapperfile=keen.map\n", s);
	CHECK(s.isSetup);
	CHECK_EQ(s.program, "KEEN4E.EXE");
	CHECK_EQ(s.title, "keen");

	// CRLF files
	GameRecord c;
	c.name = "x";
	Game_ParseConf("# title=Windows\r\n[autoexec]\r\nkeen.exe\r\n", c);
	CHECK_EQ(c.title, "Windows");
	CHECK_EQ(c.program, "KEEN.EXE");

	// empty and junk are still a conf: no title, no program
	GameRecord e;
	e.name = "empty";
	Game_ParseConf("", e);
	CHECK_EQ(e.title, "empty");
	CHECK(e.program.empty());
	Game_ParseConf("\x01\x02 not a conf [", e);
	CHECK_EQ(e.title, "empty");

	// parsing again starts clean
	GameRecord r;
	r.name = "r";
	Game_ParseConf("# title=One\n[autoexec]\na.exe\n", r);
	Game_ParseConf("# title=Two\n", r);
	CHECK_EQ(r.title, "Two");
	CHECK(r.autoexec.empty());
	CHECK(r.program.empty());
}

// ---- Games_Scan over a fake

class FakeHost : public GamesHost
{
	public:
		std::string base = "sd:/DOSBox";
		std::map<std::string, std::string> files;	// full path -> text
		std::set<std::string> dirs;
		mutable std::vector<std::string> opened;	// folders listed

		void add(const std::string& path, const std::string& text = std::string("# title=T\n")) { files[path] = text; }

		std::string directory() const override { return base; }
		std::vector<std::string> list(const std::string& folder) const override
		{
			opened.push_back(folder);
			std::set<std::string> names;
			const std::string prefix = folder + "/";
			for(std::map<std::string, std::string>::const_iterator it = files.begin(); it != files.end(); ++it)
				if(it->first.compare(0, prefix.size(), prefix) == 0)
					names.insert(it->first.substr(prefix.size(), it->first.find('/', prefix.size()) == std::string::npos ? std::string::npos : it->first.find('/', prefix.size()) - prefix.size()));
			for(std::set<std::string>::const_iterator it = dirs.begin(); it != dirs.end(); ++it)
				if(it->compare(0, prefix.size(), prefix) == 0 && it->find('/', prefix.size()) == std::string::npos)
					names.insert(it->substr(prefix.size()));
			return std::vector<std::string>(names.begin(), names.end());
		}
		bool isDirectory(const std::string& path) const override
		{
			if(dirs.count(path))
				return true;
			const std::string prefix = path + "/";
			for(std::map<std::string, std::string>::const_iterator it = files.begin(); it != files.end(); ++it)
				if(it->first.compare(0, prefix.size(), prefix) == 0)
					return true;
			return false;
		}
		bool read(const std::string& path, std::string& text, size_t maxBytes) const override
		{
			std::map<std::string, std::string>::const_iterator it = files.find(path);
			if(it == files.end())
				return false;
			text = it->second.substr(0, maxBytes);
			return true;
		}
};

static std::string names(const std::vector<GameRecord>& g)
{
	std::string s;
	for(size_t i = 0; i < g.size(); i++)
		s += (i ? "," : "") + g[i].path.substr(11);	// without "sd:/DOSBox/"
	return s;
}

static void testScan()
{
	// where it looks
	FakeHost h;
	h.add("sd:/DOSBox/dosbox.conf", "[cpu]\n");
	h.add("sd:/DOSBox/DOSBOX.CONF", "[cpu]\n");
	h.add("sd:/DOSBox/keen4.conf", "# title=Keen 4\n");
	h.add("sd:/DOSBox/doom.CONF", "# title=Doom\n");
	h.add("sd:/DOSBox/readme.txt");
	h.add("sd:/DOSBox/doom.conf.bak");
	h.add("sd:/DOSBox/mapper-SVN-r4301.map");
	h.add("sd:/DOSBox/._hidden.conf");
	h.add("sd:/DOSBox/Setups/wolf.conf", "# DOSBox Wii setup\n# program=WOLF3D.EXE\n");
	h.add("sd:/DOSBox/Setups/wolf.map");
	h.add("sd:/DOSBox/GAMES/prince.conf", "# title=Prince of Persia\n");
	h.add("sd:/DOSBox/GAMES/PRINCE/deep.conf", "# title=Too deep\n");
	h.add("sd:/DOSBox/.Trash/old.conf", "# title=Hidden folder\n");
	std::vector<GameRecord> g = Games_Scan(h);
	CHECK_EQ(names(g), "doom.CONF,keen4.conf,GAMES/prince.conf,Setups/wolf.conf");

	// the record
	for(size_t i = 0; i < g.size(); i++)
	{
		if(g[i].path == "sd:/DOSBox/Setups/wolf.conf")
		{
			CHECK(g[i].isSetup);
			CHECK_EQ(g[i].where, "Setups");
			CHECK_EQ(g[i].name, "wolf");
			CHECK_EQ(g[i].program, "WOLF3D.EXE");
		}
		if(g[i].path == "sd:/DOSBox/doom.CONF")
		{
			CHECK_EQ(g[i].where, "");
			CHECK_EQ(g[i].name, "doom");
			CHECK_EQ(g[i].title, "Doom");
		}
	}

	// the folders it opened: the base once, then each folder below once; nothing deeper
	CHECK(h.opened.size() == 3);
	CHECK(h.opened[0] == "sd:/DOSBox");

	// no storage: nothing, and nothing opened
	FakeHost none;
	none.base.clear();
	CHECK(Games_Scan(none).empty());
	CHECK(none.opened.empty());

	// an empty folder
	FakeHost empty;
	CHECK(Games_Scan(empty).empty());

	// sorted by title ignoring case, then path
	FakeHost s;
	s.add("sd:/DOSBox/b.conf", "# title=banana\n");
	s.add("sd:/DOSBox/a.conf", "# title=Cherry\n");
	s.add("sd:/DOSBox/c.conf", "# title=Apple\n");
	s.add("sd:/DOSBox/Setups/d.conf", "# title=apple\n");
	CHECK_EQ(names(Games_Scan(s)), "c.conf,Setups/d.conf,b.conf,a.conf");

	// a file that cannot be read is left out, not fatal
	class Unreadable : public FakeHost
	{
		public:
			bool read(const std::string& path, std::string& text, size_t maxBytes) const override
			{
				return path.find("bad") == std::string::npos && FakeHost::read(path, text, maxBytes);
			}
	} u;
	u.add("sd:/DOSBox/bad.conf");
	u.add("sd:/DOSBox/good.conf");
	CHECK_EQ(names(Games_Scan(u)), "good.conf");

	// "Setups" in another case is still the setups folder, and is read first
	FakeHost c;
	c.add("sd:/DOSBox/SETUPS/z.conf", "# title=Z\n");
	c.add("sd:/DOSBox/GAMES/y.conf", "# title=Y\n");
	g = Games_Scan(c);
	CHECK(g.size() == 2);
	CHECK_EQ(g[0].where, "GAMES");
	CHECK_EQ(g[1].where, "SETUPS");

	// the caps: games, and folders opened
	FakeHost many;
	for(int i = 0; i < GAMES_MAX + 50; i++)
	{
		char path[64];
		snprintf(path, sizeof(path), "sd:/DOSBox/g%04d.conf", i);
		many.add(path);
	}
	CHECK(Games_Scan(many).size() == GAMES_MAX);

	FakeHost folders;
	for(int i = 0; i < GAMES_MAX_FOLDERS + 20; i++)
	{
		char path[64];
		snprintf(path, sizeof(path), "sd:/DOSBox/d%03d/x.conf", i);
		folders.add(path);
	}
	g = Games_Scan(folders);
	CHECK(g.size() == GAMES_MAX_FOLDERS);
	CHECK(folders.opened.size() == 1 + GAMES_MAX_FOLDERS);

	// a conf bigger than the cap is read as far as the cap: its start still gives the title
	FakeHost big;
	big.add("sd:/DOSBox/big.conf", "# title=Big\n" + std::string(200 * 1024, '#'));
	g = Games_Scan(big);
	CHECK(g.size() == 1);
	CHECK_EQ(g[0].title, "Big");
}

// ---- FileGamesHost on a real folder

static void writeFile(const std::string& path, const std::string& text)
{
	FILE * f = fopen(path.c_str(), "wb");
	if(f)
	{
		fwrite(text.data(), 1, text.size(), f);
		fclose(f);
	}
}

static void testFiles()
{
	char tmpl[] = "/tmp/games_test_XXXXXX";
	char * dir = mkdtemp(tmpl);
	CHECK(dir != nullptr);
	if(!dir)
		return;
	const std::string base = dir;
	mkdir((base + "/Setups").c_str(), 0755);
	mkdir((base + "/GAMES").c_str(), 0755);
	writeFile(base + "/dosbox.conf", "[cpu]\n");
	writeFile(base + "/keen.conf", "# title=Keen\n[autoexec]\nkeen4e.exe\n");
	writeFile(base + "/Setups/s.conf", "# DOSBox Wii setup\n[sdl]\nmapperfile=s.map\n");
	writeFile(base + "/GAMES/p.conf", "# title=Prince\n");

	FileGamesHost host(base);
	CHECK(host.isDirectory(base + "/Setups"));
	CHECK(!host.isDirectory(base + "/keen.conf"));
	CHECK(!host.isDirectory(base + "/nothing"));

	std::string text;
	CHECK(host.read(base + "/keen.conf", text, 1000));
	CHECK(text.find("keen4e.exe") != std::string::npos);
	CHECK(host.read(base + "/keen.conf", text, 5));
	CHECK(text.size() == 5);
	CHECK(!host.read(base + "/nothing.conf", text, 1000));
	CHECK(host.list(base + "/nothing").empty());

	const std::vector<GameRecord> g = Games_Scan(host);
	CHECK(g.size() == 3);
	if(g.size() == 3)
	{
		CHECK_EQ(g[0].title, "Keen");
		CHECK_EQ(g[0].program, "KEEN4E.EXE");
		CHECK_EQ(g[1].title, "Prince");
		CHECK_EQ(g[1].where, "GAMES");
		CHECK_EQ(g[2].title, "s");
		CHECK(g[2].isSetup);
	}

	// a folder that is not there: nothing
	FileGamesHost gone(base + "/missing");
	CHECK(Games_Scan(gone).empty());

	std::string cmd = "rm -rf '" + base + "'";
	if(system(cmd.c_str()) != 0)
		printf("note: could not remove %s\n", base.c_str());
}

int main()
{
	testInfer();
	testParse();
	testScan();
	testFiles();
	if(failures)
	{
		printf("%d of %d checks FAILED\n", failures, checks);
		return 1;
	}
	printf("games tests: %d checks passed\n", checks);
	return 0;
}
