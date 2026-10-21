// Host tests for the rows of the Games screen and what A does on them (source/menu/gameslist.cpp).
#include "gameslist.h"
#include <stdio.h>
#include <string>

static int checks = 0, failures = 0;
#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)
#define CHECK_EQ(a, b) do { checks++; const std::string x = (a), y = (b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n   got  [%s]\n   want [%s]\n", __FILE__, __LINE__, #a, x.c_str(), y.c_str()); } } while(0)

#define CHECK_N(a, b) do { checks++; const long x = (long)(a), y = (long)(b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n   got  %ld\n   want %ld\n", __FILE__, __LINE__, #a, x, y); } } while(0)

static GameRecord game(const char * name, const char * title, const char * program, const char * where = "", bool inferred = false)
{
	GameRecord g;
	g.name = name;
	g.title = title;
	g.program = program;
	g.where = where;
	g.programInferred = inferred;
	g.path = std::string("sd:/DOSBox/") + (*where ? std::string(where) + "/" : "") + name + ".conf";
	return g;
}

static std::string text(const GamesList& l, int row)
{
	std::string n, v;
	l.rowText(row, n, v);
	return n + "|" + v;
}

static std::vector<GameRecord> three()
{
	std::vector<GameRecord> g;
	g.push_back(game("keen4", "Commander Keen 4", "KEEN4E.EXE"));
	g.push_back(game("prince", "Prince of Persia", "PRINCE.EXE", "GAMES", true));
	g.push_back(game("my-setup", "my-setup", "", "Setups"));
	return g;
}

static void testRows()
{
	GamesList l;
	// nothing found, nothing running
	CHECK(l.rowCount() == 2);
	CHECK_EQ(text(l, 0), "Plain DOSBox|RUNNING");
	CHECK_EQ(text(l, 1), "No games found|");

	l.setScanning(true);
	CHECK(l.rowCount() == 2);
	CHECK_EQ(text(l, 1), "Looking for games...|");

	l.setScanning(false);
	l.setGames(three());
	CHECK(l.rowCount() == 4);
	CHECK_EQ(text(l, 0), "Plain DOSBox|RUNNING");
	CHECK_EQ(text(l, 1), "Commander Keen 4|KEEN4E.EXE");
	CHECK_EQ(text(l, 2), "Prince of Persia|PRINCE.EXE");
	CHECK_EQ(text(l, 3), "my-setup|");

	// the one running says so, and plain does not
	l.setCurrent("sd:/DOSBox/keen4.conf");
	CHECK_EQ(text(l, 0), "Plain DOSBox|");
	CHECK_EQ(text(l, 1), "Commander Keen 4|RUNNING");
	CHECK_EQ(text(l, 2), "Prince of Persia|PRINCE.EXE");

	// scanning again keeps the games that are known
	l.setScanning(true);
	CHECK(l.rowCount() == 5);
	CHECK_EQ(text(l, 4), "Looking for games...|");

	// out of range
	CHECK_EQ(text(l, -1), "|");
	CHECK_EQ(text(l, 99), "|");

	// long text is cut to fit the list
	std::vector<GameRecord> long1;
	long1.push_back(game("x", "A very long title that cannot possibly fit in the list", "A_LONG_PROGRAM_NAME.EXE"));
	l.setScanning(false);
	l.setCurrent("");
	l.setGames(long1);
	std::string n, v;
	l.rowText(1, n, v);
	CHECK(n.size() <= 34);
	CHECK(n.substr(n.size() - 2) == "..");
	CHECK(v.size() <= 16);

	// a conf named in another way than the scan's full path
	GameRecord setup = game("keen", "keen", "", "Setups");
	GameRecord top = game("keen", "keen", "");
	CHECK(GamesList::sameConf(setup, "sd:/DOSBox/Setups/keen.conf"));
	CHECK(GamesList::sameConf(setup, "SD:/dosbox/setups/KEEN.CONF"));
	CHECK(GamesList::sameConf(setup, "Setups/keen.conf"));
	CHECK(GamesList::sameConf(top, "keen.conf"));
	CHECK(!GamesList::sameConf(setup, "keen.conf"));
	CHECK(!GamesList::sameConf(top, "Setups/keen.conf"));
	CHECK(!GamesList::sameConf(game("mykeen", "k", ""), "keen.conf"));
	CHECK(!GamesList::sameConf(top, ""));
}

