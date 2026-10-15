/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsmodel.cpp
 ***************************************************************************/
#include "controlsmodel.h"
#include "libgui/GuiControllerLayout.h"
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

int ControlsModel::joystickButtons(int player) const
{
	const std::vector<EmulatedMode> modes = emulatedModes();
	const int cur = currentEmulated();
	if(cur < 0 || (size_t)cur >= modes.size() || player < 0 || player >= modes[(size_t)cur].joysticks)
		return 0;
	return modes[(size_t)cur].buttons;
}

int ControlsModel::joystickAxes(int player) const
{
	const std::vector<EmulatedMode> modes = emulatedModes();
	const int cur = currentEmulated();
	if(cur < 0 || (size_t)cur >= modes.size() || player < 0 || player >= modes[(size_t)cur].joysticks)
		return 0;
	return modes[(size_t)cur].axes;
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

// ---- sample data

namespace {

BindingTarget key(const char * name) { return BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, name)); }
BindingTarget joy(int button) { return BindingTarget(TARGET::JOYSTICK, button); }
BindingTarget mouseBtn(const char * name) { return BindingTarget(TARGET::MOUSE, Catalog_Find(TARGET::MOUSE, name)); }

enum { PRESET_COMPATIBLE, PRESET_ARROWS, PRESET_SHOOTER, PRESET_POINT, PRESET_MENU, PRESET_COUNT };

const PresetInfo kPresets[PRESET_COUNT] =
{
	{ "Compatible", "Buttons fire the DOS joystick, as DOSBox does on its own", true },
	{ "Arrow keys", "D-pad and buttons as arrows, Ctrl, Alt and Space (platformers)", false },
	{ "Arrows + shooter", "Arrows, with Ctrl to fire, Space to use, Alt to strafe (Doom-style)", false },
	{ "Point and click", "A and B click, the D-pad moves between hot spots (adventures)", false },
	{ "Menu keys", "D-pad, Enter and Esc, for installers and the DOS prompt", false },
};

