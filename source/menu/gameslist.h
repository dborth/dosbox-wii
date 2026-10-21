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
 * A on a game restarts DOSBox into it, which loses what is running, so it does not act: it says what
 * to ask (Click::ASK_LAUNCH), and the screen puts the question in a popover.
 *
 * Below the games, when launching, come the folders found on the card that no game covers yet (games.h, FoundGame):
 *
 *     Found on your card
 *     WOLF3D                  Add
 *
 * A on one is not a press-twice: it says to ask (the screen opens the question), as adding does not lose anything.
 *
 * Saving is the same list the other way round: a first row that saves what is running
 * as a new game, then every game, each of which A replaces after a question
 * (Click::ASK_REPLACE), as that overwrites the game's file.
 *
 *     New game                Save new
 *     Commander Keen 4        Save here        <- the game DOSBox was started with
 *     Doom                    Replace
 ***************************************************************************/
#ifndef _GAMESLIST_H_
#define _GAMESLIST_H_

#include "games.h"
#include <string>
#include <vector>

class GamesList
{
	public:
		GamesList() : scanning(false), saving(false), limit(148) {}

		/** The games found (kept as given; the rows show the first `limit`). */
		void setGames(const std::vector<GameRecord>& found);
		/** The folders found that could be games (kept as given; the rows show the first ones that fit). Not shown while saving. */
		void setFound(const std::vector<FoundGame>& folders);
		/** A scan is running: a row says so. */
		void setScanning(bool on) { scanning = on; }
		/** The game conf DOSBox is running now (GetGameConf()): "" for none, else a path as given to it. */
		void setCurrent(const std::string& conf) { current = conf; }
		/** The list is for saving what is running (a New game row first, no Plain DOSBox) instead of launching. */
		void setSaving(bool on) { saving = on; }
		bool isSaving() const { return saving; }
		/** How many game rows are shown at most (the screen's list holds MAX_OPTIONS, and some rows are not games). */
		void setLimit(size_t rows) { limit = rows; }

		int rowCount() const;
		/** A row's text for the list, cut to fit it. */
		void rowText(int row, std::string& name, std::string& value) const;
		/** What the help line says while the row is highlighted. */
		std::string rowHelp(int row);

		struct Click
		{
			enum What { NOTHING, MESSAGE, ASK_LAUNCH, SAVE_NEW, ASK_REPLACE, ADD_FOUND } what;
			std::string message;	/**< For MESSAGE: what the help line says. For ASK_*: the question (the popover's title) */
			std::string detail;	/**< For ASK_*: what it does, a sentence or two */
			std::string conf;	/**< For ASK_LAUNCH: the conf to run ("" for plain DOSBox). For ASK_REPLACE: the conf to replace */
			bool running;		/**< For ASK_REPLACE: it is the game DOSBox is running (its startup is kept) */
			FoundGame found;	/**< For ADD_FOUND: the folder to add */
			Click() : what(NOTHING), running(false) {}
		};
		/** A (or a click) on a row. */
		Click click(int row);

		/** Whether a game is the conf DOSBox is running now: given as its whole path, or relative to the DOSBox folder ("Games/keen.conf"). */
		static bool sameConf(const GameRecord& game, const std::string& current);

	private:
		enum Kind { PLAIN, NEW_GAME, GAME, FOUND_HEADER, FOUND, SCANNING, NONE_FOUND, MORE };
		struct Row { Kind kind; int game; };

		std::vector<Row> rows() const;
		bool isCurrent(const GameRecord& g) const { return sameConf(g, current); }
		std::string gameHelp(const GameRecord& g) const;
		std::string foundHelp(const FoundGame& f) const;

		std::vector<GameRecord> games;
		std::vector<FoundGame> found;
		bool scanning;
		bool saving;
		std::string current;
		size_t limit;
};

#endif
