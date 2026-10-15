/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * osk.cpp
 *
 * The in-game on-screen keyboard (see osk.h).
 *
 * The keyboard is the platform's own (SystemKeyboardDriver; on the Wii U the
 * Cafe OS software keyboard), shown on the GamePad only. It edits a text field.
 * Whatever changes in that field is turned into key events for DOSBox: added
 * characters are typed, removed ones are backspaces, and the keyboard's Enter
 * button is Return. They go out through the same KeyboardDriver interface a USB
 * keyboard uses.
 *
 * Characters become USB HID usages, US layout, shifted ones with a left
 * shift around them, which is what the keyboard path in SDL_input.cpp takes
 * from a real USB keyboard.
 *
 * The same events carry text typed from elsewhere (OSK_TypeText, which the
 * menu's keyboard uses): the text waits in a buffer and is turned into key
 * events as there is room, so any length can be typed.
 *
 * The keyboard is drawn by the video driver, on the GamePad only, through
 * EmulatorVideoDriver::setGamePadOverlay(), and refreshed from here every
 * input scan without waiting. The TV is not involved.
 ***************************************************************************/
#include <stdint.h>
#include <string.h>
#include <string>

#include "drivers/Platform.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "drivers/InputData.h"
#include "drivers/InputController.h"
#include "drivers/KeyboardDriver.h"
#include "drivers/SystemKeyboardDriver.h"
#include "drivers/Time.h"
#include "osk.h"
#include "videosupport.h"

// USB HID usages (keyboard page 0x07) that are not plain characters
static const uint16_t kUsageReturn = 0x28;
static const uint16_t kUsageBackspace = 0x2A;
static const uint16_t kUsageLeftShift = 0xE1;

/****************************************************************************
 * Character -> key
 ***************************************************************************/
struct CharKey
{
	uint16_t usage;
	bool shift;
};

static bool CharToKey(char c, CharKey & out)
{
	out.shift = false;

	if (c >= 'a' && c <= 'z') { out.usage = 0x04 + (c - 'a'); return true; }
	if (c >= 'A' && c <= 'Z') { out.usage = 0x04 + (c - 'A'); out.shift = true; return true; }
	if (c >= '1' && c <= '9') { out.usage = 0x1E + (c - '1'); return true; }
	if (c == '0') { out.usage = 0x27; return true; }

	switch (c)
	{
		case ' ':  out.usage = 0x2C; return true;
		case '\n': out.usage = kUsageReturn; return true;

		case '!': out.usage = 0x1E; out.shift = true; return true;
		case '@': out.usage = 0x1F; out.shift = true; return true;
		case '#': out.usage = 0x20; out.shift = true; return true;
		case '$': out.usage = 0x21; out.shift = true; return true;
		case '%': out.usage = 0x22; out.shift = true; return true;
		case '^': out.usage = 0x23; out.shift = true; return true;
		case '&': out.usage = 0x24; out.shift = true; return true;
		case '*': out.usage = 0x25; out.shift = true; return true;
		case '(': out.usage = 0x26; out.shift = true; return true;
		case ')': out.usage = 0x27; out.shift = true; return true;

		case '-':  out.usage = 0x2D; return true;
		case '_':  out.usage = 0x2D; out.shift = true; return true;
		case '=':  out.usage = 0x2E; return true;
		case '+':  out.usage = 0x2E; out.shift = true; return true;
		case '[':  out.usage = 0x2F; return true;
		case '{':  out.usage = 0x2F; out.shift = true; return true;
		case ']':  out.usage = 0x30; return true;
		case '}':  out.usage = 0x30; out.shift = true; return true;
		case '\\': out.usage = 0x31; return true;
		case '|':  out.usage = 0x31; out.shift = true; return true;
		case ';':  out.usage = 0x33; return true;
		case ':':  out.usage = 0x33; out.shift = true; return true;
		case '\'': out.usage = 0x34; return true;
		case '"':  out.usage = 0x34; out.shift = true; return true;
		case '`':  out.usage = 0x35; return true;
		case '~':  out.usage = 0x35; out.shift = true; return true;
		case ',':  out.usage = 0x36; return true;
		case '<':  out.usage = 0x36; out.shift = true; return true;
		case '.':  out.usage = 0x37; return true;
		case '>':  out.usage = 0x37; out.shift = true; return true;
		case '/':  out.usage = 0x38; return true;
		case '?':  out.usage = 0x38; out.shift = true; return true;
	}
	return false;
}

/****************************************************************************
 * Pending key events
 *
 * Events wait here until they are polled. A real keyboard's press and release
 * are never together, and DOS programs that read the keyboard port directly
 * can miss a key that is down for less than a scan, so each event carries the
 * least time that must pass after the previous one was handed out.
 ***************************************************************************/
static const int kPendingSize = 128;
static const uint32_t kGapMs = 8;       // between events
static const uint32_t kHoldMs = 40;     // a key is held this long

struct PendingEvent
{
	KeyEvent event;
	uint32_t gapMs;
	bool starts;		//!< the first event of a key: nothing of it has been delivered before this
};

static PendingEvent pending[kPendingSize];
static int pendingHead = 0;
static int pendingCount = 0;
static Ticks lastDelivered = 0;
static bool haveDelivered = false;

static void PushEvent(uint16_t usage, uint16_t modifiers, bool pressed, uint32_t gapMs, bool starts = false)
{
	if (pendingCount >= kPendingSize)
		return;

	PendingEvent & p = pending[(pendingHead + pendingCount) % kPendingSize];
	p.event.hidUsage = usage;
	p.event.modifiers = modifiers;
	p.event.pressed = pressed;
	p.gapMs = gapMs;
	p.starts = starts;
	pendingCount++;
}

//! Press and release, with a left shift around it if the character needs one.
//! Dropped whole if the queue cannot take all of it, so shift is never left down.
static void TypeKey(uint16_t usage, bool shift)
{
	if (pendingCount + 4 > kPendingSize)
		return;

	if (shift)
		PushEvent(kUsageLeftShift, KEYMOD_LSHIFT, true, kGapMs, true);
	PushEvent(usage, shift ? KEYMOD_LSHIFT : KEYMOD_NONE, true, kGapMs, !shift);
	PushEvent(usage, shift ? KEYMOD_LSHIFT : KEYMOD_NONE, false, kHoldMs);
	if (shift)
		PushEvent(kUsageLeftShift, KEYMOD_NONE, false, kGapMs);
}

//! Text waiting to become key events (OSK_TypeText)
static const size_t kTextMax = 1024;
static std::string textQueue;
static size_t textPos = 0;

//! Turns waiting text into key events while there is room for a whole key
static void FeedText()
{
	while (textPos < textQueue.size() && pendingCount + 4 <= kPendingSize)
	{
		char c = textQueue[textPos++];
		CharKey key;

		if (c == '\r')
			c = '\n';

		if (CharToKey(c, key))
			TypeKey(key.usage, key.shift);
	}

	if (textPos >= textQueue.size())
	{
		textQueue.clear();
		textPos = 0;
	}
}

void OSK_TypeText(const char * text)
{
	if (!text)
		return;

	const size_t waiting = textQueue.size() - textPos;
	const size_t room = (waiting < kTextMax) ? kTextMax - waiting : 0;

	size_t length = 0;
	while (length < room && text[length] != 0)
		length++;

	textQueue.append(text, length);
}

void OSK_CancelTyping(void)
{
	textQueue.clear();
	textPos = 0;

	// Keep the key in progress, so nothing is left held; drop the rest
	for (int i = 0; i < pendingCount; i++)
	{
		if (pending[(pendingHead + i) % kPendingSize].starts)
		{
			pendingCount = i;
			return;
		}
	}
}

//! The next event whose time has come. Events still waiting when the keyboard
//! is closed are delivered all the same, so a key is never left down.
static bool PollTyped(KeyEvent & out)
{
	FeedText();

	if (pendingCount == 0)
		return false;

	const Ticks now = SystemTime::now();
	const PendingEvent & p = pending[pendingHead];

	if (haveDelivered && SystemTime::diffMillisecs(lastDelivered, now) < p.gapMs)
		return false;

	out = p.event;
	pendingHead = (pendingHead + 1) % kPendingSize;
	pendingCount--;
	lastDelivered = now;
	haveDelivered = true;
	return true;
}

/****************************************************************************
 * The keyboard seen by the rest of the app: the physical one, then ours
 ***************************************************************************/
class OskKeyboardMux : public KeyboardDriver
{
	public:
		OskKeyboardMux(KeyboardDriver * p) : physical(p) {}

		void init() override { physical->init(); }
		void shutdown() override { physical->shutdown(); }
		bool isConnected() const override { return physical->isConnected(); }
		void refresh() override { physical->refresh(); }

		bool poll(KeyEvent & out) override
		{
			return physical->poll(out) || PollTyped(out);
		}

	private:
		KeyboardDriver * physical;
};

KeyboardDriver * OSK_WrapKeyboard(KeyboardDriver * physical)
{
	static OskKeyboardMux mux(physical);
	return &mux;
}

/****************************************************************************
 * Text field -> keys
 ***************************************************************************/
//! What of the keyboard's text field has been typed into DOS so far
static std::u16string typed;

static bool IsTypable(char16_t c)
{
	CharKey key;
	return c < 128 && CharToKey((char)c, key);
}

//! Types what changed in the field since last time: the first difference on is
//! backspaced away and what the field now has there is typed. Stops, and picks
//! up again next scan, if the pending events have no room.
static void SyncText(const char16_t * text)
{
	size_t common = 0;
	while (common < typed.size() && text[common] != 0 && typed[common] == text[common])
		common++;

	while (typed.size() > common)
	{
		const char16_t gone = typed[typed.size() - 1];

		if (IsTypable(gone))
		{
			if (pendingCount + 2 > kPendingSize)
				return;
			TypeKey(kUsageBackspace, false);
		}

		typed.pop_back();
	}

	for (; text[common] != 0; common++)
	{
		const char16_t c = text[common];
		CharKey key;

		if (c < 128 && CharToKey((char)c, key))
		{
			if (pendingCount + 4 > kPendingSize)
				return;
			TypeKey(key.usage, key.shift);
		}

		typed.push_back(c);
	}
}

/****************************************************************************
 * State
 ***************************************************************************/
static bool active = false;
static unsigned toggleButton = INPUT_BTN_TV;
static SystemKeyboardDriver * systemKeyboard = NULL;

static void DrawOverlay(void *)
{
	if (systemKeyboard)
		systemKeyboard->drawGamePad();
}

static void Open()
{
	EmulatorVideoDriver * emu = platform->getVideo()->getEmulatorVideo();
	SystemKeyboardDriver * keyboard = platform->getSystemKeyboard();

	// No second screen or no system keyboard to put on it: nothing happens
	if (!emu || !keyboard)
		return;

	systemKeyboard = keyboard;

	if (!emu->setGamePadOverlay(DrawOverlay, NULL))
		return;

	if (!keyboard->open())
	{
		emu->setGamePadOverlay(NULL, NULL);
		return;
	}

	typed.clear();
	active = true;
}

void OSK_Close(void)
{
	if (!active)
		return;

	active = false;

	EmulatorVideoDriver * emu = platform->getVideo()->getEmulatorVideo();
	if (emu)
		emu->setGamePadOverlay(NULL, NULL);

	systemKeyboard->close();
}

bool OSK_IsActive(void)
{
	return active;
}

void OSK_SetToggleButton(unsigned button)
{
	toggleButton = button;
}

void OSK_Update(void)
{
	// Open/close on the GamePad button
	for (int i = 0; i < 4; i++)
	{
		const InputPadData & pad = controller[i]->getPadData();

		if (pad.hw_connected[INPUT_HW_DRC] && (pad.hw_buttons_d[INPUT_HW_DRC] & toggleButton))
		{
			if (active)
			{
				OSK_Close();
				GFX_Refresh(); // the game is not redrawn until DOS next draws
			}
			else
			{
				Open();
			}
			break;
		}
	}

	if (!active)
		return;

	// Nothing can be drawn without the foreground
	if (platform->getStatus() != Status::Running)
	{
		OSK_Close();
		return;
	}

	SyncText(systemKeyboard->getText());

	if (systemKeyboard->takeOk())
	{
		if (pendingCount + 2 <= kPendingSize)
			TypeKey(kUsageReturn, false);
		typed.clear(); // the field is empty again
	}

	if (systemKeyboard->takeCancel())
	{
		OSK_Close();
		GFX_Refresh();
		return;
	}

	platform->getVideo()->getEmulatorVideo()->refreshGamePadOverlay();
}
