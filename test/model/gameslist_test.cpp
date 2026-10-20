// Host tests for the rows of the Games screen and what A does on them (source/menu/gameslist.cpp).
#include "gameslist.h"
#include <stdio.h>
#include <string>

static int checks = 0, failures = 0;
#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)
#define CHECK_EQ(a, b) do { checks++; const std::string x = (a), y = (b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n   got  [%s]\n   want [%s]\n", __FILE__, __LINE__, #a, x.c_str(), y.c_str()); } } while(0)

static GameRecord game(const char * name, const char * title, const char * program, const char * where = "", bool setup = false, bool inferred = false)
{
	GameRecord g;
	g.name = name;
	g.title = title;
	g.program = program;
	g.where = where;
	g.isSetup = setup;
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
	g.push_back(game("prince", "Prince of Persia", "PRINCE.EXE", "GAMES", false, true));
	g.push_back(game("my-setup", "my-setup", "", "Setups", true));
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
	CHECK_EQ(text(l, 3), "my-setup|Controls only");

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
	GameRecord setup = game("keen", "keen", "", "Setups", true);
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
	CHECK_EQ(l.rowHelp(3), "No program named  -  Setups/my-setup.conf  -  controls setup");
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

	// a game: first A says what it costs, second launches it
	c = l.click(1);
	CHECK(c.what == GamesList::Click::MESSAGE);
	CHECK(c.message.find("Press A again") != std::string::npos);
	CHECK(c.message.find("Commander Keen 4") != std::string::npos);
	c = l.click(1);
	CHECK(c.what == GamesList::Click::LAUNCH);
	CHECK_EQ(c.conf, "sd:/DOSBox/keen4.conf");

	// and it is asked again next time
	c = l.click(1);
	CHECK(c.what == GamesList::Click::MESSAGE);

	// another row starts over
	c = l.click(2);
	CHECK(c.what == GamesList::Click::MESSAGE);
	c = l.click(1);
	CHECK(c.what == GamesList::Click::MESSAGE);
	c = l.click(2);
	CHECK(c.what == GamesList::Click::MESSAGE);
	c = l.click(2);
	CHECK(c.what == GamesList::Click::LAUNCH);
	CHECK_EQ(c.conf, "sd:/DOSBox/GAMES/prince.conf");

	// moving the highlight undoes the first press, even back onto the same row
	c = l.click(1);
	CHECK(c.what == GamesList::Click::MESSAGE);
	l.rowHelp(2);
	l.rowHelp(1);
	c = l.click(1);
	CHECK(c.what == GamesList::Click::MESSAGE);

	// the game that is running can be restarted: that is what Restart does
	l.setCurrent("sd:/DOSBox/keen4.conf");
	c = l.click(1);
	CHECK(c.what == GamesList::Click::MESSAGE);
	c = l.click(1);
	CHECK(c.what == GamesList::Click::LAUNCH);

	// plain DOSBox is a choice when a game is running; it launches no conf
	c = l.click(0);
	CHECK(c.what == GamesList::Click::MESSAGE);
	CHECK(c.message.find("plain DOSBox") != std::string::npos);
	c = l.click(0);
	CHECK(c.what == GamesList::Click::LAUNCH);
	CHECK(c.conf.empty());

	// a change of what is shown starts over too
	c = l.click(1);
	l.setScanning(true);
	c = l.click(1);
	CHECK(c.what == GamesList::Click::MESSAGE);

	// rows that are not choices
	l.setScanning(true);
	CHECK(l.click(4).what == GamesList::Click::NOTHING);
	CHECK(l.click(-1).what == GamesList::Click::NOTHING);
	CHECK(l.click(40).what == GamesList::Click::NOTHING);
	GamesList empty;
	CHECK(empty.click(1).what == GamesList::Click::NOTHING);
}

int main()
{
	testRows();
	testLimit();
	testHelp();
	testClick();
	if(failures)
	{
		printf("%d of %d checks FAILED\n", failures, checks);
		return 1;
	}
	printf("gameslist tests: %d checks passed\n", checks);
	return 0;
}
