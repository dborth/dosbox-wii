/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutKeyboardDriver.h
 ***************************************************************************/
#pragma once

#include "../KeyboardDriver.h"

class WutKeyboardDriver : public KeyboardDriver
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
