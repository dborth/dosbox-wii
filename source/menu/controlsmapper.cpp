/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsmapper.cpp
 ***************************************************************************/
#include "controlsmapper.h"
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

// The mapper's key events (key_<name>) for the catalog keys whose name is not simply the lower-cased one
struct KeyName { const char * catalog; const char * event; };
const KeyName kKeyNames[] =
{
	{ "Backspace", "bspace" },
	{ "LCtrl", "lctrl" }, { "LAlt", "lalt" }, { "LShift", "lshift" }, { "RCtrl", "rctrl" }, { "RAlt", "ralt" }, { "RShift", "rshift" },
	{ "PgUp", "pageup" }, { "PgDn", "pagedown" }, { "Ins", "insert" }, { "Del", "delete" },
	{ "[", "lbracket" }, { "]", "rbracket" },
	{ "CapsLk", "capslock" }, { "ScrLk", "scrolllock" }, { "PrtSc", "printscreen" }, { "Pause", "pause" }, { "NumLk", "numlock" },
	{ "Less", "lessthan" },
	{ "KP /", "kp_divide" }, { "KP *", "kp_multiply" }, { "KP -", "kp_minus" }, { "KP +", "kp_plus" },
	{ "KP Enter", "kp_enter" }, { "KP .", "kp_period" },
	{ "KP 0", "kp_0" }, { "KP 1", "kp_1" }, { "KP 2", "kp_2" }, { "KP 3", "kp_3" }, { "KP 4", "kp_4" },
	{ "KP 5", "kp_5" }, { "KP 6", "kp_6" }, { "KP 7", "kp_7" }, { "KP 8", "kp_8" }, { "KP 9", "kp_9" },
};

// A catalog key's event name
std::string keyEvent(const std::string& catalog)
{
	for(size_t i = 0; i < sizeof(kKeyNames) / sizeof(kKeyNames[0]); i++)
		if(catalog == kKeyNames[i].catalog)
			return std::string("key_") + kKeyNames[i].event;
	std::string e = "key_";
	for(size_t i = 0; i < catalog.size(); i++)
		e += (char)((catalog[i] >= 'A' && catalog[i] <= 'Z') ? catalog[i] - 'A' + 'a' : catalog[i]);
	return e;
}

// the event name -> catalog index of every key
const std::map<std::string, int>& keyByEvent()
{
	static std::map<std::string, int> m;
	if(m.empty())
	{
		const std::vector<std::string>& names = Catalog_Names(TARGET::KEY);
		for(size_t i = 0; i < names.size(); i++)
			m[keyEvent(names[i])] = (int)i;
	}
	return m;
}

// Same order as Catalog_Names(): a target's `code` indexes it
const char * const kMouseEvents[] = { "hand_mouseleft", "hand_mouseright", "hand_mousemid" };
const char * const kHotkeyEvents[] = { "hand_cycleup", "hand_cycledown", "hand_incfskip", "hand_decfskip", "hand_swapimg", "hand_speedlock" };

// Only the emulated joysticks 0 and 1 have buttons events (jbutton_<emulated joystick>_<button>), four buttons are offered
const int kJoystickEventSlots = 2;

} // namespace

bool Mapper_EventFor(int slot, const BindingTarget& target, std::string& event)
{
	event.clear();
	const std::vector<std::string>& names = Catalog_Names(target.kind);
	switch(target.kind)
	{
		case TARGET::NONE:
			return true;
		case TARGET::KEY:
			if(target.code < 0 || (size_t)target.code >= names.size())
				return false;
			event = keyEvent(names[(size_t)target.code]);
			return true;
		case TARGET::JOYSTICK:
		{
			if(slot < 0 || slot >= kJoystickEventSlots || target.code < 0 || (size_t)target.code >= names.size())
				return false;
			char text[24];
			snprintf(text, sizeof(text), "jbutton_%d_%d", slot, target.code);
			event = text;
			return true;
		}
		case TARGET::MOUSE:
			if(target.code < 0 || (size_t)target.code >= sizeof(kMouseEvents) / sizeof(kMouseEvents[0]))
				return false;
			event = kMouseEvents[target.code];
			return true;
		case TARGET::HOTKEY:
			if(target.code < 0 || (size_t)target.code >= sizeof(kHotkeyEvents) / sizeof(kHotkeyEvents[0]))
				return false;
			event = kHotkeyEvents[target.code];
			return true;
	}
	return false;
}

