/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menu.h
 *
 * Menu flow routines
 ***************************************************************************/
#ifndef _MENUH_
#define _MENUH_

#include <stdint.h>

/** The home menu. Blocks, stepping the GUI itself, until it is closed. */
void HomeMenu();


/* ---- Shared by menu.cpp and screens.cpp ---- */

class GuiWindow;

/** The window everything on screen is in while the menu is up. */
extern GuiWindow * mainWindow;

/** One GUI frame: input, draw, cursors, present. Whatever waits on the menu calls it in a loop. */
void UpdateGui();

/** Types text on the on-screen keyboard, starting from what `var` holds; false if cancelled (then `var` is unchanged). */
bool OnScreenKeyboard(char * var, uint32_t maxlen);

/** Opens the settings (and from there Buttons, Drives and Status) in place of the home screen's window. Returns when they are closed. */
void SettingsMenu(GuiWindow * home);


/** Opens the status page in place of the home screen's window. Returns when it is closed. */
void StatusMenu(GuiWindow * home);


/**
 * Increase or decrease the current CPU cycles.
 *
 * Implemented in src/cpu/cpu.cpp
 *
 * @param increase True for increase cycles. False for decrease cycles.
 */
void MENU_CycleIncreaseOrDecrease(bool increase);

/**
 * Increase or decrease the current frameskip
 *
 * Implemented in src/gui/render.cpp
 *
 * @param increase True for increase cycles. False for decrease cycles.
 */
void MENU_IncreaseOrDecreaseFrameSkip(bool increase);

#endif
