/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcKeyboardDriver.cpp
 *
 * libwiikeyboard runs its own USB thread and queues events; this driver
 * only drains that queue. libwiikeyboard reports HID usage IDs as
 * keycodes, which is what KeyEvent carries, so no key translation happens
 * here. It does not synthesise key repeat, which is what the emulator
 * wants (raw press/release).
 ***************************************************************************/
#include <ogc/machine/processor.h>

#ifdef HW_RVL
#include <wiikeyboard/keyboard.h>
#endif

#include "OgcKeyboardDriver.h"

void OgcKeyboardDriver::init()
{
	#ifdef HW_RVL
	if (initialized)
		return;

	// No keypress callback: events are polled via KEYBOARD_GetEvent, and
	// hotplug arrives there as KEYBOARD_CONNECTED/DISCONNECTED events.
	initialized = (KEYBOARD_Init(NULL) >= 0);
	connected = 0;
	#endif
}

void OgcKeyboardDriver::shutdown()
{
	#ifdef HW_RVL
	if (initialized)
		KEYBOARD_Deinit();
	#endif
	initialized = false;
	connected = 0;
}

#ifdef HW_RVL
static uint16_t MapModifiers(uint16_t m)
{
	uint16_t mods = KEYMOD_NONE;

	if (m & MOD_SHIFT_L)   mods |= KEYMOD_LSHIFT;
	if (m & MOD_SHIFT_R)   mods |= KEYMOD_RSHIFT;
	if (m & MOD_CONTROL_L) mods |= KEYMOD_LCTRL;
	if (m & MOD_CONTROL_R) mods |= KEYMOD_RCTRL;
	if (m & MOD_META_L)    mods |= KEYMOD_LALT;  // libwiikeyboard's "meta" is Alt
	if (m & MOD_META_R)    mods |= KEYMOD_RALT;
	if (m & MOD_CAPSLOCK)  mods |= KEYMOD_CAPS;
	if (m & MOD_NUMLOCK)   mods |= KEYMOD_NUM;

	return mods;
}
#endif

bool OgcKeyboardDriver::poll(KeyEvent & out)
{
	#ifdef HW_RVL
	if (!initialized)
		return false;

	keyboard_event ke;

	// KEYBOARD_GetEvent returns 0 when the queue is empty
	while (KEYBOARD_GetEvent(&ke)) {
		switch (ke.type) {
			case KEYBOARD_CONNECTED:
				connected++;
				break;
			case KEYBOARD_DISCONNECTED:
				if (connected > 0)
					connected--;
				break;
			case KEYBOARD_PRESSED:
			case KEYBOARD_RELEASED:
				out.hidUsage = ke.keycode;
				out.modifiers = MapModifiers(ke.modifiers);
				out.pressed = (ke.type == KEYBOARD_PRESSED);
				return true;
		}
	}
	#else
	(void)out;
	#endif
	return false;
}
