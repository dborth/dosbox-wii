/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * gameslist.cpp
 ***************************************************************************/
#include "gameslist.h"
#include <cctype>
#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace {

// What fits the list: it is 552 design pixels wide, name at the left, value at the right
const size_t kNameChars = 34;
const size_t kValueChars = 16;
// The help line scrolls, but a buffer holds it
const size_t kHelpChars = 300;

std::string fit(const std::string& s, size_t max)
{
	if(s.size() <= max)
		return s;
	return s.substr(0, max - 2) + "..";
}

std::string join(const std::vector<std::string>& parts, const char * sep)
{
	std::string s;
	for(size_t i = 0; i < parts.size(); i++)
		s += (i ? sep : "") + parts[i];
	return s;
}

} // namespace

bool GamesList::sameConf(const GameRecord& g, const std::string& cur)
{
	if(cur.empty())
		return false;
	if(strcasecmp(g.path.c_str(), cur.c_str()) == 0)
		return true;
	// typed (config -r -gameconf Games/keen.conf) relative to the DOSBox folder: where the game is below it, and its file name
	const std::string relative = (g.where.empty() ? std::string() : g.where + "/") + g.name + ".conf";
	return strcasecmp(relative.c_str(), cur.c_str()) == 0;
}

void GamesList::setGames(const std::vector<GameRecord>& found)
{
	games = found;
}

void GamesList::setFound(const std::vector<FoundGame>& folders)
{
	found = folders;
}

std::vector<GamesList::Row> GamesList::rows() const
{
	std::vector<Row> r;
	Row first = { saving ? NEW_GAME : PLAIN, -1 };
	r.push_back(first);

	// The rows for games and found folders share `limit`; the found ones are at most a quarter of it and take a header too
	size_t foundShown = saving ? 0 : found.size();
	if(foundShown > limit / 4)
		foundShown = limit / 4;
	const size_t reserved = foundShown ? foundShown + 1 : 0;
	const size_t room = limit > reserved ? limit - reserved : 0;

	const size_t shown = games.size() < room ? games.size() : room;
	for(size_t i = 0; i < shown; i++)
	{
		Row g = { GAME, (int)i };
		r.push_back(g);
	}
	if(games.size() > shown)
	{
		Row more = { MORE, (int)(games.size() - shown) };
		r.push_back(more);
	}
	if(foundShown)
	{
		Row header = { FOUND_HEADER, -1 };
		r.push_back(header);
		for(size_t i = 0; i < foundShown; i++)
		{
			Row f = { FOUND, (int)i };
			r.push_back(f);
		}
	}
	if(scanning)
	{
		Row s = { SCANNING, -1 };
		r.push_back(s);
	}
	else if(games.empty() && !foundShown)
	{
		Row none = { NONE_FOUND, -1 };
		r.push_back(none);
	}
	return r;
}

int GamesList::rowCount() const
{
	return (int)rows().size();
}

