/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsui.h
 *
 * The controller screens, built from shapes with libgui's builder layer:
 *
 *   Controllers hub    four player cards, shared devices, the control scheme
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

/** Opens the Controllers screens from the home menu. Blocks until the user backs out. */
void Controls_Menu(GuiWindow * home);

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

/** The Controllers hub. */
class ControlsHubView : public GuiColumn
{
	public:
		ControlsHubView(GuiSound * soundOver);
		GuiButton& backButton() { return *backBtn; }
		/** The player whose card was clicked since the last call, or -1. */
		int takeRequest() { const int r = request; request = -1; return r; }
		const std::string& statusText() const { return status; }
	private:
		GuiCard& playerCard(int n);
		void deviceRow(GuiLayout& grid, const char * name, std::function<bool()> present);

		UiScope ui;
		int request;
		std::string status;
		GuiTextButton * backBtn;
};

/** The controller setup screen for one player. */
class ControllerConfigView : public GuiColumn
{
	public:
		ControllerConfigView(int player, GuiSound * soundOver);
		GuiButton& backButton() { return *backBtn; }
		/** Whether a row was clicked since the last call; fills `row` with it. */
		bool takeRequest(ProfileRow& row);
		/** The control highlighted on the drawing (0 = none). */
		int highlighted() const;
		PadType type() const { return padType; }
		const std::string& statusText() const { return status; }
	private:
		enum class Side { LEFT, RIGHT, GROUP };
		GuiCard& bindingRow(const ProfileRow& row, Side side, int width);

		UiScope ui;
		PadType padType;
		GuiControllerView * view;
		std::string status;
		bool hasRequest;
		ProfileRow request;
		GuiTextButton * backBtn;
};

/** The screen for choosing what a control does. */
class TargetChooserView : public GuiColumn
{
	public:
		/**
		 * @param controlName Name of the control being set ("Y Button")
		 * @param stickMode Choose a stick's mode instead of a button action
		 * @param current What the control does now
		 */
		TargetChooserView(const char * controlName, bool stickMode, const BindingTarget& current, GuiSound * soundOver);
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
};

#endif
