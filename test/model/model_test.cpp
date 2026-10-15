// Host tests for the controls model contract and the sample model behind it.
#include "controlsmodel.h"
#include "libgui/GuiControllerLayout.h"
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string.h>

static int failures = 0, checks = 0;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)
#define CHECK_EQ(a, b) do { checks++; const std::string x = (a), y = (b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n      got  \"%s\"\n      want \"%s\"\n", __FILE__, __LINE__, #a, x.c_str(), y.c_str()); } } while(0)

static ControlsModel& fresh()
{
	// the sample model is a singleton: put it back to its starting state
	ControlsModel& m = Controls();
	m.setEmulated(0);
	m.applyPreset(1);
	return m;
}

static BindingTarget key(const char * n) { return BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, n)); }

static void testEmulatedModes()
{
	ControlsModel& m = fresh();
	const std::vector<EmulatedMode> modes = m.emulatedModes();
	CHECK(modes.size() == 3);
	CHECK_EQ(modes[0].name, "Joystick");
	CHECK_EQ(modes[1].name, "Two joysticks");
	CHECK_EQ(modes[2].name, "Keyboard + mouse");

	// Joystick: one stick with four buttons, player 1 only
	CHECK(m.currentEmulated() == 0);
	CHECK(m.joystickButtons(0) == 4);
	CHECK(m.joystickButtons(1) == 0);
	CHECK(m.joystickButtons(3) == 0);
	CHECK(m.player(0).emulated == Emulated::JOYSTICK && m.player(0).joystick == 1);
	CHECK_EQ(m.player(0).role, "Joystick 1");
	CHECK(m.player(1).emulated == Emulated::KEYS_MOUSE);
	CHECK_EQ(m.player(1).role, "Keys + mouse");

	// Two joysticks: players 1 and 2, two buttons each; joystick n is player n
	m.setEmulated(1);
	CHECK(m.joystickButtons(0) == 2 && m.joystickButtons(1) == 2 && m.joystickButtons(2) == 0);
	CHECK(m.player(1).emulated == Emulated::JOYSTICK && m.player(1).joystick == 2);
	CHECK_EQ(m.player(1).role, "Joystick 2");
	CHECK(m.player(2).emulated == Emulated::KEYS_MOUSE); // the sample's third pad has no joystick to drive

	// Keyboard + mouse: nobody has a joystick
	m.setEmulated(2);
	for(int p = 0; p < 4; p++)
		CHECK(m.joystickButtons(p) == 0);
	CHECK(m.player(0).emulated == Emulated::KEYS_MOUSE);

	// out-of-range changes are ignored
	m.setEmulated(7);
	CHECK(m.currentEmulated() == 2);
	m.setEmulated(-1);
	CHECK(m.currentEmulated() == 2);

	// slots that are empty say nothing
	CHECK(!m.player(3).connected && m.player(3).emulated == Emulated::NONE);
	CHECK(m.joystickButtons(-1) == 0);
	m.setEmulated(0);
}

static void testJoystickAxes()
{
	ControlsModel& m = fresh();
	// "Joystick" has four axes (both sticks count), two joysticks have two each, and none has none
	m.setEmulated(0);
	CHECK(m.joystickAxes(0) == 4 && m.joystickAxes(1) == 0);
	m.setEmulated(1);
	CHECK(m.joystickAxes(0) == 2 && m.joystickAxes(1) == 2 && m.joystickAxes(2) == 0);
	m.setEmulated(2);
	CHECK(m.joystickAxes(0) == 0);
	CHECK(m.joystickAxes(-1) == 0);
	m.setEmulated(0);
}

static void testSampleHeld()
{
	ControlsModel& m = fresh();
	SampleControls_SetHeld(INPUT_BTN_A | INPUT_TRIGGER_L);
	CHECK(m.held(0) == (INPUT_BTN_A | INPUT_TRIGGER_L));
	CHECK(m.held(1) == 0); // only player 1 is "pressing" in the sample
	SampleControls_SetHeld(0);
	CHECK(m.held(0) == 0);
}

