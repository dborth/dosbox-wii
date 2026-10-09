/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutKeyboardDriver.h
 *
 * USB keyboard via the OS keyboard library (nsyskbd). See
 * drivers/KeyboardDriver.h for the contract.
 ***************************************************************************/
#pragma once

#include <stdint.h>
#include "../KeyboardDriver.h"

class Mutex;

class WutKeyboardDriver : public KeyboardDriver
{
	public:
		void init() override;
		void shutdown() override;
		bool isConnected() const override { return connectedMask != 0; }
		bool poll(KeyEvent & out) override;

		//!\name OS callbacks
		//!Called from the keyboard library's context, not the main thread.
		//!Only for use by the static trampolines in WutKeyboardDriver.cpp.
		//!@{
		void onAttach(uint8_t channel);
		void onDetach(uint8_t channel);
		void onKey(uint8_t hidCode, int32_t state);
		//!@}

	private:
		static const int kQueueSize = 256;

		//! Caller holds `lock`. Returns false if the queue is full.
		bool push(uint16_t hidUsage, bool pressed);
		//! Caller holds `lock`. Releases every key still held.
		void releaseAll();

		bool initialized = false;
		volatile uint32_t connectedMask = 0; //!< one bit per keyboard channel
		Mutex * lock = nullptr;

		KeyEvent queue[kQueueSize];
		int head = 0;
		int count = 0;

		uint16_t modifiers = 0;   //!< KeyModifiers, tracked from key events
		uint8_t down[32] = {0};   //!< bitset of HID usages currently held
};
