/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsmodel.cpp
 ***************************************************************************/
#include "controlsmodel.h"
#include "libgui/GuiControllerLayout.h"
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
	return v;
}

const std::vector<std::string>& Catalog_Names(TARGET kind)
{
	static const std::vector<std::string> none;
	static const std::vector<std::string> keys = makeKeys();
	static const std::vector<std::string> joystick = { "button 1", "button 2", "button 3", "button 4" };
	static const std::vector<std::string> mouse = { "left button", "right button", "middle button", "move up", "move down", "move left", "move right" };
	static const std::vector<std::string> hotkeys = { "Cycles up", "Cycles down", "Frameskip up", "Frameskip down", "Screenshot", "Pause", "Speed lock", "Swap disc image" };
	static const std::vector<std::string> modes = { "Joystick", "Arrow keys", "Mouse", "Off" };
	switch(kind)
	{
		case TARGET::KEY: return keys;
		case TARGET::JOYSTICK: return joystick;
		case TARGET::MOUSE: return mouse;
		case TARGET::HOTKEY: return hotkeys;
		case TARGET::STICK_MODE: return modes;
		default: return none;
	}
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
		case TARGET::JOYSTICK: return "Joystick " + names[(size_t)code];
		case TARGET::MOUSE: return "Mouse " + names[(size_t)code];
		case TARGET::HOTKEY: return "Hotkey: " + names[(size_t)code];
		case TARGET::STICK_MODE: return names[(size_t)code];
		default: return "Not assigned";
	}
}

// ---- sample data

namespace {

BindingTarget key(const char * name) { return BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, name)); }

// the "Arrow keys" scheme, which is what the sample model starts with
BindingTarget defaultFor(int binding)
{
	switch(binding)
	{
		case INPUT_BTN_A: return key("Ctrl");
		case INPUT_BTN_B: return key("Alt");
		case INPUT_BTN_X: return key("Space");
		case INPUT_BTN_Y: return key("Enter");
		case INPUT_BTN_PLUS: return key("Enter");
		case INPUT_BTN_MINUS: return key("Esc");
		case INPUT_TRIGGER_L: return key("PgUp");
		case INPUT_TRIGGER_R: return key("PgDn");
		case INPUT_BTN_UP: return key("Up");
		case INPUT_BTN_DOWN: return key("Down");
		case INPUT_BTN_LEFT: return key("Left");
		case INPUT_BTN_RIGHT: return key("Right");
		case INPUT_THUMB_L: return BindingTarget(TARGET::STICK_MODE, 1);
		case INPUT_THUMB_R: return BindingTarget(TARGET::STICK_MODE, 2);
		case INPUT_THUMB_R | BIND_STICK_CLICK: return BindingTarget(TARGET::MOUSE, 0);
		case GUI_CTL_CAPTURE: return BindingTarget(TARGET::HOTKEY, 4);
		default: return BindingTarget();
	}
}

class SampleControlsModel : public ControlsModel
{
	public:
		SampleControlsModel() : scheme(1) {}

		int playerCount() const override { return 4; }

		PlayerInfo player(int i) const override
		{
			PlayerInfo p;
			p.connected = false;
			p.type = PadType::NONE;
			p.battery = -1;
			p.emulated = Emulated::NONE;
			p.joystick = 0;
			const std::string sch = schemes()[(size_t)scheme];
			switch(i)
			{
				case 0: p.connected = true; p.type = PadType::WIIMOTE_NUNCHUK; p.battery = 80; p.role = "Joystick 1"; p.emulated = Emulated::JOYSTICK; p.joystick = 1; break;
				case 1: p.connected = true; p.type = PadType::CLASSIC; p.battery = 45; p.role = "Keys + mouse"; p.emulated = Emulated::KEYS_MOUSE; break;
				case 2: p.connected = true; p.type = PadType::GAMECUBE; p.role = "Joystick 2"; p.emulated = Emulated::JOYSTICK; p.joystick = 2; break;
				default: break;
			}
			if(p.connected)
			{
				p.device = PadType_Name(p.type);
				p.scheme = sch;
			}
			return p;
		}

		DeviceStatus devices() const override
		{
			DeviceStatus d;
			d.keyboard = true;
			d.mouse = false;
			d.wiimoteIr = false;
			d.gamepadPresent = false;
			d.gamepadTouch = false;
			return d;
		}

		std::vector<std::string> schemes() const override
		{
			static const char * names[] = { "Compatible", "Arrow keys", "Arrows + shooter", "Point and click", "Stick mouse", "Menu keys" };
			return std::vector<std::string>(names, names + sizeof(names) / sizeof(names[0]));
		}
		int currentScheme() const override { return scheme; }
		void setScheme(int index) override
		{
			const int n = (int)schemes().size();
			scheme = ((index % n) + n) % n;
			edits.clear(); // a scheme brings its own defaults
		}

		BindingTarget binding(PadType type, int b) const override
		{
			const std::map<int64_t, BindingTarget>::const_iterator it = edits.find(key(type, b));
			return it != edits.end() ? it->second : defaultFor(b);
		}
		void setBinding(PadType type, int b, const BindingTarget& t) override { edits[key(type, b)] = t; }
		void resetBindings(PadType type) override
		{
			for(std::map<int64_t, BindingTarget>::iterator it = edits.begin(); it != edits.end();)
			{
				if(it->first / kStride == (int64_t)type)
					edits.erase(it++);
				else
					++it;
			}
		}

	private:
		static const int64_t kStride = (int64_t)1 << 32;
		static int64_t key(PadType type, int b) { return (int64_t)type * kStride + (int64_t)(uint32_t)b; }

		int scheme;
		std::map<int64_t, BindingTarget> edits;
};

SampleControlsModel * sampleModel()
{
	static SampleControlsModel m;
	return &m;
}

ControlsModel * installed = nullptr;

} // namespace

ControlsModel& Controls()
{
	return installed ? *installed : *sampleModel();
}

void Controls_SetModel(ControlsModel * model)
{
	installed = model;
}
