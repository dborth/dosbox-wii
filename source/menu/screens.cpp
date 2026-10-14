/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * screens.cpp
 *
 * The full-screen lists the home menu opens: the settings (a list of pages,
 * then a page of rows, or all settings by config section), the Buttons page
 * and the lists it chooses from, the Drives screen and the Status screen.
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
#include "buttons.h"
#include "memmanager.h"

static const PixelColor kWhite = {255, 255, 255, 255};
static const PixelColor kGrey = {200, 200, 200, 255};
static const PixelColor kBlack = {0, 0, 0, 255};

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
 * ListScreen
 *
 * A title, a list of rows (a name and a value), a help line under it, a hint
 * in the corner and Go Back, which is A or B; optionally one more button
 * beside Go Back. run() puts it up in place of whatever is on the main
 * window and returns when Go Back is clicked.
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
		//! extraLabel: a second button beside Go Back, or NULL for none.
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

	private:
		ListScreen(const ListScreen &) = delete;
		ListScreen & operator=(const ListScreen &) = delete;

		void fillRows();
		void rereadRows();
		void attach();
		void detach();

		OptionList * options;
		ListBrowser * browser;
		int helpRow;			//!< the row whose help is on the help line; -1: not known
		bool messageShown;		//!< a message is on the help line: leave it until the highlight moves
		bool done;

		// In the order they depend on each other
		GuiText titleTxt;
		GuiText helpTxt;
		GuiText hintTxt;
		GuiSound btnSoundOver;
		GuiSound btnSoundClick;
		GuiTrigger trigA;
		GuiTrigger trigB;
		GuiText backBtnTxt;
		MenuButtonShape backBtnImg;
		GuiButton backBtn;
		GuiText extraBtnTxt;
		MenuButtonShape extraBtnImg;
		GuiButton extraBtn;
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
	  titleTxt(title, 28, kWhite),
	  helpTxt("", 18, kWhite),
	  hintTxt(hint, 16, kGrey),
	  btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND::PCM),
	  btnSoundClick(button_click_pcm, button_click_pcm_size, SOUND::PCM),
	  backBtnTxt("Go Back", 22, kBlack),
	  backBtnImg(MenuButtonShape::NORMAL),
	  backBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H),
	  extraBtnTxt(extraLabel ? extraLabel : "", 22, kBlack),
	  extraBtnImg(MenuButtonShape::NORMAL),
	  extraBtn(MENU_BTN_NORMAL_W, MENU_BTN_NORMAL_H),
	  hasExtra(extraLabel != NULL),
	  hasHelpLine(helpLine),
	  w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight())
{
	titleTxt.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	titleTxt.setPosition(MENU_PAGE_MARGIN, 32);	// on the header bar

	helpTxt.setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	helpTxt.setPosition(0, 374);	// the top of the footer bar
	helpTxt.setMaxWidth(540);
	helpTxt.setScroll(SCROLL::HORIZONTAL);

	hintTxt.setAlignment(ALIGN_H::RIGHT, ALIGN_V::BOTTOM);
	hintTxt.setPosition(-50, -50);

	trigA.setPrimaryTrigger();
	trigB.setSecondaryTrigger();

	backBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	backBtn.setPosition(100, -35);
	backBtn.setLabel(&backBtnTxt);
	backBtn.setImage(&backBtnImg);
	backBtn.setSoundOver(&btnSoundOver);
	backBtn.setSoundClick(&btnSoundClick);
	backBtn.setTrigger(&trigA);
	backBtn.setTrigger(&trigB);
	backBtn.setEffectGrow();

	// beside Go Back, and A only
	extraBtn.setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
	extraBtn.setPosition(100 + MENU_BTN_NORMAL_W + 24, -35);
	extraBtn.setLabel(&extraBtnTxt);
	extraBtn.setImage(&extraBtnImg);
	extraBtn.setSoundOver(&btnSoundOver);
	extraBtn.setSoundClick(&btnSoundClick);
	extraBtn.setTrigger(&trigA);
	extraBtn.setEffectGrow();

	w.append(&backBtn);
	if(hasExtra)
		w.append(&extraBtn);
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
		browser = new ListBrowser(552, 248, options);
		browser->setPosition(0, 108);
		browser->setAlignment(ALIGN_H::CENTRE, ALIGN_V::TOP);
	}

	// the browser first: it is what takes focus
	mainWindow->append(browser);
	mainWindow->append(&w);
	mainWindow->append(&titleTxt);
	if(hasHelpLine)
		mainWindow->append(&helpTxt);
	mainWindow->append(&hintTxt);
}

