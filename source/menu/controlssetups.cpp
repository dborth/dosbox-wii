/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlssetups.cpp
 ***************************************************************************/
#include "controlssetups.h"
#include "controlsmodel.h"
#include <algorithm>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace {

const char kMarker[] = "# DOSBox Wii setup";

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

// One line of the file, with no line break in it (a value that had one would start a new setting)
std::string oneLine(std::string s)
{
	for(size_t i = 0; i < s.size(); i++)
		if(s[i] == '\r' || s[i] == '\n')
			s[i] = ' ';
	return s;
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

// DOSBox takes a value out of quotes (Section_prop::HandleInputline)
std::string unquote(const std::string& v)
{
	if(v.size() > 1 && ((v[0] == '"' && v[v.size() - 1] == '"') || (v[0] == '\'' && v[v.size() - 1] == '\'')))
		return v.substr(1, v.size() - 2);
	return v;
}

// "pad_2" / "stick_2" -> 2 (0..3); -1 for anything else
int playerOf(const std::string& group, const char * prefix)
{
	const size_t n = strlen(prefix);
	if(group.size() != n + 1 || strncasecmp(group.c_str(), prefix, n) != 0 || group[n] < '0' || group[n] > '3')
		return -1;
	return group[n] - '0';
}

// The words of a bind ("pad_0 button 4 mod1"), flags and all
std::vector<std::string> words(const std::string& s)
{
	std::vector<std::string> out;
	size_t i = 0;
	while(i < s.size())
	{
		while(i < s.size() && isspace((unsigned char)s[i]))
			i++;
		const size_t start = i;
		while(i < s.size() && !isspace((unsigned char)s[i]))
			i++;
		if(i > start)
			out.push_back(s.substr(start, i - start));
	}
	return out;
}

bool number(const std::string& s, int& value)
{
	if(s.empty() || s.size() > 5)
		return false;
	for(size_t i = 0; i < s.size(); i++)
		if(s[i] < '0' || s[i] > '9')
			return false;
	value = atoi(s.c_str());
	return true;
}

} // namespace

std::string Setup_FormatConf(const SetupRecord& r)
{
	std::string t = std::string(kMarker) + "\n";
	if(!r.program.empty())
		t += "# program=" + oneLine(r.program) + "\n";
	for(int p = 0; p < 4; p++)
		if(!r.presets[p].empty())
			t += "# preset." + std::string(1, (char)('1' + p)) + "=" + oneLine(r.presets[p]) + "\n";
	t += "[sdl]\nmapperfile=" + oneLine(r.mapperFile) + "\n";
	if(!r.joystickType.empty())
		t += "[joystick]\njoysticktype=" + oneLine(r.joystickType) + "\n";
	return t;
}

bool Setup_ParseConf(const std::string& text, SetupRecord& out)
{
	const std::vector<std::string> all = lines(text);
	size_t first = 0;
	while(first < all.size() && trim(all[first]).empty())
		first++;
	if(first >= all.size() || trim(all[first]) != kMarker)
		return false;

	SetupRecord r;
	r.name = out.name;
	std::string section;
	for(size_t i = first + 1; i < all.size(); i++)
	{
		const std::string line = trim(all[i]);
		if(line.empty())
			continue;
		if(line[0] == '#')
		{
			const std::string note = trim(line.substr(1));
			const size_t eq = note.find('=');
			if(eq == std::string::npos)
				continue;
			const std::string key = lower(trim(note.substr(0, eq)));
			const std::string value = trim(note.substr(eq + 1));
			if(key == "program")
				r.program = value;
			else if(key.size() == 8 && key.compare(0, 7, "preset.") == 0 && key[7] >= '1' && key[7] <= '4')
				r.presets[key[7] - '1'] = value;
			continue;
		}
		if(line[0] == '[')
		{
			const size_t close = line.find(']');
			section = close == std::string::npos ? std::string() : lower(line.substr(1, close - 1));
			continue;
		}
		const size_t eq = line.find('=');
		if(eq == std::string::npos)
			continue;
		const std::string key = lower(trim(line.substr(0, eq)));
		const std::string value = unquote(trim(line.substr(eq + 1)));
		if(section == "sdl" && key == "mapperfile")
			r.mapperFile = value;
		else if(section == "joystick" && key == "joysticktype")
			r.joystickType = value;
	}
	out = r;
	return true;
}

bool Setup_IsAbsolute(const std::string& path)
{
	// as Cross::IsPathAbsolute() has it on this build: a device prefix (sd:, usb2:, dvd:, ...)
	size_t prefix = 0;
	while(prefix < path.size() && isalnum((unsigned char)path[prefix]))
		prefix++;
	return prefix > 1 && prefix < path.size() && path[prefix] == ':';
}

std::string Setup_ResolveFile(const std::string& directory, const std::string& value)
{
	if(value.empty())
		return std::string();
	if(Setup_IsAbsolute(value) || directory.empty())
		return value;
	return directory + "/" + value;
}

