/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2026
 * osk.cpp
 *
 * The in-game on-screen keyboard (see osk.h).
 *
 * The keys are libgui's GuiKeyboard, which edits a text buffer. It is used
 * as it is, with a trick: before each update the buffer is reset to a single
 * space. Whatever the widget then appends is a character that was just
 * typed, and the space disappearing is a backspace. Those become key events
 * for DOSBox, so nothing here depends on the widget's internals and libgui
 * stays as it is upstream.
 *
 * Characters become USB HID usages, US layout, shifted ones with a left
 * shift around them, which is what the keyboard path in SDL_input.cpp takes
 * from a real USB keyboard. Newline types Return.
 *
 * The widget is drawn by the video driver, on the GamePad only, through
 * EmulatorVideoDriver::setGamePadOverlay().
 ***************************************************************************/
#include <stdint.h>
#include <string.h>

#include "libgui/Gui.h"
#include "drivers/Platform.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "drivers/InputData.h"
#include "drivers/InputController.h"
#include "drivers/KeyboardDriver.h"
#include "drivers/Time.h"
#include "osk.h"
#include "videosupport.h"

// USB HID usages (keyboard page 0x07) that are not plain characters
static const uint16_t kUsageReturn = 0x28;
static const uint16_t kUsageBackspace = 0x2A;
static const uint16_t kUsageLeftShift = 0xE1;

// While the keyboard is open the GamePad is redrawn at about this rate. DOSBox
// only presents a frame when the DOS screen changes, which at a quiet prompt
// can be never.
static const int kRefreshIntervalMs = 16;

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
 ***************************************************************************/
static const int kPendingSize = 128;
static KeyEvent pending[kPendingSize];
static int pendingHead = 0;
static int pendingCount = 0;

static void PushEvent(uint16_t usage, uint16_t modifiers, bool pressed)
{
	if (pendingCount >= kPendingSize)
		return;

	KeyEvent & e = pending[(pendingHead + pendingCount) % kPendingSize];
	e.hidUsage = usage;
	e.modifiers = modifiers;
	e.pressed = pressed;
	pendingCount++;
}

//! Press and release, with a left shift around it if the character needs one.
//! Dropped whole if the queue cannot take all of it, so shift is never left down.
static void TypeKey(uint16_t usage, bool shift)
{
	if (pendingCount + 4 > kPendingSize)
		return;

	if (shift)
		PushEvent(kUsageLeftShift, KEYMOD_LSHIFT, true);
	PushEvent(usage, shift ? KEYMOD_LSHIFT : KEYMOD_NONE, true);
	PushEvent(usage, shift ? KEYMOD_LSHIFT : KEYMOD_NONE, false);
	if (shift)
		PushEvent(kUsageLeftShift, KEYMOD_NONE, false);
}

/****************************************************************************
 * The keyboard widget
 ***************************************************************************/
class DosKeyboard : public GuiKeyboard
{
	public:
		DosKeyboard() : GuiKeyboard(seed, 16)
		{
		}

		void update(InputController * c)
		{
			// The widget edits kbtextstr like a text box; see the file comment.
			// Anything after the leading space is new, and losing the space is
			// a backspace.
			kbtextstr[0] = ' ';
			kbtextstr[1] = '\0';

			GuiKeyboard::update(c);

			const size_t len = strlen(kbtextstr);

			if (len == 0)
			{
				TypeKey(kUsageBackspace, false);
			}
			else
			{
				for (size_t i = 1; i < len; i++)
				{
					CharKey key;
					if (CharToKey(kbtextstr[i], key))
						TypeKey(key.usage, key.shift);
				}
			}

			if (len != 1)
				kbText->setText(""); // nothing is kept in the box; DOS shows what was typed
		}

	private:
		static char seed[2];
};

char DosKeyboard::seed[2] = { ' ', '\0' };

/****************************************************************************
 * State
 ***************************************************************************/
static bool active = false;
static unsigned toggleButton = INPUT_BTN_MINUS;
static GuiWindow * root = NULL;
static DosKeyboard * oskWidget = NULL;
static Ticks lastRefresh = 0;

static void DrawOverlay(void *)
{
	if (root)
		root->draw();
}

static bool Build()
{
	if (root)
		return true;

	VideoDriver * video = platform->getVideo();
	root = new GuiWindow(video->getScreenWidth(), video->getScreenHeight());
	oskWidget = new DosKeyboard();
	root->append(oskWidget);
	root->changeFocus(oskWidget);
	return true;
}

static void Open()
{
	EmulatorVideoDriver * emu = platform->getVideo()->getEmulatorVideo();
	if (!emu || !Build())
		return;

	// No second screen to put it on: nothing happens
	if (!emu->setGamePadOverlay(DrawOverlay, NULL))
		return;

	active = true;
	lastRefresh = 0;
	GFX_Refresh();
}

void OSK_Close(void)
{
	if (!active)
		return;

	active = false;

	EmulatorVideoDriver * emu = platform->getVideo()->getEmulatorVideo();
	if (emu)
		emu->setGamePadOverlay(NULL, NULL);
}

bool OSK_IsActive(void)
{
	return active;
}

void OSK_SetToggleButton(unsigned button)
{
	toggleButton = button;
}

bool OSK_PollKey(KeyEvent & out)
{
	if (pendingCount == 0)
		return false;

	out = pending[pendingHead];
	pendingHead = (pendingHead + 1) % kPendingSize;
	pendingCount--;
	return true;
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

	for (int i = 3; i >= 0; i--)
		root->update(controller[i]);

	const Ticks now = SystemTime::now();
	if (lastRefresh == 0 || SystemTime::diffMillisecs(lastRefresh, now) >= kRefreshIntervalMs)
	{
		lastRefresh = now;
		GFX_Refresh();
	}
}
