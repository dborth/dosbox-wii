/****************************************************************************
 * DOSBox Wii
 * input_hal.cpp
 *
 * DOSBox's input layer on the platform HAL. See input_hal.h.
 *
 * Responsibilities:
 *  - turn HAL pad state into the eight SDL-style joysticks the mapper expects
 *  - turn the Wiimote IR pointer and a USB mouse into mouse events
 *  - turn USB keyboard HID usages into DOSBox's key table (SDLK_*)
 *  - hold the event queue everything above posts into
 *
 * The key name table and the HID -> key mapping are ported from the SDL 1.2
 * sources (LGPL 2.1+), see input_keys.h.
 ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "input_hal.h"

#include "drivers/Platform.h"
#include "drivers/InputData.h"
#include "drivers/InputDriver.h"
#include "drivers/InputController.h"
#include "drivers/KeyboardDriver.h"
#include "drivers/MouseDriver.h"
#include "drivers/Mutex.h"
#include "drivers/Time.h"

/* Pad state is rescanned at most this often. GFX_Events() runs far more
 * often than the hardware updates, and a scan is not free. */
#define SCAN_INTERVAL_MS 4

#define NUM_WII_JOYSTICKS 4
#define NUM_GC_JOYSTICKS  4
#define NUM_JOYSTICKS     (NUM_WII_JOYSTICKS + NUM_GC_JOYSTICKS)

#define WII_AXES    9
#define WII_BUTTONS 20
#define GC_AXES     6
#define GC_BUTTONS  8

#define EVENT_QUEUE_SIZE 256

#define MOUSE_W 640
#define MOUSE_H 480

/****************************************************************************
 * Event queue
 ***************************************************************************/
static SDL_Event eventQueue[EVENT_QUEUE_SIZE];
static int eventHead = 0;
static int eventCount = 0;
static Mutex queueMutex;

int SDL_PushEvent(SDL_Event * event)
{
	MutexLock lock(queueMutex);
	if (eventCount >= EVENT_QUEUE_SIZE)
		return -1;
	eventQueue[(eventHead + eventCount) % EVENT_QUEUE_SIZE] = *event;
	eventCount++;
	return 0;
}

int SDL_PollEvent(SDL_Event * event)
{
	MutexLock lock(queueMutex);
	if (eventCount == 0)
		return 0;
	if (event)
		*event = eventQueue[eventHead];
	eventHead = (eventHead + 1) % EVENT_QUEUE_SIZE;
	eventCount--;
	return 1;
}

int SDL_WaitEvent(SDL_Event * event)
{
	while (!SDL_PollEvent(event)) {
		InputHal_Update();
		usleep(1000);
	}
	return 1;
}

/****************************************************************************
 * Keyboard
 ***************************************************************************/
static SDLMod modState = KMOD_NONE;
static const char * keynames[SDLK_LAST];

