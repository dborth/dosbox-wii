/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * autoboot.cpp
 *
 * See autoboot.h.
 ***************************************************************************/

#include "autoboot.h"

#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <strings.h>

namespace
{
	const char * const GAME_SWITCH = "-gameconf";
	const char * const FOLDER_CONF = "autoboot.conf";
	const char * const BASE_CONF = "dosbox.conf";

	// DOSBox's own switches that take the next argument as their value, which
	// must not be mistaken for a bare argument (every cmdline->FindString() in
	// the core: keep in step with it)
	bool TakesValue(const std::string & arg)
	{
		static const char * const names[] = { "-c", "-conf", "-lang", "-machine" };

		for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
			if(strcasecmp(arg.c_str(), names[i]) == 0)
				return true;

		return false;
	}

	bool EndsWithNoCase(const std::string & s, const char * tail)
	{
		const size_t n = strlen(tail);

		return s.size() >= n && strcasecmp(s.c_str() + s.size() - n, tail) == 0;
	}

	// Index of the colon of a device prefix ("sd:", "usb1:"), or npos
	size_t DeviceEnd(const std::string & p)
	{
		size_t i = 0;

		while(i < p.size() && (isalnum((unsigned char)p[i]) || p[i] == '_'))
			i++;

		return (i > 0 && i < p.size() && p[i] == ':') ? i : std::string::npos;
	}

	bool IsAbsolute(const std::string & p)
	{
		return !p.empty() && (p[0] == '/' || DeviceEnd(p) != std::string::npos);
	}

	// "sd:/games/keen/" -> "sd:/games/keen" (a device root, "sd:/", stays as it is)
	std::string TrimSlash(std::string p)
	{
		const size_t colon = DeviceEnd(p);

		while(p.size() > 1 && p[p.size() - 1] == '/' && !(colon != std::string::npos && p.size() == colon + 2))
			p.erase(p.size() - 1);

		return p;
	}

	std::string Join(std::string base, std::string rel)
	{
		while(rel.compare(0, 2, "./") == 0)
			rel.erase(0, 2);

		while(!base.empty() && base[base.size() - 1] == '/')
			base.erase(base.size() - 1);

		return base + "/" + rel;
	}

	// The folder the program was launched from, when the loader said: "sd:/apps/keen/boot.dol" -> "sd:/apps/keen".
	// Anything without a device prefix and a folder ("boot.dol", what a loader that gives only a name provides) is no help.
	std::string LoaderFolder(const std::string & argv0)
	{
		const size_t colon = DeviceEnd(argv0);

		if(colon == std::string::npos)
			return std::string();

		const size_t slash = argv0.rfind('/');

		if(slash == std::string::npos || slash <= colon)
			return std::string();

		return argv0.substr(0, slash);
	}

	enum FindResult { FOUND, MISSING, NO_CONF, SEVERAL };

	class Finder
	{
		public:
			Finder(AutobootHost & h, const std::string & dosbox, const std::string & loader) : host(h)
			{
				if(!loader.empty())
					bases.push_back(loader);
				if(!dosbox.empty())
					bases.push_back(dosbox);
			}

			// `text` as the user wrote it: a conf, or a folder with one. The first place where something exists decides.
			// With foldersOnly a file is not an answer (a bare argument that is a program is for DOSBox to run).
			FindResult find(const std::string & text, std::string & path, bool foldersOnly = false)
			{
				std::vector<std::string> candidates;

				candidates.push_back(text);

				if(!IsAbsolute(text))
					for(size_t i = 0; i < bases.size(); i++)
						candidates.push_back(Join(bases[i], text));

				for(size_t i = 0; i < candidates.size(); i++)
				{
					const std::string p = host.resolve(TrimSlash(candidates[i]));

					if(host.isFile(p))
					{
						if(foldersOnly)
							return MISSING;

						path = p;
						return FOUND;
					}

					if(host.isFolder(p))
						return pickIn(p, path);
				}

				return MISSING;
			}

		private:
			FindResult pickIn(const std::string & folder, std::string & path)
			{
				const std::string named = Join(folder, FOLDER_CONF);

				if(host.isFile(named))
				{
					path = named;
					return FOUND;
				}

				std::vector<std::string> names = host.namesIn(folder), confs;

				for(size_t i = 0; i < names.size(); i++)
					if(EndsWithNoCase(names[i], ".conf") && strcasecmp(names[i].c_str(), BASE_CONF) != 0)
						confs.push_back(names[i]);

				if(confs.empty())
					return NO_CONF;

				if(confs.size() > 1)
					return SEVERAL;

				path = Join(folder, confs[0]);
				return FOUND;
			}

