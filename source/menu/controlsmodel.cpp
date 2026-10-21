/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsmodel.cpp
 ***************************************************************************/
#include "controlsmodel.h"
#include "stickmouse.h"
#include "stickkeys.h"
#include <ctype.h>
#include "controllerlayout.h"
#include "drivers/InputData.h"
#include <deque>
#include <map>
#include <stdint.h>
#include <stdio.h>

const char * PadType_Name(PadType t)
{
	switch(t)
	{
		case PadType::WIIMOTE: return "Wiimote";
		case PadType::WIIMOTE_NUNCHUK: return "Wiimote + Nunchuk";
		case PadType::CLASSIC: return "Classic Controller";
		case PadType::WIIU_PRO: return "Wii U Pro Controller";
		case PadType::GAMECUBE: return "GameCube Controller";
		case PadType::GAMEPAD: return "Wii U GamePad";
		default: return "";
	}
}

// ---- what the HAL reports, as the screens name it

PadType PadType_FromPad(const InputPadData& pad)
{
	// Same order as the home strip: the richest device on a channel names it. The GamePad is last so a
	// controller held alongside it is the one drawn.
	if(pad.hw_connected[INPUT_HW_WUPC])
		return PadType::WIIU_PRO;
	if(pad.hw_connected[INPUT_HW_CLASSIC])
		return PadType::CLASSIC;
	if(pad.hw_connected[INPUT_HW_NUNCHUK])
		return PadType::WIIMOTE_NUNCHUK;
	if(pad.hw_connected[INPUT_HW_WIIMOTE])
		return PadType::WIIMOTE;
	if(pad.hw_connected[INPUT_HW_GAMECUBE])
		return PadType::GAMECUBE;
	if(pad.hw_connected[INPUT_HW_DRC])
		return PadType::GAMEPAD;
	return PadType::NONE;
}

std::vector<PadType> PadDevices_FromPad(const InputPadData& pad)
{
	// the same order as PadType_FromPad, so the first entry is the one it names; the Wiimote family counts once
	std::vector<PadType> list;
	if(pad.hw_connected[INPUT_HW_WUPC])
		list.push_back(PadType::WIIU_PRO);
	if(pad.hw_connected[INPUT_HW_CLASSIC])
		list.push_back(PadType::CLASSIC);
	else if(pad.hw_connected[INPUT_HW_NUNCHUK])
		list.push_back(PadType::WIIMOTE_NUNCHUK);
	else if(pad.hw_connected[INPUT_HW_WIIMOTE])
		list.push_back(PadType::WIIMOTE);
	if(pad.hw_connected[INPUT_HW_GAMECUBE])
		list.push_back(PadType::GAMECUBE);
	if(pad.hw_connected[INPUT_HW_DRC])
		list.push_back(PadType::GAMEPAD);
	return list;
}

int PadBattery(const InputPadData& pad, PadType type)
{
	switch(type)
	{
		case PadType::NONE:
		case PadType::GAMECUBE:
			return -1;
		case PadType::GAMEPAD:
			return (int)pad.hw_battery_level[INPUT_HW_DRC];
		default:
			// every Wiimote-based controller reports the battery of the Wiimote it rides on
			return (int)pad.hw_battery_level[INPUT_HW_WIIMOTE];
	}
}

// ---- the choices

static std::vector<std::string> makeKeys()
{
	const char * named[] = { "Enter", "Esc", "Space", "Tab", "Backspace", "Ctrl", "Alt", "Shift", "Up", "Down", "Left", "Right",
		"PgUp", "PgDn", "Home", "End", "Ins", "Del" };
	std::vector<std::string> v(named, named + sizeof(named) / sizeof(named[0]));
	for(int i = 1; i <= 12; i++)
	{
		char name[8];
		snprintf(name, sizeof(name), "F%d", i); // (newlib's libstdc++ has no std::to_string)
		v.push_back(name);
	}
	for(char c = '0'; c <= '9'; c++)
		v.push_back(std::string(1, c));
	for(char c = 'A'; c <= 'Z'; c++)
		v.push_back(std::string(1, c));
	// the punctuation keys the mapper binds, by name (the key buttons draw the symbol)
	const char * punctuation[] = { "Minus", "Equals", "Comma", "Period", "Slash", "Semicolon", "Quote", "[", "]", "Backslash", "Grave" };
	for(size_t i = 0; i < sizeof(punctuation) / sizeof(punctuation[0]); i++)
		v.push_back(punctuation[i]);
	return v;
}