static void testUsable()
{
	ControlsModel& m = fresh();
	const BindingTarget joy1(TARGET::JOYSTICK, 0), joy3(TARGET::JOYSTICK, 2);
	const BindingTarget mouse(TARGET::MOUSE, 0), hot(TARGET::HOTKEY, 0);

	m.setEmulated(0); // one joystick, 4 buttons
	CHECK(m.usable(0, joy1) && m.usable(0, joy3));
	CHECK(!m.usable(1, joy1)); // player 2 has no joystick here

	m.setEmulated(1); // two joysticks, 2 buttons each
	CHECK(m.usable(0, joy1) && !m.usable(0, joy3));
	CHECK(m.usable(1, joy1));

	m.setEmulated(2); // none
	CHECK(!m.usable(0, joy1));

	// keys, mouse and hotkeys never depend on the joystick
	for(int mode = 0; mode < 3; mode++)
	{
		m.setEmulated(mode);
		CHECK(m.usable(0, key("Ctrl")) && m.usable(2, mouse) && m.usable(1, hot) && m.usable(0, BindingTarget()));
	}
	m.setEmulated(0);
}

static void testDescribe()
{
	ControlsModel& m = fresh();
	m.applyPreset(0); // Compatible: A -> joystick button 1, X -> button 3
	m.setEmulated(0);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Joystick 1");
	CHECK_EQ(m.describe(0, INPUT_BTN_X), "Joystick 3");
	CHECK_EQ(m.describe(1, INPUT_BTN_A), "Joystick off"); // player 2 has none under "Joystick"

	m.setEmulated(1);
	CHECK_EQ(m.describe(0, INPUT_BTN_X), "No button 3");
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Joystick 1");

	m.setEmulated(2);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Joystick off");

	m.applyPreset(1);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Key: Ctrl");
	CHECK_EQ(m.describe(0, INPUT_BTN_UP), "Key: Up");
	CHECK_EQ(m.describe(0, INPUT_TRIGGER_ZL), "Not assigned");
	m.setEmulated(0);
}

static void testSupported()
{
	ControlsModel& m = fresh();
	const int has[] = { INPUT_BTN_A, INPUT_BTN_B, INPUT_BTN_X, INPUT_BTN_Y, INPUT_TRIGGER_L, INPUT_TRIGGER_R, INPUT_TRIGGER_ZL,
		INPUT_TRIGGER_ZR, INPUT_BTN_MINUS, INPUT_BTN_PLUS, INPUT_BTN_UP, INPUT_BTN_DOWN, INPUT_BTN_LEFT, INPUT_BTN_RIGHT,
		INPUT_THUMB_L | BIND_STICK_CLICK, INPUT_THUMB_R | BIND_STICK_CLICK };
	for(size_t i = 0; i < sizeof(has) / sizeof(has[0]); i++)
		CHECK(m.supported(has[i]));
	CHECK(sizeof(has) / sizeof(has[0]) == 16); // the mapper's pad sources: 12 buttons and the D-pad

	CHECK(!m.supported(INPUT_THUMB_L));			// stick movement is the joystick's axes, not a mapping
	CHECK(!m.supported(INPUT_THUMB_R));
	CHECK(!m.supported(GUI_CTL_CAPTURE));
	CHECK(!m.supported(INPUT_BTN_HOME));		// the menu button is fixed

	// an unsupported control cannot be set, and reads as nothing
	m.setBinding(0, GUI_CTL_CAPTURE, key("Esc"));
	CHECK(m.binding(0, GUI_CTL_CAPTURE).kind == TARGET::NONE);
	m.setBinding(0, INPUT_THUMB_L, key("Esc")); // a stick's movement is the joystick's axes, not a mapping
	CHECK(m.binding(0, INPUT_THUMB_L).kind == TARGET::NONE);
}