std::string Setup_ProgramKey(const std::string& program)
{
	std::string file = Setup_CleanProgram(program);
	const size_t dot = file.find('.');
	if(dot != std::string::npos)
		file.resize(dot);
	if(file.size() > 8)
		file.resize(8);
	return file;
}

std::string Setup_ProgramFromEnvironment(const unsigned char * env, size_t size)
{
	if(!env || size < 4)
		return std::string();
	// the variables are strings up to an empty one
	size_t pos = 0;
	while(pos < size && env[pos] != 0)
	{
		while(pos < size && env[pos] != 0)
			pos++;
		pos++;	// past the string's end
	}
	if(pos >= size)
		return std::string();
	pos++;		// past the empty string
	if(pos + 2 > size || env[pos] != 1 || env[pos + 1] != 0)
		return std::string();
	pos += 2;
	std::string path;
	while(pos < size && env[pos] != 0)
		path += (char)env[pos++];
	if(pos >= size)
		return std::string();	// no end to the name: not a block we understand
	return Setup_CleanProgram(path);
}

void Setup_ParsePadBinds(const std::string& mapText, MapPadBinds& out)
{
	for(int p = 0; p < 4; p++)
		for(int s = 0; s < PAD_SOURCE_COUNT; s++)
			out.events[p][s].clear();

	const std::vector<std::string> all = lines(mapText);
	for(size_t l = 0; l < all.size(); l++)
	{
		const std::string line = trim(all[l]);
		const size_t space = line.find_first_of(" \t");
		if(line.empty() || space == std::string::npos)
			continue;
		const std::string event = line.substr(0, space);
		// each bind is in quotes
		size_t at = line.find('"', space);
		while(at != std::string::npos)
		{
			const size_t close = line.find('"', at + 1);
			if(close == std::string::npos)
				break;
			const std::vector<std::string> w = words(line.substr(at + 1, close - at - 1));
			at = line.find('"', close + 1);
			if(w.size() < 3)
				continue;
			int player = playerOf(w[0], "pad_");
			if(player < 0)
				player = playerOf(w[0], "stick_");
			if(player < 0)
				continue;
			int source = -1;
			if(strcasecmp(w[1].c_str(), "button") == 0)
			{
				int b;
				if(number(w[2], b) && b >= 0 && b < PAD_SOURCE_COUNT - 4)
					source = b;
			}
			else if(strcasecmp(w[1].c_str(), "hat") == 0 && w.size() >= 4)
			{
				int hat, mask;
				if(number(w[2], hat) && hat == 0 && number(w[3], mask))
				{
					// up 1, right 2, down 4, left 8 (SDL_HAT_*), and the mapper tests them in that order
					for(int d = 0; d < 4; d++)
						if(mask & (1 << d))
						{
							source = PAD_SOURCE_COUNT - 4 + d;
							break;
						}
				}
			}
			if(source >= 0)
				out.events[player][source].push_back(event);
		}
	}
}

std::vector<SetupRecord> Setups_List(const SetupHost& host)
{
	std::vector<SetupRecord> list;
	const std::string dir = host.directory();
	if(dir.empty())
		return list;
	const std::vector<std::string> names = host.files();
	for(size_t i = 0; i < names.size(); i++)
	{
		const std::string& file = names[i];
		if(file.size() <= 5 || lower(file.substr(file.size() - 5)) != ".conf")
			continue;
		std::string text;
		SetupRecord r;
		r.name = file.substr(0, file.size() - 5);
		if(host.read(dir + "/" + file, text) && Setup_ParseConf(text, r))
			list.push_back(r);
	}
	std::stable_sort(list.begin(), list.end(), [](const SetupRecord& a, const SetupRecord& b) {
		const std::string x = lower(a.name), y = lower(b.name);
		return x != y ? x < y : a.name < b.name;
	});
	return list;
}

int Setups_IndexForProgram(const std::vector<SetupRecord>& setups, const std::string& program)
{
	const std::string key = Setup_ProgramKey(program);
	if(key.empty())
		return -1;
	for(size_t i = 0; i < setups.size(); i++)
		if(Setup_ProgramKey(setups[i].program) == key)
			return (int)i;
	return -1;
}

// ---- the files half of a host

namespace {

bool isDirectory(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool exists(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

} // namespace

bool FileSetupHost::makeDirectory()
{
	const std::string dir = directory();
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
			if(!parent.empty() && parent[parent.size() - 1] != ':' && !isDirectory(parent))
				mkdir(parent.c_str(), 0777);
			mkdir(dir.c_str(), 0777);
		}
	}
	return isDirectory(dir);
}

std::vector<std::string> FileSetupHost::files() const
{
	std::vector<std::string> names;
	DIR * d = opendir(directory().c_str());
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

bool FileSetupHost::read(const std::string& path, std::string& text) const
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

bool FileSetupHost::write(const std::string& path, const std::string& text)
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