const std::vector<std::string>& Catalog_Names(TARGET kind)
{
	static const std::vector<std::string> none;
	static const std::vector<std::string> keys = makeKeys();
	static const std::vector<std::string> joystick = { "button 1", "button 2", "button 3", "button 4" };
	static const std::vector<std::string> mouse = { "left button", "right button", "middle button" };
	static const std::vector<std::string> hotkeys = { "Cycles up", "Cycles down", "Frameskip up", "Frameskip down", "Swap disk image", "Fast forward (hold)" };
	switch(kind)
	{
		case TARGET::KEY: return keys;
		case TARGET::JOYSTICK: return joystick;
		case TARGET::MOUSE: return mouse;
		case TARGET::HOTKEY: return hotkeys;
		default: return none;
	}
}

int Catalog_KeyFromHid(unsigned hid)
{
	const char * name = nullptr;
	char single[2] = { 0, 0 };
	char fkey[8];
	if(hid >= 4 && hid <= 29) { single[0] = (char)('A' + (hid - 4)); name = single; }
	else if(hid >= 30 && hid <= 38) { single[0] = (char)('1' + (hid - 30)); name = single; }
	else if(hid == 39) name = "0";
	else if(hid >= 58 && hid <= 69) { snprintf(fkey, sizeof(fkey), "F%u", hid - 57); name = fkey; }
	else switch(hid)
	{
		case 40: name = "Enter"; break;
		case 41: name = "Esc"; break;
		case 42: name = "Backspace"; break;
		case 43: name = "Tab"; break;
		case 44: name = "Space"; break;
		case 45: name = "Minus"; break;
		case 46: name = "Equals"; break;
		case 47: name = "["; break;
		case 48: name = "]"; break;
		case 49: name = "Backslash"; break;
		case 51: name = "Semicolon"; break;
		case 52: name = "Quote"; break;
		case 53: name = "Grave"; break;
		case 54: name = "Comma"; break;
		case 55: name = "Period"; break;
		case 56: name = "Slash"; break;
		case 73: name = "Ins"; break;
		case 74: name = "Home"; break;
		case 75: name = "PgUp"; break;
		case 76: name = "Del"; break;
		case 77: name = "End"; break;
		case 78: name = "PgDn"; break;
		case 79: name = "Right"; break;
		case 80: name = "Left"; break;
		case 81: name = "Down"; break;
		case 82: name = "Up"; break;
		case 224: case 228: name = "Ctrl"; break;
		case 225: case 229: name = "Shift"; break;
		case 226: case 230: name = "Alt"; break;
		default: break;
	}
	return name ? Catalog_Find(TARGET::KEY, name) : -1;
}

int Catalog_Find(TARGET kind, const char * name)
{
	const std::vector<std::string>& v = Catalog_Names(kind);
	for(size_t i = 0; i < v.size(); i++)
		if(v[i] == name)
			return (int)i;
	return -1;
}

std::string BindingTarget::describe() const
{
	const std::vector<std::string>& names = Catalog_Names(kind);
	if(kind == TARGET::NONE || code < 0 || (size_t)code >= names.size())
		return "Not assigned";
	switch(kind)
	{
		case TARGET::KEY: return "Key: " + names[(size_t)code];
		// short, because the rows that show these are narrow ("button 1" -> "Joystick 1", "left button" -> "Mouse left")
		case TARGET::JOYSTICK: return "Joystick " + names[(size_t)code].substr(names[(size_t)code].rfind(' ') + 1);
		case TARGET::MOUSE: return "Mouse " + names[(size_t)code].substr(0, names[(size_t)code].find(" button"));
		case TARGET::HOTKEY: return "DOS: " + names[(size_t)code];
		default: return "Not assigned";
	}
}

// ---- derived answers every model gives the same way