static void testLimit()
{
	GamesList l;
	std::vector<GameRecord> many;
	for(int i = 0; i < 10; i++)
	{
		char name[16];
		snprintf(name, sizeof(name), "g%d", i);
		many.push_back(game(name, name, "A.EXE"));
	}
	l.setLimit(4);
	l.setGames(many);
	// plain + 4 games + the "more" row
	CHECK(l.rowCount() == 6);
	CHECK_EQ(text(l, 5), "6 more not shown|");
	CHECK(!l.rowHelp(5).empty());
	CHECK(l.click(5).what == GamesList::Click::NOTHING);

	// exactly at the limit: no more row
	l.setLimit(10);
	CHECK(l.rowCount() == 11);
}

static void testHelp()
{
	GamesList l;
	std::vector<GameRecord> g = three();
	g[0].autoexec.push_back("mount c .");
	g[0].autoexec.push_back("c:");
	g[0].autoexec.push_back("keen4e.exe");
	l.setGames(g);

	CHECK_EQ(l.rowHelp(1), "Runs KEEN4E.EXE  -  keen4.conf  -  autoexec: mount c . | c: | keen4e.exe");
	CHECK_EQ(l.rowHelp(2), "Runs PRINCE.EXE (from autoexec)  -  GAMES/prince.conf");
	CHECK_EQ(l.rowHelp(3), "No program named  -  Setups/my-setup.conf");
	CHECK(l.rowHelp(0).find("dosbox.conf") != std::string::npos);
	CHECK(l.rowHelp(-1).empty());
	CHECK(l.rowHelp(40).empty());

	// the help is bounded
	GameRecord big = game("b", "b", "B.EXE");
	for(int i = 0; i < 6; i++)
		big.autoexec.push_back(std::string(80, 'x'));
	std::vector<GameRecord> bigs(1, big);
	l.setGames(bigs);
	CHECK(l.rowHelp(1).size() <= 300);
	CHECK(l.rowHelp(1).substr(l.rowHelp(1).size() - 2) == "..");
}

static void testClick()
{
	GamesList l;
	l.setGames(three());

	// plain DOSBox is what is running: nothing to restart into
	GamesList::Click c = l.click(0);
	CHECK(c.what == GamesList::Click::MESSAGE);
	c = l.click(0);
	CHECK(c.what == GamesList::Click::MESSAGE);

	// a game: A asks, with the question and what it costs; the screen does the launching
	c = l.click(1);
	CHECK(c.what == GamesList::Click::ASK_LAUNCH);
	CHECK_EQ(c.conf, "sd:/DOSBox/keen4.conf");
	CHECK_EQ(c.message, "Restart into Commander Keen 4?");
	CHECK(c.detail.find("KEEN4E.EXE") != std::string::npos);
	CHECK(c.detail.find("What is running is lost") != std::string::npos);

	// every time, with nothing to undo: moving about changes nothing
	l.rowHelp(2);
	c = l.click(1);
	CHECK(c.what == GamesList::Click::ASK_LAUNCH);
	c = l.click(2);
	CHECK(c.what == GamesList::Click::ASK_LAUNCH);
	CHECK_EQ(c.conf, "sd:/DOSBox/GAMES/prince.conf");

	// a game with no program named still asks
	c = l.click(3);
	CHECK(c.what == GamesList::Click::ASK_LAUNCH);
	CHECK(c.detail.find("saved controls and settings") != std::string::npos);

	// the game that is running can be restarted: that is what Restart does
	l.setCurrent("sd:/DOSBox/keen4.conf");
	c = l.click(1);
	CHECK(c.what == GamesList::Click::ASK_LAUNCH);

	// plain DOSBox is a choice when a game is running; it launches no conf
	c = l.click(0);
	CHECK(c.what == GamesList::Click::ASK_LAUNCH);
	CHECK(c.message.find("plain DOSBox") != std::string::npos);
	CHECK(c.conf.empty());
	CHECK(c.detail.find("dosbox.conf alone") != std::string::npos);

	// rows that are not choices
	l.setScanning(true);
	CHECK(l.click(4).what == GamesList::Click::NOTHING);
	CHECK(l.click(-1).what == GamesList::Click::NOTHING);
	CHECK(l.click(40).what == GamesList::Click::NOTHING);
	GamesList empty;
	CHECK(empty.click(1).what == GamesList::Click::NOTHING);
}

