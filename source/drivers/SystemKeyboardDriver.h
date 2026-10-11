/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * SystemKeyboardDriver.h
 *
 * The platform's own on-screen keyboard, shown on the GamePad. On the Wii U
 * this is the Cafe OS software keyboard (nn::swkbd). The platform draws it and
 * handles the touches; the app only sees the text in its input field.
 ***************************************************************************/
#pragma once

class SystemKeyboardDriver
{
	public:
		virtual ~SystemKeyboardDriver() = default;

		//! Shows the keyboard with an empty input field. May load resources the
		//! first time, so it can take a moment. False if it can't be shown.
		virtual bool open() = 0;

		//! Hides the keyboard.
		virtual void close() = 0;

		virtual bool isOpen() const = 0;

		//! Runs the keyboard for one frame (it takes the controllers' input
		//! itself) and draws it. Call it from inside the GamePad's render
		//! context, after anything that should be under the keyboard: the
		//! video driver's GamePad overlay does.
		virtual void drawGamePad() = 0;

		//! What is in the input field now, NUL-terminated UTF-16, as of the last
		//! drawGamePad(). Never null.
		virtual const char16_t * getText() const = 0;

		//! True once each time the user confirms the input field (the Enter
		//! button). The field is empty again afterwards.
		virtual bool takeOk() = 0;

		//! True once each time the user dismisses the keyboard.
		virtual bool takeCancel() = 0;
};
