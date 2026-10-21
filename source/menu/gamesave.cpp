/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * gamesave.cpp
 ***************************************************************************/
#include "gamesave.h"
#include "games.h"
#include <algorithm>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

namespace {

const size_t kMaxTitle = 40;
const size_t kMaxName = 24;

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

bool isComment(const std::string& trimmed)
{
	return !trimmed.empty() && (trimmed[0] == '#' || trimmed[0] == '%');
}

// A text as its lines, without their breaks (the last one empty when the text ends with one is not kept)
std::vector<std::string> splitLines(const std::string& text)
{
	std::vector<std::string> out;
	size_t start = 0;
	while(start < text.size())
	{
		size_t end = text.find('\n', start);
		if(end == std::string::npos)
			end = text.size();
		out.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	return out;
}

std::string joinLines(const std::vector<std::string>& lines)
{
	std::string s;
	for(size_t i = 0; i < lines.size(); i++)
		s += lines[i] + "\n";
	return s;
}

// "[Autoexec]" -> "autoexec"; empty when the line is not a section header
std::string sectionName(const std::string& line)
{
	const std::string t = trim(line);
	if(t.empty() || t[0] != '[')
		return std::string();
	const size_t close = t.find(']');
	if(close == std::string::npos)
		return std::string();
	return lower(trim(t.substr(1, close - 1)));
}

// One line of a file: no break in it, which would start a new setting
std::string oneLine(std::string s)
{
	for(size_t i = 0; i < s.size(); i++)
		if(s[i] == '\r' || s[i] == '\n')
			s[i] = ' ';
	return s;
}

std::string normalPath(std::string p)
{
	for(size_t i = 0; i < p.size(); i++)
		if(p[i] == '\\')
			p[i] = '/';
	while(p.size() > 1 && p[p.size() - 1] == '/' && p[p.size() - 2] != ':')
		p.resize(p.size() - 1);
	return lower(p);
}

bool samePath(const std::string& a, const std::string& b)
{
	return normalPath(a) == normalPath(b);
}

bool hasDevicePrefix(const std::string& path)
{
	size_t n = 0;
	while(n < path.size() && isalnum((unsigned char)path[n]))
		n++;
	return n > 1 && n < path.size() && path[n] == ':';
}

std::string fileOf(const std::string& path)
{
	const size_t slash = path.find_last_of("/\\");
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string folderOf(const std::string& path)
{
	const size_t slash = path.find_last_of("/\\");
	return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

std::string stemOf(const std::string& file)
{
	return file.size() > 5 ? file.substr(0, file.size() - 5) : file;
}

// "# key=value" in a conf's comments: the value, empty when there is none
std::string commentValue(const std::string& text, const char * key)
{
	const std::vector<std::string> all = splitLines(text);
	for(size_t i = 0; i < all.size(); i++)
	{
		const std::string line = trim(all[i]);
		if(!sectionName(line).empty())
			break;
		if(!isComment(line))
			continue;
		const size_t eq = line.find('=');
		if(eq != std::string::npos && lower(trim(line.substr(1, eq - 1))) == key)
			return trim(line.substr(eq + 1));
	}
	return std::string();
}

// A line of a section with the spaces collapsed, for telling whether two say the same
std::string squashed(const std::string& line)
{
	std::string out;
	bool space = false;
	for(size_t i = 0; i < line.size(); i++)
	{
		const char c = line[i];
		if(isspace((unsigned char)c))
			space = !out.empty();
		else
		{
			if(space)
				out += ' ';
			space = false;
			out += (char)tolower((unsigned char)c);
		}
	}
	return out;
}

std::string quoted(const std::string& path)
{
	return path.find(' ') == std::string::npos ? path : "\"" + path + "\"";
}

// A host path as a mount line wants it: no trailing separator unless that is all that tells it is a device ("sd:/")
std::string mountPath(std::string path)
{
	while(path.size() > 1 && (path[path.size() - 1] == '/' || path[path.size() - 1] == '\\') && path[path.size() - 2] != ':')
		path.resize(path.size() - 1);
	return path;
}

bool startsWith(const std::string& s, const char * prefix)
{
	return s.compare(0, strlen(prefix), prefix) == 0;
}

} // namespace

// ---- names and paths

std::string Game_FileName(const std::string& title)
{
	std::string out;
	for(size_t i = 0; i < title.size(); i++)
	{
		const char c = title[i];
		if((unsigned char)c < 32 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == '.')
			continue;
		out += c;
	}
	const size_t first = out.find_first_not_of(' ');
	if(first == std::string::npos)
		return std::string();
	out = out.substr(first, out.find_last_not_of(' ') - first + 1);
	if(out.size() > kMaxName)
	{
		out.resize(kMaxName);
		out.resize(out.find_last_not_of(' ') + 1);
	}
	return out;
}

std::string Game_Directory(const GameSaveHost& host)
{
	const std::string base = host.dosboxDirectory();
	return base.empty() ? std::string() : base + "/Games";
}

std::string Game_ConfPath(const GameSaveHost& host, const std::string& title)
{
	const std::string dir = Game_Directory(host), name = Game_FileName(title);
	return dir.empty() || name.empty() ? std::string() : dir + "/" + name + ".conf";
}

std::string Game_ResolveConf(const GameSaveHost& host, const std::string& given)
{
	if(given.empty())
		return std::string();
	if(hasDevicePrefix(given))
		return given;
	const std::string base = host.dosboxDirectory();
	return base.empty() ? given : base + "/" + given;
}

// ---- text work

std::string Conf_SetValue(const std::string& text, const std::string& section, const std::string& key, const std::string& value)
{
	std::vector<std::string> lines = splitLines(text);
	const std::string want = lower(section), wantKey = lower(key);
	const std::string newLine = key + "=" + oneLine(value);

	std::string current;
	long sectionEnd = -1;	// after the last line of the section that is not blank
	bool seen = false;

	for(size_t i = 0; i < lines.size(); i++)
	{
		const std::string header = sectionName(lines[i]);
		if(!header.empty())
		{
			current = header;
			if(current == want)
			{
				seen = true;
				sectionEnd = (long)i + 1;
			}
			continue;
		}
		if(current != want)
			continue;

		const std::string t = trim(lines[i]);
		if(t.empty())
			continue;
		sectionEnd = (long)i + 1;
		if(isComment(t))
			continue;
		const size_t eq = t.find('=');
		if(eq != std::string::npos && lower(trim(t.substr(0, eq))) == wantKey)
		{
			lines[i] = newLine;
			return joinLines(lines);
		}
	}

	if(seen)
		lines.insert(lines.begin() + sectionEnd, newLine);
	else
	{
		if(!lines.empty() && !trim(lines.back()).empty())
			lines.push_back(std::string());
		lines.push_back("[" + lower(section) + "]");
		lines.push_back(newLine);
	}
	return joinLines(lines);
}

std::vector<std::string> Conf_SectionLines(const std::string& text, const std::string& section)
{
	std::vector<std::string> out;
	const std::vector<std::string> lines = splitLines(text);
	const std::string want = lower(section);
	std::string current;

	for(size_t i = 0; i < lines.size(); i++)
	{
		const std::string header = sectionName(lines[i]);
		if(!header.empty())
		{
			current = header;
			continue;
		}
		const std::string t = trim(lines[i]);
		if(current == want && !t.empty() && !isComment(t))
			out.push_back(t);
	}
	return out;
}

std::string Conf_ReplaceSectionLines(const std::string& text, const std::string& section, const std::vector<std::string>& wanted)
{
	std::vector<std::string> lines = splitLines(text);
	const std::string want = lower(section);

	long header = -1;
	for(size_t i = 0; i < lines.size() && header < 0; i++)
		if(sectionName(lines[i]) == want)
			header = (long)i;

	if(header < 0)
	{
		if(wanted.empty())
			return joinLines(lines);
		if(!lines.empty() && !trim(lines.back()).empty())
			lines.push_back(std::string());
		lines.push_back("[" + want + "]");
		lines.insert(lines.end(), wanted.begin(), wanted.end());
		return joinLines(lines);
	}

	size_t end = (size_t)header + 1;
	while(end < lines.size() && sectionName(lines[end]).empty())
		end++;

	// the comment lines at the start of the section stay (DOSBox's own help for it)
	size_t keep = (size_t)header + 1;
	while(keep < end && (trim(lines[keep]).empty() || isComment(trim(lines[keep]))))
		keep++;
	while(keep > (size_t)header + 1 && trim(lines[keep - 1]).empty())
		keep--;

	std::vector<std::string> out(lines.begin(), lines.begin() + keep);
	out.insert(out.end(), wanted.begin(), wanted.end());
	if(end < lines.size())
		out.push_back(std::string());
	out.insert(out.end(), lines.begin() + end, lines.end());
	return joinLines(out);
}

GameStartup Game_BuildStartup(const std::string& dosboxDirectory, const std::vector<GameDrive>& drives,
	const std::string& programPath, const std::vector<std::string>& baseAutoexec)
{
	GameStartup out;

	std::vector<std::string> base;
	for(size_t i = 0; i < baseAutoexec.size(); i++)
		base.push_back(squashed(baseAutoexec[i]));

	std::vector<GameDrive> sorted = drives;
	std::stable_sort(sorted.begin(), sorted.end(), [](const GameDrive& a, const GameDrive& b) { return toupper((unsigned char)a.letter) < toupper((unsigned char)b.letter); });

	for(size_t i = 0; i < sorted.size(); i++)
	{
		const char letter = (char)toupper((unsigned char)sorted[i].letter);
		const std::string label = std::string(1, letter) + ":";
		const std::string& info = sorted[i].info;

		if(letter == 'Z' || info == "Internal Virtual Drive")
			continue;

		if(letter == 'C')
		{
			// the app mounts the DOSBox folder as C: before the shell starts
			if(!samePath(info, dosboxDirectory))
				out.notes.push_back(label + " is not the DOSBox folder, so it was not saved");
			continue;
		}

		std::string line;
		const std::string drive(1, (char)tolower((unsigned char)letter));
		if(startsWith(info, "CDRom "))
			line = "mount " + drive + " " + quoted(mountPath(info.substr(6))) + " -t cdrom";
		else if(startsWith(info, "isoDrive "))
			line = "imgmount " + drive + " " + quoted(info.substr(9)) + " -t iso";
		else if(startsWith(info, "fatDrive "))
		{
			out.notes.push_back(label + " is a disk image that cannot be saved yet");
			continue;
		}
		else
		{
			const std::string path = startsWith(info, "local directory ") ? info.substr(16) : info;
			if(path.empty())
				continue;
			line = "mount " + drive + " " + quoted(mountPath(path));
		}

		// dosbox.conf mounts it already
		if(std::find(base.begin(), base.end(), squashed(line)) != base.end())
			continue;
		out.lines.push_back(line);
	}

	if(!programPath.empty())
	{
		std::string rest = programPath;
		char drive = 0;
		if(rest.size() > 1 && rest[1] == ':')
		{
			drive = (char)tolower((unsigned char)rest[0]);
			rest = rest.substr(2);
		}
		const size_t slash = rest.find_last_of("\\/");
		std::string dir = slash == std::string::npos ? std::string() : rest.substr(0, slash);
		const std::string file = slash == std::string::npos ? rest : rest.substr(slash + 1);

		if(!file.empty())
		{
			if(drive)
				out.lines.push_back(std::string(1, drive) + ":");
			if(!dir.empty())
			{
				if(dir[0] != '\\' && dir[0] != '/')
					dir = "\\" + dir;
				out.lines.push_back("cd " + dir);
			}
			out.lines.push_back(file);
		}
	}
	return out;
}

// ---- the files half of a host

namespace {

bool isDirectory(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace

bool FileGameSaveHost::makeDirectory(const std::string& dir)
{
	if(dir.empty())
		return false;
	if(isDirectory(dir))
		return true;
	mkdir(dir.c_str(), 0777);
	if(!isDirectory(dir))
	{
		// the parent may be missing (the DOSBox folder is made with the config, so normally it is not)
		const size_t slash = dir.find_last_of('/');
		if(slash != std::string::npos && slash > 0)
		{
			const std::string parent = dir.substr(0, slash);
			if(parent[parent.size() - 1] != ':' && !isDirectory(parent))
				mkdir(parent.c_str(), 0777);
			mkdir(dir.c_str(), 0777);
		}
	}
	return isDirectory(dir);
}

bool FileGameSaveHost::exists(const std::string& path) const
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

bool FileGameSaveHost::read(const std::string& path, std::string& text) const
{
	FILE * f = fopen(path.c_str(), "rb");
	if(!f)
		return false;
	std::string all;
	char buf[1024];
	size_t n;
	while((n = fread(buf, 1, sizeof(buf), f)) > 0)
		all.append(buf, n);
	const bool ok = !ferror(f);
	fclose(f);
	if(ok)
		text = all;
	return ok;
}

bool FileGameSaveHost::write(const std::string& path, const std::string& text)
{
	const std::string tmp = path + ".tmp", bak = path + ".bak";
	remove(tmp.c_str());	// left by a write that was cut short

	FILE * f = fopen(tmp.c_str(), "wb");
	if(!f)
		return false;
	// a full card makes fwrite fail part way, and fclose is where the rest is written
	const bool written = fwrite(text.data(), 1, text.size(), f) == text.size() && !ferror(f);
	if(fclose(f) != 0 || !written)
	{
		remove(tmp.c_str());
		return false;
	}

	const bool replacing = exists(path);
	if(replacing)
	{
		remove(bak.c_str());
		if(rename(path.c_str(), bak.c_str()) != 0)
		{
			remove(tmp.c_str());
			return false;
		}
	}
	if(rename(tmp.c_str(), path.c_str()) != 0)
	{
		if(replacing)
			rename(bak.c_str(), path.c_str());	// put the old one back
		remove(tmp.c_str());
		return false;
	}
	if(replacing)
		remove(bak.c_str());
	return true;
}

// ---- saving

namespace {

GameSaveResult fail(const std::string& path, const std::string& why)
{
	GameSaveResult r;
	r.path = path;
	r.message = "Not saved. " + why;
	return r;
}

// The comment lines at the top of a file (before its first section) that are worth keeping: not the title and program,
// which are written afresh, and not what DOSBox writes there itself, which the new text has too
std::vector<std::string> keptHeader(const std::string& oldText, const std::string& newText)
{
	std::vector<std::string> kept;
	std::vector<std::string> inNew;

	const std::vector<std::string> fresh = splitLines(newText);
	for(size_t i = 0; i < fresh.size() && sectionName(fresh[i]).empty(); i++)
		inNew.push_back(trim(fresh[i]));

	const std::vector<std::string> old = splitLines(oldText);
	for(size_t i = 0; i < old.size() && sectionName(old[i]).empty(); i++)
	{
		const std::string line = trim(old[i]);
		if(!isComment(line))
			continue;
		const size_t eq = line.find('=');
		if(eq != std::string::npos)
		{
			const std::string key = lower(trim(line.substr(1, eq - 1)));
			if(key == "title" || key == "program")
				continue;
		}
		if(std::find(inNew.begin(), inNew.end(), line) != inNew.end() || std::find(kept.begin(), kept.end(), line) != kept.end())
			continue;
		kept.push_back(line);
	}
	return kept;
}

} // namespace

GameSaveResult Game_Save(GameSaveHost& host, const GameSave& what)
{
	const std::string path = what.path;
	if(path.empty())
		return fail(path, "There is nowhere to save it.");

	const std::string dir = folderOf(path);
	const std::string name = stemOf(fileOf(path));
	if(dir.empty() || name.empty() || !host.makeDirectory(dir))
		return fail(path, "Could not make the folder " + dir + ".");

	std::string text, error;
	if(!host.runningConfig(text, error))
		return fail(path, error.empty() ? "The running config could not be read." : error);

	std::string existing;
	const bool had = host.exists(path) && host.read(path, existing);

	// the map first: a save cut short never leaves a conf with no map
	const std::string mapName = name + ".map";
	const std::string mapPath = dir + "/" + mapName;
	if(!host.saveMapper())
		return fail(path, "The mapper file could not be written.");
	const std::string inUse = host.mapperFile();
	std::string map;
	if(inUse.empty() || !host.read(inUse, map))
		return fail(path, "The mapper file could not be read.");
	if(!samePath(inUse, mapPath) && !host.write(mapPath, map))
		return fail(path, "Could not write " + mapName + ".");

	text = Conf_SetValue(text, "sdl", "mapperfile", mapName);
	const std::string joystick = host.joystickType();
	if(!joystick.empty())
		text = Conf_SetValue(text, "joystick", "joysticktype", joystick);

	// the game's own [autoexec], not the running one (dosbox.conf's lines and the game's)
	std::string base;
	if(!host.baseConfig().empty())
		host.read(host.baseConfig(), base);
	const std::string programPath = host.runningProgramPath();
	std::vector<std::string> startup;
	std::vector<std::string> notes;
	bool keep = had && what.keepStartup;
	if(!keep)
	{
		const GameStartup built = Game_BuildStartup(host.dosboxDirectory(), host.drives(), programPath, Conf_SectionLines(base, "autoexec"));
		startup = built.lines;
		notes = built.notes;
		// nothing running and nothing mounted to go on: what the game started with is better than nothing
		keep = had && startup.empty() && !Conf_SectionLines(existing, "autoexec").empty();
	}
	if(keep)
		startup = Conf_SectionLines(existing, "autoexec");
	text = Conf_ReplaceSectionLines(text, "autoexec", startup);

	// title and program, then the comments that were already there
	std::string title = what.title.empty() ? commentValue(existing, "title") : what.title;
	title = trim(oneLine(title));
	if(title.size() > kMaxTitle)
		title.resize(kMaxTitle);
	if(title.empty())
		title = name;

	std::string program = commentValue(existing, "program");
	if((!what.keepStartup || !had) && !programPath.empty())
		program = upper(fileOf(programPath));

	std::string header = "# title=" + title + "\n";
	if(!program.empty())
		header += "# program=" + oneLine(program) + "\n";
	header += joinLines(keptHeader(existing, text));
	text = header + text;

	if(!host.write(path, text))
		return fail(path, "Could not write " + fileOf(path) + ".");

	GameSaveResult r;
	r.ok = true;
	r.path = path;
	r.message = "Saved " + fileOf(path) + " and " + mapName + ". dosbox.conf was not changed.";
	if(!notes.empty())
		r.message += " " + notes[0] + ".";
	return r;
}

GameSaveResult Game_SaveNew(GameSaveHost& host, const std::string& title)
{
	const std::string path = Game_ConfPath(host, title);
	if(path.empty())
		return fail(path, "That name can't be used.");

	if(host.exists(path))
	{
		GameSaveResult r;
		r.exists = true;
		r.path = path;
		r.message = fileOf(path) + " already exists.";
		return r;
	}

	GameSave what;
	what.path = path;
	what.title = trim(title);
	return Game_Save(host, what);
}

GameSaveResult Game_SaveReplace(GameSaveHost& host, const std::string& path, bool keepStartup)
{
	GameSave what;
	what.path = path;
	what.keepStartup = keepStartup;
	return Game_Save(host, what);
}

GameSaveResult Game_SaveActive(GameSaveHost& host, const std::string& given)
{
	GameSave what;
	what.path = Game_ResolveConf(host, given);
	what.keepStartup = true;
	return Game_Save(host, what);
}

// ---- adding a found game

std::string Game_FoundConfPath(const GameSaveHost& host, const FoundGame& found, const std::string& title)
{
	const std::string dir = Game_Directory(host);
	if(dir.empty())
		return std::string();

	std::vector<std::string> names;
	names.push_back(Game_FileName(found.folder));
	names.push_back(Game_FileName(title));
	for(int n = 2; n <= 9; n++)
		names.push_back(Game_FileName(found.folder) + " " + std::to_string(n));

	for(size_t i = 0; i < names.size(); i++)
	{
		if(names[i].empty())
			continue;
		const std::string path = dir + "/" + names[i] + ".conf";
		if(!host.exists(path))
			return path;
	}
	return std::string();
}

std::string Game_FoundConfText(const FoundGame& found, const std::string& title, const std::string& program)
{
	std::string name = trim(oneLine(title));
	if(name.size() > kMaxTitle)
		name.resize(kMaxTitle);
	if(name.empty())
		name = found.folder;

	const std::string prog = upper(oneLine(program.empty() && !found.programs.empty() ? found.programs[0] : program));

	std::string text = "# title=" + name + "\n";
	if(!prog.empty())
		text += "# program=" + prog + "\n";
	text += "\n[autoexec]\nc:\ncd \\" + upper(oneLine(found.folder)) + "\n";
	if(!prog.empty())
		text += prog + "\n";
	return text;
}

GameSaveResult Game_AddFound(GameSaveHost& host, const FoundGame& found, const std::string& title, const std::string& program)
{
	if(found.folder.empty() || (program.empty() && found.programs.empty()))
		return fail(std::string(), "There is no program to run.");

	const std::string dir = Game_Directory(host);
	if(dir.empty())
		return fail(std::string(), "There is nowhere to save it.");

	const std::string path = Game_FoundConfPath(host, found, title);
	if(path.empty())
	{
		GameSaveResult r;
		r.exists = true;
		r.path = Game_ConfPath(host, found.folder);
		r.message = "Not added. Every name for it is taken in " + dir + ".";
		return r;
	}

	if(!host.makeDirectory(dir))
		return fail(path, "Could not make the folder " + dir + ".");

	if(!host.write(path, Game_FoundConfText(found, title, program)))
		return fail(path, "Could not write " + fileOf(path) + ".");

	GameSaveResult r;
	r.ok = true;
	r.path = path;
	r.message = "Added " + fileOf(path) + ". Its controls start from the defaults.";
	return r;
}