static void testPresets()
{
	ControlsModel& m = fresh();
	const std::vector<PresetInfo> ps = m.presets();
	CHECK(ps.size() == 5);
	CHECK_EQ(ps[0].name, "Compatible");
	for(size_t i = 0; i < ps.size(); i++)
	{
		CHECK(!ps[i].description.empty());
		CHECK(ps[i].needsJoystick == (i == 0)); // only Compatible fires the joystick
	}

	// presets must actually differ (otherwise the picker is a dud)
	const int controls[] = { INPUT_BTN_A, INPUT_BTN_B, INPUT_BTN_X, INPUT_BTN_Y };
	for(size_t a = 0; a < ps.size(); a++)
	{
		for(size_t b = a + 1; b < ps.size(); b++)
		{
			bool differs = false;
			m.applyPreset((int)a);
			BindingTarget ta[4];
			for(int c = 0; c < 4; c++) ta[c] = m.binding(0, controls[c]);
			m.applyPreset((int)b);
			for(int c = 0; c < 4; c++) differs = differs || ta[c] != m.binding(0, controls[c]);
			if(!differs) { // Arrow keys and Menu keys share a D-pad; compare more than the face buttons
				m.applyPreset((int)a); const BindingTarget l = m.binding(0, INPUT_TRIGGER_L); m.applyPreset((int)b);
				differs = l != m.binding(0, INPUT_TRIGGER_L);
			}
			CHECK(differs);
		}
	}

	m.applyPreset(0);
	CHECK(m.currentPreset() == 0);
	CHECK(m.binding(0, INPUT_BTN_A) == BindingTarget(TARGET::JOYSTICK, 0));
	CHECK(m.binding(0, INPUT_BTN_UP).kind == TARGET::NONE); // the D-pad is the joystick's hat there
	m.applyPreset(3); // Point and click
	CHECK(m.binding(0, INPUT_BTN_A).kind == TARGET::MOUSE);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Mouse left");
	CHECK_EQ(m.describe(0, INPUT_BTN_B), "Mouse right");

	// applying a preset throws away edits, for every player
	m.setBinding(0, INPUT_BTN_A, key("F1"));
	m.setBinding(2, INPUT_BTN_B, key("F2"));
	m.applyPreset(3);
	CHECK(m.binding(0, INPUT_BTN_A).kind == TARGET::MOUSE && m.binding(2, INPUT_BTN_B).kind == TARGET::MOUSE);

	// bad indexes are ignored
	m.applyPreset(99);
	CHECK(m.currentPreset() == 3);
	m.applyPreset(-1);
	CHECK(m.currentPreset() == 3);
	CHECK_EQ(m.player(0).preset, "Point and click");
	m.applyPreset(1);
}

static void testPerPlayerBindings()
{
	ControlsModel& m = fresh();
	m.setBinding(0, INPUT_BTN_A, key("F1"));
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Key: F1");
	CHECK_EQ(m.describe(1, INPUT_BTN_A), "Key: Ctrl"); // another player is untouched
	m.setBinding(1, INPUT_BTN_A, BindingTarget(TARGET::MOUSE, 1));
	CHECK_EQ(m.describe(1, INPUT_BTN_A), "Mouse right");
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Key: F1");

	// a "not assigned" edit is an edit, not a return to the default
	m.setBinding(0, INPUT_BTN_B, BindingTarget());
	CHECK_EQ(m.describe(0, INPUT_BTN_B), "Not assigned");

	m.resetBindings(0);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Key: Ctrl");
	CHECK_EQ(m.describe(0, INPUT_BTN_B), "Key: Alt");
	CHECK_EQ(m.describe(1, INPUT_BTN_A), "Mouse right"); // player 2's edit survives player 1's reset
	m.resetBindings(1);
	CHECK_EQ(m.describe(1, INPUT_BTN_A), "Key: Ctrl");

	// stick clicks are controls too
	m.setBinding(0, INPUT_THUMB_L | BIND_STICK_CLICK, key("Tab"));
	CHECK_EQ(m.describe(0, INPUT_THUMB_L | BIND_STICK_CLICK), "Key: Tab");
	CHECK_EQ(m.describe(0, INPUT_THUMB_R | BIND_STICK_CLICK), "Not assigned");
	m.resetBindings(0);
}

