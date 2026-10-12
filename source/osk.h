/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * osk.h
 *
 * The in-game on-screen keyboard. A button on the GamePad swaps the GamePad
 * screen from the game to the platform's own keyboard (Wii U: the system
 * software keyboard); what is typed on it goes into DOS, as if from a real
 * keyboard. The TV keeps showing the game. Does nothing on a platform without
 * a second screen and a system keyboard.
 *
 * Deliberately includes nothing: used by both DOSBox core files and platform
 * files, whose headers collide (see videosupport.h).
 ***************************************************************************/
#ifndef DOSBOX_OSK_H
#define DOSBOX_OSK_H

class KeyboardDriver;

//! Once per input scan, after the pads have been read: handles the toggle
//! button and runs the keyboard while it is open: types what was entered and
//! refreshes the GamePad.
void OSK_Update(void);

//! True while the keyboard is open.
bool OSK_IsActive(void);

//! Closes the keyboard without redrawing the game (the caller is about to
//! take the display, eg. for the menu).
void OSK_Close(void);

//! Wraps the physical keyboard driver. The returned driver is what the rest of
//! the app polls: it hands out the physical keyboard's events, then the ones
//! typed on the on-screen keyboard, so both arrive the same way. Call once,
//! where the `keyboard` global is defined.
KeyboardDriver * OSK_WrapKeyboard(KeyboardDriver * physical);

//! Which button (InputDataButtons) on the GamePad opens and closes the
//! keyboard. The GamePad's TV button by default: nothing else uses it, so the
//! game never sees the press. For the future mapping screen.
void OSK_SetToggleButton(unsigned button);

#endif
