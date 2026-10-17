/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlssetupsfiles.cpp
 ***************************************************************************/
#include "controlssetupsfiles.h"
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

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
