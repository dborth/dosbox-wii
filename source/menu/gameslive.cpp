/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * gameslive.cpp
 ***************************************************************************/
#include "gameslive.h"
#include "menubusy.h"
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

// Saving a game: the files are the DOSBox folder's, the rest is the running core's (config and controls are read from memory,
// nothing is written to get them). Main thread, between decoder runs (the menu is).
class LiveGameSave : public FileGameSaveHost
{
	public:
		std::string dosboxDirectory() const override
		{
			return appDrive[0] ? std::string(appDrive) + "/" DOSBOX_DIR_NAME : std::string();
		}

		bool runningConfig(std::string& text, std::string& error) override { return ConfigText(&text, &error); }

		std::string baseConfig() const override { return ConfigSavePath(); }

		bool mapperText(std::string& text) override
		{
			MAPPER_BindsText(text);
			return true;
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

	stale = false;
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

	if(stale)
	{
		// the menu wrote or deleted a file after this scan began: it may have listed the card before that, and
		// would put back what the list already shows is gone (or the old name). Throw it away and look again.
		{
			MutexLock guard(lock);
			pending.clear();
			pendingFound.clear();
			finished = false;
		}
		start();
		return false;
	}

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

// A scan that is running, or has finished and waits for poll(), may have listed the card before this edit
void GamesScanner::edited()
{
	if(worker.isRunning())
	{
		stale = true;
		return;
	}
	MutexLock guard(lock);
	if(finished)
		stale = true;
}

void GamesScanner::put(const GameRecord& game)
{
	Games_Put(current, game);
	edited();
}

void GamesScanner::drop(const std::string& path)
{
	Games_Drop(current, path);
	edited();
}

void GamesScanner::dropFound(const std::string& folder)
{
	Found_Drop(currentFound, folder);
	edited();
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

void Games_Saved(const GameSaveResult& r)
{
	if(r.ok && !r.text.empty())
		Games_Scanner().put(Game_RecordFor(r.path, GameSave_Live().dosboxDirectory(), r.text));
}

void Games_Deleted(const std::string& path)
{
	Games_Scanner().drop(path);
}

GameSaveResult GameSave_Busy(const char * title, const std::string& detail, bool withCore, const std::function<GameSaveResult(GameSaveHost&)>& job)
{
	// what the core is asked is asked here, on the menu thread; the worker is given answers
	GameSaveSnapshot snapshot(GameSave_Live(), withCore);
	GameSaveResult result;

	MenuBusy_Run(title, detail, [&]{ result = job(snapshot); });
	return result;
}
