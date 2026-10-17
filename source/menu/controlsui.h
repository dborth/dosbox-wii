/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsui.h
 *
 * The controller screens, built from shapes with libgui's builder layer:
 *
 *   Controller setup   where Controllers opens: a controller drawing with its controls listed
 *                      around it, each showing what it does; the selected row is highlighted
 *                      on the drawing. Arrows in the header switch between connected players.
 *                      Header: Preset (this player) and In DOS (the whole game); footer: Setup
 *                      (load or save, the whole game) and Back
 *   Target chooser     pick what a control does: key, joystick button, mouse,
 *                      DOSBox function, or nothing
 *
 * Each screen is a view class (testable on its own) plus a small loop that runs
 * it on the menu's main window. Everything reads and writes through ControlsModel.
 ***************************************************************************/
#ifndef _CONTROLSUI_H_
#define _CONTROLSUI_H_

#include <string>
#include "libgui/Gui.h"
#include "controlsmodel.h"
#include "controllerprofile.h"
#include "menupage.h"
#include "popover.h"

/** Opens the controller setup from the home menu. Blocks until the user backs out. Opens on the given player (0-3),
    or on the first connected one when there is none given. */
void Controls_Menu(GuiWindow * home, int player = -1);

/** Font sizes used by the controller screens. */
enum {
	UI_FONT_TITLE = 26,
	UI_FONT_HEADING = 20,
	UI_FONT_BODY = 16,
	UI_FONT_SMALL = 13
};

/** The dimmed backdrop a full-screen page draws over the paused game (the theme's background, mostly opaque). */
GuiPaint UiBackdropPaint();

/** The controller setup screen for one player. */
class ControllerConfigView : public GuiOverlay
{
	public:
		/** `show` is the device to draw when the player holds several (PadType::NONE, or one that is not there: the primary one). */
		ControllerConfigView(int player, GuiSound * soundOver, PadType show = PadType::NONE);
		GuiButton& backButton() { return *backBtn; }
		/**
		 * The device the user chose on the rail since the last call (then forgotten), PadType::NONE if none. The screen
		 * is built for one device, so the caller builds a new view that shows it.
		 */
		PadType takeFlip() { const PadType t = flipRequest; flipRequest = PadType::NONE; return t; }
		/** The devices on the rail, primary first; fewer than two means no rail. */
		const std::vector<PadType>& devices() const { return deviceList; }
		/** The device drawn (the primary one unless another was chosen). */
		PadType shownType() const { return deviceList.empty() ? padType : deviceList[shownIndex]; }
		/** Which way the player switcher was clicked since the last call (-1 previous, +1 next, 0 neither). */
		int takeSwitch() { const int r = switchRequest; switchRequest = 0; return r; }
		/** Whether the screen needs building again: the controller in the slot came or went, changed kind, or the number of connected players changed. */
		bool outOfDate() const;
		/** Whether a row was clicked since the last call; fills `row` with it. */
		bool takeRequest(ProfileRow& row);
		/** Which list the user asked to open from the header, if any (then it is forgotten). */
		enum class Pick { NONE, EMULATED, PRESET, SETUP };
		Pick takePick() { const Pick p = pickRequest; pickRequest = Pick::NONE; return p; }
		/** Says something on the hint line (what a Save or Load did). */
		void setStatus(const std::string& text) { status = text; }
		/** The control highlighted on the drawing (0 = none). */
		int highlighted() const;
		PadType type() const { return padType; }
		/** Whether the slot has no controller (the screen then says so, and offers only what DOS sees and Setup). */
		bool isEmpty() const { return padType == PadType::NONE; }
		/** The player slot being set up (0-3). */
		int player() const { return playerSlot; }
		const std::string& statusText() const { return status; }
		/** Puts the selection on the header button that opens `which` (Pick::EMULATED or PRESET): where it was when a modal was open over it. */
		void selectHeader(Pick which);
		/** The button that opens `which`, in canvas pixels: where its popover opens beside. */
		PopoverAnchor anchorFor(Pick which);
		/** While a preset popover is open: the rows show what this preset would give them (-1 = what they do now). */
		void setPreview(int preset) { previewPreset = preset; }
		/** Runs the screen's bindings now (a screen under a popover is not updated, but should follow what the popover previews). */
		void refresh() { ui.runBindings(); }
		/** The row for a control (the id its binding is stored under), in canvas pixels: the popover opens on the opposite side of the screen, leaving it in view. */
		PopoverAnchor anchorForRow(int binding);
	private:
		enum class Side { LEFT, RIGHT, GROUP };
		GuiCard& bindingRow(const ProfileRow& row, Side side, int width);

		UiScope ui;
		int playerSlot;
		PadType padType;		// the slot's primary device (what outOfDate() compares), which is not always the one drawn
		std::vector<PadType> deviceList;
		size_t shownIndex;
		PadType flipRequest;
		bool sideways;
		GuiControllerView * view;
		std::vector<GuiCard*> rowCards;
		std::vector<std::pair<int, GuiCard*> > rowsById;
		std::string status;
		bool hasRequest;
		ProfileRow request;
		Pick pickRequest;
		int switchRequest;
		int shownPlayers;
		GuiTextButton * backBtn;
		GuiTextButton * emulatedButton;
		GuiTextButton * presetButton;
		GuiTextButton * setupButton;
		int previewPreset;
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
	std::string confirm;				/**< When not empty, choosing needs a second press, and this is what the caption says meanwhile */
	bool divider = false;				/**< A rule is drawn after this item: what follows is a different kind of choice */
};

