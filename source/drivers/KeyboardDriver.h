/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * KeyboardDriver.h
 *
 * Physical (USB) keyboard input
 *
 * ---------------------------------------------------------------------
 * HAL BUILD-OUT PLAN
 *
 * Events: KeyEvent carries a USB HID usage code (page 0x07), not an
 * SDLK_/DOSBox code. HID usages are what libwiikeyboard reports as
 * keycodes and what Wii U's WUT keyboard API reports, so every backend
 * produces the same codes. DOSBox translates HID -> its own key table
 * (the SDLK_* enum we are keeping) in one place in the mapper layer.
 *
 * Wii (WiiKeyboardDriver, libogc2 + libwiikeyboard, already in LIBS):
 *  - init(): KEYBOARD_Init(cb) with a callback that only sets a flag
 *    (it runs in the USB thread); hotplug is reported by the callback.
 *  - poll(): KEYBOARD_ScanKeyboard() / KEYBOARD_GetEvent into KeyEvent
 *    (type PRESSED/RELEASED -> pressed, keycode -> hidUsage, mod -> mods).
 *  - Key repeat: libwiikeyboard does not repeat; the driver must
 *    synthesise it (initial delay + rate) only for GUI use. The emulator
 *    wants raw press/release with no repeat.
 *  - IOS: needs a USB v0/v2 capable IOS. WiiPlatform's IOS selection
 *    (SupportedIOS) already guards this; confirm before enabling.
 *  - Conflict: libwiikeyboard and WiiUsbMulti (USB mass storage) share
 *    the USB stack; verify both coexist on IOS58/IOS249 before shipping.
 *
 * Wii U (WutKeyboardDriver, WUT keyboard API):
 *  - init(): KBDSetup(connectCb, disconnectCb, keyCb); keyCb enqueues.
 *  - poll(): drain the queue. KBD scancodes map 1:1 to HID usages.
 *  - Lives in drivers/wut/ per the Wii/Wut file split; no #ifdefs.
 *
 * Also owed (separate stubs, same treatment): mouse (Wiimote IR / USB),
 * joystick-to-mapper events, CD audio (SDL_CD* replacement, can stay a
 * stub: DOSBox only needs image mounting).
 ***************************************************************************/
#pragma once

#include <stdint.h>

enum KeyModifiers : uint16_t {
	KEYMOD_NONE   = 0,
	KEYMOD_LSHIFT = (1 << 0),
	KEYMOD_RSHIFT = (1 << 1),
	KEYMOD_LCTRL  = (1 << 2),
	KEYMOD_RCTRL  = (1 << 3),
	KEYMOD_LALT   = (1 << 4),
	KEYMOD_RALT   = (1 << 5),
	KEYMOD_LGUI   = (1 << 6),
	KEYMOD_RGUI   = (1 << 7),
	KEYMOD_CAPS   = (1 << 8),
	KEYMOD_NUM    = (1 << 9)
};

struct KeyEvent
{
	uint16_t hidUsage;   //!< USB HID usage ID, keyboard page (0x07)
	uint16_t modifiers;  //!< KeyModifiers bitmask in effect for this event
	bool     pressed;    //!< true = key down, false = key up
};

//! Physical keyboard. The base class is the driver for a platform that has
//! none (eg. GameCube): it is never connected and never has an event, so the
//! app can poll it without asking whether there is a keyboard.
class KeyboardDriver
{
	public:
		virtual ~KeyboardDriver() = default;

		virtual void init() {}
		virtual void shutdown() {}

		//! True while at least one keyboard is attached
		virtual bool isConnected() const { return false; }

		//! Pops the next pending event into `out`. Returns false when the
		//! queue is empty. Call from the main thread, once per frame.
		virtual bool poll(KeyEvent & out) { (void)out; return false; }

		//! Lets isConnected() follow hot-plug while nothing is polling (in the
		//! menu). Keeps pending key events for the next poll(). Main thread.
		virtual void refresh() {}
};