static void InitKeyNames(void)
{
	memset(keynames, 0, sizeof(keynames));
	keynames[SDLK_BACKSPACE] = "backspace";
	keynames[SDLK_TAB] = "tab";
	keynames[SDLK_CLEAR] = "clear";
	keynames[SDLK_RETURN] = "return";
	keynames[SDLK_PAUSE] = "pause";
	keynames[SDLK_ESCAPE] = "escape";
	keynames[SDLK_SPACE] = "space";
	keynames[SDLK_EXCLAIM]  = "!";
	keynames[SDLK_QUOTEDBL]  = "\"";
	keynames[SDLK_HASH]  = "#";
	keynames[SDLK_DOLLAR]  = "$";
	keynames[SDLK_AMPERSAND]  = "&";
	keynames[SDLK_QUOTE] = "'";
	keynames[SDLK_LEFTPAREN] = "(";
	keynames[SDLK_RIGHTPAREN] = ")";
	keynames[SDLK_ASTERISK] = "*";
	keynames[SDLK_PLUS] = "+";
	keynames[SDLK_COMMA] = ",";
	keynames[SDLK_MINUS] = "-";
	keynames[SDLK_PERIOD] = ".";
	keynames[SDLK_SLASH] = "/";
	keynames[SDLK_0] = "0";
	keynames[SDLK_1] = "1";
	keynames[SDLK_2] = "2";
	keynames[SDLK_3] = "3";
	keynames[SDLK_4] = "4";
	keynames[SDLK_5] = "5";
	keynames[SDLK_6] = "6";
	keynames[SDLK_7] = "7";
	keynames[SDLK_8] = "8";
	keynames[SDLK_9] = "9";
	keynames[SDLK_COLON] = ":";
	keynames[SDLK_SEMICOLON] = ";";
	keynames[SDLK_LESS] = "<";
	keynames[SDLK_EQUALS] = "=";
	keynames[SDLK_GREATER] = ">";
	keynames[SDLK_QUESTION] = "?";
	keynames[SDLK_AT] = "@";
	keynames[SDLK_LEFTBRACKET] = "[";
	keynames[SDLK_BACKSLASH] = "\\";
	keynames[SDLK_RIGHTBRACKET] = "]";
	keynames[SDLK_CARET] = "^";
	keynames[SDLK_UNDERSCORE] = "_";
	keynames[SDLK_BACKQUOTE] = "`";
	keynames[SDLK_a] = "a";
	keynames[SDLK_b] = "b";
	keynames[SDLK_c] = "c";
	keynames[SDLK_d] = "d";
	keynames[SDLK_e] = "e";
	keynames[SDLK_f] = "f";
	keynames[SDLK_g] = "g";
	keynames[SDLK_h] = "h";
	keynames[SDLK_i] = "i";
	keynames[SDLK_j] = "j";
	keynames[SDLK_k] = "k";
	keynames[SDLK_l] = "l";
	keynames[SDLK_m] = "m";
	keynames[SDLK_n] = "n";
	keynames[SDLK_o] = "o";
	keynames[SDLK_p] = "p";
	keynames[SDLK_q] = "q";
	keynames[SDLK_r] = "r";
	keynames[SDLK_s] = "s";
	keynames[SDLK_t] = "t";
	keynames[SDLK_u] = "u";
	keynames[SDLK_v] = "v";
	keynames[SDLK_w] = "w";
	keynames[SDLK_x] = "x";
	keynames[SDLK_y] = "y";
	keynames[SDLK_z] = "z";
	keynames[SDLK_DELETE] = "delete";
	keynames[SDLK_WORLD_0] = "world 0";
	keynames[SDLK_WORLD_1] = "world 1";
	keynames[SDLK_WORLD_2] = "world 2";
	keynames[SDLK_WORLD_3] = "world 3";
	keynames[SDLK_WORLD_4] = "world 4";
	keynames[SDLK_WORLD_5] = "world 5";
	keynames[SDLK_WORLD_6] = "world 6";
	keynames[SDLK_WORLD_7] = "world 7";
	keynames[SDLK_WORLD_8] = "world 8";
	keynames[SDLK_WORLD_9] = "world 9";
	keynames[SDLK_WORLD_10] = "world 10";
	keynames[SDLK_WORLD_11] = "world 11";
	keynames[SDLK_WORLD_12] = "world 12";
	keynames[SDLK_WORLD_13] = "world 13";
	keynames[SDLK_WORLD_14] = "world 14";
	keynames[SDLK_WORLD_15] = "world 15";
	keynames[SDLK_WORLD_16] = "world 16";
	keynames[SDLK_WORLD_17] = "world 17";
	keynames[SDLK_WORLD_18] = "world 18";
	keynames[SDLK_WORLD_19] = "world 19";
	keynames[SDLK_WORLD_20] = "world 20";
	keynames[SDLK_WORLD_21] = "world 21";
	keynames[SDLK_WORLD_22] = "world 22";
	keynames[SDLK_WORLD_23] = "world 23";
	keynames[SDLK_WORLD_24] = "world 24";
	keynames[SDLK_WORLD_25] = "world 25";
	keynames[SDLK_WORLD_26] = "world 26";
	keynames[SDLK_WORLD_27] = "world 27";
	keynames[SDLK_WORLD_28] = "world 28";
	keynames[SDLK_WORLD_29] = "world 29";
	keynames[SDLK_WORLD_30] = "world 30";
	keynames[SDLK_WORLD_31] = "world 31";
	keynames[SDLK_WORLD_32] = "world 32";
	keynames[SDLK_WORLD_33] = "world 33";
	keynames[SDLK_WORLD_34] = "world 34";
	keynames[SDLK_WORLD_35] = "world 35";
	keynames[SDLK_WORLD_36] = "world 36";
	keynames[SDLK_WORLD_37] = "world 37";
	keynames[SDLK_WORLD_38] = "world 38";
	keynames[SDLK_WORLD_39] = "world 39";
	keynames[SDLK_WORLD_40] = "world 40";
	keynames[SDLK_WORLD_41] = "world 41";
	keynames[SDLK_WORLD_42] = "world 42";
	keynames[SDLK_WORLD_43] = "world 43";
	keynames[SDLK_WORLD_44] = "world 44";
	keynames[SDLK_WORLD_45] = "world 45";
	keynames[SDLK_WORLD_46] = "world 46";
	keynames[SDLK_WORLD_47] = "world 47";
	keynames[SDLK_WORLD_48] = "world 48";
	keynames[SDLK_WORLD_49] = "world 49";
	keynames[SDLK_WORLD_50] = "world 50";
	keynames[SDLK_WORLD_51] = "world 51";
	keynames[SDLK_WORLD_52] = "world 52";
	keynames[SDLK_WORLD_53] = "world 53";
	keynames[SDLK_WORLD_54] = "world 54";
	keynames[SDLK_WORLD_55] = "world 55";
	keynames[SDLK_WORLD_56] = "world 56";
	keynames[SDLK_WORLD_57] = "world 57";
	keynames[SDLK_WORLD_58] = "world 58";
	keynames[SDLK_WORLD_59] = "world 59";
	keynames[SDLK_WORLD_60] = "world 60";
	keynames[SDLK_WORLD_61] = "world 61";
	keynames[SDLK_WORLD_62] = "world 62";
	keynames[SDLK_WORLD_63] = "world 63";
	keynames[SDLK_WORLD_64] = "world 64";
	keynames[SDLK_WORLD_65] = "world 65";
	keynames[SDLK_WORLD_66] = "world 66";
	keynames[SDLK_WORLD_67] = "world 67";
	keynames[SDLK_WORLD_68] = "world 68";
	keynames[SDLK_WORLD_69] = "world 69";
	keynames[SDLK_WORLD_70] = "world 70";
	keynames[SDLK_WORLD_71] = "world 71";
	keynames[SDLK_WORLD_72] = "world 72";
	keynames[SDLK_WORLD_73] = "world 73";
	keynames[SDLK_WORLD_74] = "world 74";
	keynames[SDLK_WORLD_75] = "world 75";
	keynames[SDLK_WORLD_76] = "world 76";
	keynames[SDLK_WORLD_77] = "world 77";
	keynames[SDLK_WORLD_78] = "world 78";
	keynames[SDLK_WORLD_79] = "world 79";
	keynames[SDLK_WORLD_80] = "world 80";
	keynames[SDLK_WORLD_81] = "world 81";
	keynames[SDLK_WORLD_82] = "world 82";
	keynames[SDLK_WORLD_83] = "world 83";
	keynames[SDLK_WORLD_84] = "world 84";
	keynames[SDLK_WORLD_85] = "world 85";
	keynames[SDLK_WORLD_86] = "world 86";
	keynames[SDLK_WORLD_87] = "world 87";
	keynames[SDLK_WORLD_88] = "world 88";
	keynames[SDLK_WORLD_89] = "world 89";
	keynames[SDLK_WORLD_90] = "world 90";
	keynames[SDLK_WORLD_91] = "world 91";
	keynames[SDLK_WORLD_92] = "world 92";
	keynames[SDLK_WORLD_93] = "world 93";
	keynames[SDLK_WORLD_94] = "world 94";
	keynames[SDLK_WORLD_95] = "world 95";
	keynames[SDLK_KP0] = "[0]";
	keynames[SDLK_KP1] = "[1]";
	keynames[SDLK_KP2] = "[2]";
	keynames[SDLK_KP3] = "[3]";
	keynames[SDLK_KP4] = "[4]";
	keynames[SDLK_KP5] = "[5]";
	keynames[SDLK_KP6] = "[6]";
	keynames[SDLK_KP7] = "[7]";
	keynames[SDLK_KP8] = "[8]";
	keynames[SDLK_KP9] = "[9]";
	keynames[SDLK_KP_PERIOD] = "[.]";
	keynames[SDLK_KP_DIVIDE] = "[/]";
	keynames[SDLK_KP_MULTIPLY] = "[*]";
	keynames[SDLK_KP_MINUS] = "[-]";
	keynames[SDLK_KP_PLUS] = "[+]";
	keynames[SDLK_KP_ENTER] = "enter";
	keynames[SDLK_KP_EQUALS] = "equals";
	keynames[SDLK_UP] = "up";
	keynames[SDLK_DOWN] = "down";
	keynames[SDLK_RIGHT] = "right";
	keynames[SDLK_LEFT] = "left";
	keynames[SDLK_DOWN] = "down";
	keynames[SDLK_INSERT] = "insert";
	keynames[SDLK_HOME] = "home";
	keynames[SDLK_END] = "end";
	keynames[SDLK_PAGEUP] = "page up";
	keynames[SDLK_PAGEDOWN] = "page down";
	keynames[SDLK_F1] = "f1";
	keynames[SDLK_F2] = "f2";
	keynames[SDLK_F3] = "f3";
	keynames[SDLK_F4] = "f4";
	keynames[SDLK_F5] = "f5";
	keynames[SDLK_F6] = "f6";
	keynames[SDLK_F7] = "f7";
	keynames[SDLK_F8] = "f8";
	keynames[SDLK_F9] = "f9";
	keynames[SDLK_F10] = "f10";
	keynames[SDLK_F11] = "f11";
	keynames[SDLK_F12] = "f12";
	keynames[SDLK_F13] = "f13";
	keynames[SDLK_F14] = "f14";
	keynames[SDLK_F15] = "f15";
	keynames[SDLK_NUMLOCK] = "numlock";
	keynames[SDLK_CAPSLOCK] = "caps lock";
	keynames[SDLK_SCROLLOCK] = "scroll lock";
	keynames[SDLK_RSHIFT] = "right shift";
	keynames[SDLK_LSHIFT] = "left shift";
	keynames[SDLK_RCTRL] = "right ctrl";
	keynames[SDLK_LCTRL] = "left ctrl";
	keynames[SDLK_RALT] = "right alt";
	keynames[SDLK_LALT] = "left alt";
	keynames[SDLK_RMETA] = "right meta";
	keynames[SDLK_LMETA] = "left meta";
	keynames[SDLK_LSUPER] = "left super";
	keynames[SDLK_RSUPER] = "right super";
	keynames[SDLK_MODE] = "alt gr";
	keynames[SDLK_COMPOSE] = "compose";
	keynames[SDLK_HELP] = "help";
	keynames[SDLK_PRINT] = "print screen";
	keynames[SDLK_SYSREQ] = "sys req";
	keynames[SDLK_BREAK] = "break";
	keynames[SDLK_MENU] = "menu";
	keynames[SDLK_POWER] = "power";
	keynames[SDLK_EURO] = "euro";
	keynames[SDLK_UNDO] = "undo";
}

