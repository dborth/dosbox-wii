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
	out.folders.clear();

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

	out.folders = Game_AutoexecFolders(autoexec);

	if(out.title.empty())
		out.title = out.name;

	if(out.program.empty())
	{
		out.program = Game_InferProgram(autoexec);
		out.programInferred = !out.program.empty();
	}
}


// ---- folders an autoexec uses, and names

namespace {

// Splits a path on both separators into its parts, leaving out empty ones
std::vector<std::string> pathParts(const std::string& path)
{
	std::vector<std::string> parts;
	std::string part;
	for(size_t i = 0; i <= path.size(); i++)
	{
		if(i == path.size() || path[i] == '/' || path[i] == '\\')
		{
			if(!part.empty())
				parts.push_back(part);
			part.clear();
		}
		else
			part += path[i];
	}
	return parts;
}

// A command's arguments as words, a quoted path being one
std::vector<std::string> words(const std::string& text)
{
	std::vector<std::string> out;
	std::string word;
	bool quoted = false, any = false;
	for(size_t i = 0; i < text.size(); i++)
	{
		const char c = text[i];
		if(c == '"')
		{
			quoted = !quoted;
			any = true;
		}
		else if((c == ' ' || c == '\t') && !quoted)
		{
			if(any)
				out.push_back(word);
			word.clear();
			any = false;
		}
		else
		{
			word += c;
			any = true;
		}
	}
	if(any)
		out.push_back(word);
	return out;
}

void addUnique(std::vector<std::string>& v, const std::string& s)
{
	for(size_t i = 0; i < v.size(); i++)
		if(v[i] == s)
			return;
	v.push_back(s);
}

} // namespace

std::vector<std::string> Game_AutoexecFolders(const std::vector<std::string>& autoexec)
{
	std::vector<std::string> out;

	for(size_t i = 0; i < autoexec.size(); i++)
	{
		std::string line = trim(autoexec[i]);
		if(line.empty() || isComment(line))
			continue;
		if(line[0] == '@')
			line = trim(line.substr(1));

		std::string rest;
		const std::string word = lower(firstWord(line, &rest));

		if(word == "cd" || word == "chdir" || (word.size() > 2 && word.compare(0, 2, "cd") == 0 && (word[2] == '\\' || word[2] == '/')))
		{
			// "cd \keen", "cd\keen", "cd keen", "cd c:\keen"
			std::string path = word.size() > 2 && word[0] == 'c' && word[1] == 'd' && word != "chdir" ? word.substr(2) + rest : rest;
			if(path.size() > 1 && path[1] == ':')
				path = path.substr(2);
			const std::vector<std::string> parts = pathParts(path);
			if(!parts.empty() && parts[0] != "." && parts[0] != "..")
				addUnique(out, lower(parts[0]));
		}
		else if(word == "mount")
		{
			// mount <drive> <path> [options]: every folder in the path (a device such as "sd:" is not one)
			const std::vector<std::string> args = words(rest);
			if(args.size() < 2)
				continue;
			const std::vector<std::string> parts = pathParts(args[1]);
			for(size_t p = 0; p < parts.size(); p++)
				if(parts[p].find(':') == std::string::npos && parts[p] != "." && parts[p] != "..")
					addUnique(out, lower(parts[p]));
		}
	}
	return out;
}

bool Game_IsDosName(const std::string& name)
{
	if(name.empty())
		return false;

	const size_t dot = name.find('.');
	if(dot != std::string::npos && name.find('.', dot + 1) != std::string::npos)
		return false;

	const std::string base = dot == std::string::npos ? name : name.substr(0, dot);
	const std::string ext = dot == std::string::npos ? std::string() : name.substr(dot + 1);
	if(base.empty() || base.size() > 8 || ext.size() > 3 || (dot != std::string::npos && ext.empty()))
		return false;

	for(size_t i = 0; i < name.size(); i++)
	{
		const unsigned char c = (unsigned char)name[i];
		if(c == '.' || isalnum(c) || (c < 128 && strchr("_-!#$%&'(){}@^", c) != nullptr && c != 0))
			continue;
		return false;
	}
	return true;
}

std::string Game_SuggestTitle(const std::string& folder)
{
	bool hasUpper = false, hasLower = false;
	for(size_t i = 0; i < folder.size(); i++)
	{
		if(isupper((unsigned char)folder[i]))
			hasUpper = true;
		else if(islower((unsigned char)folder[i]))
			hasLower = true;
	}
	if(hasUpper && hasLower)
		return folder;

	std::string out;
	bool start = true;
	for(size_t i = 0; i < folder.size(); i++)
	{
		const char c = folder[i];
		if(c == '_' || c == '-' || c == ' ')
		{
			if(!out.empty() && out[out.size() - 1] != ' ')
				out += ' ';
			start = true;
			continue;
		}
		out += start ? (char)toupper((unsigned char)c) : (char)tolower((unsigned char)c);
		start = false;
	}
	return out.empty() ? folder : out;
}

