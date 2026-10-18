/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlssetupsui.h
 *
 * Loading and saving setups from the menu (see controlssetupsui.cpp).
 ***************************************************************************/
#ifndef _CONTROLSSETUPSUI_H_
#define _CONTROLSSETUPSUI_H_

#include <functional>
#include <string>
#include <vector>
#include "libgui/Gui.h"
#include "menupopover.h"

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

/** What a flow needs of the screen it runs over: to take it away while the on-screen keyboard has the display, and bring it back. */
struct ScreenHooks {
	std::function<void()> hide;
	std::function<void()> show;
};

/** The Setup button: load a setup, or save this one, in popovers beside `anchor` over the screen that called. What the hint line should say after (empty if nothing was done). */
std::string Setups_Run(const ScreenHooks& screen, const PopoverAnchor& anchor);

/** Saves over the setup that is loaded: no questions, it is the same name and the same program. What the hint line should say after. */
std::string Setups_SaveCurrent();

/** The Setups popover (load a setup, or save this one) over the home screen, opening beside `anchor`. Blocks until it is
    closed. What the hint line should say about it (empty if nothing was done). */
std::string Controls_SetupMenu(GuiWindow * home, const PopoverAnchor& anchor);

#endif