static void testTags()
{
	// the help line says what a game has of its own: its controls and its cycles, when its conf says them
	GameRecord g = game("keen4", "Commander Keen 4", "KEEN4E.EXE");
	g.ownControls = true;
	g.cycles = "fixed 6000";
	GameRecord plain = game("doom", "Doom", "DOOM.EXE");
	GamesList l;
	std::vector<GameRecord> both;
	both.push_back(g);
	both.push_back(plain);
	l.setGames(both);
	CHECK(l.rowHelp(1).find("Runs KEEN4E.EXE  -  its own controls, 6000 cycles  -  ") == 0);
	CHECK(l.rowHelp(2).find("its own controls") == std::string::npos);
	CHECK(l.rowHelp(2).find("cycles") == std::string::npos);
}

static void testSaving()
{
	GamesList l;
	l.setGames(three());
	l.setSaving(true);
	CHECK(l.isSaving());

	// a New game row first, no Plain DOSBox, then the games with what A does to them
	CHECK(l.rowCount() == 4);
	CHECK_EQ(text(l, 0), "New game|Save new");
	CHECK_EQ(text(l, 1), "Commander Keen 4|Replace");
	CHECK_EQ(text(l, 3), "my-setup|Replace");
	CHECK(l.rowHelp(0).find("name you type") != std::string::npos);
	CHECK(l.rowHelp(1).find("Replaces this game") != std::string::npos);

	// the game that is running is the one it saves over
	l.setCurrent("sd:/DOSBox/keen4.conf");
	CHECK_EQ(text(l, 1), "Commander Keen 4|Save here");
	CHECK_EQ(text(l, 2), "Prince of Persia|Replace");
	CHECK(l.rowHelp(1).find("Its startup is kept") != std::string::npos);

	// New game is one press
	GamesList::Click c = l.click(0);
	CHECK(c.what == GamesList::Click::SAVE_NEW);

	// replacing asks, as it overwrites a file; whether it is the running game goes with it
	c = l.click(2);
	CHECK(c.what == GamesList::Click::ASK_REPLACE);
	CHECK_EQ(c.message, "Replace Prince of Persia?");
	CHECK(c.detail.find("overwritten") != std::string::npos);
	CHECK_EQ(c.conf, "sd:/DOSBox/GAMES/prince.conf");
	CHECK(!c.running);
	c = l.click(1);
	CHECK(c.what == GamesList::Click::ASK_REPLACE);
	CHECK(c.running);
	CHECK(c.detail.find("startup is kept") != std::string::npos);

	// the same question every time, whatever was clicked before
	l.click(1);
	l.click(2);
	c = l.click(1);
	CHECK(c.what == GamesList::Click::ASK_REPLACE);

	// nothing found: the New game row is still there to start with
	GamesList e;
	e.setSaving(true);
	CHECK(e.rowCount() == 2);
	CHECK_EQ(text(e, 0), "New game|Save new");
	CHECK(e.rowHelp(1).find("No games yet") != std::string::npos);

	// back to launching
	l.setSaving(false);
	CHECK_EQ(text(l, 0), "Plain DOSBox|");
}

static FoundGame foundGame(const char * folder, const char * program, int more = 0)
{
	FoundGame f;
	f.folder = folder;
	f.title = folder;
	f.programs.push_back(program);
	for(int i = 0; i < more; i++)
		f.programs.push_back("SETUP" + std::to_string(i) + ".EXE");
	return f;
}

