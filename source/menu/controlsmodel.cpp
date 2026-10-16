/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsmodel.cpp
 ***************************************************************************/
#include "controlsmodel.h"
#include "controlspresets.h"
#include "libgui/GuiControllerLayout.h"
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
	p.battery = h.battery;
	p.sideways = h.sideways;
	p.stickClicks = h.stickClicks;
	p.emulated = Emulated::NONE;
	p.joystick = 0;
	if(p.connected)
	{
		p.device = PadType_Name(p.type);
		p.preset = preset;
		// joystick n is player n (the mapper's stick n); players past the joysticks that exist use keys and mouse
		if(index < mode.joysticks)
		{
			p.emulated = Emulated::JOYSTICK;
			p.joystick = index + 1;
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

std::string Setup_CleanName(const std::string& typed)
{
	std::string out;
	for(size_t i = 0; i < typed.size(); i++)
	{
		const char c = typed[i];
		if((unsigned char)c < 32 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == '.')
			continue;
		out += c;
	}
	const size_t first = out.find_first_not_of(' ');
	if(first == std::string::npos)
		return std::string();
	out = out.substr(first, out.find_last_not_of(' ') - first + 1);
	if(out.size() > 24)
	{
		out.resize(24);
		const size_t last = out.find_last_not_of(' ');
		out.resize(last + 1);
	}
	return out;
}

std::string Setup_CleanProgram(const std::string& typed)
{
	size_t start = typed.find_last_of("/\\:");
	start = start == std::string::npos ? 0 : start + 1;
	std::string out;
	for(size_t i = start; i < typed.size(); i++)
	{
		const char c = typed[i];
		if((unsigned char)c <= 32 || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			continue;
		out += (char)((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
	}
	return out;
}

std::string Setup_SuggestedName(const std::string& program)
{
	const std::string file = Setup_CleanProgram(program);
	const size_t dot = file.rfind('.');
	const std::string name = Setup_CleanName(dot == std::string::npos ? file : file.substr(0, dot));
	return name.empty() ? "My setup" : name;
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

bool hasSource(int control) { return Pad_SourceOfControl(control) >= 0; }

// ---- hardware: what the screens read from the console

// Invented players for the host tests and previews, until the real hardware is installed. A test or preview
// can make a player something else, and set what is held.
class SampleHardware : public ControlsHardware
{
	public:
		SampleHardware() : heldBits(0) {}

		PlayerHardware player(int i) const override
		{
			PlayerHardware h;
			if(i >= 0 && i < 4 && over[i].set)
			{
				h.type = over[i].type;
				h.connected = h.type != PadType::NONE;
				h.sideways = over[i].sideways;
				h.stickClicks = over[i].stickClicks;
				h.battery = h.connected && h.type != PadType::GAMECUBE ? 80 : -1;
				return h;
			}
			switch(i)
			{
				case 0: h.connected = true; h.type = PadType::WIIMOTE_NUNCHUK; h.battery = 80; break;
				case 1: h.connected = true; h.type = PadType::CLASSIC; h.battery = 45; break;
				case 2: h.connected = true; h.type = PadType::GAMECUBE; break;
				default: break;
			}
			return h;
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

		void setHeld(uint32_t bits) { heldBits = bits; }
		void setHardware(int player, PadType type, bool sideways, bool stickClicks)
		{
			if(player < 0 || player >= 4)
				return;
			over[player].set = true;
			over[player].type = type;
			over[player].sideways = sideways;
			over[player].stickClicks = stickClicks;
		}
		void resetHardware()
		{
			for(int i = 0; i < 4; i++)
				over[i].set = false;
		}

	private:
		struct Override { bool set = false; PadType type = PadType::NONE; bool sideways = false; bool stickClicks = false; };
		Override over[4];
		uint32_t heldBits;
};

SampleHardware * sampleHardware()
{
	static SampleHardware h;
	return &h;
}

ControlsHardware * hardwareInstalled = nullptr;

const ControlsHardware& hardware()
{
	return hardwareInstalled ? *hardwareInstalled : *sampleHardware();
}

class SampleControlsModel : public ControlsModel
{
	public:
		SampleControlsModel() : mode(0), dirty(false)
		{
			for(int i = 0; i < 4; i++)
				preset[i] = PRESET_ARROWS;
		}

		int playerCount() const override { return 4; }

		PlayerInfo player(int i) const override
		{
			const bool ok = validPlayer(i);
			return Controls_ComposePlayer(hardware().player(i), i, Emulated_Modes()[(size_t)mode],
				ok ? Presets_All()[(size_t)preset[i]].name : std::string());
		}

		DeviceStatus devices() const override { return hardware().devices(); }

		uint32_t held(int player) const override { return hardware().held(player); }

		std::vector<EmulatedMode> emulatedModes() const override { return Emulated_Modes(); }
		int currentEmulated() const override { return mode; }
		void setEmulated(int index) override
		{
			if(index >= 0 && index < (int)Emulated_Modes().size() && index != mode)
			{
				mode = index;
				dirty = true;
			}
		}

		std::vector<PresetInfo> presets() const override { return Presets_All(); }
		int currentPreset(int player) const override { return validPlayer(player) ? preset[player] : 0; }
		void applyPreset(int player, int index) override
		{
			if(!validPlayer(player) || index < 0 || index >= PRESET_COUNT)
				return;
			preset[player] = index;
			dropEdits(player); // a preset brings its own defaults, for this player only
			dirty = true;
		}

		bool supported(int control) const override { return hasSource(control); }

		BindingTarget binding(int player, int control) const override
		{
			const std::map<int64_t, BindingTarget>::const_iterator it = edits.find(editKey(player, control));
			if(it != edits.end())
				return it->second;
			return hasSource(control) ? Presets_Target(validPlayer(player) ? preset[player] : 0, control) : BindingTarget();
		}
		BindingTarget presetBinding(int p, int control) const override
		{
			return p >= 0 && p < PRESET_COUNT && hasSource(control) ? Presets_Target(p, control) : BindingTarget();
		}
		bool modified(int player) const override
		{
			for(std::map<int64_t, BindingTarget>::const_iterator it = edits.begin(); it != edits.end(); ++it)
				if(it->first / kStride == (int64_t)player)
					return true;
			return false;
		}

		BindingTarget defaultBinding(int player, int control) const override { return hasSource(control) ? Presets_Target(validPlayer(player) ? preset[player] : 0, control) : BindingTarget(); }
		void setBinding(int player, int control, const BindingTarget& t) override
		{
			if(!hasSource(control) || binding(player, control) == t)
				return;
			edits[editKey(player, control)] = t;
			dirty = true;
		}
		void resetBindings(int player) override
		{
			if(modified(player))
			{
				dropEdits(player);
				dirty = true;
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
				s.emulated = Emulated_Modes()[(size_t)saved[i].mode].name;
				s.preset = presetSummary(saved[i].preset);
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
			for(int i = 0; i < 4; i++)
				s.preset[i] = preset[i];
			s.edits = edits;
			current = name;
			dirty = false;
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
			for(int i = 0; i < 4; i++)
				preset[i] = saved[(size_t)index].preset[i];
			edits = saved[(size_t)index].edits;
			current = saved[(size_t)index].name;
			dirty = false;
			return true;
		}
		std::string runningProgram() const override { return "KEEN4E.EXE"; }
		std::string setupName() const override { return current; }
		bool setupModified() const override { return dirty; }

		std::vector<BindingTarget> suggestions(int player, int control) const override { return Presets_Suggestions(*this, player, control); }

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
			int mode;
			int preset[4];
			std::map<int64_t, BindingTarget> edits;
		};

		static bool validPlayer(int player) { return player >= 0 && player < 4; }

		void dropEdits(int player)
		{
			for(std::map<int64_t, BindingTarget>::iterator it = edits.begin(); it != edits.end();)
			{
				if(it->first / kStride == (int64_t)player)
					edits.erase(it++);
				else
					++it;
			}
		}

		// the preset every connected player started from, or "Mixed" when they differ
		std::string presetSummary(const int (&p)[4]) const
		{
			int first = -1;
			for(int i = 0; i < 4; i++)
			{
				if(!player(i).connected)
					continue;
				if(first < 0)
					first = p[i];
				else if(p[i] != first)
					return "Mixed";
			}
			return Presets_All()[(size_t)(first < 0 ? p[0] : first)].name;
		}

		static const int64_t kStride = (int64_t)1 << 32;
		static int64_t editKey(int player, int control) { return (int64_t)player * kStride + (int64_t)(uint32_t)control; }

		int mode;
		int preset[4];
		bool dirty;
		std::string current;
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

void Controls_SetHardware(ControlsHardware * hardware)
{
	hardwareInstalled = hardware;
}

void SampleControls_SetHeld(uint32_t bits)
{
	sampleHardware()->setHeld(bits);
}

void SampleControls_PushCapture(const CaptureEvent& event)
{
	if(!installed)
		sampleModel()->pushCapture(event);
}

void SampleControls_SetHardware(int player, PadType type, bool sideways, bool stickClicks)
{
	sampleHardware()->setHardware(player, type, sideways, stickClicks);
}

void SampleControls_ResetHardware()
{
	sampleHardware()->resetHardware();
}