void ListScreen::detach()
{
	mainWindow->remove(browser);
	mainWindow->remove(&w);
	mainWindow->remove(&titleTxt);
	if(hasHelpLine)
		mainWindow->remove(&helpTxt);
	mainWindow->remove(&hintTxt);
}

//! The rows have changed: read them again, and the help for the highlighted one.
void ListScreen::rereadRows()
{
	const int oldLength = options->length;

	fillRows();

	if(options->length != oldLength)
	{
		detach();
		delete browser;
		browser = NULL;
		attach();
		helpRow = -1;
	}
	else
		browser->triggerUpdate();

	if(!messageShown)
		helpRow = -1;
}

void ListScreen::showMessage(const char * text)
{
	helpTxt.setText(text);
	messageShown = true;
	helpRow = browser->getSelectedOption();
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
		const int clicked = browser->getClickedOption();

		if(hasExtra && extraBtn.getState() == STATE::CLICKED)
		{
			extraBtn.resetState();
			onExtraButton();
		}
		else if(clicked >= 0 && clicked < options->length)
		{
			messageShown = false;

			if(onClick(clicked))
				changed = true;
		}

		// the browser does not use left and right itself
		if(!changed && stepsWithArrows() && browser->isFocused() == 1 && browser->getSelectedOption() >= 0)
		{
			for(int i = 0; i < 4 && !changed; i++)
			{
				if(controller[i]->left())
					changed = onStep(browser->getSelectedOption(), -1);
				else if(controller[i]->right())
					changed = onStep(browser->getSelectedOption(), 1);
			}
		}

		if(changed)
			rereadRows();

		const int selected = browser->getSelectedOption();

		if(selected >= 0 && selected != helpRow)
		{
			char help[320];

			if(rowHelp(selected, help, sizeof(help)))
				helpTxt.setText(help);

			helpRow = selected;
			messageShown = false;
		}

		if(backBtn.getState() == STATE::CLICKED)
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

		bool onClick(int row) override { return Settings_RowStep(page, row, 1); }
		bool stepsWithArrows() const override { return true; }
		bool onStep(int row, int direction) override { return Settings_RowStep(page, row, direction); }

	private:
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
 * how a controller connected since then shows up.
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
 * ChooserScreen
 *
 * One of the Buttons page's lists (what to do, which key, which hotkey):
 * a click chooses the row, and Go Back or B chooses nothing.
 ***************************************************************************/
class ChooserScreen : public ListScreen
{
	public:
		ChooserScreen(const char * title, int whichList)
			: ListScreen(title, "A: choose    B: cancel", NULL, false), list(whichList), chosenItem(-1) {}

		//! The row chosen, or -1 if the screen was left without choosing.
		int chosen() const { return chosenItem; }

	protected:
		int rowCount() override { return Buttons_ListCount(list); }

		void rowText(int row, char * name, size_t nameSize, char *, size_t) override
		{
			snprintf(name, nameSize, "%s", Buttons_ListLabel(list, row));
		}

		bool onClick(int row) override
		{
			chosenItem = row;
			close();
			return false;
		}

	private:
		int list;
		int chosenItem;
};

//! Returns the item chosen, or -1 if the list was left with B or Go Back.
static int ChooseFromList(const char * title, int list)
{
	ChooserScreen screen(title, list);

	screen.run();
	return screen.chosen();
}

//! What a button should do: the action list, which opens the key list or the
//! hotkey list for two of its items. Fills message with what happened (it is
//! left empty if nothing was chosen).
static void ChooseButtonAction(int row, char * message, size_t size)
{
	char title[64];

	message[0] = 0;
	snprintf(title, sizeof(title), "Buttons - %s", Buttons_RowLabel(row));

	int item = ChooseFromList(title, BUTTONS_LIST_ACTIONS);

	if(item < 0)
		return;

	const int result = Buttons_Pick(row, BUTTONS_LIST_ACTIONS, item, message, size);

	if(result != BUTTONS_PICK_OPEN_KEYS && result != BUTTONS_PICK_OPEN_HOTKEYS)
		return;

	const int list = (result == BUTTONS_PICK_OPEN_KEYS) ? BUTTONS_LIST_KEYS : BUTTONS_LIST_HOTKEYS;

	item = ChooseFromList(title, list);

	if(item >= 0)
		Buttons_Pick(row, list, item, message, size);
}

/****************************************************************************
 * ButtonsScreen
 *
 * What each button of player 1's pad does in DOS: a row per button, its
 * action as the value. A on a button opens the choices; a change takes effect
 * at once and is saved to the mapper file (see buttons.h). The rows never
 * change in number, so the browser is kept, and keeps the highlight, while
 * the choices are up.
 ***************************************************************************/
class ButtonsScreen : public ListScreen
{
	public:
		ButtonsScreen() : ListScreen("Buttons - Player 1", "A: change") {}

	protected:
		void onOpen() override { Buttons_Refresh(); }

		int rowCount() override { return Buttons_RowCount(); }

		void rowText(int row, char * name, size_t nameSize, char * value, size_t valueSize) override
		{
			snprintf(name, nameSize, "%s", Buttons_RowLabel(row));
			Buttons_RowValue(row, value, valueSize);
		}

		bool rowHelp(int row, char * help, size_t size) override
		{
			Buttons_RowHelp(row, help, size);
			return true;
		}

		bool onClick(int row) override
		{
			char message[320];

			message[0] = 0;

			const int kind = Buttons_RowKind(row);

			if(kind == BUTTONS_ROW_BUTTON)
			{
				// the screen stays up behind the choices; take it off so only one has focus
				putAway();
				ChooseButtonAction(row, message, sizeof(message));
				bringBack();
			}
			else if(kind == BUTTONS_ROW_RESTORE)
			{
				// writes a small file to the card, as the Save button does
				Buttons_RestoreAll(message, sizeof(message));
			}

			Buttons_Refresh();

			// what happened stays where the help is until the highlight moves
			if(message[0])
				showMessage(message);

			return true;
		}
};

/****************************************************************************
 * SettingsListScreen
 *
 * A list of settings pages, one row per page. A (or clicking) opens the page.
 *
 * Two lists use it. The first (sections = false) is the curated pages followed
 * by "Buttons", "Drives", "Status" and "All settings", which opens the second: one row
 * per config section, each opening its all-settings page. The second one is
 * opened from inside the first, so each level has its own OptionList. Only
 * the first has the Save button, whose result goes where the pages show help.
 ***************************************************************************/
class SettingsListScreen : public ListScreen
{
	public:
		SettingsListScreen(bool allSettings)
			: ListScreen(allSettings ? "Settings - All settings" : "Settings", "A: open", allSettings ? NULL : "Save"),
			  sections(allSettings),
			  pageCount(allSettings ? Settings_AllSectionCount() : SETTINGS_PAGE_COUNT) {}

	protected:
		int rowCount() override { return pageCount + (sections ? 0 : 4); }

		void rowText(int row, char * name, size_t nameSize, char *, size_t) override
		{
			if(row < pageCount)
				snprintf(name, nameSize, "%s", Settings_PageTitle(sections ? Settings_AllPage(row) : row));
			else if(row == pageCount && !sections)
				snprintf(name, nameSize, "Buttons");
			else if(row == pageCount + 1 && !sections)
				snprintf(name, nameSize, "Drives");
			else if(row == pageCount + 2 && !sections)
				snprintf(name, nameSize, "Status");
			else
				snprintf(name, nameSize, "All settings");
		}

		bool onClick(int row) override
		{
			// the list stays up behind the page; take it off so only one has focus
			putAway();

			if(!sections && row == pageCount)
			{
				ButtonsScreen screen;
				screen.run();
			}
			else if(!sections && row == pageCount + 1)
			{
				DrivesScreen screen;
				screen.run();
			}
			else if(!sections && row == pageCount + 2)
			{
				StatusScreen screen;
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