/** The choices for what DOS sees, as picker items. */
/** The Emulating list. `index` (optional) gets each item's index in emulatedModes(): the advanced types are left out unless in effect. */
std::vector<PickerItem> Picker_EmulatedItems(std::vector<int> * index = nullptr);
/**
 * The presets a player can choose, as picker items. `index` receives, for each item, the preset it stands for
 * (a preset that needs a joystick is left out when the player has none, unless it is the one in use). When the
 * player has changed buttons, the first item is "Undo my changes" and stands for -1.
 */
std::vector<PickerItem> Picker_PresetItems(int player, std::vector<int>& index);

/** What the Setup button can do. */
enum class SetupAction { LOAD, SAVE_CURRENT, SAVE_NEW };
/** The choices behind the Setup button; `kinds` receives each item's action. Saving over the current setup is offered only when there is one. */
std::vector<PickerItem> Picker_SetupMenuItems(std::vector<SetupAction>& kinds);

/** The saved setups, as picker items for Load; `index` receives each item's index in the model's list. */
std::vector<PickerItem> Picker_SetupItems(std::vector<int>& index);

/** What Save can tie a setup to. */
enum class AttachChoice { RUNNING, NONE, TYPED };
/**
 * The choices for what program a new setup is for: the program running now (when there is one), none, or one typed.
 * `kinds` receives each item's choice.
 */
std::vector<PickerItem> Picker_AttachItems(const std::string& runningProgram, std::vector<AttachChoice>& kinds);

/**
 * A list of choices in a popover: one flat panel beside the control that asked, over the screen it came from. Plain
 * rows (the one in use has a dot), a rule where an item says so, and under the list a caption for the row under the
 * cursor: its description, or what a second press of A will do. Nothing else is drawn on it.
 */
class PickerView : public GuiOverlay
{
	public:
		/** `anchor` is the control it opens beside; with none it is centred. B or a click outside closes it (backButton()). */
		PickerView(const char * title, const std::vector<PickerItem>& items, GuiSound * soundOver, const PopoverAnchor& anchor = PopoverAnchor());
		GuiButton& backButton() { return *backBtn; }
		/** Whether a choice was made (after any confirmation). */
		bool chosen() const { return done; }
		/** The index of the item chosen. */
		int result() const { return picked; }
		/** The item under the cursor. */
		int focused() const { return focus; }
		/** Whether an item is waiting for its second press. */
		bool armed() const { return armedIdx >= 0; }
		/** What A does on an item; the first press only arms an item that asks for confirmation. */
		void choose(int index);
		/** The cursor moved to an item (a pending confirmation is dropped). */
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
		int armedIdx;
		GuiButton * backBtn;
		std::vector<GuiCard*> itemCards;
		std::vector<std::string> captionLines;
		std::function<void(int)> focusCb;
		int inUse;
		PopoverPlace place;
		int panelWidth, panelHeight, rowHeight;
};

/**
 * Choosing what a control does, in a popover beside the row that was pressed: the kinds of target down the left
 * (moving over one shows its choices on the right), plain rows on the right. The row being edited stays in view.
 */
class TargetChooserView : public GuiOverlay
{
	public:
		/**
		 * @param controlName Name of the control being set ("Y Button")
		 * @param player Whose control it is (what DOS gives that player decides what is offered)
		 * @param control The control's id (for the suggestions)
		 * @param current What the control does now
		 * @param defaultTarget What the scheme makes it do (the Default button)
		 * @param anchor The row it opens beside
		 */
		TargetChooserView(const char * controlName, int player, int control, const BindingTarget& current, const BindingTarget& defaultTarget, GuiSound * soundOver,
			const PopoverAnchor& anchor = PopoverAnchor());
		/** B anywhere, or a click outside the panel. */
		GuiButton& backButton() { return *backBtn; }
		/** Whether the user picked something. */
		bool chosen() const { return done; }
		BindingTarget result() const { return picked; }
		/** The kind whose choices are showing: an index into the kinds listed down the left (Suggested first when there are suggestions). */
		int category() const { return shown; }
		/** How many kinds are listed down the left. */
		int categoryCount() const { return kinds; }
		/** Waiting for a key or mouse button to be pressed. */
		bool listening() const { return listen; }
		void startListening(TARGET kind);
		void stopListening();
		/** Shows a kind, as moving over it does. */
		void showCategory(int index);
		/** Puts the selection on the kind showing (once the popover has the focus). */
		void selectCategory();
		int panelX() const { return place.x; }
		int panelY() const { return place.y; }
		int panelW() const { return panelWidth; }
		int panelH() const { return panelHeight; }
	private:
		void optionButtons(GuiLayout& grid, TARGET kind, const std::vector<int>& codes, int w, int h, int fontPx);
		void optionRows(GuiLayout& list, TARGET kind, const std::vector<int>& codes, int w, int h);
		void keyPane(GuiLayout& pane, int player, int w);
		void targetRows(GuiLayout& list, const std::vector<BindingTarget>& targets, int w, int h);

		UiScope ui;
		BindingTarget current;
		BindingTarget picked;
		int shown;
		bool done;
		GuiButton * backBtn;
		std::string hintText;
		bool listen;
		TARGET listenKind;
		int kinds;
		int keyTab;
		std::vector<GuiCard*> kindCards;
		PopoverPlace place;
		int panelWidth, panelHeight;
};

#endif
