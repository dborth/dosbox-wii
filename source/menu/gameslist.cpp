/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * gameslist.cpp
 ***************************************************************************/
#include "gameslist.h"
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
	// typed (config -r -gameconf Setups/keen.conf) relative to the DOSBox folder: where the game is below it, and its file name
	const std::string relative = (g.where.empty() ? std::string() : g.where + "/") + g.name + ".conf";
	return strcasecmp(relative.c_str(), cur.c_str()) == 0;
}

void GamesList::setGames(const std::vector<GameRecord>& found)
{
	games = found;
	armed = -1;
}

std::vector<GamesList::Row> GamesList::rows() const
{
	std::vector<Row> r;
	Row plain = { PLAIN, -1 };
	r.push_back(plain);

	const size_t shown = games.size() < limit ? games.size() : limit;
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
	if(scanning)
	{
		Row s = { SCANNING, -1 };
		r.push_back(s);
	}
	else if(games.empty())
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
		case GAME:
		{
			const GameRecord& g = games[(size_t)r[(size_t)row].game];
			name = fit(g.title, kNameChars);
			if(isCurrent(g))
				value = "RUNNING";
			else if(!g.program.empty())
				value = fit(g.program, kValueChars);
			else if(g.isSetup)
				value = "Controls only";
			break;
		}
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

	if(g.program.empty())
		parts.push_back("No program named");
	else
		parts.push_back("Runs " + g.program + (g.programInferred ? " (from autoexec)" : ""));

	parts.push_back((g.where.empty() ? std::string() : g.where + "/") + g.name + ".conf");
	if(g.isSetup)
		parts.push_back("controls setup");
	if(!g.autoexec.empty())
		parts.push_back("autoexec: " + join(g.autoexec, " | "));

	std::string text = join(parts, "  -  ");
	return fit(text, kHelpChars);
}

std::string GamesList::rowHelp(int row)
{
	armed = -1;
	const std::vector<Row> r = rows();
	if(row < 0 || (size_t)row >= r.size())
		return std::string();

	switch(r[(size_t)row].kind)
	{
		case PLAIN:
			return "dosbox.conf alone, with no game on top of it. A restarts DOSBox";
		case GAME:
			return gameHelp(games[(size_t)r[(size_t)row].game]);
		case MORE:
			return "Only the first ones are listed. Keep fewer confs in the DOSBox folder, or put them in a folder inside it";
		case SCANNING:
			return "Reading the DOSBox folder";
		case NONE_FOUND:
			return "Put a .conf in the DOSBox folder, or a folder inside it, and press Rescan. Add a # title= line to name it";
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
				armed = -1;
				return c;
			}
			name = "plain DOSBox";
			break;
		case GAME:
		{
			const GameRecord& g = games[(size_t)chosen.game];
			name = g.title;
			conf = g.path;
			break;
		}
		default:
			// a row that says something, not a choice
			return c;
	}

	if(armed != row)
	{
		armed = row;
		c.what = Click::MESSAGE;
		c.message = "Press A again to restart into " + fit(name, 40) + ". Progress in what is running is lost";
		return c;
	}

	armed = -1;
	c.what = Click::LAUNCH;
	c.conf = conf;
	return c;
}