static void testCatalogs()
{
	// the lists the chooser offers must not contain what the mapper cannot do
	CHECK(Catalog_Names(TARGET::MOUSE).size() == 3);
	CHECK(Catalog_Find(TARGET::MOUSE, "move up") < 0);
	CHECK(Catalog_Names(TARGET::JOYSTICK).size() == 4);
	CHECK(Catalog_Names(TARGET::HOTKEY).size() == 6);
	CHECK(Catalog_Find(TARGET::HOTKEY, "Pause") < 0 && Catalog_Find(TARGET::HOTKEY, "Screenshot") < 0);

	// every name finds itself, and names are unique within a list
	const TARGET kinds[] = { TARGET::KEY, TARGET::JOYSTICK, TARGET::MOUSE, TARGET::HOTKEY };
	for(size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++)
	{
		const std::vector<std::string>& v = Catalog_Names(kinds[k]);
		for(size_t i = 0; i < v.size(); i++)
			CHECK(Catalog_Find(kinds[k], v[i].c_str()) == (int)i);
	}

	// out-of-range codes describe as nothing rather than reading past the list
	CHECK_EQ(BindingTarget(TARGET::JOYSTICK, 2).describe(), "Joystick 3");
	CHECK_EQ(BindingTarget(TARGET::HOTKEY, 0).describe(), "DOS: Cycles up");
	CHECK_EQ(BindingTarget(TARGET::KEY, 9999).describe(), "Not assigned");
	CHECK_EQ(BindingTarget(TARGET::KEY, -1).describe(), "Not assigned");
	CHECK(BindingTarget() == BindingTarget(TARGET::NONE, 5)); // "nothing" equals nothing whatever the code
}

// The model's lists mirror buttons.cpp (the real targets); a rename there that is not made here shows up as a failure
static void testMatchesButtonsCpp()
{
	std::ifstream f(std::string(SRC_DIR) + "/menu/buttons.cpp");
	CHECK(f.good());
	std::stringstream ss;
	ss << f.rdbuf();
	const std::string text = ss.str();
	const TARGET kinds[] = { TARGET::HOTKEY, TARGET::MOUSE };
	for(size_t k = 0; k < 2; k++)
	{
		const std::vector<std::string>& v = Catalog_Names(kinds[k]);
		for(size_t i = 0; i < v.size(); i++)
		{
			const std::string label = kinds[k] == TARGET::MOUSE ? "\"Mouse " + v[i] + "\"" : "\"" + v[i] + "\"";
			if(text.find(label) == std::string::npos)
				printf("      not in buttons.cpp: %s\n", label.c_str());
			CHECK(text.find(label) != std::string::npos);
		}
	}
	for(size_t i = 0; i < Catalog_Names(TARGET::JOYSTICK).size(); i++)
		CHECK(text.find("\"Joystick " + Catalog_Names(TARGET::JOYSTICK)[i] + "\"") != std::string::npos);
}

static void testHidKeys()
{
	// letters, digits, function keys
	for(unsigned i = 0; i < 26; i++)
		CHECK(Catalog_KeyFromHid(4 + i) == Catalog_Find(TARGET::KEY, std::string(1, (char)('A' + i)).c_str()));
	for(unsigned i = 0; i < 9; i++)
		CHECK(Catalog_KeyFromHid(30 + i) == Catalog_Find(TARGET::KEY, std::string(1, (char)('1' + i)).c_str()));
	CHECK(Catalog_KeyFromHid(39) == Catalog_Find(TARGET::KEY, "0"));
	CHECK(Catalog_KeyFromHid(58) == Catalog_Find(TARGET::KEY, "F1"));
	CHECK(Catalog_KeyFromHid(69) == Catalog_Find(TARGET::KEY, "F12"));

	// the named keys
	struct { unsigned hid; const char * name; } named[] = {
		{ 40, "Enter" }, { 41, "Esc" }, { 42, "Backspace" }, { 43, "Tab" }, { 44, "Space" }, { 73, "Ins" }, { 74, "Home" },
		{ 75, "PgUp" }, { 76, "Del" }, { 77, "End" }, { 78, "PgDn" }, { 79, "Right" }, { 80, "Left" }, { 81, "Down" }, { 82, "Up" },
		{ 45, "Minus" }, { 46, "Equals" }, { 47, "[" }, { 48, "]" }, { 49, "Backslash" }, { 51, "Semicolon" }, { 52, "Quote" },
		{ 53, "Grave" }, { 54, "Comma" }, { 55, "Period" }, { 56, "Slash" },
	};
	for(size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++)
	{
		const int code = Catalog_Find(TARGET::KEY, named[i].name);
		CHECK(code >= 0);
		CHECK(Catalog_KeyFromHid(named[i].hid) == code);
	}

	// left and right modifiers are the same key to us
	CHECK(Catalog_KeyFromHid(224) == Catalog_Find(TARGET::KEY, "Ctrl") && Catalog_KeyFromHid(228) == Catalog_KeyFromHid(224));
	CHECK(Catalog_KeyFromHid(225) == Catalog_Find(TARGET::KEY, "Shift") && Catalog_KeyFromHid(229) == Catalog_KeyFromHid(225));
	CHECK(Catalog_KeyFromHid(226) == Catalog_Find(TARGET::KEY, "Alt") && Catalog_KeyFromHid(230) == Catalog_KeyFromHid(226));

	// keys the mapper cannot bind here are refused rather than guessed
	CHECK(Catalog_KeyFromHid(0) == -1);		// no key
	CHECK(Catalog_KeyFromHid(57) == -1);	// caps lock
	CHECK(Catalog_KeyFromHid(83) == -1);	// num lock
	CHECK(Catalog_KeyFromHid(89) == -1);	// keypad
	CHECK(Catalog_KeyFromHid(227) == -1);	// the GUI key opens the menu
	CHECK(Catalog_KeyFromHid(1000) == -1);

	// every key in the catalog can be pressed (except nothing is unreachable by a USB keyboard)
	const std::vector<std::string>& keys = Catalog_Names(TARGET::KEY);
	CHECK(keys.size() == 77);
	std::vector<bool> reached(keys.size(), false);
	for(unsigned hid = 0; hid < 232; hid++)
	{
		const int code = Catalog_KeyFromHid(hid);
		if(code >= 0)
			reached[(size_t)code] = true;
	}
	for(size_t i = 0; i < keys.size(); i++)
	{
		if(!reached[i])
			printf("      no USB key for catalog key %s\n", keys[i].c_str());
		CHECK(reached[i]);
	}
}

