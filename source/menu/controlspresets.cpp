/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlspresets.cpp
 ***************************************************************************/
#include "controlspresets.h"
#include "libgui/GuiControllerLayout.h"
#include "drivers/InputData.h"

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

int Emulated_IndexOfType(const std::string& type)
{
	const std::vector<EmulatedMode>& modes = Emulated_Modes();
	for(size_t i = 0; i < modes.size(); i++)
		if(modes[i].type == type)
			return (int)i;
	// auto is 4axis here (InitializeJoysticks), and the type only reads auto between the config being set and the mapper building its groups
	return type == "auto" ? 0 : -1;
}
