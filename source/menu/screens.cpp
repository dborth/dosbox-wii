/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * screens.cpp
 *
 * The full-screen lists the home menu opens: the settings (a list of pages,
 * then a page of rows, or all settings by config section), the Drives screen
 * and the Status screen.
 * They are all one ListScreen; a screen is a subclass that says what its
 * rows are and what a click does. Like the rest of the
 * menu this is single-threaded: run() steps the GUI itself with UpdateGui().
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <string>

#include "libgui/Gui.h"
#include "drivers/Platform.h"
#include "drivers/VideoDriver.h"
#include "drivers/InputController.h"
#include "filelist.h"
#include "menu.h"
#include "menupage.h"
#include "menuart.h"
#include "settings.h"
#include "storage.h"
#include "status.h"
#include "memmanager.h"
#include "dosboxsupport.h"
#include "gameslist.h"
#include "gameslive.h"
#include "gamesave.h"
#include "menupopover.h"
#include "menutoggle.h"

/****************************************************************************
 * ListBrowser
 *
 * A GuiOptionBrowser that says which option is highlighted, which it keeps
 * to itself. The screens need it to show help for that option and to step it
 * with left and right.
 ***************************************************************************/
class ListBrowser : public GuiOptionBrowser
{
	public:
		ListBrowser(int w, int h, OptionList * l) : GuiOptionBrowser(w, h, l)
		{
			for(int i = 0; i < OPTION_PAGESIZE; i++)
				optionIndex[i] = -1; // not set until the first update()
		}

		//! The highlighted option's index, or -1 until the list has been drawn.
		int getSelectedOption() const { return optionIndex[selectedItem]; }
};

/****************************************************************************
 * ListSurface
 *
 * What a ListScreen shows its rows on: a GuiOptionBrowser (a name and a value on each row) or a
 * GuiCardBrowser (a card each). The screen asks the same four things of either.
 ***************************************************************************/
class ListSurface
{
	public:
		virtual ~ListSurface() {}
		virtual GuiElement * element() = 0;
		virtual int clicked() = 0;
		virtual int selected() = 0;
		virtual void refresh() = 0;
		//! The rows are worked out when the list is made, so a different number of them means making it anew
		virtual bool remakeOnResize() const = 0;
};

class OptionSurface : public ListSurface
{
	public:
		OptionSurface(OptionList * l) : browser(552, 248, l) {}
		GuiElement * element() override { return &browser; }
		int clicked() override { return browser.getClickedOption(); }
		int selected() override { return browser.getSelectedOption(); }
		void refresh() override { browser.triggerUpdate(); }
		bool remakeOnResize() const override { return true; }

	private:
		ListBrowser browser;
};

class CardSurface : public ListSurface
{
	public:
		CardSurface(GuiCardSource * s) : browser(552, 248, s) {}
		GuiElement * element() override { return &browser; }
		int clicked() override { return browser.getClickedOption(); }
		int selected() override { return browser.getSelectedOption(); }
		void refresh() override { browser.triggerUpdate(); }
		bool remakeOnResize() const override { return false; }	// it asks its source again, whatever the count

	private:
		GuiCardBrowser browser;
};

/****************************************************************************
 * ListScreen
 *
 * A title, a list of rows (a name and a value), and the same header and footer
 * as every other page (menupage.h): what the highlighted row does at the left of
 * the footer, the key hints at the right of the header, Back (A, or B when there is no
 * extra button; otherwise B moves between the list and the footer buttons) at the
 * right of the footer; optionally one more button beside Back. run() puts it up in place of whatever is on the main
 * window and returns when Back is clicked.
 *
 * A subclass supplies the rows and what a click on one does. The help line
 * follows the highlight (rowHelp), or shows a message until the highlight
 * moves (showMessage). Whenever something returns true for "the rows have
 * changed" the rows are read again; if there are a different number of them
 * the browser is made anew, as it works out its first row only when it is
 * constructed.
 ***************************************************************************/
class ListScreen
{
	public:
		virtual ~ListScreen() {}

		//! Runs the screen until it is closed.
		void run();

	protected:
		//! extraLabel: a second button beside Back, or NULL for none.
		//! helpLine: whether there is a help line (a screen that has neither
		//! row help nor messages does without).
		ListScreen(const char * title, const char * hint, const char * extraLabel = NULL, bool helpLine = true);