SDLMod SDL_GetModState(void)
{
	return modState;
}

const char * SDL_GetKeyName(SDLKey key)
{
	const char * name = NULL;

	if (key < SDLK_LAST)
		name = keynames[key];
	if (name == NULL)
		name = "unknown key";
	return name;
}

/* USB HID usage (keyboard page 0x07) -> key table */
static SDLKey keymap[232];

static void InitKeymap(void)
{
	int i;

	for (i = 0; i < 232; i++)
		keymap[i] = SDLK_UNKNOWN;

	for (i = 0; i < 26; i++)
		keymap[4 + i] = (SDLKey)(SDLK_a + i);
	for (i = 0; i < 9; i++)
		keymap[30 + i] = (SDLKey)(SDLK_1 + i);
	keymap[39] = SDLK_0;
	keymap[40] = SDLK_RETURN;
	keymap[41] = SDLK_ESCAPE;
	keymap[42] = SDLK_BACKSPACE;
	keymap[43] = SDLK_TAB;
	keymap[44] = SDLK_SPACE;
	keymap[45] = SDLK_MINUS;
	keymap[46] = SDLK_EQUALS;
	keymap[47] = SDLK_LEFTBRACKET;
	keymap[48] = SDLK_RIGHTBRACKET;
	keymap[49] = SDLK_BACKSLASH;
	keymap[51] = SDLK_SEMICOLON;
	keymap[52] = SDLK_QUOTE;
	keymap[53] = SDLK_BACKQUOTE;
	keymap[54] = SDLK_COMMA;
	keymap[55] = SDLK_PERIOD;
	keymap[56] = SDLK_SLASH;
	keymap[57] = SDLK_CAPSLOCK;
	for (i = 0; i < 12; i++)
		keymap[58 + i] = (SDLKey)(SDLK_F1 + i);
	keymap[70] = SDLK_PRINT;
	keymap[71] = SDLK_SCROLLOCK;
	keymap[72] = SDLK_PAUSE;
	keymap[73] = SDLK_INSERT;
	keymap[74] = SDLK_HOME;
	keymap[75] = SDLK_PAGEUP;
	keymap[76] = SDLK_DELETE;
	keymap[77] = SDLK_END;
	keymap[78] = SDLK_PAGEDOWN;
	keymap[79] = SDLK_RIGHT;
	keymap[80] = SDLK_LEFT;
	keymap[81] = SDLK_DOWN;
	keymap[82] = SDLK_UP;
	keymap[83] = SDLK_NUMLOCK;
	keymap[84] = SDLK_KP_DIVIDE;
	keymap[85] = SDLK_KP_MULTIPLY;
	keymap[86] = SDLK_KP_MINUS;
	keymap[87] = SDLK_KP_PLUS;
	keymap[88] = SDLK_KP_ENTER;
	for (i = 0; i < 9; i++)
		keymap[89 + i] = (SDLKey)(SDLK_KP1 + i);
	keymap[98] = SDLK_KP0;
	keymap[99] = SDLK_KP_PERIOD;
	keymap[100] = SDLK_LESS;   /* the extra key on ISO keyboards */
	keymap[102] = SDLK_POWER;
	keymap[103] = SDLK_KP_EQUALS;
	keymap[104] = SDLK_F13;
	keymap[105] = SDLK_F14;
	keymap[106] = SDLK_F15;
	keymap[117] = SDLK_HELP;
	keymap[118] = SDLK_MENU;
	keymap[122] = SDLK_UNDO;
	keymap[134] = SDLK_KP_EQUALS;
	keymap[154] = SDLK_SYSREQ;
	keymap[224] = SDLK_LCTRL;
	keymap[225] = SDLK_LSHIFT;
	keymap[226] = SDLK_LALT;
	keymap[227] = SDLK_LMETA;
	keymap[228] = SDLK_RCTRL;
	keymap[229] = SDLK_RSHIFT;
	keymap[230] = SDLK_RALT;
	keymap[231] = SDLK_RMETA;
}