namespace {

// Programs that are not the game: set-up, installers, drivers and the like
bool isUtility(const std::string& stem)
{
	static const char * const names[] = {
		"setup", "install", "installer", "inst", "uninst", "uninstal", "uninstall", "config", "configure", "cfg", "sound",
		"setsound", "sndsetup", "soundset", "setblast", "dos4gw", "dos32a", "cwsdpmi", "pmodew", "mouse", "ctmouse",
		"vendor", "order", "help", "readme", "convert", "patch", "update", "pkunzip", "pkzip", "unzip", "arj", "lha", nullptr
	};
	const std::string s = lower(stem);
	for(int i = 0; names[i]; i++)
		if(s == names[i])
			return true;
	return s.compare(0, 5, "unins") == 0 || s.compare(0, 5, "setup") == 0 || s.compare(0, 6, "instal") == 0;
}

// Lower is better
int programScore(const std::string& folder, const std::string& stem, const std::string& ext)
{
	const bool bat = ext == "bat";
	int base;
	if(isUtility(stem))
		base = 100;
	else
	{
		const std::string f = lower(folder), s = lower(stem);
		const bool same = f == s;
		const std::string& shorter = f.size() < s.size() ? f : s;
		const std::string& longer = f.size() < s.size() ? s : f;
		const bool related = shorter.size() >= 3 && longer.compare(0, shorter.size(), shorter) == 0;
		base = same ? (bat ? 0 : 1) : related ? (bat ? 2 : 3) : (bat ? 4 : 5);
	}
	return base * 10 + (ext == "com" ? 1 : 0);
}

} // namespace

std::vector<std::string> Game_RankPrograms(const std::string& folder, const std::vector<std::string>& names)
{
	struct Entry { int score; std::string name; };
	std::vector<Entry> entries;

	for(size_t i = 0; i < names.size(); i++)
	{
		const std::string name = upper(names[i]);
		const size_t dot = name.rfind('.');
		if(dot == std::string::npos || !Game_IsDosName(name))
			continue;
		const std::string ext = lower(name.substr(dot + 1)), stem = name.substr(0, dot);
		if(ext != "exe" && ext != "com" && ext != "bat")
			continue;

		bool repeat = false;
		for(size_t e = 0; e < entries.size(); e++)
			repeat = repeat || entries[e].name == name;
		if(repeat)
			continue;

		Entry entry = { programScore(folder, stem, ext), name };
		entries.push_back(entry);
	}

	std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
		return a.score != b.score ? a.score < b.score : a.name < b.name;
	});

	std::vector<std::string> out;
	for(size_t i = 0; i < entries.size() && out.size() < FOUND_PROGRAMS_MAX; i++)
		out.push_back(entries[i].name);
	return out;
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

namespace {

bool sameNoCase(const std::string& a, const std::string& b)
{
	return strcasecmp(a.c_str(), b.c_str()) == 0;
}

// Whether a game already covers a folder that holds these programs (upper case)
bool covered(const std::string& folder, const std::vector<std::string>& programs, const std::vector<GameRecord>& games)
{
	const std::string f = lower(folder);
	for(size_t g = 0; g < games.size(); g++)
	{
		const GameRecord& game = games[g];

		// its startup goes into the folder or mounts it
		for(size_t i = 0; i < game.folders.size(); i++)
			if(game.folders[i] == f)
				return true;

		// otherwise its program is one of the folder's (a game with a cd of its own is somewhere else), or it is a conf kept in the folder
		if(game.program.empty())
			continue;
		const bool listed = std::find(programs.begin(), programs.end(), upper(game.program)) != programs.end();
		if(listed && (game.folders.empty() || sameNoCase(game.where, folder)))
			return true;
	}
	return false;
}

bool byFolder(const FoundGame& a, const FoundGame& b)
{
	return strcasecmp(a.folder.c_str(), b.folder.c_str()) < 0;
}

} // namespace

GamesScan Games_ScanAll(const GamesHost& host)
{
	GamesScan result;
	std::vector<GameRecord>& games = result.games;
	const std::string base = host.directory();
	if(base.empty())
		return result;

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

	// Each folder is listed once, for its confs and for its programs
	struct Candidate { std::string folder; std::vector<std::string> programs; };
	std::vector<Candidate> candidates;

	for(size_t f = 0; f < folders.size(); f++)
	{
		const std::vector<std::string> names = host.list(base + "/" + folders[f]);
		scanFolder(host, base + "/" + folders[f], folders[f], names, games);

		if(sameNoCase(folders[f], "Games") || !Game_IsDosName(folders[f]))
			continue;
		Candidate c;
		c.folder = folders[f];
		c.programs = Game_RankPrograms(folders[f], names);
		if(!c.programs.empty())
			candidates.push_back(c);
	}

	// Which of them no game covers is known once every conf has been read
	for(size_t i = 0; i < candidates.size() && result.found.size() < FOUND_MAX; i++)
	{
		if(covered(candidates[i].folder, candidates[i].programs, games))
			continue;
		FoundGame found;
		found.folder = candidates[i].folder;
		found.title = Game_SuggestTitle(candidates[i].folder);
		found.programs = candidates[i].programs;
		result.found.push_back(found);
	}

	std::stable_sort(games.begin(), games.end(), byTitle);
	std::stable_sort(result.found.begin(), result.found.end(), byFolder);
	return result;
}

std::vector<GameRecord> Games_Scan(const GamesHost& host)
{
	return Games_ScanAll(host).games;
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