		//! The number of rows now. More than MAX_OPTIONS are not shown.
		virtual int rowCount() = 0;

		//! A row's name and value (left empty if it has none).
		virtual void rowText(int row, char * name, size_t nameSize, char * value, size_t valueSize) = 0;

		//! A row's help, which is shown while it is highlighted. False if the
		//! screen has none, which leaves the help line alone.
		virtual bool rowHelp(int, char *, size_t) { return false; }

		//! Before the rows are first read / after the screen has come down.
		virtual void onOpen() {}
		virtual void onClose() {}

		//! Once a frame. True if the rows have changed.
		virtual bool onFrame() { return false; }

		//! A (or a click) on a row. True if the rows have changed.
		virtual bool onClick(int) { return false; }

		//! Whether Left and Right step the highlighted row (onStep)
		virtual bool stepsWithArrows() const { return false; }

		//! Left (-1) or Right (1) on the highlighted row. True if the rows have changed.
		virtual bool onStep(int, int) { return false; }

		//! The extra button was clicked.
		virtual void onExtraButton() {}

		//! Ends run() once this frame is done.
		void close() { done = true; }

		//! Text for the help line that stays until the highlight moves.
		void showMessage(const char * text);

		//! Takes the whole screen off the main window, and puts it back as it
		//! was, around code that runs a screen of its own, so that only one has
		//! focus. The highlight is kept.
		void putAway() { detach(); }
		void bringBack() { attach(); }

		//! Around a popover opened over the screen (MenuPopover_Pick / _Ask): off, the screen stays drawn, dimmed, and
		//! takes no input, so the popover is all the D-pad and pointer reach. On again it takes the focus back, with
		//! the highlight where it was.
		void setEnabled(bool on)
		{
			const STATE state = on ? STATE::DEFAULT : STATE::DISABLED;
			browser->element()->setState(state);
			w.setState(state);
			if(on)
				mainWindow->changeFocus(browser->element());
		}

		//! A screen that shows cards instead of name and value rows says where the cards come from
		virtual GuiCardSource * cardSource() { return NULL; }

		//! For a screen that puts its own controls in the header, beside the title (the Games screen's Load | Save)
		UiScope& uiScope() { return ui; }
		GuiLayout& headerRight() { return *chrome.headerRight; }

	private:
		ListScreen(const ListScreen &) = delete;
		ListScreen & operator=(const ListScreen &) = delete;

		void fillRows();
		void rereadRows();
		void attach();
		void detach();

		OptionList * options;
		ListSurface * browser;
		int helpRow;			//!< the row whose help is on the help line; -1: not known
		bool messageShown;		//!< a message is on the help line: leave it until the highlight moves
		bool done;

		// In the order they depend on each other
		UiScope ui;
		GuiOverlay chromePage;
		MenuPageFrame chrome;
		std::string helpStr;			//!< what the highlighted row does (or a message), left of the footer
		std::string hintStr;			//!< the keys, right of the header
		GuiTextButton * backBtn;
		GuiTextButton * extraBtn;
		bool extraClicked;
		bool hasExtra;
		bool hasHelpLine;
		GuiWindow w;
};

ListScreen::ListScreen(const char * title, const char * hint, const char * extraLabel, bool helpLine)
	: options(NULL),
	  browser(NULL),
	  helpRow(-1),
	  messageShown(false),
	  done(false),
	  chrome(),
	  hintStr(hint ? hint : ""),
	  backBtn(NULL),
	  extraBtn(NULL),
	  extraClicked(false),
	  hasExtra(extraLabel != NULL),
	  hasHelpLine(helpLine),
	  w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight())
{
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	// Footer, left: what the highlighted row does. A screen with no help line shows its key hints there instead,
	// and one that has a help line shows them at the right of the header.
	chrome = MenuPage_BuildBars(ui, chromePage, title, [this]{ return hasHelpLine ? helpStr : hintStr; });
	if(hasHelpLine && !hintStr.empty())
		chrome.headerRight->add(ui.label(hintStr.c_str()).fontSize(14).textColor(MenuChromeTextMuted()));

	GuiTextButton& back = MenuPage_Button(ui, "Back");

	// B is the main window's focus toggle (browser <-> footer buttons). With a button beside Back (Save, Rescan)
	// a pad has to be able to reach it, so there B must not also click Back: it would close the screen on the same
	// press. With Back alone, B is Back, as on every other page.
	if(!hasExtra)
		back.setTrigger(1, &cancel);
	backBtn = &back;
	if(hasExtra)
	{
		extraBtn = &MenuPage_Button(ui, extraLabel).onClick([this]{ extraClicked = true; });
		chrome.footerRight->add(*extraBtn);
	}
	chrome.footerRight->add(back);

	ui.attachTo(chromePage);
	w.append(&chromePage);
}

