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
	return mods;
}

bool WutKeyboardDriver::poll(KeyEvent & out)
{
	return false;
}