static void testKindsAndChoices()
{
	ControlsModel& m = fresh();
	// "Joystick": player 1 has one, the others do not
	m.setEmulated(0);
	CHECK(m.kindsOffered(0).size() == 4 && m.kindsOffered(0)[1] == TARGET::JOYSTICK);
	CHECK(m.kindsOffered(1).size() == 3);
	for(size_t i = 0; i < m.kindsOffered(1).size(); i++)
		CHECK(m.kindsOffered(1)[i] != TARGET::JOYSTICK);
	CHECK(m.choices(0, TARGET::JOYSTICK).size() == 4);
	CHECK(m.choices(1, TARGET::JOYSTICK).empty());

	// two joysticks with two buttons each
	m.setEmulated(1);
	CHECK(m.kindsOffered(1).size() == 4);
	CHECK(m.choices(0, TARGET::JOYSTICK).size() == 2 && m.choices(1, TARGET::JOYSTICK).size() == 2);
	CHECK(m.choices(2, TARGET::JOYSTICK).empty());
	CHECK(m.choices(0, TARGET::JOYSTICK)[1] == 1);

	// keyboard + mouse: no joystick kind for anyone
	m.setEmulated(2);
	CHECK(m.kindsOffered(0).size() == 3);
	CHECK(m.choices(0, TARGET::JOYSTICK).empty());

	// keys, mouse buttons and DOS functions are always all there
	CHECK(m.choices(0, TARGET::KEY).size() == Catalog_Names(TARGET::KEY).size());
	CHECK(m.choices(3, TARGET::MOUSE).size() == 3);
	CHECK(m.choices(0, TARGET::HOTKEY).size() == Catalog_Names(TARGET::HOTKEY).size());
	const std::vector<int> hot = m.choices(0, TARGET::HOTKEY);
	for(size_t i = 0; i < hot.size(); i++)
		CHECK(hot[i] == (int)i);
	m.setEmulated(0);
}