// what a preset gives a control (a control with no entry does nothing)
BindingTarget presetDefault(int preset, int control)
{
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

const EmulatedMode kModes[3] =
{
	{ "Joystick", "One joystick with four buttons, for player 1", 1, 4, 4 },
	{ "Two joysticks", "Players 1 and 2 each get a joystick with two buttons", 2, 2, 2 },
	{ "Keyboard + mouse", "No joystick: the buttons act as keys and the mouse", 0, 0, 0 },
};

// the controls the mapper has a source for: every button, the D-pad and the two stick clicks.
// Not stick movement (it is the joystick's axes) and not Capture or Home.
bool mapperHasSource(int control)
{
	switch(control)
	{
		case INPUT_BTN_A: case INPUT_BTN_B: case INPUT_BTN_X: case INPUT_BTN_Y:
		case INPUT_TRIGGER_L: case INPUT_TRIGGER_R: case INPUT_TRIGGER_ZL: case INPUT_TRIGGER_ZR:
		case INPUT_BTN_MINUS: case INPUT_BTN_PLUS:
		case INPUT_BTN_UP: case INPUT_BTN_DOWN: case INPUT_BTN_LEFT: case INPUT_BTN_RIGHT:
		case INPUT_THUMB_L | BIND_STICK_CLICK:
		case INPUT_THUMB_R | BIND_STICK_CLICK:
			return true;
		default:
			return false;
	}
}

class SampleControlsModel : public ControlsModel
{
	public:
		SampleControlsModel() : mode(0), preset(PRESET_ARROWS), heldBits(0) {}

		int playerCount() const override { return 4; }

		PlayerInfo player(int i) const override
		{
			PlayerInfo p;
			p.connected = false;
			p.type = PadType::NONE;
			p.battery = -1;
			p.emulated = Emulated::NONE;
			p.joystick = 0;
			switch(i)
			{
				case 0: p.connected = true; p.type = PadType::WIIMOTE_NUNCHUK; p.battery = 80; break;
				case 1: p.connected = true; p.type = PadType::CLASSIC; p.battery = 45; break;
				case 2: p.connected = true; p.type = PadType::GAMECUBE; break;
				default: break;
			}
			if(p.connected)
			{
				p.device = PadType_Name(p.type);
				p.preset = kPresets[preset].name;
				// joystick n is player n (the mapper's stick n); players past the joysticks that exist use keys and mouse
				if(i < kModes[mode].joysticks)
				{
					p.emulated = Emulated::JOYSTICK;
					p.joystick = i + 1;
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

		uint32_t held(int player) const override { return player == 0 ? heldBits : 0; }
		void setHeld(uint32_t bits) { heldBits = bits; } // for tests and previews

		std::vector<EmulatedMode> emulatedModes() const override { return std::vector<EmulatedMode>(kModes, kModes + 3); }
		int currentEmulated() const override { return mode; }
		void setEmulated(int index) override
		{
			if(index >= 0 && index < 3)
				mode = index;
		}

		std::vector<PresetInfo> presets() const override { return std::vector<PresetInfo>(kPresets, kPresets + PRESET_COUNT); }
		int currentPreset() const override { return preset; }
		void applyPreset(int index) override
		{
			if(index < 0 || index >= PRESET_COUNT)
				return;
			preset = index;
			edits.clear(); // a preset brings its own defaults
		}

		bool supported(int control) const override { return mapperHasSource(control); }

		BindingTarget binding(int player, int control) const override
		{
			const std::map<int64_t, BindingTarget>::const_iterator it = edits.find(editKey(player, control));
			if(it != edits.end())
				return it->second;
			return mapperHasSource(control) ? presetDefault(preset, control) : BindingTarget();
		}
		BindingTarget presetBinding(int p, int control) const override
		{
			return p >= 0 && p < PRESET_COUNT && mapperHasSource(control) ? presetDefault(p, control) : BindingTarget();
		}
		bool modified() const override { return !edits.empty(); }

		BindingTarget defaultBinding(int control) const override { return mapperHasSource(control) ? presetDefault(preset, control) : BindingTarget(); }
		void setBinding(int player, int control, const BindingTarget& t) override
		{
			if(mapperHasSource(control))
				edits[editKey(player, control)] = t;
		}
		void resetBindings(int player) override
		{
			for(std::map<int64_t, BindingTarget>::iterator it = edits.begin(); it != edits.end();)
			{
				if(it->first / kStride == (int64_t)player)
					edits.erase(it++);
				else
					++it;
			}
		}

		std::vector<SetupInfo> setups() const override
		{
			std::vector<SetupInfo> v;
			for(size_t i = 0; i < saved.size(); i++)
			{
				SetupInfo s;
				s.name = saved[i].name;
				s.program = saved[i].program;
				v.push_back(s);
			}
			return v;
		}
		bool saveSetup(const std::string& name, const std::string& program) override
		{
			if(name.empty())
				return false;
			Setup s;
			s.name = name;
			s.program = program;
			s.mode = mode;
			s.preset = preset;
			s.edits = edits;
			for(size_t i = 0; i < saved.size(); i++)
			{
				if(saved[i].name == name)
				{
					saved[i] = s;
					return true;
				}
			}
			saved.push_back(s);
			return true;
		}
		bool loadSetup(int index) override
		{
			if(index < 0 || (size_t)index >= saved.size())
				return false;
			mode = saved[(size_t)index].mode;
			preset = saved[(size_t)index].preset;
			edits = saved[(size_t)index].edits;
			return true;
		}
		std::string runningProgram() const override { return "KEEN4E.EXE"; }

		std::vector<BindingTarget> suggestions(int player, int control) const override
		{
			std::vector<BindingTarget> out;
			if(!mapperHasSource(control))
				return out;
			for(int p = 0; p < PRESET_COUNT; p++)
			{
				const BindingTarget t = presetDefault(p, control);
				if(t.kind == TARGET::NONE || !usable(player, t))
					continue;
				bool seen = false;
				for(size_t i = 0; i < out.size(); i++)
					seen = seen || out[i] == t;
				if(!seen)
					out.push_back(t);
			}
			return out;
		}

		void beginCapture() override { pending.clear(); }
		bool pollCapture(CaptureEvent& e) override
		{
			if(pending.empty())
				return false;
			e = pending.front();
			pending.pop_front();
			return true;
		}
		void pushCapture(const CaptureEvent& e) { pending.push_back(e); }

	private:
		struct Setup
		{
			std::string name, program;
			int mode, preset;
			std::map<int64_t, BindingTarget> edits;
		};

		static const int64_t kStride = (int64_t)1 << 32;
		static int64_t editKey(int player, int control) { return (int64_t)player * kStride + (int64_t)(uint32_t)control; }

		int mode;
		int preset;
		uint32_t heldBits;
		std::map<int64_t, BindingTarget> edits;
		std::vector<Setup> saved;
		std::deque<CaptureEvent> pending;
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

void SampleControls_SetHeld(uint32_t bits)
{
	if(!installed)
		sampleModel()->setHeld(bits);
}

void SampleControls_PushCapture(const CaptureEvent& event)
{
	if(!installed)
		sampleModel()->pushCapture(event);
}
