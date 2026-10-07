/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutKeyboardDriver.cpp
 ***************************************************************************/

#include "WutKeyboardDriver.h"

void WutKeyboardDriver::init()
{

}

void WutKeyboardDriver::shutdown()
{

}

static uint16_t MapModifiers(uint16_t m)
{
	uint16_t mods = KEYMOD_NONE;

	if (m & MOD_SHIFT_L)   mods |= KEYMOD_LSHIFT;
	if (m & MOD_SHIFT_R)   mods |= KEYMOD_RSHIFT;
	if (m & MOD_CONTROL_L) mods |= KEYMOD_LCTRL;
	if (m & MOD_CONTROL_R) mods |= KEYMOD_RCTRL;
	if (m & MOD_META_L)    mods |= KEYMOD_LALT;
	if (m & MOD_META_R)    mods |= KEYMOD_RALT;
	if (m & MOD_CAPSLOCK)  mods |= KEYMOD_CAPS;
	if (m & MOD_NUMLOCK)   mods |= KEYMOD_NUM;

	return mods;
}

bool WutKeyboardDriver::poll(KeyEvent & out)
{
	return false;
}
