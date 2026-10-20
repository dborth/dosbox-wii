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
 *                      Header: Preset (this player) and In DOS (the whole game); footer: Back
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
#include "controllerview.h"
#include "controllerprofile.h"
#include "menupage.h"
#include "menupopover.h"

/** Opens the controller setup from the home menu. Blocks until the user backs out. Opens on the given player (0-3),
    or on the first connected one when there is none given. */
void Controls_Menu(GuiWindow * home, int player = -1);

/** The black device icon for a kind of controller (a PNG, as the asset arrays hold it). */
const unsigned char * Controls_PadIcon(PadType type);

/** The dimmed backdrop a full-screen page draws over the paused game (the theme's background, mostly opaque). */
GuiPaint UiBackdropPaint();

/** The controller setup screen for one player. */
class ControllerConfigView : public GuiOverlay
{
	public:
		/** `show` is the device to draw when the player holds several (PadType::NONE, or one that is not there: the primary one). */
		ControllerConfigView(int player, PadType show = PadType::NONE);
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
		enum class Pick { NONE, EMULATED, PRESET };
		Pick takePick() { const Pick p = pickRequest; pickRequest = Pick::NONE; return p; }
		/** Says something on the hint line (what a Save or Load did). */
		void setStatus(const std::string& text) { status = text; }
		/** The control highlighted on the drawing (0 = none). */
		int highlighted() const;
		PadType type() const { return padType; }
		/** Whether the slot has no controller (the screen then says so, and offers only what DOS sees). */
		bool isEmpty() const { return padType == PadType::NONE; }
		/** The player slot being set up (0-3). */
		int player() const { return playerSlot; }
		/** Puts the selection on the header button that opens `which` (Pick::EMULATED or PRESET): where it was when a modal was open over it. */
		void selectHeader(Pick which);
		/** The button that opens `which`, in canvas pixels: where its popover opens beside. */
		GuiPopoverAnchor anchorFor(Pick which);
		/** While a preset popover is open: the rows show what this preset would give them (-1 = what they do now). */
		void setPreview(int preset) { previewPreset = preset; }
		/** Runs the screen's bindings now (a screen under a popover is not updated, but should follow what the popover previews). */
		void refresh() { ui.runBindings(); }
		/** The row for a control (the id its binding is stored under), in canvas pixels: the popover opens on the opposite side of the screen, leaving it in view. */
		GuiPopoverAnchor anchorForRow(int binding);
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
		ControllerView * view;
		std::vector<GuiCard*> rowCards;
		std::vector<std::pair<int, GuiCard*> > rowsById;
		std::string status;
		bool hasRequest;
		ProfileRow request;
		Pick pickRequest;
		int switchRequest;
		int shownPlayers;
		GuiTextButton * backBtn;
		GuiCard * emulatedButton;
		GuiCard * presetButton;
		int previewPreset;
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

/**
 * Choosing what a control does, in a popover beside the row that was pressed: the kinds of target down the left
 * (moving over one shows its choices on the right), plain rows on the right. The row being edited stays in view.
 */
class TargetChooserView : public GuiPopover
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
		TargetChooserView(const char * controlName, int player, int control, const BindingTarget& current, const BindingTarget& defaultTarget,
			const GuiPopoverAnchor& anchor = GuiPopoverAnchor());
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
		/** L and R (any controller) change the key tab while the Key kind is showing. */
		void update(InputController * c) override;
		/** Moves the key tab by `dir` (+1 / -1, wrapping). */
		void stepKeyTab(int dir);
		int keyTabIndex() const { return keyTab; }
		int keyTabCount() const { return keyTabs; }
		/** How many keycaps the key picker draws, over all tabs (every key the player can bind should have one). */
		int keyCapCount() const { return keyCaps; }
		/** Shows a kind, as moving over it does. */
		void showCategory(int index);
		/** Puts the selection on the kind showing (once the popover has the focus). */
		void selectCategory();
		int panelX() const { return position().x; }
		int panelY() const { return position().y; }
		int panelW() const { return panelWidth; }
		int panelH() const { return panelHeight; }
	private:
		void optionButtons(GuiLayout& grid, TARGET kind, const std::vector<int>& codes, int w, int h, int fontPx);
		void optionRows(GuiLayout& list, TARGET kind, const std::vector<int>& codes, int w, int h);
		void keyPane(GuiLayout& pane, int player, int w);
		std::string defaultHint() const;
		void targetRows(GuiLayout& list, const std::vector<BindingTarget>& targets, int w, int h);

		BindingTarget current;
		BindingTarget picked;
		int shown;
		bool done;
		std::string hintText;
		bool listen;
		TARGET listenKind;
		int kinds;
		int keyTab;
		int keyTabs;
		int keyCaps;
		int keyPanel;
		std::vector<GuiCard*> kindCards;
		int panelWidth, panelHeight;
};

#endif
