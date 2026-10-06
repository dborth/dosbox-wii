/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcKeyboardDriver.h
 *
 * USB keyboard via libwiikeyboard. See drivers/KeyboardDriver.h for the
 * contract.
 ***************************************************************************/
#pragma once

#include "../KeyboardDriver.h"

class OgcKeyboardDriver : public KeyboardDriver
{
	public:
		void init() override;
		void shutdown() override;
		bool isConnected() const override { return connected > 0; }
		bool poll(KeyEvent & out) override;

	private:
		bool initialized = false;
		int connected = 0; //!< keyboards currently attached
};