bool Mapper_TargetFor(int slot, const std::string& event, BindingTarget& target)
{
	target = BindingTarget();
	if(event.compare(0, 4, "key_") == 0)
	{
		const std::map<std::string, int>::const_iterator it = keyByEvent().find(event);
		if(it == keyByEvent().end() || it->second < 0)
			return false;
		target = BindingTarget(TARGET::KEY, it->second);
		return true;
	}
	if(event.compare(0, 8, "jbutton_") == 0)
	{
		int stick = -1, button = -1;
		if(sscanf(event.c_str() + 8, "%d_%d", &stick, &button) != 2 || slot < 0 || stick != slot || button < 0 || (size_t)button >= Catalog_Names(TARGET::JOYSTICK).size())
			return false;
		target = BindingTarget(TARGET::JOYSTICK, button);
		return true;
	}
	for(size_t i = 0; i < sizeof(kMouseEvents) / sizeof(kMouseEvents[0]); i++)
		if(event == kMouseEvents[i])
		{
			target = BindingTarget(TARGET::MOUSE, (int)i);
			return true;
		}
	for(size_t i = 0; i < sizeof(kHotkeyEvents) / sizeof(kHotkeyEvents[0]); i++)
		if(event == kHotkeyEvents[i])
		{
			target = BindingTarget(TARGET::HOTKEY, (int)i);
			return true;
		}
	return false;
}

bool Mapper_MigrateJoystickBinds(PadMapper& mapper)
{
	const std::vector<EmulatedMode>& modes = Emulated_Modes();
	int mode = Emulated_IndexOfType(mapper.joystickType());
	if(mode < 0)
		return false;
	const EmulatedMode& m = modes[(size_t)mode];

	// the DOS joystick each player drives (-1 for none)
	int slotOf[4];
	for(int p = 0; p < 4; p++)
		slotOf[p] = p - m.firstPlayer >= 0 && p - m.firstPlayer < m.joysticks ? p - m.firstPlayer : -1;

	bool changed = false;
	for(int p = 0; p < 4; p++)
		for(int s = 0; s < PAD_SOURCE_COUNT; s++)
		{
			const std::vector<std::string> events = mapper.events(p, s);
			for(size_t i = 0; i < events.size(); i++)
			{
				int stick = -1, button = -1;
				if(events[i].compare(0, 8, "jbutton_") != 0 || sscanf(events[i].c_str() + 8, "%d_%d", &stick, &button) != 2 || stick == slotOf[p])
					continue;
				// not the player's own joystick. A joystick nobody drives is inert, so it is left alone for a player with none.
				bool drivenByOther = false;
				for(int q = 0; q < 4; q++)
					drivenByOther = drivenByOther || (q != p && slotOf[q] == stick);
				if(slotOf[p] < 0 && !drivenByOther)
					continue;
				std::string to;
				if(slotOf[p] >= 0 && button < m.buttons)
				{
					char text[24];
					snprintf(text, sizeof(text), "jbutton_%d_%d", slotOf[p], button);
					to = text;
				}
				if(mapper.setEvent(p, s, to.c_str()))
					changed = true;
				break;	// a source is one thing: setEvent took all its binds
			}
		}
	return changed;
}

MapperControlsModel::MapperControlsModel(ControlsHardware& hardware, PadMapper& m, std::function<std::string()> runningProgram)
	: hw(hardware), mapper(m), program(runningProgram), failedSave(false)
{
	for(int i = 0; i < 4; i++)
		presetOf[i] = -1;
}

void MapperControlsModel::forget()
{
	failedSave = false;
	seenType.clear();
	for(int i = 0; i < 4; i++)
		presetOf[i] = -1;
}

// ---- players and devices

PlayerInfo MapperControlsModel::player(int index) const
{
	const PlayerHardware h = hw.player(index);
	const std::vector<EmulatedMode>& modes = Emulated_Modes();
	return Controls_ComposePlayer(h, index, modes[(size_t)currentEmulated()],
		h.connected && validPlayer(index) ? Presets_All()[(size_t)currentPreset(index)].name : std::string());
}

DeviceStatus MapperControlsModel::devices() const { return hw.devices(); }

uint32_t MapperControlsModel::held(int player) const { return hw.held(player); }

// ---- the emulated mode

std::vector<EmulatedMode> MapperControlsModel::emulatedModes() const { return Emulated_Modes(); }

int MapperControlsModel::currentEmulated() const
{
	// auto reads as 4axis; a type none of the modes stands for (not expected) is shown as Joystick
	const int index = Emulated_IndexOfType(mapper.joystickType());
	return index < 0 ? 0 : index;
}

void MapperControlsModel::setEmulated(int index)
{
	const std::vector<EmulatedMode>& modes = Emulated_Modes();
	if(index < 0 || (size_t)index >= modes.size() || index == currentEmulated())
		return;
	if(!mapper.setJoystickType(modes[(size_t)index].type))
		return;
	// the groups are rebuilt from the mapper file, which still holds the binds of the old type
	Mapper_MigrateJoystickBinds(mapper);
	saved();
	followJoystickType();
}

void MapperControlsModel::followJoystickType() const
{
	seenType = mapper.joystickType();
	for(int p = 0; p < 4; p++)
		if(presetOf[p] >= 0 && Presets_All()[(size_t)presetOf[p]].needsJoystick && joystickButtons(p) == 0)
			presetOf[p] = -1;
}

