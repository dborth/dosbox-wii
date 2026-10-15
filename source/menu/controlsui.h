/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsui.h
 *
 * The controller screens, built from shapes with libgui's builder layer:
 *
 *   Controllers hub    a row per player: the controller, then what DOS sees it as (set by the DOS
 *                      mode: player n drives joystick n); the preset
 *   Controller setup   a controller drawing with its controls listed around it,
 *                      each showing what it does; the selected row is highlighted
 *                      on the drawing
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

/** Opens the Controllers screens from the home menu. Blocks until the user backs out. With a player (0-3), goes
    straight to that player's controller setup instead of the hub. */
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

/** A small battery gauge: a capsule track with a fill that changes color as it drains. */
class UiBatteryBar : public GuiElement
{
	public:
		UiBatteryBar(int w = 48, int h = 10);
		/** Sets the charge in percent (negative draws nothing). */
		void setLevel(int percent);
		int level() const { return pct; }
		void draw() override;
	private:
		int pct;
		bool dirty;
		float meshFeather;
		GuiMesh mesh;
		std::vector<MeshVertex> scratch;
};

/** The Controllers hub: one card per controller (what is connected, then what DOS sees it as), the shared devices on the header. */
class ControlsHubView : public GuiOverlay
{
	public:
		ControlsHubView(GuiSound * soundOver);
		GuiButton& backButton() { return *backBtn; }
		/** The player whose card was clicked since the last call, or -1. */
		int takeRequest() { const int r = request; request = -1; return r; }
		const std::string& statusText() const { return status; }
	private:
		GuiCard& playerRow(int n);
		std::string modeHint() const;

		UiScope ui;
		int request;
		std::string status;
		GuiTextButton * backBtn;
		GuiCard * rows[4];
		GuiCard * hw[4];
		GuiCard * dos[4];
		bool heroMode;
};

/** The controller setup screen for one player. */
class ControllerConfigView : public GuiOverlay
{
	public:
		ControllerConfigView(int player, GuiSound * soundOver);
		GuiButton& backButton() { return *backBtn; }
		/** Whether a row was clicked since the last call; fills `row` with it. */
		bool takeRequest(ProfileRow& row);
		/** Which list the user asked to open from the header, if any (then it is forgotten). */
		enum class Pick { NONE, EMULATED, PRESET };
		Pick takePick() { const Pick p = pickRequest; pickRequest = Pick::NONE; return p; }
		/** The control highlighted on the drawing (0 = none). */
		int highlighted() const;
		PadType type() const { return padType; }
		/** The player slot being set up (0-3). */
		int player() const { return playerSlot; }
		const std::string& statusText() const { return status; }
	private:
		enum class Side { LEFT, RIGHT, GROUP };
		GuiCard& bindingRow(const ProfileRow& row, Side side, int width);

		UiScope ui;
		int playerSlot;
		PadType padType;
		GuiControllerView * view;
		std::vector<GuiCard*> rowCards;
		std::string status;
		bool hasRequest;
		ProfileRow request;
		Pick pickRequest;
		GuiTextButton * backBtn;
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
	std::string confirm;				/**< When not empty, choosing needs a second press, and this is what the hint line says meanwhile */
};

/** The choices for what DOS sees, as picker items. */
std::vector<PickerItem> Picker_EmulatedItems();
/**
 * The presets a player can choose, as picker items. `index` receives, for each item, the preset it stands for
 * (a preset that needs a joystick is left out when the player has none, unless it is the one in use).
 */
std::vector<PickerItem> Picker_PresetItems(int player, std::vector<int>& index);

/** A list of choices on the left, and what the one under the cursor means on the right. */
class PickerView : public GuiOverlay
{
	public:
		PickerView(const char * title, const std::vector<PickerItem>& items, GuiSound * soundOver);
		GuiButton& backButton() { return *backBtn; }
		/** Whether a choice was made (after any confirmation). */
		bool chosen() const { return done; }
		/** The index of the item chosen. */
		int result() const { return picked; }
		/** The item the right pane describes. */
		int focused() const { return focus; }
		/** Whether an item is waiting for its second press. */
		bool armed() const { return armedIdx >= 0; }
		/** What A does on an item; the first press only arms an item that asks for confirmation. */
		void choose(int index);
		/** The cursor moved to an item (a pending confirmation is dropped). */
		void focusItem(int index);
	private:
		UiScope ui;
		std::vector<PickerItem> items;
		int picked;
		bool done;
		int focus;
		int armedIdx;
		GuiTextButton * backBtn;
		std::string hintText;
};

/** The screen for choosing what a control does. */
class TargetChooserView : public GuiOverlay
{
	public:
		/**
		 * @param controlName Name of the control being set ("Y Button")
		 * @param player Whose control it is (what DOS gives that player decides what is offered)
		 * @param control The control's id (for the suggestions)
		 * @param current What the control does now
		 * @param defaultTarget What the scheme makes it do (the Default button)
		 */
		TargetChooserView(const char * controlName, int player, int control, const BindingTarget& current, const BindingTarget& defaultTarget, GuiSound * soundOver);
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
	private:
		void optionButtons(GuiLayout& grid, TARGET kind, const std::vector<int>& codes, int w, int h, int fontPx);
		void targetButtons(GuiLayout& grid, const std::vector<BindingTarget>& targets, int w, int h, int fontPx);

		UiScope ui;
		BindingTarget current;
		BindingTarget picked;
		int shown;
		bool done;
		GuiTextButton * backBtn;
		std::string hintText;
		bool listen;
		TARGET listenKind;
		int kinds;
};

#endif
