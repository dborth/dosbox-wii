/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * buttons.cpp
 *
 * See buttons.h. Everything a button can do is a mapper event, and the
 * mapper's "pad_N" group (sdl_mapper.cpp) binds a button to one, whatever the
 * joystick type is. The events are the mapper's own names: key_*, hand_* for
 * the hotkeys (and, from input.cpp, the mouse buttons), jbutton_* for the
 * emulated joystick's buttons.
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <vector>

#include "buttons.h"
#include "dosboxsupport.h"

// What the mapper offers (sdl_mapper.cpp)
int MAPPER_PadSourceCount(void);
bool MAPPER_EventExists(const char * name);
int MAPPER_PadGetEvents(int pad, int source, char names[][16], int max, bool * from_stick);
bool MAPPER_PadSetEvent(int pad, int source, const char * event);
bool MAPPER_PadSetDefault(int pad, int source);
bool MAPPER_SaveBindsNow(void);

#define PLAYER		0	// player 1; the mapper has all four
#define HELP_LENGTH	320

struct Target
{
	const char * label;
	const char * event;
};

// The first four buttons of the joystick type the shipped binds give player 1. Past the number the type has
// they do nothing, so no more are offered.
static const Target joystickTargets[] =
{
	{ "Joystick button 1", "jbutton_0_0" },
	{ "Joystick button 2", "jbutton_0_1" },
	{ "Joystick button 3", "jbutton_0_2" },
	{ "Joystick button 4", "jbutton_0_3" },
};

static const Target mouseTargets[] =
{
	{ "Mouse left button", "hand_mouseleft" },
	{ "Mouse right button", "hand_mouseright" },
	{ "Mouse middle button", "hand_mousemid" },
};

// Not here, on purpose: shutdown, restart (it does not restart), pause (it can only be left with the Pause
// key), the mapper screen, capture and recording hotkeys (not known to work on a console), and the mouse
// capture toggle (the mouse is always captured).
static const Target hotkeyTargets[] =
{
	{ "Cycles up", "hand_cycleup" },
	{ "Cycles down", "hand_cycledown" },
	{ "Frameskip up", "hand_incfskip" },
	{ "Frameskip down", "hand_decfskip" },
	{ "Swap disk image", "hand_swapimg" },
	{ "Fast forward (hold)", "hand_speedlock" },
};

static const Target keyTargets[] =
{
	{ "Enter", "key_enter" }, { "Escape", "key_esc" }, { "Space", "key_space" }, { "Tab", "key_tab" },
	{ "Backspace", "key_bspace" },
	{ "Up", "key_up" }, { "Down", "key_down" }, { "Left", "key_left" }, { "Right", "key_right" },
	{ "Left Ctrl", "key_lctrl" }, { "Left Alt", "key_lalt" }, { "Left Shift", "key_lshift" },
	{ "F1", "key_f1" }, { "F2", "key_f2" }, { "F3", "key_f3" }, { "F4", "key_f4" }, { "F5", "key_f5" },
	{ "F6", "key_f6" }, { "F7", "key_f7" }, { "F8", "key_f8" }, { "F9", "key_f9" }, { "F10", "key_f10" },
	{ "F11", "key_f11" }, { "F12", "key_f12" },
	{ "0", "key_0" }, { "1", "key_1" }, { "2", "key_2" }, { "3", "key_3" }, { "4", "key_4" },
	{ "5", "key_5" }, { "6", "key_6" }, { "7", "key_7" }, { "8", "key_8" }, { "9", "key_9" },
	{ "A", "key_a" }, { "B", "key_b" }, { "C", "key_c" }, { "D", "key_d" }, { "E", "key_e" },
	{ "F", "key_f" }, { "G", "key_g" }, { "H", "key_h" }, { "I", "key_i" }, { "J", "key_j" },
	{ "K", "key_k" }, { "L", "key_l" }, { "M", "key_m" }, { "N", "key_n" }, { "O", "key_o" },
	{ "P", "key_p" }, { "Q", "key_q" }, { "R", "key_r" }, { "S", "key_s" }, { "T", "key_t" },
	{ "U", "key_u" }, { "V", "key_v" }, { "W", "key_w" }, { "X", "key_x" }, { "Y", "key_y" },
	{ "Z", "key_z" },
	{ "Insert", "key_insert" }, { "Delete", "key_delete" }, { "Home", "key_home" }, { "End", "key_end" },
	{ "Page Up", "key_pageup" }, { "Page Down", "key_pagedown" },
	{ "Right Ctrl", "key_rctrl" }, { "Right Alt", "key_ralt" }, { "Right Shift", "key_rshift" },
	{ "Minus", "key_minus" }, { "Equals", "key_equals" }, { "Comma", "key_comma" },
	{ "Period", "key_period" }, { "Slash", "key_slash" }, { "Semicolon", "key_semicolon" },
	{ "Quote", "key_quote" }, { "Left bracket", "key_lbracket" }, { "Right bracket", "key_rbracket" },
	{ "Backslash", "key_backslash" }, { "Grave", "key_grave" },
};

