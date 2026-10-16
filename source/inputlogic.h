/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * inputlogic.h
 *
 * Decisions about input: when the user is asking for the menu, and whether
 * mouse motion and clicks reach DOS. Pure logic: no DOSBox types and no
 * calls into the platform, only the HAL's pad data.
 *
 * Menu requests have two sources:
 *   - a controller: HOME, or L + R + Plus/Start held on one device (so a
 *     GameCube pad, which has no HOME button, can open the menu)
 *   - a USB keyboard: a tap of the Windows / GUI / Command key
 *
 * The mouse is captured from startup (see GUI_StartUp): there is no desktop
 * to hand it back to, and a capturing click used up the first click. Motion
 * is then relative, as it was after that first click.
 ***************************************************************************/

#ifndef DOSBOX_INPUTLOGIC_H
#define DOSBOX_INPUTLOGIC_H

#include <stdint.h>
#include "drivers/InputData.h"

/* ---- Buttons as DOS sees them ---- */

//! True when the channel holds a Wiimote and nothing else that has its own D-pad (a Nunchuk adds no D-pad).
bool PadIsLoneWiimote(const InputPadData & pad);

//! The channel's merged held buttons as the emulated machine gets them: a lone Wiimote held sideways has its
//! D-pad turned a quarter turn, so the directions are the ones the player holds it by. The joystick the shim
//! builds and the live highlight on the setup screen both read this, so they cannot disagree.
uint32_t PadButtonsForDos(const InputPadData & pad, bool sideways);

/* ---- Menu requests ---- */

//! True while this channel holds HOME, or L + R + Plus/Start on a single device.
//! The combo is checked per device, not on the merged state, so two devices
//! sharing a channel cannot complete it between them.
bool MenuRequestFromPad(const InputPadData & pad);

//! USB HID usage IDs of the left and right GUI keys.
enum { MENU_KEY_LGUI = 0xE3, MENU_KEY_RGUI = 0xE7 };

//! Recognises a tap of the GUI key: pressed and released with no other key
//! pressed in between. A GUI key used as a modifier (GUI + another key)
//! never counts as a tap.
class MenuKeyTap
{
	public:
		//! True for the keys this class swallows (the GUI keys).
		static bool isMenuKey(unsigned hidUsage);

		//! Feed every key event, in order. Other keys must be fed too: pressing one
		//! while a GUI key is held cancels the tap.
		void onKey(unsigned hidUsage, bool pressed);

		//! True once per completed tap.
		bool consume();

		//! Forgets everything: held keys and any tap not yet consumed.
		void reset();

		//! Forgets a tap not yet consumed, keeping held-key state.
		void discardPending() { pending = false; }

	private:
		bool leftHeld = false;
		bool rightHeld = false;
		bool armed = false;    //!< a GUI key went down and nothing else has since
		bool pending = false;  //!< a tap completed and has not been consumed
};

/* ---- Mouse capture ---- */

//! Motion reaches DOS while captured, or when capture is not in use at all.
inline bool MouseMotionForwarded(bool locked, bool autoenable)
{
	return locked || !autoenable;
}

//! A click is used up capturing the mouse when DOS has asked for a lock and
//! the mouse is not locked yet. Never the case once it is captured at startup.
inline bool MouseClickSwallowed(bool requestlock, bool locked)
{
	return requestlock && !locked;
}

#endif