			AutobootHost & host;
			std::vector<std::string> bases;
	};

	std::string Why(FindResult r, const std::string & text)
	{
		switch(r)
		{
			case MISSING:	return "Can't find " + text;
			case NO_CONF:	return text + " has no .conf file";
			case SEVERAL:	return text + " has several .conf files";
			default:		return std::string();
		}
	}
}

LaunchArguments Autoboot_Parse(const std::vector<std::string> & argv, const std::string & dosboxFolder, AutobootHost & host)
{
	LaunchArguments out;

	out.args.push_back(argv.empty() ? std::string("dosbox") : argv[0]);

	Finder finder(host, dosboxFolder, argv.empty() ? std::string() : LoaderFolder(argv[0]));

	bool haveGame = false;		// a game was taken from the arguments (it may have failed)
	bool triedFolder = false;	// the first bare argument has been looked at as a game folder
	std::string error;

	for(size_t i = 1; i < argv.size(); i++)
	{
		const std::string & a = argv[i];

		// the explicit form: always ours, so a problem with it is an error
		if(strcasecmp(a.c_str(), GAME_SWITCH) == 0)
		{
			if(i + 1 >= argv.size())
			{
				if(error.empty())
					error = std::string(GAME_SWITCH) + " needs a file";
				haveGame = true;
				continue;
			}

			const std::string & text = argv[++i];

			if(haveGame)
			{
				error = "More than one game was named";
				continue;
			}

			haveGame = true;
			out.requested = text;

			std::string path;
			const FindResult r = finder.find(text, path);

			if(r == FOUND)
				out.conf = path;
			else
				error = Why(r, text);

			continue;
		}

		if(TakesValue(a))
		{
			out.args.push_back(a);

			if(i + 1 < argv.size())
				out.args.push_back(argv[++i]);

			continue;
		}

		if(!a.empty() && a[0] == '-')
		{
			out.args.push_back(a);
			continue;
		}

		// A bare argument. One ending in .conf is a game; the first of the others may be a game folder.
		if(EndsWithNoCase(a, ".conf"))
		{
			if(haveGame)
			{
				error = "More than one game was named";
				continue;
			}

			haveGame = true;
			out.requested = a;

			std::string path;
			const FindResult r = finder.find(a, path);

			if(r == FOUND)
				out.conf = path;
			else
				error = Why(r, a);

			continue;
		}

		if(!triedFolder && !haveGame)
		{
			triedFolder = true;

			std::string path;
			const FindResult r = finder.find(a, path, true);

			// A folder with no conf (or no such folder) is what stock DOSBox
			// mounts and runs: not ours. A folder of several confs is.
			if(r == FOUND || r == SEVERAL)
			{
				haveGame = true;
				out.requested = a;

				if(r == FOUND)
					out.conf = path;
				else
					error = Why(r, a);

				continue;
			}
		}

		out.args.push_back(a);
	}

	// Two games named: neither is guessed
	if(!error.empty())
	{
		out.conf.clear();
		out.error = error;
	}

	return out;
}

Bundle Autoboot_FindBundle(const std::vector<std::string> & folders, AutobootHost & host)
{
	Bundle bundle;

	for(size_t i = 0; i < folders.size(); i++)
	{
		const std::string conf = host.resolve(Join(TrimSlash(folders[i]), FOLDER_CONF));

		if(host.isFile(conf))
		{
			bundle.folder = TrimSlash(folders[i]);
			bundle.conf = conf;
			break;
		}
	}

	return bundle;
}

std::string Autoboot_Notice(const std::string & error)
{
	if(error.empty())
		return std::string();

	// 80 columns less "echo " and some margin. DOS echo would act on these.
	std::string text = "Autoboot: " + error;

	for(size_t i = 0; i < text.size(); i++)
	{
		const unsigned char c = (unsigned char)text[i];

		if(c < 0x20 || c > 0x7e || strchr("%<>|&^\"", c))
			text[i] = '?';
	}

	const size_t MAX_TEXT = 72;

	if(text.size() > MAX_TEXT)
	{
		text.resize(MAX_TEXT - 3);
		text += "...";
	}

	return "echo " + text;
}