#define ARRAY_COUNT(a)	((int)(sizeof(a) / sizeof((a)[0])))

// The first list: what the button can be made to do
enum
{
	ACTION_DEFAULT,
	ACTION_NOTHING,
	ACTION_JOYSTICK,	// four of them
	ACTION_MOUSE = ACTION_JOYSTICK + 4,	// three
	ACTION_KEY = ACTION_MOUSE + 3,
	ACTION_HOTKEY,
	ACTION_COUNT
};

static const char * const buttonNames[] =
{
	"A", "B", "X / 1", "Y / 2", "L", "R", "ZL", "ZR", "Minus", "Plus", "L3", "R3",
	"D-pad Up", "D-pad Right", "D-pad Down", "D-pad Left",
};

struct Row
{
	std::string label;
	std::string value;
	std::string help;
	int kind;
};

static std::vector<Row> rows;
static std::vector<Target> keys, hotkeys;	// the ones the mapper has

/****************************************************************************
 * Text for an event
 ***************************************************************************/
static std::string Number(int n)
{
	char text[16];

	snprintf(text, sizeof(text), "%d", n);
	return text;
}

static std::string LabelOf(const Target * list, int count, const char * event, const char * fallback)
{
	for(int i = 0; i < count; i++)
	{
		if(!strcmp(list[i].event, event))
			return list[i].label;
	}
	return fallback;
}

//! The buttons and hats of each joystick type (the CStickBindGroup subclasses in sdl_mapper.cpp); -1 if the
//! type is not one this knows.
static int EmulatedButtons(const char * type)
{
	if(!strcmp(type, "none")) return 0;
	if(!strcmp(type, "2axis")) return 2;
	if(!strcmp(type, "4axis") || !strcmp(type, "4axis_2") || !strcmp(type, "fcs")) return 4;
	if(!strcmp(type, "ch")) return 6;
	return -1;
}

static bool HasHat(const char * type)
{
	return !strcmp(type, "fcs") || !strcmp(type, "ch");
}

//! What an event does, in words. A joystick event says so if the joystick type does not use it.
static std::string EventText(const char * event)
{
	const char * type = GetJoystickTypeName();
	const bool joystickOff = !strcmp(type, "none");

	if(!strncmp(event, "jbutton_", 8) && event[8] >= '0' && event[8] <= '1' && event[9] == '_')
	{
		const int number = atoi(event + 10) + 1;
		const int has = EmulatedButtons(type);
		std::string text = (event[8] == '0') ? "Joystick button " + Number(number)
			: "Joystick 2 button " + Number(number);

		if(joystickOff)
			return text + " (joystick off)";

		// joystick 2's own buttons are used by 2axis only; the second port of a 4axis joystick is driven by
		// buttons 3 and 4 of joystick 1
		const bool used = (event[8] == '1') ? !strcmp(type, "2axis") && number <= 2 : number <= has;

		return (has >= 0 && !used) ? text + " (not used)" : text;
	}

	if(!strncmp(event, "jhat_", 5))
	{
		static const char * const directions[] = { "up", "right", "down", "left" };
		const int d = atoi(event + 9);	// jhat_0_0_<d>
		const std::string text = std::string("Joystick hat ") + ((d >= 0 && d < 4) ? directions[d] : "?");

		if(joystickOff)
			return text + " (joystick off)";

		return (EmulatedButtons(type) >= 0 && !HasHat(type)) ? text + " (not used)" : text;
	}

	if(!strncmp(event, "key_", 4))
		return "Key: " + LabelOf(keyTargets, ARRAY_COUNT(keyTargets), event, event + 4);

	if(!strncmp(event, "hand_mouse", 10))
		return LabelOf(mouseTargets, ARRAY_COUNT(mouseTargets), event, event);

	if(!strncmp(event, "hand_", 5))
		return "Hotkey: " + LabelOf(hotkeyTargets, ARRAY_COUNT(hotkeyTargets), event, event + 5);

	return event;
}