PlayerInfo Controls_ComposePlayer(const PlayerHardware& h, int index, const EmulatedMode& mode, const std::string& preset)
{
	PlayerInfo p;
	p.connected = h.connected;
	p.type = h.type;
	if(p.connected)
	{
		p.devices = h.devices;
		if(p.devices.empty())
			p.devices.push_back(h.type);
	}
	p.battery = h.battery;
	p.sideways = h.sideways;
	p.stickClicks = h.stickClicks;
	p.emulated = Emulated::NONE;
	p.joystick = 0;
	if(p.connected)
	{
		p.device = PadType_Name(p.type);
		p.preset = preset;
		// the joysticks go to players firstPlayer.. (4axis_2 starts at player 2); the others use keys and mouse
		const int slot = index - mode.firstPlayer;
		if(slot >= 0 && slot < mode.joysticks)
		{
			p.emulated = Emulated::JOYSTICK;
			p.joystick = slot + 1;
			p.role = "Joystick " + std::string(1, (char)('0' + p.joystick));
		}
		else
		{
			p.emulated = Emulated::KEYS_MOUSE;
			p.role = "Keys + mouse";
		}
	}
	return p;
}

int ControlsModel::joystickSlot(int player) const
{
	const std::vector<EmulatedMode> modes = emulatedModes();
	const int cur = currentEmulated();
	if(cur < 0 || (size_t)cur >= modes.size())
		return -1;
	const int slot = player - modes[(size_t)cur].firstPlayer;
	return player >= 0 && slot >= 0 && slot < modes[(size_t)cur].joysticks ? slot : -1;
}

int ControlsModel::joystickButtons(int player) const
{
	return joystickSlot(player) < 0 ? 0 : emulatedModes()[(size_t)currentEmulated()].buttons;
}

int ControlsModel::joystickAxes(int player) const
{
	return joystickSlot(player) < 0 ? 0 : emulatedModes()[(size_t)currentEmulated()].axes;
}

bool ControlsModel::stickFree(int player, int stick) const
{
	return joystickAxes(player) < (stick == 0 ? 2 : 4);
}

bool ControlsModel::stickMouse(int player, int stick) const
{
	return StickMouse_Get(player, stick);
}

void ControlsModel::setStickMouse(int player, int stick, bool on)
{
	StickMouse_Set(player, stick, on);
}

ControlsModel::StickUse ControlsModel::stickUse(int player, int stick) const
{
	if(StickKeys_Get(player, stick))
		return StickUse::KEYS;
	return stickFree(player, stick) && StickMouse_Get(player, stick) ? StickUse::MOUSE : StickUse::DEFAULT;
}

ControlsModel::StickUse ControlsModel::nextStickUse(int player, int stick) const
{
	switch(stickUse(player, stick))
	{
		case StickUse::DEFAULT:
			return stickFree(player, stick) ? StickUse::MOUSE : StickUse::KEYS;
		case StickUse::MOUSE:
			return StickUse::KEYS;
		default:
			return StickUse::DEFAULT;
	}
}

void ControlsModel::setStickUse(int player, int stick, StickUse use)
{
	StickMouse_Set(player, stick, use == StickUse::MOUSE);
	StickKeys_Set(player, stick, use == StickUse::KEYS);
}

bool ControlsModel::usable(int player, const BindingTarget& target) const
{
	if(target.kind == TARGET::JOYSTICK)
		return target.code >= 0 && target.code < joystickButtons(player);
	return true;
}

std::string ControlsModel::describe(int player, int control) const
{
	return describeTarget(player, binding(player, control));
}

std::string ControlsModel::describeTarget(int player, const BindingTarget& t) const
{
	std::string text = t.describe();
	// say what is wrong instead of the target: the row is too narrow to show both
	if(t.kind == TARGET::JOYSTICK && text != "Not assigned" && !usable(player, t))
		text = joystickButtons(player) ? "No button " + std::string(1, (char)('1' + t.code)) : "Joystick off";
	return text;
}

std::vector<TARGET> ControlsModel::kindsOffered(int player) const
{
	std::vector<TARGET> kinds;
	kinds.push_back(TARGET::KEY);
	if(joystickButtons(player) > 0)
		kinds.push_back(TARGET::JOYSTICK);
	kinds.push_back(TARGET::MOUSE);
	kinds.push_back(TARGET::HOTKEY);
	return kinds;
}