static SDLMod ModsFromKeyEvent(uint16_t m)
{
	int mod = KMOD_NONE;

	if (m & KEYMOD_LSHIFT) mod |= KMOD_LSHIFT;
	if (m & KEYMOD_RSHIFT) mod |= KMOD_RSHIFT;
	if (m & KEYMOD_LCTRL)  mod |= KMOD_LCTRL;
	if (m & KEYMOD_RCTRL)  mod |= KMOD_RCTRL;
	if (m & KEYMOD_LALT)   mod |= KMOD_LALT;
	if (m & KEYMOD_RALT)   mod |= KMOD_RALT;
	if (m & KEYMOD_LGUI)   mod |= KMOD_LMETA;
	if (m & KEYMOD_RGUI)   mod |= KMOD_RMETA;
	if (m & KEYMOD_CAPS)   mod |= KMOD_CAPS;
	if (m & KEYMOD_NUM)    mod |= KMOD_NUM;
	return (SDLMod)mod;
}

static void PollKeyboard(void)
{
	KeyEvent ke;

	while (keyboard->poll(ke)) {
		SDLKey sym = (ke.hidUsage < 232) ? keymap[ke.hidUsage] : SDLK_UNKNOWN;
		modState = ModsFromKeyEvent(ke.modifiers);

		if (sym == SDLK_UNKNOWN)
			continue;

		SDL_Event event;
		memset(&event, 0, sizeof(event));
		event.type = ke.pressed ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.state = ke.pressed ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.scancode = (Uint8)ke.hidUsage;
		event.key.keysym.sym = sym;
		event.key.keysym.mod = modState;
		SDL_PushEvent(&event);
	}
}