//! Reads every row from the subclass.
void ListScreen::fillRows()
{
	memset(options, 0, sizeof(OptionList));

	options->length = rowCount();
	if(options->length > MAX_OPTIONS)
		options->length = MAX_OPTIONS;

	for(int i = 0; i < options->length; i++)
		rowText(i, options->name[i], sizeof(options->name[i]), options->value[i], sizeof(options->value[i]));
}

//! Puts the screen on the main window. The browser is made on the first call
//! (so the rows must have been read) and kept after that.
void ListScreen::attach()
{
	if(!browser)
	{
		GuiCardSource * cards = cardSource();
		browser = cards ? (ListSurface *) new CardSurface(cards) : (ListSurface *) new OptionSurface(options);
		browser->element()->setPosition(0, 108);
		browser->element()->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	}

	// the browser first: it is what takes focus
	mainWindow->append(browser->element());
	mainWindow->append(&w);
}

void ListScreen::detach()
{
	mainWindow->remove(browser->element());
	mainWindow->remove(&w);
}

//! The rows have changed: read them again, and the help for the highlighted one.
void ListScreen::rereadRows()
{
	const int oldLength = options->length;

	fillRows();

	if(options->length != oldLength && browser->remakeOnResize())
	{
		detach();
		delete browser;
		browser = NULL;
		attach();
		helpRow = -1;
	}
	else
		browser->refresh();

	if(!messageShown)
		helpRow = -1;
}

void ListScreen::showMessage(const char * text)
{
	helpStr = text;
	messageShown = true;
	helpRow = browser->selected();
}

void ListScreen::run()
{
	// about 15 KB: the menu heap, not the stack (this runs deep in the core's
	// call chain) and not permanent memory the emulator would be sharing
	options = (OptionList *) memspace_malloc(sizeof(OptionList));

	if(!options)
		return;

	onOpen();
	fillRows();
	attach();

	done = false;

	while(!done)
	{
		UpdateGui();

		bool changed = onFrame();
		const int clicked = browser->clicked();

		if(extraClicked)
		{
			extraClicked = false;
			onExtraButton();
		}
		else if(clicked >= 0 && clicked < options->length)
		{
			messageShown = false;

			if(onClick(clicked))
				changed = true;
		}

		// the browser does not use left and right itself
		if(!changed && stepsWithArrows() && browser->element()->isFocused() == 1 && browser->selected() >= 0)
		{
			for(int i = 0; i < 4 && !changed; i++)
			{
				if(controller[i]->left())
					changed = onStep(browser->selected(), -1);
				else if(controller[i]->right())
					changed = onStep(browser->selected(), 1);
			}
		}

		if(changed)
			rereadRows();

		const int selected = browser->selected();

		if(selected >= 0 && selected != helpRow)
		{
			char help[320];

			if(rowHelp(selected, help, sizeof(help)))
				helpStr = help;

			helpRow = selected;
			messageShown = false;
		}

		if(backBtn->getState() == STATE::CLICKED)
			done = true;
	}

	detach();
	delete browser;
	browser = NULL;

	onClose();
	memspace_free(options);
	options = NULL;
}

/****************************************************************************
 * SettingsPageScreen
 *
 * One page of the settings registry: a row per setting. A (or clicking)
 * moves the row to its next value, left and right step it back and forth.
 * Every change is applied to DOSBox as it is made. One change can alter other
 * rows (the cycles amount follows the mode, a CPU core may rule out a CPU
 * type), so a change reads them all again.
 ***************************************************************************/
