/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * games.cpp
 ***************************************************************************/
#include "games.h"
#include <algorithm>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

namespace {

// Enough for any conf DOSBox writes (a full dosbox.conf with its comments is about 15 KB)
const size_t kMaxConfBytes = 64 * 1024;

std::string trim(const std::string& s)
{
	const size_t first = s.find_first_not_of(" \t\r\n");
	if(first == std::string::npos)
		return std::string();
	return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

std::string lower(std::string s)
{
	for(size_t i = 0; i < s.size(); i++)
		s[i] = (char)tolower((unsigned char)s[i]);
	return s;
}

std::string upper(std::string s)
{
	for(size_t i = 0; i < s.size(); i++)
		s[i] = (char)toupper((unsigned char)s[i]);
	return s;
}

bool endsWithNoCase(const std::string& s, const char * suffix)
{
	const size_t n = strlen(suffix);
	return s.size() >= n && strcasecmp(s.c_str() + s.size() - n, suffix) == 0;
}

std::vector<std::string> lines(const std::string& text)
{
	std::vector<std::string> out;
	size_t start = 0;
	while(start <= text.size())
	{
		size_t end = text.find('\n', start);
		if(end == std::string::npos)
			end = text.size();
		out.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	return out;
}

// DOSBox's comments (Section_prop / Section_line: '#' and '%')
bool isComment(const std::string& line)
{
	return !line.empty() && (line[0] == '#' || line[0] == '%');
}

// The first word of a line, as typed
std::string firstWord(const std::string& line, std::string * rest = nullptr)
{
	const size_t end = line.find_first_of(" \t");
	if(rest)
		*rest = end == std::string::npos ? std::string() : trim(line.substr(end));
	return end == std::string::npos ? line : line.substr(0, end);
}

// Commands that are not the program: DOS built-ins and the shell's own, which an autoexec uses to set the scene
bool isBuiltIn(const std::string& word)
{
	static const char * const names[] = {
		"exit", "mount", "imgmount", "cd", "chdir", "cls", "echo", "set", "path", "rem", "keyb", "mode", "config",
		"ver", "pause", "boot", "mem", "dir", "type", "del", "erase", "copy", "ren", "rename", "md", "mkdir", "rd",
		"rmdir", "goto", "if", "for", "choice", "shift", "date", "time", "vol", "label", "subst", "attrib", "tree",
		"deltree", "move", "more", "append", "assign", "intro", "help", "loadfix", "lh", "loadhigh", "call", nullptr
	};
	const std::string w = lower(word);

	// "cd..", "cd\games": the command with no space before its argument
	if(w.size() > 2 && w.compare(0, 2, "cd") == 0 && (w[2] == '.' || w[2] == '\\' || w[2] == '/'))
		return true;
	// "x:" a drive change
	if(w.size() == 2 && isalpha((unsigned char)w[0]) && w[1] == ':')
		return true;

	for(int i = 0; names[i]; i++)
		if(w == names[i])
			return true;
	return false;
}

bool isPrefixCommand(const std::string& word)
{
	const std::string w = lower(word);
	return w == "loadfix" || w == "lh" || w == "loadhigh" || w == "call";
}

} // namespace

std::string Game_InferProgram(const std::vector<std::string>& autoexec)
{
	for(size_t i = autoexec.size(); i-- > 0;)
	{
		std::string line = trim(autoexec[i]);
		if(line.empty() || isComment(line))
			continue;
		if(line[0] == '@')
			line = trim(line.substr(1));

		std::string rest;
		std::string word = firstWord(line, &rest);

		// "loadfix keen4e.exe", "lh game": the program is the word after
		while(isPrefixCommand(word) && !rest.empty())
		{
			line = rest;
			word = firstWord(line, &rest);
		}

		if(word.empty() || isBuiltIn(word))
			continue;

		// path and extension as typed; the file name alone
		const size_t sep = word.find_last_of("\\/");
		if(sep != std::string::npos)
			word = word.substr(sep + 1);
		if(word.empty())
			continue;
		return upper(word);
	}
	return std::string();
}

void Game_ParseConf(const std::string& text, GameRecord& out)
{
	const std::vector<std::string> all = lines(text);

	out.title.clear();
	out.program.clear();
	out.programInferred = false;
	out.autoexec.clear();

	std::string section;
	std::vector<std::string> autoexec;	// all of it, for working out the program
	for(size_t i = 0; i < all.size(); i++)
	{
		const std::string line = trim(all[i]);
		if(line.empty())
			continue;

		if(isComment(line))
		{
			// "# key=value", with any space around them
			const size_t eq = line.find('=');
			if(eq == std::string::npos)
				continue;
			const std::string key = lower(trim(line.substr(1, eq - 1)));
			const std::string value = trim(line.substr(eq + 1));
			if(key == "title" && out.title.empty())
				out.title = value;
			else if(key == "program" && out.program.empty())
				out.program = upper(value);
			continue;
		}

		if(line[0] == '[')
		{
			const size_t close = line.find(']');
			section = lower(line.substr(1, close == std::string::npos ? std::string::npos : close - 1));
			continue;
		}

		if(section == "autoexec")
		{
			autoexec.push_back(line);
			if(out.autoexec.size() < GAME_AUTOEXEC_LINES)
				out.autoexec.push_back(line);
		}
	}

	if(out.title.empty())
		out.title = out.name;

	if(out.program.empty())
	{
		out.program = Game_InferProgram(autoexec);
		out.programInferred = !out.program.empty();
	}
}

// ---- the scan

namespace {

bool isConfName(const std::string& name)
{
	// ".conf" files only, not dosbox.conf (the base the games are layered over), nor a hidden one (macOS "._x.conf")
	return name.size() > 5 && name[0] != '.' && endsWithNoCase(name, ".conf") && strcasecmp(name.c_str(), "dosbox.conf") != 0;
}

void scanFolder(const GamesHost& host, const std::string& folder, const std::string& where,
	const std::vector<std::string>& names, std::vector<GameRecord>& out)
{
	for(size_t i = 0; i < names.size() && out.size() < GAMES_MAX; i++)
	{
		if(!isConfName(names[i]))
			continue;

		GameRecord g;
		g.name = names[i].substr(0, names[i].size() - 5);
		g.path = folder + "/" + names[i];
		g.where = where;

		std::string text;
		if(!host.read(g.path, text, kMaxConfBytes))
			continue;

		Game_ParseConf(text, g);
		out.push_back(g);
	}
}

bool byTitle(const GameRecord& a, const GameRecord& b)
{
	const int c = strcasecmp(a.title.c_str(), b.title.c_str());
	if(c != 0)
		return c < 0;
	return strcasecmp(a.path.c_str(), b.path.c_str()) < 0;
}

} // namespace

std::vector<GameRecord> Games_Scan(const GamesHost& host)
{
	std::vector<GameRecord> games;
	const std::string base = host.directory();
	if(base.empty())
		return games;

	const std::vector<std::string> top = host.list(base);
	scanFolder(host, base, std::string(), top, games);

	// The folders below it, Games first (whatever its case on disk: it is where the menu saves games), the rest as they are listed
	std::vector<std::string> folders;
	for(size_t i = 0; i < top.size() && folders.size() < GAMES_MAX_FOLDERS; i++)
	{
		if(top[i][0] == '.' || !host.isDirectory(base + "/" + top[i]))
			continue;
		if(strcasecmp(top[i].c_str(), "Games") == 0)
			folders.insert(folders.begin(), top[i]);
		else
			folders.push_back(top[i]);
	}

	for(size_t f = 0; f < folders.size() && games.size() < GAMES_MAX; f++)
		scanFolder(host, base + "/" + folders[f], folders[f], host.list(base + "/" + folders[f]), games);

	std::stable_sort(games.begin(), games.end(), byTitle);
	return games;
}

// ---- FileGamesHost

std::vector<std::string> FileGamesHost::list(const std::string& folder) const
{
	std::vector<std::string> names;
	DIR * d = opendir(folder.c_str());
	if(!d)
		return names;
	while(struct dirent * e = readdir(d))
	{
		const std::string name = e->d_name;
		if(name != "." && name != "..")
			names.push_back(name);
	}
	closedir(d);
	return names;
}

bool FileGamesHost::isDirectory(const std::string& path) const
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool FileGamesHost::read(const std::string& path, std::string& text, size_t maxBytes) const
{
	FILE * f = fopen(path.c_str(), "rb");
	if(!f)
		return false;
	std::string all;
	char buf[1024];
	while(all.size() < maxBytes)
	{
		const size_t want = std::min(sizeof(buf), maxBytes - all.size());
		const size_t n = fread(buf, 1, want, f);
		if(n == 0)
			break;
		all.append(buf, n);
	}
	const bool ok = !ferror(f);
	fclose(f);
	if(ok)
		text = all;
	return ok;
}
