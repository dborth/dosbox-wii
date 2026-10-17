/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * inputlogic.cpp
 *
 * See inputlogic.h.
 ***************************************************************************/

#include "inputlogic.h"
#include <string.h>

static const uint32_t MENU_COMBO =
	INPUT_TRIGGER_L | INPUT_TRIGGER_R | INPUT_BTN_PLUS;

bool PadIsLoneWiimote(const InputPadData & pad)
{
	return !(pad.hw_connected[INPUT_HW_GAMECUBE] || pad.hw_connected[INPUT_HW_CLASSIC] ||
			 pad.hw_connected[INPUT_HW_WUPC] || pad.hw_connected[INPUT_HW_DRC]);
}

bool DpadEventKeepsJoystick(const char * event)
{
	return event && strncmp(event, "jhat_", 5) == 0;
}

uint32_t PadButtonsForDos(const InputPadData & pad, bool sideways)
{
	const uint32_t b = pad.buttons_h;
	if (!sideways || !PadIsLoneWiimote(pad))
		return b;

	const uint32_t dpad = INPUT_BTN_UP | INPUT_BTN_DOWN | INPUT_BTN_LEFT | INPUT_BTN_RIGHT;
	uint32_t r = b & ~dpad;
	if (b & INPUT_BTN_UP)    r |= INPUT_BTN_LEFT;
	if (b & INPUT_BTN_DOWN)  r |= INPUT_BTN_RIGHT;
	if (b & INPUT_BTN_LEFT)  r |= INPUT_BTN_DOWN;
	if (b & INPUT_BTN_RIGHT) r |= INPUT_BTN_UP;
	return r;
}

bool MenuRequestFromPad(const InputPadData & pad)
{
	if (pad.buttons_h & INPUT_BTN_HOME)
		return true;

	for (uint32_t hw = 0; hw < INPUT_HW_MAX; hw++)
	{
		if (!pad.hw_connected[hw])
			continue;
		if ((pad.hw_buttons_h[hw] & MENU_COMBO) == MENU_COMBO)
			return true;
	}

	return false;
}

bool MenuKeyTap::isMenuKey(unsigned hidUsage)
{
	return hidUsage == MENU_KEY_LGUI || hidUsage == MENU_KEY_RGUI;
}

void MenuKeyTap::onKey(unsigned hidUsage, bool pressed)
{
	if (hidUsage == MENU_KEY_LGUI || hidUsage == MENU_KEY_RGUI)
	{
		bool & held = (hidUsage == MENU_KEY_LGUI) ? leftHeld : rightHeld;

		if (pressed)
		{
			// A tap starts when the first GUI key goes down; a second one
			// pressed while the first is held joins it
			if (!leftHeld && !rightHeld)
				armed = true;
			held = true;
		}
		else
		{
			held = false;
			if (!leftHeld && !rightHeld && armed)
			{
				pending = true;
				armed = false;
			}
		}
	}
	else if (pressed)
	{
		// GUI + another key is a shortcut, not a tap
		armed = false;
	}
}

bool MenuKeyTap::consume()
{
	const bool tapped = pending;
	pending = false;
	return tapped;
}

void MenuKeyTap::reset()
{
	leftHeld = rightHeld = false;
	armed = pending = false;
}