std::vector<int> ControlsModel::choices(int player, TARGET kind) const
{
	std::vector<int> codes;
	int count = (int)Catalog_Names(kind).size();
	if(kind == TARGET::JOYSTICK)
		count = joystickButtons(player) < count ? joystickButtons(player) : count;
	for(int i = 0; i < count; i++)
		codes.push_back(i);
	return codes;
}

// ---- presets, what DOS sees, and the mapper's pad sources

namespace {

BindingTarget key(const char * name) { return BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, name)); }
BindingTarget joy(int button) { return BindingTarget(TARGET::JOYSTICK, button); }
BindingTarget mouseBtn(const char * name) { return BindingTarget(TARGET::MOUSE, Catalog_Find(TARGET::MOUSE, name)); }

// control, in the order of the mapper's pad sources
const int kSourceControl[PAD_SOURCE_COUNT] =
{
	INPUT_BTN_A, INPUT_BTN_B, INPUT_BTN_X, INPUT_BTN_Y,
	INPUT_TRIGGER_L, INPUT_TRIGGER_R, INPUT_TRIGGER_ZL, INPUT_TRIGGER_ZR,
	INPUT_BTN_MINUS, INPUT_BTN_PLUS,
	INPUT_THUMB_L | BIND_STICK_CLICK, INPUT_THUMB_R | BIND_STICK_CLICK,
	INPUT_BTN_UP, INPUT_BTN_RIGHT, INPUT_BTN_DOWN, INPUT_BTN_LEFT
};

} // namespace

int Pad_SourceOfControl(int control)
{
	for(int i = 0; i < PAD_SOURCE_COUNT; i++)
		if(kSourceControl[i] == control)
			return i;
	return -1;
}

int Pad_ControlOfSource(int source)
{
	return source >= 0 && source < PAD_SOURCE_COUNT ? kSourceControl[source] : -1;
}

const std::vector<PresetInfo>& Presets_All()
{
	static const std::vector<PresetInfo> presets =
	{
		{ "Compatible", "Buttons fire the DOS joystick, as DOSBox does on its own", true },
		{ "Arrow keys", "D-pad and buttons as arrows, Ctrl, Alt and Space (platformers)", false },
		{ "Arrows + shooter", "Arrows, with Ctrl to fire, Space to use, Alt to strafe (Doom-style)", false },
		{ "Point and click", "A and B click, the D-pad moves between hot spots (adventures)", false },
		{ "Menu keys", "D-pad, Enter and Esc, for installers and the DOS prompt", false },
	};
	return presets;
}

BindingTarget Presets_Target(int preset, int control)
{
	if(preset < 0 || preset >= PRESET_COUNT || Pad_SourceOfControl(control) < 0)
		return BindingTarget();

	const bool dpadArrows = preset != PRESET_COMPATIBLE;
	switch(control)
	{
		case INPUT_BTN_UP: return dpadArrows ? key("Up") : BindingTarget();
		case INPUT_BTN_DOWN: return dpadArrows ? key("Down") : BindingTarget();
		case INPUT_BTN_LEFT: return dpadArrows ? key("Left") : BindingTarget();
		case INPUT_BTN_RIGHT: return dpadArrows ? key("Right") : BindingTarget();
		default: break;
	}

	switch(preset)
	{
		case PRESET_COMPATIBLE:
			switch(control)
			{
				case INPUT_BTN_A: return joy(0);
				case INPUT_BTN_B: return joy(1);
				case INPUT_BTN_X: return joy(2);
				case INPUT_BTN_Y: return joy(3);
				default: return BindingTarget();
			}
		case PRESET_ARROWS:
			switch(control)
			{
				case INPUT_BTN_A: return key("Ctrl");
				case INPUT_BTN_B: return key("Alt");
				case INPUT_BTN_X: return key("Space");
				case INPUT_BTN_Y: return key("Enter");
				case INPUT_BTN_PLUS: return key("Enter");
				case INPUT_BTN_MINUS: return key("Esc");
				case INPUT_TRIGGER_L: return key("PgUp");
				case INPUT_TRIGGER_R: return key("PgDn");
				default: return BindingTarget();
			}
		case PRESET_SHOOTER:
			switch(control)
			{
				case INPUT_BTN_A: return key("Ctrl");
				case INPUT_BTN_B: return key("Space");
				case INPUT_BTN_X: return key("Alt");
				case INPUT_BTN_Y: return key("Shift");
				case INPUT_BTN_PLUS: return key("Enter");
				case INPUT_BTN_MINUS: return key("Esc");
				case INPUT_TRIGGER_L: return key("Z");
				case INPUT_TRIGGER_R: return key("X");
				default: return BindingTarget();
			}
		case PRESET_POINT:
			switch(control)
			{
				case INPUT_BTN_A: return mouseBtn("left button");
				case INPUT_BTN_B: return mouseBtn("right button");
				case INPUT_BTN_PLUS: return key("Enter");
				case INPUT_BTN_MINUS: return key("Esc");
				default: return BindingTarget();
			}
		case PRESET_MENU:
			switch(control)
			{
				case INPUT_BTN_A: return key("Enter");
				case INPUT_BTN_B: return key("Esc");
				case INPUT_BTN_X: return key("Y");
				case INPUT_BTN_Y: return key("N");
				case INPUT_BTN_PLUS: return key("Enter");
				case INPUT_BTN_MINUS: return key("Esc");
				default: return BindingTarget();
			}
		default: return BindingTarget();
	}
}

