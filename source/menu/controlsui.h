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
		std::string status;
		bool hasRequest;
		ProfileRow request;
		GuiTextButton * backBtn;
};

/** The screen for choosing what a control does. */
class TargetChooserView : public GuiOverlay
{
	public:
		/**
		 * @param controlName Name of the control being set ("Y Button")
		 * @param current What the control does now
		 * @param defaultTarget What the scheme makes it do (the Default button)
		 */
		TargetChooserView(const char * controlName, const BindingTarget& current, const BindingTarget& defaultTarget, GuiSound * soundOver);
		GuiButton& backButton() { return *backBtn; }
		/** Whether the user picked something. */
		bool chosen() const { return done; }
		BindingTarget result() const { return picked; }
		/** The category whose choices are showing (0 key, 1 joystick, 2 mouse, 3 DOSBox function, 4 nothing). */
		int category() const { return shown; }
	private:
		void optionButtons(GuiLayout& grid, TARGET kind, int w, int h, int fontPx);

		UiScope ui;
		BindingTarget current;
		BindingTarget picked;
		int shown;
		bool done;
		GuiTextButton * backBtn;
		std::string hintText;
};

#endif
