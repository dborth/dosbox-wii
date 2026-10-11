/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutSystemKeyboard.h
 *
 * The Cafe OS software keyboard (nn::swkbd) on the GamePad.
 ***************************************************************************/
#pragma once

#include <coreinit/filesystem.h>
#include "../SystemKeyboardDriver.h"

class WutInputDriver;

class WutSystemKeyboard : public SystemKeyboardDriver
{
	public:
		//! `input` supplies the GamePad's samples; it must outlive this.
		explicit WutSystemKeyboard(WutInputDriver * input);
		~WutSystemKeyboard() override;

		bool open() override;
		void close() override;
		bool isOpen() const override { return opened; }
		void drawGamePad() override;
		const char16_t * getText() const override;
		bool takeOk() override;
		bool takeCancel() override;

	private:
		//! nn::swkbd is created on first use and kept, as creating it loads
		//! the library and its resources.
		bool ensureCreated();
		bool appear();
		//! One frame of the keyboard's logic, no drawing.
		void calc();
		//! Runs the keyboard's frames until a hidden form has finished fading out.
		void pumpUntilHidden();
		void destroy();

		WutInputDriver * input;
		FSClient * fsClient = nullptr;
		void * workMemory = nullptr;
		bool created = false;
		bool opened = false;
};
