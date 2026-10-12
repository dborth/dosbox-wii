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
 * This is also where text gets typed into DOS from anywhere else: the menu's
 * own on-screen keyboard hands its command to OSK_TypeText(), on every
 * platform, with or without a GamePad.
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

//! Types text into DOS as if from a keyboard: '\n' (and '\r') type Return,
//! characters with no key on a US keyboard are skipped. The text is typed as
//! the emulator polls the keyboard, paced like a real one, so it can be handed
//! over at any time, even while the menu is up. Adds to text still being
//! typed; more than 1024 characters waiting are cut off.
void OSK_TypeText(const char * text);

//! Stops typing, for when the emulator is about to stop polling (the menu).
//! The key in progress is finished, so no key or shift is left held.
void OSK_CancelTyping(void);

//! Which button (InputDataButtons) on the GamePad opens and closes the
//! keyboard. The GamePad's TV button by default: nothing else uses it, so the
//! game never sees the press. For the future mapping screen.
void OSK_SetToggleButton(unsigned button);

#endif