class SettingsPageScreen : public ListScreen
{
	public:
		SettingsPageScreen(int pageIndex) : ListScreen(Title(pageIndex).c_str(), "A or Left/Right: change"), page(pageIndex) {}

	protected:
		int rowCount() override { return Settings_RowCount(page); }

		void rowText(int row, char * name, size_t nameSize, char * value, size_t valueSize) override
		{
			snprintf(name, nameSize, "%s", Settings_RowLabel(page, row));
			Settings_RowValue(page, row, value, valueSize);
		}

		bool rowHelp(int row, char * help, size_t size) override
		{
			Settings_RowHelp(page, row, help, size);
			return true;
		}

		bool onClick(int row) override { return step(row, 1); }
		bool stepsWithArrows() const override { return true; }
		bool onStep(int row, int direction) override { return step(row, direction); }

	private:
		//! Steps the row, and says why a value was skipped or refused
		bool step(int row, int direction)
		{
			const bool changed = Settings_RowStep(page, row, direction);
			const char * why = Settings_TakeRefusal();

			if(why)
				showMessage(why);
			return changed;
		}

		static std::string Title(int pageIndex)
		{
			char title[64];

			snprintf(title, sizeof(title), "Settings - %s", Settings_PageTitle(pageIndex));
			return title;
		}

		int page;
};

/****************************************************************************
 * DrivesScreen
 *
 * A row per storage device with the DOS drive letter it is mounted on, and a
 * last row listing every DOS drive. A (or clicking) mounts or unmounts the
 * device; if that is refused the help line says why. Devices plugged in or
 * removed while the screen is up appear and disappear (see DriveMap_Update).
 ***************************************************************************/
class DrivesScreen : public ListScreen
{
	public:
		DrivesScreen() : ListScreen("Drives", "A: mount / unmount") {}

	protected:
		void onOpen() override { DriveMap_Open(); }
		void onClose() override { DriveMap_Close(); }

		int rowCount() override { return DriveMap_RowCount(); }

		void rowText(int row, char * name, size_t nameSize, char * value, size_t valueSize) override
		{
			DriveMap_RowText(row, name, nameSize, value, valueSize);
		}

		bool rowHelp(int row, char * help, size_t size) override
		{
			DriveMap_RowHelp(row, help, size);
			return true;
		}

		bool onFrame() override { return DriveMap_Update(); }

		bool onClick(int row) override
		{
			const char * refusal = DriveMap_RowAction(row);

			if(refusal)
				showMessage(refusal);

			return true;
		}
};

/****************************************************************************
 * StatusScreen
 *
 * What DOSBox is set to and what it is doing, in groups: a row per fact,
 * the configured value with the value in effect after it in brackets, and a
 * * on anything that is not the default (see status.h). Nothing here can be
 * changed. The state is read when the screen opens and again on A, which is
 * how a controller connected since then shows up. It is opened from the Status
 * tile on the home screen, and from nowhere else.
 ***************************************************************************/
class StatusScreen : public ListScreen
{
	public:
		StatusScreen() : ListScreen("Status", "A: refresh    * not the default") {}

	protected:
		void onOpen() override { Status_Refresh(); }

		int rowCount() override { return Status_RowCount(); }

		void rowText(int row, char * name, size_t nameSize, char * value, size_t valueSize) override
		{
			snprintf(name, nameSize, "%s", Status_RowLabel(row));
			Status_RowValue(row, value, valueSize);
		}

		bool rowHelp(int row, char * help, size_t size) override
		{
			Status_RowHelp(row, help, size);
			return true;
		}

		bool onClick(int) override
		{
			Status_Refresh();
			return true;
		}
};

/****************************************************************************
 * GamesScreen
 *
 * Plain DOSBox, then a row per game found in the DOSBox folder (see games.h),
 * with the program it runs; the one DOSBox was started with says Playing. A
 * asks, in a popover, before it restarts DOSBox into the game or replaces one
 * (gameslist.h). Load and Save, at the right of the header, switch what A does
 * (Left and Right on a row switch them too). The folder is read on the scanner's
 * thread (gameslive.h) while the list is up, and again with Rescan.
 ***************************************************************************/