/****************************************************************************
 * Mouse
 *
 * Two sources, as before: the pointer of Wiimote 0 (IR, converted to relative
 * motion) and a USB mouse. Wiimote 0 A and B act as the left and right button.
 ***************************************************************************/
static int mouseX = MOUSE_W / 2;
static int mouseY = MOUSE_H / 2;
static Uint8 mouseButtons = 0;
static int cursorVisible = 1;
static SDL_GrabMode grabMode = SDL_GRAB_OFF;

static bool irWasValid = false;
static float irLastX = 0, irLastY = 0;

Uint8 SDL_GetMouseState(int * x, int * y)
{
	if (x) *x = mouseX;
	if (y) *y = mouseY;
	return mouseButtons;
}

int SDL_ShowCursor(int toggle)
{
	int previous = cursorVisible;
	if (toggle != SDL_QUERY)
		cursorVisible = toggle ? 1 : 0;
	return previous;
}

SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode)
{
	if (mode != SDL_GRAB_QUERY)
		grabMode = mode;
	return grabMode;
}

static void PostMouseMotion(int dx, int dy)
{
	if (dx == 0 && dy == 0)
		return;

	mouseX += dx;
	mouseY += dy;
	if (mouseX < 0) mouseX = 0;
	if (mouseX > MOUSE_W - 1) mouseX = MOUSE_W - 1;
	if (mouseY < 0) mouseY = 0;
	if (mouseY > MOUSE_H - 1) mouseY = MOUSE_H - 1;

	SDL_Event event;
	memset(&event, 0, sizeof(event));
	event.type = SDL_MOUSEMOTION;
	event.motion.state = mouseButtons;
	event.motion.x = (Uint16)mouseX;
	event.motion.y = (Uint16)mouseY;
	event.motion.xrel = (Sint16)dx;
	event.motion.yrel = (Sint16)dy;
	SDL_PushEvent(&event);
}

static void PostMouseButton(int button, bool pressed)
{
	if (pressed)
		mouseButtons |= SDL_BUTTON(button);
	else
		mouseButtons &= ~SDL_BUTTON(button);

	SDL_Event event;
	memset(&event, 0, sizeof(event));
	event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.button = (Uint8)button;
	event.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
	event.button.x = (Uint16)mouseX;
	event.button.y = (Uint16)mouseY;
	SDL_PushEvent(&event);
}

// Sources are tracked separately so a button held on one isn't released by
// the other source's idle state.
static Uint8 irButtons = 0;   // SDL_BUTTON masks held via Wiimote 0
static Uint8 usbButtons = 0;  // SDL_BUTTON masks held via USB mouse

static void SetMouseButton(Uint8 * source, int button, bool pressed)
{
	Uint8 mask = SDL_BUTTON(button);
	bool was = (*source & mask) != 0;

	if (pressed == was)
		return;
	if (pressed) *source |= mask; else *source &= ~mask;

	// Report only a change of the combined state
	bool nowHeld = ((irButtons | usbButtons) & mask) != 0;
	bool reported = (mouseButtons & mask) != 0;
	if (nowHeld != reported)
		PostMouseButton(button, nowHeld);
}