std::vector<BindingTarget> Presets_Suggestions(const ControlsModel& model, int player, int control)
{
	std::vector<BindingTarget> out;
	if(Pad_SourceOfControl(control) < 0)
		return out;
	for(int p = 0; p < PRESET_COUNT; p++)
	{
		const BindingTarget t = Presets_Target(p, control);
		if(t.kind == TARGET::NONE || !model.usable(player, t))
			continue;
		bool seen = false;
		for(size_t i = 0; i < out.size(); i++)
			seen = seen || out[i] == t;
		if(!seen)
			out.push_back(t);
	}
	return out;
}

const std::vector<EmulatedMode>& Emulated_Modes()
{
	// The first three are the Emulating list. The rest are the mapper's other joystick types, reached from
	// Settings: they are listed only when one is in effect, so the player can see it and leave it. Counts are the
	// ones the mapper's groups use (C4AxisBindGroup 4 axes / 4 buttons, CFCSBindGroup 4 / 4 + hat,
	// CCHBindGroup 4 / 6 + hat; CStickBindGroup 2 / 2 for two joysticks).
	static const std::vector<EmulatedMode> modes =
	{
		{ "Joystick", "One joystick with four buttons, for player 1", 1, 4, 4, "4axis", 0, false, false },
		{ "Two joysticks", "Players 1 and 2 each get a joystick with two buttons", 2, 2, 2, "2axis", 0, false, false },
		{ "Keyboard + mouse", "No joystick: the buttons act as keys and the mouse", 0, 0, 0, "none", 0, false, false },
		{ "Joystick for player 2", "The one joystick, with four buttons, is player 2's", 1, 4, 4, "4axis_2", 1, false, true },
		{ "Flight stick (FCS)", "One joystick with four buttons and a hat, for player 1", 1, 4, 4, "fcs", 0, true, true },
		{ "Flight stick (CH)", "One joystick with six buttons and a hat, for player 1", 1, 6, 4, "ch", 0, true, true },
	};
	return modes;
}

bool Emulated_StickUsed(const EmulatedMode& mode, int player, int stick)
{
	const int slot = player - mode.firstPlayer;
	if(player < 0 || slot < 0 || slot >= mode.joysticks)
		return false;
	return mode.axes >= (stick == 0 ? 2 : 4);
}

int Emulated_IndexOfType(const std::string& type)
{
	const std::vector<EmulatedMode>& modes = Emulated_Modes();
	for(size_t i = 0; i < modes.size(); i++)
		if(modes[i].type == type)
			return (int)i;
	// auto is 4axis here (InitializeJoysticks), and the type only reads auto between the config being set and the mapper building its groups
	return type == "auto" ? 0 : -1;
}

// ---- the installed model

namespace {

ControlsModel * installed = nullptr;
ControlsModel * fallback = nullptr;

}

ControlsModel& Controls()
{
	return installed ? *installed : *fallback;
}

void Controls_SetModel(ControlsModel * model)
{
	installed = model;
}

void Controls_SetFallback(ControlsModel * model)
{
	fallback = model;
}