class GamesScreen : public ListScreen, public GuiCardSource
{
	public:
		~GamesScreen() { delete mode; }

		GamesScreen() : ListScreen("Games", "Left / Right: Load or Save", "Rescan")
		{
			std::vector<std::string> modes;
			modes.push_back("Load");
			modes.push_back("Save");
			mode = new MenuToggle(uiScope(), modes, 0, 64, [this](int chosen){ pendingMode = chosen; });
			headerRight().add(mode->element());
		}

	protected:
		void onOpen() override
		{
			GamesScanner& scanner = Games_Scanner();

			// what the last scan found is there at once; a new scan replaces it
			if(scanner.scannedOnce())
			{
				list.setGames(scanner.games());
				list.setFound(scanner.found());
			}
			list.setCurrent(GetGameConf());
			list.setScanning(true);
			scanner.start();
		}

		void onClose() override { Games_Scanner().finish(); }

		bool onFrame() override
		{
			GamesScanner& scanner = Games_Scanner();

			// the mode was switched (a button was clicked), or a game was saved: the rows are different
			if(pendingMode >= 0)
			{
				setMode(pendingMode == 1);
				pendingMode = -1;
			}

			if(changed)
			{
				changed = false;
				return true;
			}

			if(!scanner.poll())
				return false;

			list.setGames(scanner.games());
			list.setFound(scanner.found());
			list.setScanning(false);
			return true;
		}

		int rowCount() override { return list.rowCount(); }

		void rowText(int row, char * name, size_t nameSize, char * value, size_t valueSize) override
		{
			std::string n, v;
			list.rowText(row, n, v);
			snprintf(name, nameSize, "%s", n.c_str());
			snprintf(value, valueSize, "%s", v.c_str());
		}

		bool rowHelp(int row, char * help, size_t size) override
		{
			snprintf(help, size, "%s", list.rowHelp(row).c_str());
			return true;
		}

		bool onClick(int row) override
		{
			const GamesList::Click c = list.click(row);

			switch(c.what)
			{
				case GamesList::Click::MESSAGE:
					showMessage(c.message.c_str());
					break;
				case GamesList::Click::ASK_LAUNCH:
					if(c.game ? gameMenu(c) : ask(c, "Restart"))
					{
						// the menu closes when this returns (HomeMenu sees the request) and DOSBox restarts into it
						RequestRestartInto(c.conf.empty() ? NULL : c.conf.c_str());
						close();
						return true;
					}
					break;
				case GamesList::Click::SAVE_NEW:
					saveNew();
					break;
				case GamesList::Click::ASK_REPLACE:
					if(ask(c, "Replace"))
						finishSave(Game_SaveReplace(GameSave_Live(), c.conf, c.running));
					else
						showMessage("Not saved.");
					break;
				case GamesList::Click::ADD_FOUND:
					return addFound(c.found);
				default:
					break;
			}

			return false;
		}

		// Rescan: read the DOSBox folder again (after copying a game to the card, say)
		void onExtraButton() override
		{
			list.setScanning(true);
			Games_Scanner().start();
			changed = true;
		}

		// Left is Load and Right is Save, on any row
		bool stepsWithArrows() const override { return true; }

		// The rows are cards
		GuiCardSource * cardSource() override { return this; }

		int cardCount() override { return list.rowCount(); }

		void card(int index, GuiCardInfo& out) override
		{
			GamesList::Card c;
			list.card(index, c);
			out.kind = c.kind == GamesList::Card::CARD ? GuiCardInfo::CARD : c.kind == GamesList::Card::FOUND ? GuiCardInfo::DASHED
				: c.kind == GamesList::Card::HEADER ? GuiCardInfo::HEADER : GuiCardInfo::NOTE;
			out.tile = c.tile;
			out.title = c.title;
			out.detail = c.detail;
			out.action = c.action;
			out.accent = c.accent;
		}

		bool onStep(int, int direction) override
		{
			const bool wantSaving = direction > 0;

			if(wantSaving == list.isSaving())
				return false;

			setMode(wantSaving);
			return true;
		}