static std::string ValueText(int source)
{
	char names[4][16];
	bool fromStick = false;
	const int count = MAPPER_PadGetEvents(PLAYER, source, names, 4, &fromStick);

	if(count == 0)
		return "Nothing";

	std::string text = EventText(names[0]);

	if(count > 1)
		text += " +" + Number(count - 1);

	return text;
}

/****************************************************************************
 * Rows
 ***************************************************************************/
static void AddRow(const char * label, const std::string & value, const std::string & help, int kind)
{
	Row r;

	r.label = label;
	r.value = value.size() > 49 ? value.substr(0, 49) : value;
	r.help = help.size() >= HELP_LENGTH ? help.substr(0, HELP_LENGTH - 1) : help;
	r.kind = kind;
	rows.push_back(r);
}

static std::string HelpFor(int source)
{
	std::string help = "Press A to choose what this button does. A change takes effect at once and is saved.";

	if(source == 2)
		help += " X on a Classic, GameCube or Wii U controller, 1 on a Wiimote.";
	else if(source == 3)
		help += " Y on a Classic, GameCube or Wii U controller, 2 on a Wiimote.";
	else if(source == 10 || source == 11)
		help += " Pushing in a stick, on controllers that can.";
	else if(source >= 12)
		help += " The D-pad also moves the joystick whatever you choose, unless Joystick type is none.";

	return help;
}

void Buttons_Refresh()
{
	rows.clear();
	keys.clear();
	hotkeys.clear();

	for(int i = 0; i < ARRAY_COUNT(keyTargets); i++)
	{
		if(MAPPER_EventExists(keyTargets[i].event))
			keys.push_back(keyTargets[i]);
	}

	for(int i = 0; i < ARRAY_COUNT(hotkeyTargets); i++)
	{
		if(MAPPER_EventExists(hotkeyTargets[i].event))
			hotkeys.push_back(hotkeyTargets[i]);
	}

	AddRow("HOME", "Menu", "HOME opens this menu and cannot be changed. L, R and Plus (or Start) held together "
		"open it too, so keep that in mind when you give those buttons an action.", BUTTONS_ROW_INFO);

	const int sources = MAPPER_PadSourceCount();

	for(int i = 0; i < sources && i < ARRAY_COUNT(buttonNames); i++)
		AddRow(buttonNames[i], ValueText(i), HelpFor(i), BUTTONS_ROW_BUTTON);

	AddRow("Pointer clicks", "Wiimote A / B", "With a Wiimote on player 1, A is the left mouse button and B the "
		"right one, on top of whatever is set above. That cannot be changed yet.", BUTTONS_ROW_INFO);

	AddRow("Restore all buttons", "", "Gives every button what it did when DOSBox was installed (the joystick), "
		"and saves.", BUTTONS_ROW_RESTORE);
}

int Buttons_RowCount()
{
	return (int)rows.size();
}

const char * Buttons_RowLabel(int row)
{
	return (row >= 0 && row < (int)rows.size()) ? rows[row].label.c_str() : "";
}

void Buttons_RowValue(int row, char * buf, size_t size)
{
	if(size == 0)
		return;

	snprintf(buf, size, "%s", (row >= 0 && row < (int)rows.size()) ? rows[row].value.c_str() : "");
}

