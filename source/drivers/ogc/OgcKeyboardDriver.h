/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * OgcKeyboardDriver.h
 *
 * STUB. Reports no keyboard and no events. The real implementation
 * (libwiikeyboard) is specified in drivers/KeyboardDriver.h.
 ***************************************************************************/
#pragma once

#include "../KeyboardDriver.h"

class OgcKeyboardDriver : public KeyboardDriver
{
	public:
		void init() override;
		void shutdown() override;
		bool isConnected() const override { return false; }
		bool poll(KeyEvent & out) override;
};