static void testFound()
{
	GamesList l;
	l.setGames(three());
	std::vector<FoundGame> f;
	f.push_back(foundGame("WOLF3D", "WOLF3D.EXE", 2));
	f.push_back(foundGame("DOOM", "DOOM.EXE"));
	l.setFound(f);

	// after the games: a header, then one row each
	CHECK(l.rowCount() == 7);
	CHECK_EQ(text(l, 3), "my-setup|");
	CHECK_EQ(text(l, 4), "Found on your card|");
	CHECK_EQ(text(l, 5), "WOLF3D|Add");
	CHECK_EQ(text(l, 6), "DOOM|Add");

	CHECK_EQ(l.rowHelp(5), "Not set up yet. DOSBox/WOLF3D holds WOLF3D.EXE and 2 more. A adds it as a game");
	CHECK_EQ(l.rowHelp(6), "Not set up yet. DOSBox/DOOM holds DOOM.EXE. A adds it as a game");
	CHECK(l.rowHelp(4).find("A on one adds it") != std::string::npos);

	// A asks at once (no second press), and says which folder
	GamesList::Click c = l.click(5);
	CHECK(c.what == GamesList::Click::ADD_FOUND);
	CHECK_EQ(c.found.folder, "WOLF3D");
	CHECK_N(c.found.programs.size(), 3);
	c = l.click(5);
	CHECK(c.what == GamesList::Click::ADD_FOUND);
	// the header does nothing
	CHECK(l.click(4).what == GamesList::Click::NOTHING);

	// a game asks to restart; a found one asks to add
	CHECK(l.click(1).what == GamesList::Click::ASK_LAUNCH);
	CHECK(l.click(5).what == GamesList::Click::ADD_FOUND);

	// with games and nothing else found, nothing changes; with found and no games the list is not "No games found"
	GamesList only;
	only.setFound(f);
	CHECK(only.rowCount() == 4);
	CHECK_EQ(text(only, 1), "Found on your card|");
	CHECK_EQ(text(only, 2), "WOLF3D|Add");

	GamesList none;
	none.setFound(std::vector<FoundGame>());
	CHECK_EQ(text(none, 1), "No games found|");

	// scanning still says so, after them
	l.setScanning(true);
	CHECK(l.rowCount() == 8);
	CHECK_EQ(text(l, 7), "Looking for games...|");
	l.setScanning(false);

	// a long folder name is cut to fit
	std::vector<FoundGame> long1;
	long1.push_back(foundGame("ABCDEFGH", "A.EXE"));
	long1[0].folder = std::string(60, 'X');
	l.setFound(long1);
	std::string n, v;
	l.rowText(5, n, v);
	CHECK_N(n.size(), 34);

	// saving shows none of them: a found folder has nothing running to save
	l.setFound(f);
	l.setSaving(true);
	CHECK(l.rowCount() == 4);
	CHECK_EQ(text(l, 0), "New game|Save new");
	for(int i = 0; i < l.rowCount(); i++)
		CHECK(text(l, i).find("Found on your card") == std::string::npos);
	l.setSaving(false);
	CHECK(l.rowCount() == 7);
}

static void testFoundLimit()
{
	// games and found folders share the rows the screen can hold; the found ones take at most a quarter and keep a header
	std::vector<GameRecord> games;
	for(int i = 0; i < 200; i++)
		games.push_back(game(("g" + std::to_string(i)).c_str(), ("Game " + std::to_string(i)).c_str(), "G.EXE"));
	std::vector<FoundGame> found;
	for(int i = 0; i < 80; i++)
		found.push_back(foundGame(("F" + std::to_string(i)).c_str(), "F.EXE"));

	GamesList l;
	l.setLimit(40);
	l.setGames(games);
	l.setFound(found);

	// Plain + 29 games + 1 more + header + 10 found = 42 rows: games + found + header = the limit
	CHECK(l.rowCount() == 1 + 29 + 1 + 1 + 10);
	CHECK_EQ(text(l, 30), "171 more not shown|");
	CHECK_EQ(text(l, 31), "Found on your card|");
	CHECK_EQ(text(l, 41), "F9|Add");

	// the default limit keeps the whole list inside what the screen holds
	GamesList d;
	d.setGames(games);
	d.setFound(found);
	CHECK(d.rowCount() <= 152);
	d.setScanning(true);
	CHECK(d.rowCount() <= 153);
}

int main()
{
	testRows();
	testLimit();
	testHelp();
	testClick();
	testSaving();
	testTags();
	testFound();
	testFoundLimit();
	if(failures)
	{
		printf("%d of %d checks FAILED\n", failures, checks);
		return 1;
	}
	printf("gameslist tests: %d checks passed\n", checks);
	return 0;
}