static void testSuggestions()
{
	ControlsModel& m = fresh();
	m.setEmulated(0);
	const std::vector<BindingTarget> a = m.suggestions(0, INPUT_BTN_A);
	CHECK(a.size() >= 3);
	bool hasJoystick = false, hasMouse = false, hasKey = false;
	for(size_t i = 0; i < a.size(); i++)
	{
		CHECK(a[i].kind != TARGET::NONE);
		CHECK(m.usable(0, a[i]));
		hasJoystick = hasJoystick || a[i].kind == TARGET::JOYSTICK;
		hasMouse = hasMouse || a[i].kind == TARGET::MOUSE;
		hasKey = hasKey || a[i].kind == TARGET::KEY;
		for(size_t j = i + 1; j < a.size(); j++)
			CHECK(a[i] != a[j]); // no duplicates
	}
	CHECK(hasJoystick && hasMouse && hasKey);

	// a control no preset uses has nothing to suggest, and neither does one the mapper has no source for
	CHECK(m.suggestions(0, INPUT_TRIGGER_ZR).empty());
	CHECK(m.suggestions(0, INPUT_BTN_HOME).empty());
	CHECK(m.suggestions(0, GUI_CTL_CAPTURE).empty());

	// no joystick, no joystick suggestions; a joystick without the button, none for that button
	m.setEmulated(2);
	const std::vector<BindingTarget> k = m.suggestions(0, INPUT_BTN_A);
	for(size_t i = 0; i < k.size(); i++)
		CHECK(k[i].kind != TARGET::JOYSTICK);
	CHECK(!k.empty());
	m.setEmulated(1); // two buttons: X is Compatible's joystick button 3
	const std::vector<BindingTarget> x = m.suggestions(0, INPUT_BTN_X);
	for(size_t i = 0; i < x.size(); i++)
		CHECK(!(x[i].kind == TARGET::JOYSTICK && x[i].code >= 2));
	m.setEmulated(0);
}

static void testCapture()
{
	ControlsModel& m = fresh();
	CaptureEvent e;
	m.beginCapture();
	CHECK(!m.pollCapture(e)); // nothing pressed

	// presses come out in order, once each
	SampleControls_PushCapture({ TARGET::KEY, Catalog_Find(TARGET::KEY, "Z") });
	SampleControls_PushCapture({ TARGET::MOUSE, 1 });
	CHECK(m.pollCapture(e) && e.kind == TARGET::KEY && e.code == Catalog_Find(TARGET::KEY, "Z"));
	CHECK(m.pollCapture(e) && e.kind == TARGET::MOUSE && e.code == 1);
	CHECK(!m.pollCapture(e));

	// starting again drops what was pressed before
	SampleControls_PushCapture({ TARGET::KEY, 0 });
	m.beginCapture();
	CHECK(!m.pollCapture(e));
}

static void testPresetPreview()
{
	ControlsModel& m = fresh();
	// what a preset gives a control, before it is applied, agrees with what applying it does
	for(int preset = 0; preset < (int)m.presets().size(); preset++)
	{
		m.applyPreset(preset);
		const int controls[] = { INPUT_BTN_A, INPUT_BTN_B, INPUT_BTN_X, INPUT_BTN_Y, INPUT_BTN_UP, INPUT_BTN_PLUS, INPUT_BTN_MINUS, INPUT_TRIGGER_L };
		for(size_t c = 0; c < sizeof(controls) / sizeof(controls[0]); c++)
		{
			CHECK(m.presetBinding(preset, controls[c]) == m.binding(0, controls[c]));
			CHECK(m.presetBinding(preset, controls[c]) == m.defaultBinding(controls[c]));
		}
	}
	CHECK(m.presetBinding(-1, INPUT_BTN_A).kind == TARGET::NONE);
	CHECK(m.presetBinding(99, INPUT_BTN_A).kind == TARGET::NONE);
	CHECK(m.presetBinding(0, GUI_CTL_CAPTURE).kind == TARGET::NONE);

	// describing a target that is not set yet says what it would do for that player
	m.setEmulated(1);
	CHECK_EQ(m.describeTarget(0, BindingTarget(TARGET::JOYSTICK, 1)), "Joystick 2");
	CHECK_EQ(m.describeTarget(0, BindingTarget(TARGET::JOYSTICK, 2)), "No button 3");
	CHECK_EQ(m.describeTarget(2, BindingTarget(TARGET::JOYSTICK, 0)), "Joystick off"); // player 3 has no joystick
	CHECK_EQ(m.describeTarget(0, BindingTarget()), "Not assigned");
	CHECK_EQ(m.describeTarget(0, key("Esc")), "Key: Esc");
	m.setEmulated(0);
}

static void testModified()
{
	ControlsModel& m = fresh();
	CHECK(!m.modified());
	m.setBinding(1, INPUT_BTN_A, key("F2"));
	CHECK(m.modified());					// any player's change counts
	m.resetBindings(1);
	CHECK(!m.modified());
	m.setBinding(0, INPUT_BTN_B, key("F2"));
	m.applyPreset(2);						// applying a preset throws the change away
	CHECK(!m.modified());
	m.setEmulated(2);						// the emulated mode is not a button edit
	CHECK(!m.modified());
	m.setEmulated(0);
	m.applyPreset(1);
}