	private:
		// Loading (A restarts into a game) or saving (A saves what is running): the rows, the buttons and the help line follow
		void setMode(bool saving)
		{
			if(list.isSaving() == saving)
				return;

			list.setSaving(saving);
			mode->select(saving ? 1 : 0);
			changed = true;
			showMessage(saving ? "Saving what is running. Pick New game, or a game to replace" : "Pick a game to restart into");
		}

		// The question in a popover over the list, which stays drawn and takes no input meanwhile. True for the first answer.
		bool ask(const GamesList::Click& c, const char * yes)
		{
			std::vector<std::string> answers;
			answers.push_back(yes);
			answers.push_back("Cancel");
			setEnabled(false);
			const int answer = MenuPopover_Ask(c.message.c_str(), answers, c.detail, GuiPopover::Anchor());
			setEnabled(true);
			return answer == 0;
		}

		// What to do with a game: restart into it, give it another name, or delete it. One popover, so there is
		// nothing to learn: each choice says what it does. The running game is not offered Delete. True when the
		// choice was Restart, which the caller then does.
		bool gameMenu(const GamesList::Click& c)
		{
			enum { RESTART, RENAME, DELETE, CANCEL };
			std::vector<PickerItem> items;
			std::vector<int> what;

			const std::string file = c.conf.substr(c.conf.find_last_of("/\\") + 1);
			const struct { int id; const char * name; std::string text; } choices[] = {
				{ RESTART, "Restart", c.detail },
				{ RENAME, "Rename", "Change the name the Games list shows. Its files keep their names." },
				{ DELETE, "Delete", "Removes " + file + " and its map from the card. This cannot be undone." },
				{ CANCEL, "Cancel", "Leave it as it is." },
			};
			for(size_t i = 0; i < sizeof(choices) / sizeof(choices[0]); i++)
			{
				if(choices[i].id == DELETE && c.running)
					continue;		// DOSBox is running from it
				PickerItem item;
				item.name = choices[i].name;
				item.current = false;
				MenuPopover_Wrap(item.lines, choices[i].text, 44);
				items.push_back(item);
				what.push_back(choices[i].id);
			}

			setEnabled(false);
			const int chosen = MenuPopover_Pick(GamesList_Fit(c.title, 28).c_str(), items, GuiPopover::Anchor());
			setEnabled(true);

			if(chosen < 0 || (size_t)chosen >= what.size())
				return false;

			if(what[(size_t)chosen] == RENAME)
				renameGame(c);
			else if(what[(size_t)chosen] == DELETE)
				deleteGame(c);

			return what[(size_t)chosen] == RESTART;
		}

		// The title on the on-screen keyboard; only the name the list shows changes
		void renameGame(const GamesList::Click& c)
		{
			char typed[64];
			snprintf(typed, sizeof(typed), "%s", c.title.c_str());

			putAway();		// the on-screen keyboard is a screen of its own
			const bool named = OnScreenKeyboard(typed, sizeof(typed));
			bringBack();

			if(named)
				finishManage(Game_Rename(GameSave_Live(), c.conf, typed));
		}

		// Asks once more, as it cannot be undone
		void deleteGame(const GamesList::Click& c)
		{
			std::vector<std::string> answers;
			answers.push_back("Delete it");
			answers.push_back("Cancel");
			setEnabled(false);
			const bool sure = MenuPopover_Ask(("Delete " + GamesList_Fit(c.title, 22) + "?").c_str(), answers,
				"Its conf and map are removed from the card. This cannot be undone.", GuiPopover::Anchor()) == 0;
			setEnabled(true);

			if(sure)
				finishManage(Game_Delete(GameSave_Live(), c.conf));
			else
				showMessage("Not deleted.");
		}

		// The list is read again (the name sorts somewhere else, or the game is gone) and the result is said
		void finishManage(const GameSaveResult& r)
		{
			if(r.ok)
			{
				list.setScanning(true);
				Games_Scanner().start();
				changed = true;
			}
			showMessage(r.message.c_str());
		}