void Buttons_RowHelp(int row, char * buf, size_t size)
{
	if(size == 0)
		return;

	snprintf(buf, size, "%s", (row >= 0 && row < (int)rows.size()) ? rows[row].help.c_str() : "");
}

int Buttons_RowKind(int row)
{
	return (row >= 0 && row < (int)rows.size()) ? rows[row].kind : BUTTONS_ROW_INFO;
}

/****************************************************************************
 * Choices
 ***************************************************************************/
int Buttons_ListCount(int list)
{
	switch(list)
	{
		case BUTTONS_LIST_ACTIONS: return ACTION_COUNT;
		case BUTTONS_LIST_KEYS: return (int)keys.size();
		case BUTTONS_LIST_HOTKEYS: return (int)hotkeys.size();
	}
	return 0;
}

const char * Buttons_ListLabel(int list, int item)
{
	if(item < 0 || item >= Buttons_ListCount(list))
		return "";

	switch(list)
	{
		case BUTTONS_LIST_ACTIONS:
			if(item == ACTION_DEFAULT) return "Original (joystick)";
			if(item == ACTION_NOTHING) return "Nothing";
			if(item >= ACTION_JOYSTICK && item < ACTION_MOUSE) return joystickTargets[item - ACTION_JOYSTICK].label;
			if(item >= ACTION_MOUSE && item < ACTION_KEY) return mouseTargets[item - ACTION_MOUSE].label;
			if(item == ACTION_KEY) return "A key...";
			return "A hotkey...";

		case BUTTONS_LIST_KEYS: return keys[item].label;
		case BUTTONS_LIST_HOTKEYS: return hotkeys[item].label;
	}
	return "";
}

static int Finish(bool changed, char * message, size_t size)
{
	if(!changed)
	{
		snprintf(message, size, "Not changed.");
		return BUTTONS_PICK_FAILED;
	}

	if(MAPPER_SaveBindsNow())
		snprintf(message, size, "Saved.");
	else
		snprintf(message, size, "Changed, but the mapper file could not be written, so it is lost when DOSBox closes.");

	Buttons_Refresh();
	return BUTTONS_PICK_DONE;
}

int Buttons_Pick(int row, int list, int item, char * message, size_t size)
{
	const int source = row - 1;	// the first row is HOME

	if(size)
		message[0] = 0;

	if(Buttons_RowKind(row) != BUTTONS_ROW_BUTTON || source < 0 || item < 0 || item >= Buttons_ListCount(list))
	{
		if(size)
			snprintf(message, size, "Not changed.");
		return BUTTONS_PICK_FAILED;
	}

	if(list == BUTTONS_LIST_KEYS)
		return Finish(MAPPER_PadSetEvent(PLAYER, source, keys[item].event), message, size);

	if(list == BUTTONS_LIST_HOTKEYS)
		return Finish(MAPPER_PadSetEvent(PLAYER, source, hotkeys[item].event), message, size);

	if(item == ACTION_KEY)
		return BUTTONS_PICK_OPEN_KEYS;

	if(item == ACTION_HOTKEY)
		return BUTTONS_PICK_OPEN_HOTKEYS;

	if(item == ACTION_DEFAULT)
		return Finish(MAPPER_PadSetDefault(PLAYER, source), message, size);

	if(item == ACTION_NOTHING)
		return Finish(MAPPER_PadSetEvent(PLAYER, source, ""), message, size);

	if(item >= ACTION_JOYSTICK && item < ACTION_MOUSE)
		return Finish(MAPPER_PadSetEvent(PLAYER, source, joystickTargets[item - ACTION_JOYSTICK].event), message, size);

	return Finish(MAPPER_PadSetEvent(PLAYER, source, mouseTargets[item - ACTION_MOUSE].event), message, size);
}

bool Buttons_RestoreAll(char * message, size_t size)
{
	bool all = true;

	for(int i = 0; i < MAPPER_PadSourceCount(); i++)
		all = MAPPER_PadSetDefault(PLAYER, i) && all;

	return Finish(all, message, size) == BUTTONS_PICK_DONE;
}
