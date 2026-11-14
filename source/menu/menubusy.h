/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menubusy.h
 *
 * "Please wait": work that waits on the card (writing, deleting) runs on a
 * worker thread while the menu keeps drawing, behind a small panel with a
 * spinner. The menu thread is the only one that draws, so a slow card shows as a
 * moving spinner instead of a frozen screen.
 ***************************************************************************/
#ifndef _MENUBUSY_H_
#define _MENUBUSY_H_

#include <functional>
#include <string>
#include "libgui/Gui.h"

class MenuSpinner;

/**
 * The panel: a title, a spinner, and a line or two saying what is being done, centred over the screen with the screen
 * washed behind it (the look of a disabled screen). It is not shown until setShown(true), so work that finishes at
 * once never flashes it; hidden, it still holds the focus and takes the input. B and clicks do nothing here:
 * nothing it waits for can be cancelled.
 */
class MenuBusyView : public GuiPopover
{
	public:
		MenuBusyView(const char * title, const std::string& detail);
		void setShown(bool on) { shown = on; }
		bool isShown() const { return shown; }
		/** Moves the spinner's head one dot clockwise for each step of `phase`. */
		void setPhase(int phase);
		void draw() override;
	private:
		bool shown;
		MenuSpinner * spinner;
};

/**
 * Runs `work` on a worker thread and waits for it, drawing the menu meanwhile: the screen is not touched for a
 * moment, then the panel comes up and stays long enough to be read (MenuBusyPacer has the times). The input goes to the
 * panel throughout (the screen under it hears nothing), from the start.
 *
 * `work` runs off the menu thread, so it must do nothing but plain file work on data it was given: no DOSBox core, no
 * libgui, no input. Whatever it needs from those is taken before the call (GameSaveSnapshot, gamesave.h). What it
 * produces is read after the call returns, when it is finished. When no thread can be made the work is done here,
 * without the panel.
 *
 * Call it from the menu loop only (it pumps UpdateGui()).
 *
 * @param title    the panel's heading ("Saving")
 * @param detail   what is being done, a sentence ("Saving Doom.conf and Doom.map.")
 */
void MenuBusy_Run(const char * title, const std::string& detail, const std::function<void()>& work);

#endif