		// A name on the on-screen keyboard, the program that is running as the suggestion
		void saveNew()
		{
			char typed[64];
			char program[64];
			std::string suggestion = "My game";

			if(GetRunningProgram(program, sizeof(program)))
			{
				std::string stem = program;
				const size_t dot = stem.rfind('.');
				if(dot != std::string::npos)
					stem.resize(dot);
				stem = Game_FileName(stem);
				if(!stem.empty())
					suggestion = stem;
			}
			snprintf(typed, sizeof(typed), "%s", suggestion.c_str());

			putAway();		// the on-screen keyboard is a screen of its own
			const bool named = OnScreenKeyboard(typed, sizeof(typed));
			bringBack();

			if(!named)
				return;

			GameSaveResult r = Game_SaveNew(GameSave_Live(), typed);

			if(r.exists)
			{
				// the same name: Replace asks the question and goes ahead
				std::vector<std::string> answers;
				answers.push_back("Replace it");
				answers.push_back("Cancel");
				setEnabled(false);
				const bool replace = MenuPopover_Ask(r.message.c_str(), answers,
					"Replacing it overwrites its saved controls, settings and startup with what is running.", GuiPopover::Anchor()) == 0;
				setEnabled(true);

				if(replace)
					finishSave(Game_SaveReplace(GameSave_Live(), r.path, sameGame(r.path)));
				else
					showMessage("Not saved.");
				return;
			}

			finishSave(r);
		}

		// A folder found on the card, as a game. The question is a popover over the list: the name, the program when the
		// folder has several, and what is written. Add leaves the list up with the new game in it, so several can be added in a row.
		// True when the rows changed or the screen is closing.
		bool addFound(const FoundGame& found)
		{
			std::string title = found.title;
			size_t program = 0;

			while(true)
			{
				const std::string confPath = Game_FoundConfPath(GameSave_Live(), found, title);
				const std::string confName = confPath.empty() ? std::string("a game") : confPath.substr(confPath.find_last_of('/') + 1);
				const std::string runs = found.programs[program];

				std::vector<PickerItem> items;
				PickerItem add;
				add.name = "Add";
				add.current = false;
				MenuPopover_Wrap(add.lines, "Adds " + confName + ": a startup that runs " + runs + ". Its controls and settings start from your defaults", 44);
				items.push_back(add);

				PickerItem rename;
				rename.name = "Name   " + title;
				rename.current = false;
				MenuPopover_Wrap(rename.lines, "A to rename. It is the name in the Games list", 44);
				items.push_back(rename);

				const bool several = found.programs.size() > 1;
				if(several)
				{
					PickerItem pick;
					pick.name = "Runs   " + runs;
					pick.current = false;
					MenuPopover_Wrap(pick.lines, "A to choose (" + std::to_string(found.programs.size()) + " found in DOSBox/" + found.folder + ")", 44);
					items.push_back(pick);
				}

				PickerItem cancel;
				cancel.name = "Cancel";
				cancel.current = false;
				MenuPopover_Wrap(cancel.lines, "Nothing is added", 44);
				items.push_back(cancel);

				const std::string question = "Add " + (title.size() > 28 ? title.substr(0, 26) + ".." : title) + " as a game?";
				setEnabled(false);
				const int choice = MenuPopover_Pick(question.c_str(), items, GuiPopover::Anchor());
				setEnabled(true);

				// the choices are Add, Name, Runs (when there is one), then Cancel
				if(choice == 1)
				{
					char typed[64];
					snprintf(typed, sizeof(typed), "%s", title.c_str());
					putAway();		// the on-screen keyboard is a screen of its own
					const bool named = OnScreenKeyboard(typed, 41);
					bringBack();
					const std::string trimmed = typed;
					const size_t first = trimmed.find_first_not_of(' ');
					if(named && first != std::string::npos)
						title = trimmed.substr(first, trimmed.find_last_not_of(' ') - first + 1);
					continue;
				}

				if(several && choice == 2)
				{
					std::vector<PickerItem> programs;
					for(size_t i = 0; i < found.programs.size(); i++)
					{
						PickerItem p;
						p.name = found.programs[i];
						p.current = i == program;
						programs.push_back(p);
					}
					setEnabled(false);
					const int chosen = MenuPopover_Pick("Program to run", programs, GuiPopover::Anchor());
					setEnabled(true);
					if(chosen >= 0)
						program = (size_t)chosen;
					continue;
				}

				if(choice != 0)
					return false;		// Cancel, or the popover was closed

				const GameSaveResult r = Game_AddFound(GameSave_Live(), found, title, found.programs[program]);
				if(!r.ok)
				{
					showMessage(r.message.c_str());
					return false;
				}

				list.setScanning(true);
				Games_Scanner().start();
				changed = true;
				showMessage(r.message.c_str());
				return true;
			}
		}

