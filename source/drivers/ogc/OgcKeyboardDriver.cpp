/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcKeyboardDriver.cpp
 *
 * STUB: USB keyboards do not work until this is implemented per the plan
 * in drivers/KeyboardDriver.h. The on-screen keyboard (GuiKeyboard +
 * PressKeys in wiihardware.cpp) is unaffected.
 ***************************************************************************/
#include "OgcKeyboardDriver.h"

void OgcKeyboardDriver::init() {}

void OgcKeyboardDriver::shutdown() {}

bool OgcKeyboardDriver::poll(KeyEvent &)
{
	return false;
}
