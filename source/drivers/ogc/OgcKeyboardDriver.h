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
		void refresh() override;

	private:
		bool fetch(KeyEvent & out); //!< next key event from libwiikeyboard, tracking attach/detach on the way

		static const int kHeldSize = 64;
		KeyEvent held[kHeldSize]; //!< key events refresh() took off libwiikeyboard's queue, oldest first
		int heldHead = 0;
		int heldCount = 0;

		bool initialized = false;
		int connected = 0; //!< keyboards currently attached
};
