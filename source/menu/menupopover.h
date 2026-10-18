/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menupopover.h
 *
 * The menu's popover look: the one light panel every question asked in place
 * uses (see popover.h for where it goes and how it closes). A row, a rule and
 * a close button to build a panel from, and PickerView, the list of choices.
 ***************************************************************************/
#ifndef _MENUPOPOVER_H_
#define _MENUPOPOVER_H_

#include "popover.h"

/** The space the menu's popovers keep clear: the header and footer bars and the page margin. */
PopoverInsets MenuPopover_Insets();

/** One row of a popover list: the name on the left, a dot on the right when `marked` (the one in use). The caller adds onClick / onFocus. */
GuiCard& MenuPopover_Row(UiScope& ui, int width, int height, const std::string& title, bool marked, bool dotSlot = true);

/**
 * A small round close button (an X) for the right end of a popover's title row: a way out that does not depend on
 * B or on finding empty space to click. `onClick` is what it does; pass the backdrop's dismiss().
 */
GuiCard& MenuPopover_CloseButton(UiScope& ui, std::function<void()> onClick);

/** A one pixel rule `w` wide, in the outline color. */
GuiShape& MenuPopover_Divider(UiScope& ui, int w);

/** One line of a picker's detail pane: what a button does under that choice. */
struct PickerRow {
	std::string label;	/**< "A Button" */
	std::string value;	/**< "Key: Ctrl" */
};

/** A choice in a picker. */
struct PickerItem {
	std::string name;					/**< "Arrow keys" */
	std::vector<std::string> lines;		/**< What it is, a few short lines (at most three are shown) */
	std::vector<PickerRow> rows;		/**< What it does for the buttons (at most seven are shown) */
	bool current;						/**< The one in use now */
	bool divider = false;				/**< A rule is drawn after this item: what follows is a different kind of choice */
};

/**
 * A list of choices in a popover: one flat panel beside the control that asked, over the screen it came from. Plain
 * rows (the one in use has a dot), a rule where an item says so, and under the list a caption for the row under the
 * cursor: its description. Nothing else is drawn on it.
 */
class PickerView : public GuiOverlay
{
	public:
		/** `anchor` is the control it opens beside; with none it is centred. B or a click outside closes it (backButton()). */
		PickerView(const char * title, const std::vector<PickerItem>& items, const PopoverAnchor& anchor = PopoverAnchor());
		GuiButton& backButton() { return *backBtn; }
		/** Whether a choice was made (after any confirmation). */
		bool chosen() const { return done; }
		/** The index of the item chosen. */
		int result() const { return picked; }
		/** The item under the cursor. */
		int focused() const { return focus; }
		/** What A does on an item: it is chosen. */
		void choose(int index);
		/** The cursor moved to an item . */
		void focusItem(int index);
		/** Puts the selection on the item in use (or the first), whatever it was on: once the popover has the focus. */
		void selectInUse();
		/** Puts the selection (and the cursor) on an item. */
		void selectItem(int index);
		/** Called with an item's index whenever the cursor moves to it, and once now: the screen under the popover previews it. */
		void onFocusChange(std::function<void(int)> fn);
		/** The caption lines under the list (always the same number, some empty). */
		const std::vector<std::string>& caption() const { return captionLines; }
		/** Where the panel is, and how tall its rows are (design pixels). */
		int panelX() const { return place.x; }
		int panelY() const { return place.y; }
		int panelW() const { return panelWidth; }
		int panelH() const { return panelHeight; }
		int rowH() const { return rowHeight; }
	private:
		void refreshCaption();

		UiScope ui;
		std::vector<PickerItem> items;
		int picked;
		bool done;
		int focus;
		GuiButton * backBtn;
		std::vector<GuiCard*> itemCards;
		std::vector<std::string> captionLines;
		std::function<void(int)> focusCb;
		int inUse;
		PopoverPlace place;
		int panelWidth, panelHeight, rowHeight;
};

/**
 * Splits a sentence into lines of at most `max` characters, at spaces, and appends them to `lines`: for the panes and
 * captions of a popover, which cannot wrap text (a label does not wrap).
 */
void MenuPopover_Wrap(std::vector<std::string>& lines, const std::string& text, size_t max);

#endif
