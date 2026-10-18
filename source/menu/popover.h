/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * popover.h
 *
 * The one look for a question asked in place: a small light panel that opens
 * beside the control that asked it, over the screen it came from (dimmed, but
 * still there). Two layers at most: the screen, then the popover. This file
 * holds the parts every popover shares, and PickerView, the list built on them.
 ***************************************************************************/
#ifndef _POPOVER_H_
#define _POPOVER_H_

#include "libgui/Gui.h"

/** The control a popover opens beside, in canvas pixels. */
struct PopoverAnchor {
	/** Which way the panel lines up: with the anchor's nearer edge, or against a margin (to leave the other side of the screen clear). */
	enum Edge { AUTO, LEFT_MARGIN, RIGHT_MARGIN };
	int x, y, w, h;
	bool valid;
	Edge edge;
	PopoverAnchor() : x(0), y(0), w(0), h(0), valid(false), edge(AUTO) {}
	PopoverAnchor(int ax, int ay, int aw, int ah, Edge e = AUTO) : x(ax), y(ay), w(aw), h(ah), valid(true), edge(e) {}
};

/** Where the popover's top-left corner goes. */
struct PopoverPlace { int x, y; };

/** The most height a popover can have: the band between the header and footer bars. */
int Popover_MaxHeight(int screenHeight);

/**
 * Places a popover of `w` x `h` beside its anchor: below it when it fits in the band between the bars, else above,
 * lined up with the anchor's right edge when the anchor is on the right half of the screen and its left edge
 * otherwise, and kept inside the margins. With no anchor it is centred.
 */
PopoverPlace Popover_Place(const PopoverAnchor& anchor, int w, int h, int screenWidth, int screenHeight);

/** One row of a popover list: the name on the left, a dot on the right when `marked` (the one in use). The caller adds onClick / onFocus. */
GuiCard& Popover_Row(UiScope& ui, int width, int height, const std::string& title, bool marked, bool dotSlot = true);

/**
 * Where the little tail on a popover goes, and which way it points (toward the anchor); none when the panel covers the anchor
 * or there is no anchor. `x`,`y` are the tail's top-left corner; `w`,`h` its size.
 */
struct PopoverTail { bool shown; int direction; int x, y, w, h; };
enum { POPOVER_TAIL_UP, POPOVER_TAIL_DOWN, POPOVER_TAIL_LEFT, POPOVER_TAIL_RIGHT };
PopoverTail Popover_Tail(const PopoverAnchor& anchor, const PopoverPlace& at, int panelW, int panelH);

/**
 * Puts a built panel on a popover view: the backdrop behind it (B or a click outside closes: its button is returned),
 * and the panel at `at` on the screen.
 */
GuiButton& Popover_Mount(UiScope& ui, GuiLayout& view, GuiTrigger& cancel, GuiCard& panel, const PopoverPlace& at, int panelW, int panelH, const PopoverAnchor& anchor);

/**
 * A small round close button (an X) for the right end of a popover's title row: a way out that does not depend on
 * B or on finding empty space to click. `onClick` is what it does; pass the backdrop's dismiss().
 */
GuiCard& Popover_CloseButton(UiScope& ui, std::function<void()> onClick);

/** A one pixel rule `w` wide, in the outline color. */
GuiShape& Popover_Divider(UiScope& ui, int w);

/**
 * The invisible button across the whole screen behind a popover: B anywhere, or a click outside the panel, presses
 * it (that is how a popover is dismissed). A click on the panel itself is not a dismissal and is swallowed.
 * It cannot take the D-pad's selection.
 */
class PopoverBackdrop : public GuiButton
{
	public:
		PopoverBackdrop(int w, int h);
		/** Closes the popover as B does (the close button calls this): the backdrop reads as clicked, and stays so. */
		void dismiss();
		/** The panel: a click inside it does not count. */
		void setPanel(GuiElement * panelElement) { panel = panelElement; }
		/** The trigger that means "close" (B), which counts wherever the pointer is. */
		void setCancel(GuiTrigger * t) { cancel = t; }
		/** The trigger whose press, with the pointer off the panel, closes the popover. */
		void setPrimary(GuiTrigger * t) { primary = t; }
		void update(InputController * c) override;
	private:
		GuiElement * panel;
		GuiTrigger * cancel;
		GuiTrigger * primary;
		bool closing;
};

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
void Popover_Wrap(std::vector<std::string>& lines, const std::string& text, size_t max);

#endif