// ---- reading and writing a button

BindingTarget MapperControlsModel::current(int player, int control) const
{
	const int source = Pad_SourceOfControl(control);
	if(source < 0 || !validPlayer(player))
		return BindingTarget();
	const std::vector<std::string> events = mapper.events(player, source);
	for(size_t i = 0; i < events.size(); i++)
	{
		BindingTarget t;
		if(Mapper_TargetFor(joystickSlot(player), events[i], t))
			return t;
	}
	return BindingTarget();
}

BindingTarget MapperControlsModel::effective(int player, const BindingTarget& t) const
{
	return t.kind == TARGET::JOYSTICK && !usable(player, t) ? BindingTarget() : t;
}

bool MapperControlsModel::write(int player, int control, const BindingTarget& t)
{
	const int source = Pad_SourceOfControl(control);
	std::string event;
	if(source < 0 || !validPlayer(player) || !Mapper_EventFor(joystickSlot(player), t, event))
		return false;
	if(!event.empty() && !mapper.eventExists(event.c_str()))
		return false;
	return mapper.setEvent(player, source, event.c_str());
}

void MapperControlsModel::saved()
{
	failedSave = !mapper.save();
}

bool MapperControlsModel::supported(int control) const { return Pad_SourceOfControl(control) >= 0; }

BindingTarget MapperControlsModel::binding(int player, int control) const { return current(player, control); }

void MapperControlsModel::setBinding(int player, int control, const BindingTarget& t)
{
	if(!validPlayer(player) || !supported(control))
		return;
	// a joystick button the joystick does not have would do nothing: it is not offered, and not taken
	if(t.kind == TARGET::JOYSTICK && !usable(player, t))
		return;
	if(effective(player, current(player, control)) == t)
		return;
	if(write(player, control, t))
		saved();
}

// ---- presets

std::vector<PresetInfo> MapperControlsModel::presets() const { return Presets_All(); }

BindingTarget MapperControlsModel::presetBinding(int preset, int control) const { return Presets_Target(preset, control); }

int MapperControlsModel::differences(int player, int preset) const
{
	int n = 0;
	for(int s = 0; s < PAD_SOURCE_COUNT; s++)
	{
		const int control = Pad_ControlOfSource(s);
		if(effective(player, current(player, control)) != effective(player, Presets_Target(preset, control)))
			n++;
	}
	return n;
}

int MapperControlsModel::currentPreset(int player) const
{
	if(!validPlayer(player))
		return 0;
	if(seenType != mapper.joystickType())
		followJoystickType();	// the type was changed from Settings
	if(presetOf[player] < 0)
	{
		// the mapper file does not record a preset: take the closest one the player can use
		int best = -1, bestDiff = 0;
		for(int p = 0; p < PRESET_COUNT; p++)
		{
			if(Presets_All()[(size_t)p].needsJoystick && joystickButtons(player) == 0)
				continue;
			const int d = differences(player, p);
			if(best < 0 || d < bestDiff)
			{
				best = p;
				bestDiff = d;
			}
		}
		presetOf[player] = best < 0 ? PRESET_ARROWS : best;
	}
	return presetOf[player];
}

// Gives every button of a player what a preset gives it, limited to what the joystick has. True if any changed.
bool MapperControlsModel::writePreset(int player, int preset)
{
	bool changed = false;
	for(int s = 0; s < PAD_SOURCE_COUNT; s++)
	{
		const int control = Pad_ControlOfSource(s);
		const BindingTarget want = effective(player, Presets_Target(preset, control));
		if(effective(player, current(player, control)) != want && write(player, control, want))
			changed = true;
	}
	return changed;
}

void MapperControlsModel::applyPreset(int player, int index)
{
	if(!validPlayer(player) || index < 0 || index >= PRESET_COUNT)
		return;
	const bool changed = writePreset(player, index);
	presetOf[player] = index;
	if(changed)
		saved();
}

bool MapperControlsModel::modified(int player) const
{
	return validPlayer(player) && differences(player, currentPreset(player)) > 0;
}

BindingTarget MapperControlsModel::defaultBinding(int player, int control) const
{
	if(!validPlayer(player) || !supported(control))
		return BindingTarget();
	return effective(player, Presets_Target(currentPreset(player), control));
}

void MapperControlsModel::resetBindings(int player)
{
	if(!modified(player))
		return;
	if(writePreset(player, currentPreset(player)))
		saved();
}

std::vector<BindingTarget> MapperControlsModel::suggestions(int player, int control) const
{
	return Presets_Suggestions(*this, player, control);
}

// ---- press to choose

void MapperControlsModel::beginCapture() { hw.beginCapture(); }
bool MapperControlsModel::pollCapture(CaptureEvent& event) { return hw.pollCapture(event); }

// ---- what is running

std::string MapperControlsModel::runningProgram() const
{
	return program ? program() : std::string();
}