void GamesList::card(int row, Card& out) const
{
	out = Card();
	const std::vector<Row> r = rows();
	if(row < 0 || (size_t)row >= r.size())
		return;

	switch(r[(size_t)row].kind)
	{
		case PLAIN:
			out.tile = "D";
			out.title = "Plain DOSBox";
			out.detail = "dosbox.conf only, no game's settings";
			out.accent = current.empty();
			out.action = current.empty() ? "RUNNING" : "Load";
			break;
		case NEW_GAME:
			out.kind = Card::FOUND;
			out.tile = "+";
			out.title = "New game";
			out.detail = "Save what is running now as a game";
			out.accent = true;
			out.action = "Save new";
			break;
		case GAME:
		{
			const GameRecord& g = games[(size_t)r[(size_t)row].game];
			const unsigned char first = g.title.empty() ? 0 : (unsigned char)g.title[0];
			out.tile = std::string(1, (first < 128 && isalnum(first)) ? (char)toupper(first) : '#');
			out.title = fit(g.title, 34);
			std::vector<std::string> parts;
			if(!g.program.empty())
				parts.push_back(g.program);
			if(g.ownControls)
				parts.push_back("own controls");
			const std::string cycles = Game_CyclesTag(g.cycles);
			if(!cycles.empty())
				parts.push_back(cycles);
			out.detail = fit(join(parts, "  -  "), 52);
			out.accent = isCurrent(g);
			if(saving)
				out.action = out.accent ? "Save here" : "Replace";
			else
				out.action = out.accent ? "RUNNING" : "Load";
			break;
		}
		case FOUND_HEADER:
			out.kind = Card::HEADER;
			out.title = "FOUND ON YOUR CARD";
			break;
		case FOUND:
		{
			const FoundGame& f = found[(size_t)r[(size_t)row].game];
			out.kind = Card::FOUND;
			out.tile = "+";
			out.title = fit(f.title.empty() ? f.folder : f.title, 34);
			out.detail = fit(f.folder + (f.programs.empty() ? std::string() : "  -  " + f.programs[0]), 52);
			out.action = "Add";
			break;
		}
		case MORE:
			out.kind = Card::NOTE;
			out.title = std::to_string(r[(size_t)row].game) + " more not shown";
			break;
		case SCANNING:
			out.kind = Card::NOTE;
			out.title = "Looking for games...";
			break;
		case NONE_FOUND:
			out.kind = Card::NOTE;
			out.title = "No games found";
			break;
	}
}

void GamesList::rowText(int row, std::string& name, std::string& value) const
{
	name.clear();
	value.clear();
	const std::vector<Row> r = rows();
	if(row < 0 || (size_t)row >= r.size())
		return;

	switch(r[(size_t)row].kind)
	{
		case PLAIN:
			name = "Plain DOSBox";
			value = current.empty() ? "RUNNING" : "";
			break;
		case NEW_GAME:
			name = "New game";
			value = "Save new";
			break;
		case GAME:
		{
			const GameRecord& g = games[(size_t)r[(size_t)row].game];
			name = fit(g.title, kNameChars);
			if(saving)
				value = isCurrent(g) ? "Save here" : "Replace";
			else if(isCurrent(g))
				value = "RUNNING";
			else if(!g.program.empty())
				value = fit(g.program, kValueChars);
			break;
		}
		case FOUND_HEADER:
			name = "Found on your card";
			break;
		case FOUND:
			name = fit(found[(size_t)r[(size_t)row].game].folder, kNameChars);
			value = "Add";
			break;
		case MORE:
			name = fit(std::to_string(r[(size_t)row].game) + " more not shown", kNameChars);
			break;
		case SCANNING:
			name = "Looking for games...";
			break;
		case NONE_FOUND:
			name = "No games found";
			break;
	}
}

std::string GamesList::gameHelp(const GameRecord& g) const
{
	std::vector<std::string> parts;

	if(saving)
		parts.push_back(isCurrent(g) ? "Saves what is running over this game, which started it. Its startup is kept"
			: "Replaces this game with what is running: its controls, settings and startup");
	else if(g.program.empty())
		parts.push_back("No program named");
	else
		parts.push_back("Runs " + g.program + (g.programInferred ? " (from autoexec)" : ""));

	std::vector<std::string> tags;
	if(g.ownControls)
		tags.push_back("its own controls");
	const std::string cycles = Game_CyclesTag(g.cycles);
	if(!cycles.empty())
		tags.push_back(cycles);
	if(!tags.empty())
		parts.push_back(join(tags, ", "));

	parts.push_back((g.where.empty() ? std::string() : g.where + "/") + g.name + ".conf");
	if(!g.autoexec.empty())
		parts.push_back("autoexec: " + join(g.autoexec, " | "));

	std::string text = join(parts, "  -  ");
	return fit(text, kHelpChars);
}