static void PollMouse(void)
{
	// Wiimote 0 pointer
	const InputPadData & pad = controller[0]->getPadData();
	const uint32_t wm = pad.hw_buttons_h[INPUT_HW_WIIMOTE];

	if (pad.hw_connected[INPUT_HW_WIIMOTE] && pad.validPointer) {
		if (irWasValid)
			PostMouseMotion((int)(pad.cursor_x - irLastX), (int)(pad.cursor_y - irLastY));
		irLastX = pad.cursor_x;
		irLastY = pad.cursor_y;
		irWasValid = true;
	}
	else {
		irWasValid = false; // the next valid sample is a fresh start, not a jump
	}

	if (pad.hw_connected[INPUT_HW_WIIMOTE]) {
		SetMouseButton(&irButtons, SDL_BUTTON_LEFT, (wm & INPUT_BTN_A) != 0);
		SetMouseButton(&irButtons, SDL_BUTTON_RIGHT, (wm & INPUT_BTN_B) != 0);
	}

	// USB mouse
	MouseEvent me;
	while (usbMouse->poll(me)) {
		PostMouseMotion(me.dx, me.dy);
		SetMouseButton(&usbButtons, SDL_BUTTON_LEFT, (me.buttons & MOUSE_BTN_LEFT) != 0);
		SetMouseButton(&usbButtons, SDL_BUTTON_RIGHT, (me.buttons & MOUSE_BTN_RIGHT) != 0);
		SetMouseButton(&usbButtons, SDL_BUTTON_MIDDLE, (me.buttons & MOUSE_BTN_MIDDLE) != 0);
	}
}

/****************************************************************************
 * Joysticks
 ***************************************************************************/
struct SDL_Joystick {
	int index;
	bool opened;
	int naxes, nbuttons, nhats;
	Sint16 axes[WII_AXES];
	Uint8 buttons[WII_BUTTONS];
	Uint8 hat;
};

static SDL_Joystick joysticks[NUM_JOYSTICKS];
static bool joystickEvents = true;

static inline bool IsGC(int index) { return index >= NUM_WII_JOYSTICKS; }

static inline Sint16 AxisValue(float v)
{
	if (v > 1.0f) v = 1.0f;
	if (v < -1.0f) v = -1.0f;
	return (Sint16)(v * 32767.0f);
}

// The SDL port reported triggers as (0..255) << 7
static inline Sint16 TriggerValue(float v)
{
	if (v > 1.0f) v = 1.0f;
	if (v < 0.0f) v = 0.0f;
	return (Sint16)(((int)(v * 255.0f)) << 7);
}

static inline Uint8 DpadToHat(uint32_t b, bool sideways)
{
	Uint8 up = SDL_HAT_UP, down = SDL_HAT_DOWN, left = SDL_HAT_LEFT, right = SDL_HAT_RIGHT;
	if (sideways) { // Wiimote held on its side
		up = SDL_HAT_LEFT; down = SDL_HAT_RIGHT; left = SDL_HAT_DOWN; right = SDL_HAT_UP;
	}

	Uint8 hat = SDL_HAT_CENTERED;
	if (b & INPUT_BTN_UP)    hat |= up;
	if (b & INPUT_BTN_DOWN)  hat |= down;
	if (b & INPUT_BTN_LEFT)  hat |= left;
	if (b & INPUT_BTN_RIGHT) hat |= right;
	return hat;
}

static inline Uint8 Held(uint32_t buttons, uint32_t mask) { return (buttons & mask) ? 1 : 0; }

