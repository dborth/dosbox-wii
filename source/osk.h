/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * osk.h
 *
 * The in-game on-screen keyboard. A button on the GamePad swaps the GamePad
 * screen from the game to a keyboard; touching its keys types into DOS, as
 * if from a real keyboard. The TV keeps showing the game.
 *
 * Deliberately includes nothing: used by both DOSBox core files and platform
 * files, whose headers collide (see videosupport.h).
 ***************************************************************************/
#ifndef DOSBOX_OSK_H
#define DOSBOX_OSK_H

struct KeyEvent;

//! Once per input scan, after the pads have been read: handles the toggle
//! button, runs the keyboard while it is open, and keeps it drawn.
void OSK_Update(void);

//! True while the keyboard is open.
bool OSK_IsActive(void);

//! Closes the keyboard without redrawing the game (the caller is about to
//! take the display, eg. for the menu).
void OSK_Close(void);

//! The next key event typed on the on-screen keyboard, one per call so a
//! press and its release are never delivered in the same scan.
bool OSK_PollKey(KeyEvent & out);

//! Which button (InputDataButtons) on the GamePad opens and closes the
//! keyboard. Minus (Select) by default. For the future mapping screen.
void OSK_SetToggleButton(unsigned button);

#endif
