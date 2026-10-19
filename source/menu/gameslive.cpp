/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * gameslive.cpp
 ***************************************************************************/
#include "gameslive.h"
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

void * GamesScanner::Main(void * self)
{
	GamesScanner * s = (GamesScanner *)self;
	std::vector<GameRecord> found = Games_Scan(Games_Live());

	MutexLock guard(s->lock);
	s->pending.swap(found);
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