// Builds the joystick's new state from the HAL pad data
static void ReadWiimote(const InputPadData & pad, SDL_Joystick & js)
{
	memset(js.buttons, 0, sizeof(js.buttons));
	memset(js.axes, 0, sizeof(js.axes));
	js.hat = SDL_HAT_CENTERED;

	const bool wm = pad.hw_connected[INPUT_HW_WIIMOTE];
	const bool nun = pad.hw_connected[INPUT_HW_NUNCHUK];
	int cl = -1;
	if (pad.hw_connected[INPUT_HW_CLASSIC]) cl = INPUT_HW_CLASSIC;
	else if (pad.hw_connected[INPUT_HW_WUPC]) cl = INPUT_HW_WUPC;

	if (wm) {
		const uint32_t b = pad.hw_buttons_h[INPUT_HW_WIIMOTE];
		js.buttons[0] = Held(b, INPUT_BTN_A);
		js.buttons[1] = Held(b, INPUT_BTN_B);
		js.buttons[2] = Held(b, INPUT_BTN_1);
		js.buttons[3] = Held(b, INPUT_BTN_2);
		js.buttons[4] = Held(b, INPUT_BTN_MINUS);
		js.buttons[5] = Held(b, INPUT_BTN_PLUS);
		js.buttons[6] = Held(b, INPUT_BTN_HOME);
		js.hat = DpadToHat(b, true);

		js.axes[6] = AxisValue(-pad.hw_pitch[INPUT_HW_WIIMOTE] / 180.0f);
		js.axes[7] = AxisValue(pad.hw_roll[INPUT_HW_WIIMOTE] / 180.0f);
		js.axes[8] = AxisValue(pad.hw_yaw[INPUT_HW_WIIMOTE] / 180.0f);
	}

	if (nun) {
		const uint32_t b = pad.hw_buttons_h[INPUT_HW_NUNCHUK];
		js.buttons[7] = Held(b, INPUT_TRIGGER_ZL); // Z
		js.buttons[8] = Held(b, INPUT_TRIGGER_L);  // C
		js.axes[0] = AxisValue(pad.hw_stickX[INPUT_HW_NUNCHUK]);
		js.axes[1] = AxisValue(-pad.hw_stickY[INPUT_HW_NUNCHUK]);
	}

	if (cl >= 0) {
		const uint32_t b = pad.hw_buttons_h[cl];
		js.buttons[9]  = Held(b, INPUT_BTN_A);
		js.buttons[10] = Held(b, INPUT_BTN_B);
		js.buttons[11] = Held(b, INPUT_BTN_X);
		js.buttons[12] = Held(b, INPUT_BTN_Y);
		js.buttons[13] = Held(b, INPUT_TRIGGER_L);
		js.buttons[14] = Held(b, INPUT_TRIGGER_R);
		js.buttons[15] = Held(b, INPUT_TRIGGER_ZL);
		js.buttons[16] = Held(b, INPUT_TRIGGER_ZR);
		js.buttons[17] = Held(b, INPUT_BTN_MINUS);
		js.buttons[18] = Held(b, INPUT_BTN_PLUS);
		js.buttons[19] = Held(b, INPUT_BTN_HOME);
		js.hat = DpadToHat(b, false);

		js.axes[0] = AxisValue(pad.hw_stickX[cl]);
		js.axes[1] = AxisValue(-pad.hw_stickY[cl]);
		js.axes[2] = AxisValue(pad.hw_substickX[cl]);
		js.axes[3] = AxisValue(-pad.hw_substickY[cl]);
		js.axes[4] = TriggerValue(pad.hw_triggerR[cl]); // R then L, as the SDL port had it
		js.axes[5] = TriggerValue(pad.hw_triggerL[cl]);
	}
}

static void ReadGameCube(const InputPadData & pad, SDL_Joystick & js)
{
	memset(js.buttons, 0, sizeof(js.buttons));
	memset(js.axes, 0, sizeof(js.axes));
	js.hat = SDL_HAT_CENTERED;

	if (!pad.hw_connected[INPUT_HW_GAMECUBE])
		return;

	const uint32_t b = pad.hw_buttons_h[INPUT_HW_GAMECUBE];
	js.buttons[0] = Held(b, INPUT_BTN_A);
	js.buttons[1] = Held(b, INPUT_BTN_B);
	js.buttons[2] = Held(b, INPUT_BTN_X);
	js.buttons[3] = Held(b, INPUT_BTN_Y);
	js.buttons[4] = Held(b, INPUT_TRIGGER_ZR); // Z
	js.buttons[5] = Held(b, INPUT_TRIGGER_R);
	js.buttons[6] = Held(b, INPUT_TRIGGER_L);
	js.buttons[7] = Held(b, INPUT_BTN_PLUS);   // Start
	js.hat = DpadToHat(b, false);

	js.axes[0] = AxisValue(pad.hw_stickX[INPUT_HW_GAMECUBE]);
	js.axes[1] = AxisValue(-pad.hw_stickY[INPUT_HW_GAMECUBE]);
	js.axes[2] = AxisValue(pad.hw_substickX[INPUT_HW_GAMECUBE]);
	js.axes[3] = AxisValue(-pad.hw_substickY[INPUT_HW_GAMECUBE]);
	js.axes[4] = TriggerValue(pad.hw_triggerL[INPUT_HW_GAMECUBE]);
	js.axes[5] = TriggerValue(pad.hw_triggerR[INPUT_HW_GAMECUBE]);
}

static void PostJoyEvent(Uint8 type, int which, int number, int value)
{
	SDL_Event event;
	memset(&event, 0, sizeof(event));
	event.type = type;

	switch (type) {
		case SDL_JOYAXISMOTION:
			event.jaxis.which = (Uint8)which;
			event.jaxis.axis = (Uint8)number;
			event.jaxis.value = (Sint16)value;
			break;
		case SDL_JOYHATMOTION:
			event.jhat.which = (Uint8)which;
			event.jhat.hat = (Uint8)number;
			event.jhat.value = (Uint8)value;
			break;
		default: // button down/up
			event.jbutton.which = (Uint8)which;
			event.jbutton.button = (Uint8)number;
			event.jbutton.state = (type == SDL_JOYBUTTONDOWN) ? SDL_PRESSED : SDL_RELEASED;
			break;
	}
	SDL_PushEvent(&event);
}

