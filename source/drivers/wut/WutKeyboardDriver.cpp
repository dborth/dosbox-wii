/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutKeyboardDriver.cpp
 *
 * The OS keyboard library (nsyskbd) delivers key events through a callback
 * on its own context. The callback only appends to a small queue; poll()
 * drains it on the main thread.
 *
 * The library reports USB HID usage IDs (KBDKeyEvent::hidCode), which is
 * what KeyEvent carries, so no key translation happens here. It does not
 * report modifier state with each key, so it is tracked here from the
 * modifier usages (0xE0-0xE7) and the caps/num lock keys.
 *
 * The emulator wants raw press/release. Anything the library reports as
 * "still held" is dropped: a press is only passed on for a key that is not
 * already down, a release only for a key that is.
 ***************************************************************************/
#include <nsyskbd/nsyskbd.h>

#include "WutKeyboardDriver.h"
#include "../Mutex.h"

// KBDKeyEvent::isPressedDown is typed as a BOOL, but the library has been
// observed to use 1 = pressed, 0 = released, 3 = held (auto-repeat).
static const int32_t kKeyStateHeld = 3;

// USB HID usages (keyboard page 0x07)
static const uint8_t kUsageCapsLock = 0x39;
static const uint8_t kUsageNumLock = 0x53;
static const uint8_t kUsageLeftCtrl = 0xE0;
static const uint8_t kUsageRightGui = 0xE7;

static WutKeyboardDriver * instance = nullptr;

static void AttachTrampoline(KBDAttachEvent * ev)
{
	if (instance && ev)
		instance->onAttach(ev->channel);
}

static void DetachTrampoline(KBDAttachEvent * ev)
{
	if (instance && ev)
		instance->onDetach(ev->channel);
}

static void KeyTrampoline(KBDKeyEvent * ev)
{
	if (instance && ev)
		instance->onKey(ev->hidCode, (int32_t)ev->isPressedDown);
}

void WutKeyboardDriver::init()
{
	if (initialized)
		return;

	// Callbacks can fire from inside KBDSetup() (for keyboards that are
	// already plugged in), so everything they touch is set up first.
	lock = new Mutex();
	instance = this;
	connectedMask = 0;
	head = count = 0;
	modifiers = 0;
	for (int i = 0; i < 32; i++)
		down[i] = 0;

	initialized = (KBDSetup(AttachTrampoline, DetachTrampoline, KeyTrampoline) == KDB_ERROR_NONE);

	if (!initialized)
	{
		instance = nullptr;
		delete lock;
		lock = nullptr;
	}
}

void WutKeyboardDriver::shutdown()
{
	if (!initialized)
		return;

	// Stop callbacks reaching us before tearing down what they use
	instance = nullptr;
	KBDTeardown();

	delete lock;
	lock = nullptr;
	connectedMask = 0;
	initialized = false;
}

bool WutKeyboardDriver::push(uint16_t hidUsage, bool pressed)
{
	if (count >= kQueueSize)
		return false;

	KeyEvent & e = queue[(head + count) % kQueueSize];
	e.hidUsage = hidUsage;
	e.modifiers = modifiers;
	e.pressed = pressed;
	count++;
	return true;
}

void WutKeyboardDriver::releaseAll()
{
	for (int usage = 0; usage < 256; usage++)
	{
		if (down[usage >> 3] & (1 << (usage & 7)))
		{
			// Only forget the key once its release is queued, so a full
			// queue can't leave the emulator with a key stuck down
			if (push((uint16_t)usage, false))
				down[usage >> 3] &= ~(1 << (usage & 7));
		}
	}
	modifiers = 0;
}

void WutKeyboardDriver::onAttach(uint8_t channel)
{
	if (!lock || channel >= 32)
		return;

	MutexLock guard(*lock);
	connectedMask |= (1u << channel);
}

void WutKeyboardDriver::onDetach(uint8_t channel)
{
	if (!lock || channel >= 32)
		return;

	MutexLock guard(*lock);
	connectedMask &= ~(1u << channel);

	// Nothing will release the keys of a keyboard that is gone
	if (connectedMask == 0)
		releaseAll();
}

void WutKeyboardDriver::onKey(uint8_t hidCode, int32_t state)
{
	if (!lock || state == kKeyStateHeld)
		return;

	const bool pressed = (state != 0);
	const uint8_t bit = (uint8_t)(1 << (hidCode & 7));
	uint8_t & slot = down[hidCode >> 3];

	MutexLock guard(*lock);

	if (pressed == ((slot & bit) != 0))
		return; // repeat of a held key, or release of a key we never saw go down

	// Track modifier state, so it is current for this event
	if (hidCode >= kUsageLeftCtrl && hidCode <= kUsageRightGui)
	{
		static const uint16_t modBits[8] = {
			KEYMOD_LCTRL, KEYMOD_LSHIFT, KEYMOD_LALT, KEYMOD_LGUI,
			KEYMOD_RCTRL, KEYMOD_RSHIFT, KEYMOD_RALT, KEYMOD_RGUI
		};
		const uint16_t m = modBits[hidCode - kUsageLeftCtrl];
		if (pressed) modifiers |= m; else modifiers &= ~m;
	}
	else if (pressed && hidCode == kUsageCapsLock)
		modifiers ^= KEYMOD_CAPS;
	else if (pressed && hidCode == kUsageNumLock)
		modifiers ^= KEYMOD_NUM;

	// Only record the key as down/up if the event made it into the queue
	if (push(hidCode, pressed))
	{
		if (pressed) slot |= bit; else slot &= ~bit;
	}
}

bool WutKeyboardDriver::poll(KeyEvent & out)
{
	if (!initialized || !lock)
		return false;

	MutexLock guard(*lock);
	if (count == 0)
		return false;

	out = queue[head];
	head = (head + 1) % kQueueSize;
	count--;
	return true;
}