static void testSetups()
{
	ControlsModel& m = fresh();
	CHECK(m.setups().empty());
	CHECK(!m.saveSetup("", "X.EXE"));	// a setup needs a name
	CHECK(!m.loadSetup(0));				// and there is nothing to load yet
	CHECK_EQ(m.runningProgram(), "KEEN4E.EXE");

	m.setEmulated(1);
	m.applyPreset(2);
	m.setBinding(0, INPUT_BTN_A, key("F5"));
	CHECK(m.saveSetup("Keen", m.runningProgram()));
	CHECK(m.setups().size() == 1);
	CHECK_EQ(m.setups()[0].name, "Keen");
	CHECK_EQ(m.setups()[0].program, "KEEN4E.EXE");

	// change everything, load, and it all comes back
	m.setEmulated(0);
	m.applyPreset(4);
	CHECK(m.loadSetup(0));
	CHECK(m.currentEmulated() == 1);
	CHECK(m.currentPreset() == 2);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Key: F5");
	CHECK_EQ(m.describe(0, INPUT_BTN_B), "Key: Space"); // the preset's own default

	// saving under the same name replaces; a different name adds
	m.setBinding(0, INPUT_BTN_A, key("F6"));
	CHECK(m.saveSetup("Keen", ""));
	CHECK(m.setups().size() == 1 && m.setups()[0].program.empty());
	CHECK(m.saveSetup("Doom", "DOOM.EXE"));
	CHECK(m.setups().size() == 2);
	m.applyPreset(0);
	CHECK(m.loadSetup(0));
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Key: F6");

	CHECK(!m.loadSetup(-1) && !m.loadSetup(2));
	m.setEmulated(0);
	m.applyPreset(1);
	m.resetBindings(0);
}

static void testHeldAndInstall()
{
	ControlsModel& m = fresh();
	for(int p = 0; p < 4; p++)
		CHECK(m.held(p) == 0);

	// an installed model replaces the sample, and null puts the sample back
	struct Other : ControlsModel
	{
		int playerCount() const override { return 1; }
		PlayerInfo player(int) const override { return PlayerInfo(); }
		DeviceStatus devices() const override { return DeviceStatus(); }
		uint32_t held(int) const override { return INPUT_BTN_A; }
		std::vector<EmulatedMode> emulatedModes() const override { return {}; }
		int currentEmulated() const override { return 0; }
		void setEmulated(int) override {}
		std::vector<PresetInfo> presets() const override { return {}; }
		int currentPreset() const override { return 0; }
		void applyPreset(int) override {}
		BindingTarget presetBinding(int, int) const override { return BindingTarget(); }
		bool modified() const override { return false; }
		bool supported(int) const override { return false; }
		BindingTarget binding(int, int) const override { return BindingTarget(); }
		void setBinding(int, int, const BindingTarget&) override {}
		void resetBindings(int) override {}
		BindingTarget defaultBinding(int) const override { return BindingTarget(); }
		std::vector<SetupInfo> setups() const override { return {}; }
		bool saveSetup(const std::string&, const std::string&) override { return false; }
		bool loadSetup(int) override { return false; }
		std::string runningProgram() const override { return ""; }
		std::vector<BindingTarget> suggestions(int, int) const override { return {}; }
		void beginCapture() override {}
		bool pollCapture(CaptureEvent&) override { return false; }
	} other;
	Controls_SetModel(&other);
	CHECK(&Controls() == &other);
	CHECK(Controls().held(0) == INPUT_BTN_A);
	CHECK(Controls().joystickButtons(0) == 0); // a model with no modes has no joysticks, and does not crash
	CHECK(Controls().usable(0, BindingTarget(TARGET::JOYSTICK, 0)) == false);
	Controls_SetModel(nullptr);
	CHECK(&Controls() != &other);
}

int main()
{
	testEmulatedModes();
	testUsable();
	testDescribe();
	testJoystickAxes();
	testSampleHeld();
	testSupported();
	testPresets();
	testPerPlayerBindings();
	testCatalogs();
	testMatchesButtonsCpp();
	testHidKeys();
	testKindsAndChoices();
	testSuggestions();
	testCapture();
	testPresetPreview();
	testModified();
	testSetups();
	testHeldAndInstall();
	printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
