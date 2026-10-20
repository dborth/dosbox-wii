/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * gameslist.h
 *
 * The rows of the Games screen and what pressing A on one does, apart from the
 * screen itself (screens.cpp has the ListScreen over this), so it tests on a PC.
 *
 *     Plain DOSBox            dosbox.conf alone
 *     Commander Keen 4        KEEN4E.EXE
 *     Doom                    RUNNING          <- the game DOSBox was started with
 *     ...
 *
 * A launches a game, which restarts DOSBox and loses what is running, so it takes two
 * presses on the same row: the first only says so (the help line). Moving to another
 * row starts over.
 ***************************************************************************/
#ifndef _GAMESLIST_H_
#define _GAMESLIST_H_

#include "games.h"
#include <string>
#include <vector>

class GamesList
{
	public:
		GamesList() : scanning(false), armed(-1), limit(148) {}

		/** The games found (kept as given; the rows show the first `limit`). */
		void setGames(const std::vector<GameRecord>& found);
		/** A scan is running: a row says so. */
		void setScanning(bool on) { scanning = on; armed = -1; }
		/** The game conf DOSBox is running now (GetGameConf()): "" for none, else a path as given to it. */
		void setCurrent(const std::string& conf) { current = conf; armed = -1; }
		/** How many game rows are shown at most (the screen's list holds MAX_OPTIONS, and some rows are not games). */
		void setLimit(size_t rows) { limit = rows; }

		int rowCount() const;
		/** A row's text for the list, cut to fit it. */
		void rowText(int row, std::string& name, std::string& value) const;
		/** What the help line says while the row is highlighted. Moving the highlight undoes a first press of A. */
		std::string rowHelp(int row);

		struct Click
		{
			enum What { NOTHING, MESSAGE, LAUNCH } what;
			std::string message;	/**< For MESSAGE: what the help line says */
			std::string conf;	/**< For LAUNCH: the conf to run ("" for plain DOSBox) */
			Click() : what(NOTHING) {}
		};
		/** A (or a click) on a row. */
		Click click(int row);

		/** Whether a game is the conf DOSBox is running now: given as its whole path, or relative to the DOSBox folder ("Setups/keen.conf"). */
		static bool sameConf(const GameRecord& game, const std::string& current);

	private:
		enum Kind { PLAIN, GAME, SCANNING, NONE_FOUND, MORE };
		struct Row { Kind kind; int game; };

		std::vector<Row> rows() const;
		bool isCurrent(const GameRecord& g) const { return sameConf(g, current); }
		std::string gameHelp(const GameRecord& g) const;

		std::vector<GameRecord> games;
		bool scanning;
		std::string current;
		int armed;
		size_t limit;
};

#endif
