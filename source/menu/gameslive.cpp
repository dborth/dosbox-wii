/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * gameslive.cpp
 ***************************************************************************/
#include "gameslive.h"
#include "gamesave.h"
#include "dosboxsupport.h"
#include "preferences.h"
#include "storage.h"

namespace {

class LiveGames : public FileGamesHost
{
	public:
		// <app drive>/DOSBox: where dosbox.conf is, so a conf here is found by name as well as by path
		std::string directory() const override
		{
			return appDrive[0] ? std::string(appDrive) + "/" DOSBOX_DIR_NAME : std::string();
		}
};

// A listing and a few small reads: not much stack, but std::string and dirent are not free
const uint32_t kStackSize = 32 * 1024;

} // namespace

GamesHost& Games_Live()
{
	static LiveGames live;
	return live;
}

namespace {

// Saving a game: the files are the DOSBox folder's, the rest is the running core's. Main thread, between decoder runs (the menu is).
class LiveGameSave : public FileGameSaveHost
{
	public:
		std::string dosboxDirectory() const override
		{
			return appDrive[0] ? std::string(appDrive) + "/" DOSBOX_DIR_NAME : std::string();
		}

		bool runningConfig(std::string& text, std::string& error) override { return ConfigText(&text, &error); }

		std::string baseConfig() const override { return ConfigSavePath(); }

		bool saveMapper() override { return MAPPER_SaveBindsNow(); }

		std::string mapperFile() const override
		{
			char path[256];
			return GetMapperFilePath(path, sizeof(path)) ? std::string(path) : std::string();
		}

		std::string joystickType() const override { return GetJoystickTypeName(); }

		std::string runningProgramPath() const override
		{
			char path[256];
			return GetRunningProgramPath(path, sizeof(path)) ? std::string(path) : std::string();
		}

		std::vector<GameDrive> drives() const override
		{
			std::vector<GameDrive> out;
			char info[256];

			for(char letter = 'A'; letter <= 'Y'; letter++)
			{
				if(!GetDOSDriveInfo(letter, info, sizeof(info)))
					continue;

				GameDrive d;
				d.letter = letter;
				d.info = info;
				out.push_back(d);
			}
			return out;
		}
};

} // namespace

GameSaveHost& GameSave_Live()
{
	static LiveGameSave live;
	return live;
}

void * GamesScanner::Main(void * self)
{
	GamesScanner * s = (GamesScanner *)self;
	GamesScan scan = Games_ScanAll(Games_Live());

	MutexLock guard(s->lock);
	s->pending.swap(scan.games);
	s->pendingFound.swap(scan.found);
	s->finished = true;
	return nullptr;
}

void GamesScanner::start()
{
	if(worker.isRunning())
		return;

	{
		MutexLock guard(lock);
		finished = false;
	}

	if(!worker.start(Main, this, kStackSize, ThreadPriority::Low))
	{
		// no thread to be had: the scan is short, do it here and let poll() pick it up
		Main(this);
	}
}

bool GamesScanner::poll()
{
	bool done;
	{
		MutexLock guard(lock);
		done = finished;
	}
	if(!done)
		return false;

	if(worker.isRunning())
		worker.join();

	{
		MutexLock guard(lock);
		current.swap(pending);
		pending.clear();
		currentFound.swap(pendingFound);
		pendingFound.clear();
		finished = false;
	}
	scanned = true;
	return true;
}

void GamesScanner::finish()
{
	if(worker.isRunning())
		worker.join();
}

GamesScanner& Games_Scanner()
{
	static GamesScanner scanner;
	return scanner;
}
