/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * gameslive.h
 *
 * The games scan on this console: the DOSBox folder on the app drive, and a
 * scanner that does the listing and reading on a worker thread, so a slow card
 * or a long list does not stall the menu. The menu starts it when the Games
 * page opens and polls it from its own loop (UpdateGui() keeps running).
 ***************************************************************************/
#ifndef _GAMESLIVE_H_
#define _GAMESLIVE_H_

#include "games.h"
#include "drivers/ThreadDriver.h"
#include "drivers/Thread.h"

/** The host over the app drive's DOSBox folder. */
GamesHost& Games_Live();

class GamesScanner
{
	public:
		GamesScanner() : finished(false), scanned(false) {}

		/** Starts a scan unless one is running. If no thread can be made it scans here and now (the result is ready at the next poll()). */
		void start();
		/** True while a scan is running on its thread. A finished scan's result waits for poll(). */
		bool busy() const { return worker.isRunning(); }
		/** Call from the menu loop. True once each time a finished scan has replaced games(). */
		bool poll();
		/** The result of the last finished scan; empty until there was one. */
		const std::vector<GameRecord>& games() const { return current; }
		/** Whether a scan has finished since the app started, even if it found nothing. */
		bool scannedOnce() const { return scanned; }
		/** Stops waiting for a running scan (joins it; a scan is short and cannot be cut off mid-listing). For shutting down. */
		void finish();

	private:
		static void * Main(void * self);

		Thread worker;
		Mutex lock;
		std::vector<GameRecord> pending;	// the worker's result, until poll() takes it
		bool finished;				// the worker is done (under lock)
		std::vector<GameRecord> current;
		bool scanned;
};

GamesScanner& Games_Scanner();

#endif
