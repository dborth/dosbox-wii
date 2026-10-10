/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * menurequest.cpp
 *
 * See menurequest.h.
 ***************************************************************************/

#include "menurequest.h"

static const uint32_t MENU_COMBO =
	INPUT_TRIGGER_L | INPUT_TRIGGER_R | INPUT_BTN_PLUS;

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