		// The game that is running was saved: the list shows it, and it is the one running
		void finishSave(const GameSaveResult& r)
		{
			if(r.ok)
			{
				SetGameConf(r.path.c_str());
				list.setCurrent(r.path);
				list.setScanning(true);
				Games_Scanner().start();
				changed = true;
			}
			showMessage(r.message.c_str());
		}

		bool sameGame(const std::string& path) const
		{
			GameRecord g;
			g.path = path;
			return GamesList::sameConf(g, GetGameConf());
		}

		GamesList list;
		bool changed = false;
		int pendingMode = -1;			//!< 0 or 1 when Load or Save was clicked: done at the next frame
		MenuToggle * mode = NULL;		//!< Load | Save, in the header
};

/****************************************************************************
 * SettingsListScreen
 *
 * A list of settings pages, one row per page. A (or clicking) opens the page.
 *
 * Two lists use it. The first (sections = false) is the curated pages followed
 * by "Drives" and "All settings", which opens the second: one row
 * per config section, each opening its all-settings page. The second one is
 * opened from inside the first, so each level has its own OptionList. Only
 * the first has the Save button, whose result goes where the pages show help.
 ***************************************************************************/
class SettingsListScreen : public ListScreen
{
	public:
		SettingsListScreen(bool allSettings)
			: ListScreen(allSettings ? "Settings - All settings" : "Settings", allSettings ? "A: open" : "A: open    B: Save / Back", allSettings ? NULL : "Save"),
			  sections(allSettings),
			  pageCount(allSettings ? Settings_AllSectionCount() : SETTINGS_PAGE_COUNT) {}

	protected:
		int rowCount() override { return pageCount + (sections ? 0 : 2); }

		void rowText(int row, char * name, size_t nameSize, char *, size_t) override
		{
			if(row < pageCount)
				snprintf(name, nameSize, "%s", Settings_PageTitle(sections ? Settings_AllPage(row) : row));
			else if(row == pageCount && !sections)
				snprintf(name, nameSize, "Drives");
			else
				snprintf(name, nameSize, "All settings");
		}

		bool onClick(int row) override
		{
			// the list stays up behind the page; take it off so only one has focus
			putAway();

			if(!sections && row == pageCount)
			{
				DrivesScreen screen;
				screen.run();
			}
			else if(row >= pageCount)
			{
				SettingsListScreen screen(true);
				screen.run();
			}
			else
			{
				SettingsPageScreen screen(sections ? Settings_AllPage(row) : row);
				screen.run();
			}

			bringBack();
			return false;
		}

		void onExtraButton() override
		{
			// writes a few KB to the card, which is quick enough not to need the
			// worker thread; if it ever hitches the menu on a slow USB drive it should
			char message[320];

			Settings_Save(message, sizeof(message));
			showMessage(message);
		}

	private:
		bool sections;
		int pageCount;
};

/****************************************************************************
 * SettingsMenu
 *
 * Opens the settings in place of the home screen, which is taken off the
 * main window while they are up: the list of pages, then the chosen page.
 ***************************************************************************/
void SettingsMenu(GuiWindow * home)
{
	mainWindow->remove(home);

	{
		SettingsListScreen list(false);
		list.run();
	}

	mainWindow->append(home);
}

/****************************************************************************
 * GamesMenu
 *
 * Opens the games list in place of the home screen, as SettingsMenu does for the
 * settings. When a game was chosen RestartRequested() is true afterwards.
 ***************************************************************************/
void GamesMenu(GuiWindow * home)
{
	mainWindow->remove(home);

	{
		GamesScreen screen;
		screen.run();
	}

	mainWindow->append(home);
}

/****************************************************************************
 * StatusMenu
 *
 * Opens the status page in place of the home screen, as SettingsMenu does
 * for the settings.
 ***************************************************************************/
void StatusMenu(GuiWindow * home)
{
	mainWindow->remove(home);

	{
		StatusScreen screen;
		screen.run();
	}

	mainWindow->append(home);
}