std::string GamesList::foundHelp(const FoundGame& f) const
{
	std::string text = "Not set up yet. DOSBox/" + f.folder + " holds " + f.programs[0];
	if(f.programs.size() > 1)
		text += " and " + std::to_string(f.programs.size() - 1) + " more";
	return fit(text + ". A adds it as a game", kHelpChars);
}

std::string GamesList::rowHelp(int row)
{
	const std::vector<Row> r = rows();
	if(row < 0 || (size_t)row >= r.size())
		return std::string();

	switch(r[(size_t)row].kind)
	{
		case PLAIN:
			return "dosbox.conf alone, with no game on top of it. A loads it, which restarts DOSBox";
		case NEW_GAME:
			return "Saves what is running under a name you type: its controls, settings and what starts it. dosbox.conf is not changed";
		case GAME:
			return gameHelp(games[(size_t)r[(size_t)row].game]);
		case FOUND_HEADER:
			return "Folders in the DOSBox folder that hold a program but are not a game yet. A on one adds it";
		case FOUND:
			return foundHelp(found[(size_t)r[(size_t)row].game]);
		case MORE:
			return "Only the first ones are listed. Keep fewer confs in the DOSBox folder, or put them in a folder inside it";
		case SCANNING:
			return "Reading the DOSBox folder";
		case NONE_FOUND:
			return saving ? "No games yet. New game saves what is running as the first"
				: "No games yet. Save one from the Save tab, or put a .conf in the DOSBox folder or a folder inside it. A # title= line names it";
	}
	return std::string();
}

GamesList::Click GamesList::click(int row)
{
	Click c;
	const std::vector<Row> r = rows();
	if(row < 0 || (size_t)row >= r.size())
		return c;

	const Row& chosen = r[(size_t)row];
	std::string name, conf;

	switch(chosen.kind)
	{
		case PLAIN:
			if(current.empty())
			{
				c.what = Click::MESSAGE;
				c.message = "Plain DOSBox is what is running";
				return c;
			}
			name = "plain DOSBox";
			break;
		case NEW_GAME:
			c.what = Click::SAVE_NEW;
			return c;
		case GAME:
		{
			const GameRecord& g = games[(size_t)chosen.game];
			name = g.title;
			conf = g.path;
			break;
		}
		case FOUND:
			c.what = Click::ADD_FOUND;
			c.found = found[(size_t)chosen.game];
			return c;
		default:
			// a row that says something, not a choice
			return c;
	}

	// launching restarts DOSBox and replacing overwrites a game: the screen asks first, in a popover, with this as the question
	if(saving)
	{
		c.what = Click::ASK_REPLACE;
		c.conf = conf;
		c.running = isCurrent(games[(size_t)chosen.game]);
		c.message = "Replace " + fit(name, 24) + "?";
		c.detail = c.running ? "Saves what is running over it, the game that started it. Its startup is kept."
			: "Its saved controls, settings and startup are overwritten with what is running.";
		return c;
	}
	c.what = Click::ASK_LAUNCH;
	c.conf = conf;
	c.message = "Load " + fit(name, 20) + "?";
	if(conf.empty())
		c.detail = "DOSBox restarts with dosbox.conf alone, with no game's controls or settings. What is running is lost.";
	else
	{
		const GameRecord& g = games[(size_t)chosen.game];
		const std::string& program = g.program;
		c.game = true;
		c.title = g.title;
		c.running = isCurrent(g);
		if(c.running)
			c.message = "Restart " + fit(name, 20) + "?";	// it is already loaded: all that is left is to start it again
		c.detail = (program.empty() ? std::string("DOSBox restarts with its saved controls and settings.")
			: "DOSBox restarts and runs " + program + " with its saved controls and settings.") + " What is running is lost.";
	}
	return c;
}

std::string GamesList_Fit(const std::string& s, size_t max)
{
	return fit(s, max);
}
