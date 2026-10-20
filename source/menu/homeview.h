/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2026
 * homeview.h
 *
 * The home screen, built from shapes with libgui's builder layer:
 *
 *   header   logo, the running program and its setup                 [Resume]
 *   body     Quick settings: speed, frameskip, aspect, filter  |  Controllers: the four
 *            players (device, battery), keyboard and mouse, and Configure
 *                                                                  |  Keyboard, Settings, Status, Games
 *   footer   [Exit DOSBox]            what the highlighted thing does              [Credits]
 *
 * It is a view over two models: HomeSettings for the quick settings (the running
 * cycles and frameskip, the display choices) and the ControlsModel for the
 * players, devices, program and setup. It has no DOSBox or HAL types, so the
 * host preview can draw it. The loop that runs it (HomeMenu) asks what was
 * pressed with takeAction().
 ***************************************************************************/
#ifndef _HOMEVIEW_H_
#define _HOMEVIEW_H_

#include <string>
#include "libgui/Gui.h"
#include "menupage.h"
#include "menupopover.h"

/** What the quick settings read and change. All of them take effect at once. */
class HomeSettings
{
	public:
		virtual ~HomeSettings() {}
		/** The running speed: a cycle count ("3000") or, in auto mode, a share ("100%") */
		virtual std::string speed() const = 0;
		/** One line about it: "Dynamic core" */
		virtual std::string speedNote() const = 0;
		virtual void stepSpeed(bool up) = 0;
		virtual std::string frameskip() const = 0;
		virtual void stepFrameskip(bool up) = 0;
		/** Whether the picture is drawn 4:3 (corrected) and not with square pixels */
		virtual bool aspectCorrected() const = 0;
		virtual void setAspectCorrected(bool corrected) = 0;
		/** Whether pixel edges are kept sharp and not smoothed */
		virtual bool filterSharp() const = 0;
		virtual void setFilterSharp(bool sharp) = 0;
		/** The game DOSBox is running ("Keen 4"), or empty at the plain prompt */
		virtual std::string gameName() const { return std::string(); }
};

class HomeView : public GuiOverlay
{
	public:
		enum class Action { NONE, RESUME, EXIT, KEYBOARD, CONTROLLERS, SETTINGS, STATUS, GAMES, CREDITS, RESTART };

		HomeView(HomeSettings& settings, const char * version);

		/** What was pressed since the last call (then forgotten) */
		Action takeAction();
		/** With Action::CONTROLLERS: the player whose setup was asked for, or -1 for the first connected one */
		int takePlayer() const { return actionPlayer; }
		/** Says something on the footer's hint line (what a Save did); empty goes back to the button hints */
		void setStatus(const std::string& text) { status = text; }
		/** Puts the selection on a button again (after something that was open over the screen closes) */
		void select(Action which);

	private:
		GuiCard& pill(const char * text, int icon, int w, int h, int font, UiCallback onClick, UiCallback onFocus);
		GuiCard& tile(const char * title, const char * note, int icon, Action action, const char * hint);
		void quickRow(GuiLayout& column, int icon, const char * label, std::function<std::string()> note, GuiLayout& control);
		GuiLayout& stepper(std::function<std::string()> value, std::function<void(bool)> step, const char * hint);
		GuiLayout& choice(const char * firstText, const char * secondText, std::function<bool()> firstOn, std::function<void(bool)> set, const char * hint);
		GuiLayout& playersStrip();

		UiScope ui;
		HomeSettings& settings;
		Action pending;
		int actionPlayer;
		std::string status;
		GuiCard * resumeCard;
		GuiCard * exitCard;
		GuiCard * restartCard;
		GuiCard * creditsCard;
		GuiCard * gamesCard;
		GuiCard * configureCard;
		GuiCard * keyboardCard;
		GuiCard * settingsCard;
		GuiCard * statusCard;
		std::string hint;
};

#endif