static void UpdateJoysticks(void)
{
	for (int i = 0; i < NUM_JOYSTICKS; i++) {
		SDL_Joystick & js = joysticks[i];
		const int ch = IsGC(i) ? i - NUM_WII_JOYSTICKS : i;
		const InputPadData & pad = controller[ch]->getPadData();

		// Previous state, to find what changed
		Sint16 oldAxes[WII_AXES];
		Uint8 oldButtons[WII_BUTTONS];
		const Uint8 oldHat = js.hat;
		memcpy(oldAxes, js.axes, sizeof(oldAxes));
		memcpy(oldButtons, js.buttons, sizeof(oldButtons));

		if (IsGC(i))
			ReadGameCube(pad, js);
		else
			ReadWiimote(pad, js);

		if (!js.opened || !joystickEvents)
			continue;

		for (int a = 0; a < js.naxes; a++)
			if (js.axes[a] != oldAxes[a])
				PostJoyEvent(SDL_JOYAXISMOTION, i, a, js.axes[a]);
		for (int b = 0; b < js.nbuttons; b++)
			if (js.buttons[b] != oldButtons[b])
				PostJoyEvent(js.buttons[b] ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP, i, b, 0);
		if (js.hat != oldHat)
			PostJoyEvent(SDL_JOYHATMOTION, i, 0, js.hat);
	}
}

int SDL_NumJoysticks(void)
{
	return NUM_JOYSTICKS;
}

const char * SDL_JoystickName(int index)
{
	static char name[16];

	if (index < 0 || index >= NUM_JOYSTICKS)
		return NULL;
	snprintf(name, sizeof(name), IsGC(index) ? "Gamecube %d" : "Wiimote %d", index);
	return name;
}

SDL_Joystick * SDL_JoystickOpen(int index)
{
	if (index < 0 || index >= NUM_JOYSTICKS)
		return NULL;
	joysticks[index].opened = true;
	return &joysticks[index];
}

void SDL_JoystickClose(SDL_Joystick * joystick)
{
	if (joystick)
		joystick->opened = false;
}

int SDL_JoystickNumAxes(SDL_Joystick * joystick)    { return joystick ? joystick->naxes : 0; }
int SDL_JoystickNumButtons(SDL_Joystick * joystick) { return joystick ? joystick->nbuttons : 0; }
int SDL_JoystickNumHats(SDL_Joystick * joystick)    { return joystick ? joystick->nhats : 0; }

Sint16 SDL_JoystickGetAxis(SDL_Joystick * joystick, int axis)
{
	return (joystick && axis >= 0 && axis < joystick->naxes) ? joystick->axes[axis] : 0;
}

Uint8 SDL_JoystickGetButton(SDL_Joystick * joystick, int button)
{
	return (joystick && button >= 0 && button < joystick->nbuttons) ? joystick->buttons[button] : 0;
}

Uint8 SDL_JoystickGetHat(SDL_Joystick * joystick, int hat)
{
	return (joystick && hat == 0 && joystick->nhats > 0) ? joystick->hat : SDL_HAT_CENTERED;
}

int SDL_JoystickEventState(int state)
{
	if (state == SDL_ENABLE) joystickEvents = true;
	else if (state == SDL_DISABLE) joystickEvents = false;
	return joystickEvents ? SDL_ENABLE : SDL_DISABLE;
}

void SDL_JoystickUpdate(void)
{
	InputHal_Update();
}

/****************************************************************************
 * HAL entry points
 ***************************************************************************/
void InputHal_Init(void)
{
	InitKeyNames();
	InitKeymap();

	for (int i = 0; i < NUM_JOYSTICKS; i++) {
		SDL_Joystick & js = joysticks[i];
		memset(&js, 0, sizeof(js));
		js.index = i;
		js.naxes = IsGC(i) ? GC_AXES : WII_AXES;
		js.nbuttons = IsGC(i) ? GC_BUTTONS : WII_BUTTONS;
		js.nhats = 1;
	}

	eventHead = eventCount = 0;
	modState = KMOD_NONE;
	mouseButtons = irButtons = usbButtons = 0;
	irWasValid = false;
}

void InputHal_Update(void)
{
	static Ticks lastScan = 0;
	const Ticks now = SystemTime::now();

	if (lastScan != 0 && SystemTime::diffMillisecs(lastScan, now) < SCAN_INTERVAL_MS)
		return;
	lastScan = now;

	platform->getInput()->update();

	UpdateJoysticks();
	PollKeyboard();
	PollMouse();
}
