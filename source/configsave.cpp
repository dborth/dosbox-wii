/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * configsave.cpp
 ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <string>

#include "dosbox.h"
#include "setup.h"
#include "control.h"
#include "cross.h"
#include "configsave.h"

static bool Exists(const std::string & path, off_t * size = NULL)
{
	struct stat st;

	if(stat(path.c_str(), &st) != 0)
		return false;

	if(size)
		*size = st.st_size;

	return true;
}

//! The whole file, or false if it cannot be read
static bool ReadAll(const std::string & path, std::string * out)
{
	FILE * f = fopen(path.c_str(), "rb");
	char buf[1024];
	size_t n;

	if(!f)
		return false;

	out->clear();
	while((n = fread(buf, 1, sizeof(buf), f)) > 0)
		out->append(buf, n);

	fclose(f);
	return true;
}

/**
 * PrintConfig() ignores write errors (a full card makes it stop part way and
 * still return true), so what it left is looked at before it replaces
 * anything: it must be there and not empty, and the header of the last
 * section, which is written last, must be in it. That catches a card that
 * filled up; it cannot prove a file complete.
 */
static bool LooksComplete(const std::string & path)
{
	off_t size = 0;
	std::string text;

	if(!Exists(path, &size) || size <= 0 || !ReadAll(path, &text))
		return false;

	int last = 0;

	while(control->GetSection(last + 1) != NULL)
		last++;

	Section * section = control->GetSection(last);

	if(!section)
		return false;

	std::string name = section->GetName();

	for(size_t i = 0; i < name.size(); i++)
		name[i] = (char)tolower((unsigned char)name[i]);	// PrintConfig writes it lower case

	return text.find("[" + name + "]\n") != std::string::npos;
}

std::string ConfigSavePath()
{
	if(control && !control->configfiles.empty())
		return control->configfiles[0];

	std::string dir, name;

	Cross::CreatePlatformConfigDir(dir);	// creates the folder; ends with a separator
	Cross::GetPlatformConfigName(name);
	return dir + name;
}

bool ConfigSave(const char * path, std::string * error)
{
	std::string ignored;

	if(!error)
		error = &ignored;

	if(!control || !path || !*path)
	{
		*error = "There is no config to save.";
		return false;
	}

	const std::string target(path);
	const std::string tmp = target + ".tmp";
	const std::string bak = target + ".bak";

	remove(tmp.c_str());	// left by a save that was cut short

	if(!control->PrintConfig(tmp.c_str()))
	{
		*error = "Could not write " + tmp + ".";
		return false;
	}

	if(!LooksComplete(tmp))
	{
		remove(tmp.c_str());
		*error = "The new file was not written in full. Is the card full?";
		return false;
	}

	if(Exists(target))
	{
		remove(bak.c_str());

		if(Exists(bak) || rename(target.c_str(), bak.c_str()) != 0)
		{
			remove(tmp.c_str());
			*error = "Could not keep the old file as " + bak + ". Nothing was changed.";
			return false;
		}
	}

	if(rename(tmp.c_str(), target.c_str()) != 0)
	{
		const std::string why = strerror(errno);

		if(!Exists(target) && Exists(bak))
			rename(bak.c_str(), target.c_str());	// put the old one back

		remove(tmp.c_str());
		*error = "Could not put the new file in place (" + why + ").";
		return false;
	}

	return true;
}
