// Host tests for the controls model contract and the sample model behind it.
#include "controlsmodel.h"
#include "libgui/GuiControllerLayout.h"
#include "controllerprofile.h"
#include "drivers/InputData.h"
#include "inputlogic.h"
#include <set>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string.h>

static int failures = 0, checks = 0;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)
#define CHECK_EQ(a, b) do { checks++; const std::string x = (a), y = (b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n      got  \"%s\"\n      want \"%s\"\n", __FILE__, __LINE__, #a, x.c_str(), y.c_str()); } } while(0)

// every player starts from the same preset (what a single global preset used to mean)
static void applyAll(ControlsModel& m, int preset)
{
	for(int p = 0; p < m.playerCount(); p++)
		m.applyPreset(p, preset);
}

static ControlsModel& fresh()
{
	// the sample model is a singleton: put it back to its starting state
	ControlsModel& m = Controls();
	m.setEmulated(0);
	applyAll(m, 1);
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
	applyAll(m, 0); // Compatible: A -> joystick button 1, X -> button 3
	m.setEmulated(0);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Joystick 1");
	CHECK_EQ(m.describe(0, INPUT_BTN_X), "Joystick 3");
	CHECK_EQ(m.describe(1, INPUT_BTN_A), "Joystick off"); // player 2 has none under "Joystick"

	m.setEmulated(1);
	CHECK_EQ(m.describe(0, INPUT_BTN_X), "No button 3");
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Joystick 1");

	m.setEmulated(2);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Joystick off");

	applyAll(m, 1);
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
			applyAll(m, (int)a);
			BindingTarget ta[4];
			for(int c = 0; c < 4; c++) ta[c] = m.binding(0, controls[c]);
			applyAll(m, (int)b);
			for(int c = 0; c < 4; c++) differs = differs || ta[c] != m.binding(0, controls[c]);
			if(!differs) { // Arrow keys and Menu keys share a D-pad; compare more than the face buttons
				applyAll(m, (int)a); const BindingTarget l = m.binding(0, INPUT_TRIGGER_L); applyAll(m, (int)b);
				differs = l != m.binding(0, INPUT_TRIGGER_L);
			}
			CHECK(differs);
		}
	}

	applyAll(m, 0);
	CHECK(m.currentPreset(0) == 0);
	CHECK(m.binding(0, INPUT_BTN_A) == BindingTarget(TARGET::JOYSTICK, 0));
	CHECK(m.binding(0, INPUT_BTN_UP).kind == TARGET::NONE); // the D-pad is the joystick's hat there
	applyAll(m, 3); // Point and click
	CHECK(m.binding(0, INPUT_BTN_A).kind == TARGET::MOUSE);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Mouse left");
	CHECK_EQ(m.describe(0, INPUT_BTN_B), "Mouse right");

	// applying a preset throws away edits, for every player
	m.setBinding(0, INPUT_BTN_A, key("F1"));
	m.setBinding(2, INPUT_BTN_B, key("F2"));
	applyAll(m, 3);
	CHECK(m.binding(0, INPUT_BTN_A).kind == TARGET::MOUSE && m.binding(2, INPUT_BTN_B).kind == TARGET::MOUSE);

	// bad indexes are ignored
	applyAll(m, 99);
	CHECK(m.currentPreset(0) == 3);
	applyAll(m, -1);
	CHECK(m.currentPreset(0) == 3);
	CHECK_EQ(m.player(0).preset, "Point and click");
	applyAll(m, 1);
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
		applyAll(m, preset);
		const int controls[] = { INPUT_BTN_A, INPUT_BTN_B, INPUT_BTN_X, INPUT_BTN_Y, INPUT_BTN_UP, INPUT_BTN_PLUS, INPUT_BTN_MINUS, INPUT_TRIGGER_L };
		for(size_t c = 0; c < sizeof(controls) / sizeof(controls[0]); c++)
		{
			CHECK(m.presetBinding(preset, controls[c]) == m.binding(0, controls[c]));
			CHECK(m.presetBinding(preset, controls[c]) == m.defaultBinding(0, controls[c]));
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
	CHECK(!m.modified(0) && !m.modified(1));
	m.setBinding(1, INPUT_BTN_A, key("F2"));
	CHECK(m.modified(1));
	CHECK(!m.modified(0));					// one player's change is theirs alone
	m.resetBindings(1);
	CHECK(!m.modified(1));
	m.setBinding(0, INPUT_BTN_B, key("F2"));
	applyAll(m, 2);							// applying a preset throws the change away
	CHECK(!m.modified(0));
	m.setEmulated(2);						// the emulated mode is not a button edit
	CHECK(!m.modified(0));
	m.setEmulated(0);
	applyAll(m, 1);
}

static void testPerPlayerPresets()
{
	ControlsModel& m = fresh();
	applyAll(m, 1);								// Arrow keys for everyone
	m.setBinding(0, INPUT_BTN_B, key("F2"));
	const BindingTarget p0b = m.binding(0, INPUT_BTN_B);
	m.applyPreset(1, 4);						// Menu keys for player 2 only
	CHECK(m.currentPreset(1) == 4);
	CHECK(m.currentPreset(0) == 1 && m.currentPreset(2) == 1);
	CHECK(m.modified(0));						// player 1's edit survives
	CHECK(m.binding(0, INPUT_BTN_B) == p0b);
	CHECK(m.binding(1, INPUT_BTN_A) == m.presetBinding(4, INPUT_BTN_A));
	CHECK(m.binding(0, INPUT_BTN_A) == m.presetBinding(1, INPUT_BTN_A));
	CHECK(m.defaultBinding(1, INPUT_BTN_A) == m.presetBinding(4, INPUT_BTN_A));
	CHECK(m.defaultBinding(0, INPUT_BTN_A) == m.presetBinding(1, INPUT_BTN_A));
	CHECK_EQ(m.player(1).preset, "Menu keys");
	CHECK_EQ(m.player(0).preset, "Arrow keys");
	m.setBinding(1, INPUT_BTN_A, key("F3"));
	m.applyPreset(1, 0);						// starting player 2 over leaves player 1 alone
	CHECK(!m.modified(1) && m.modified(0));
	m.applyPreset(7, 0);						// no such player: nothing happens
	m.applyPreset(-1, 0);
	CHECK(m.currentPreset(7) == 0);
	m.resetBindings(0);
	CHECK(!m.modified(0));
	applyAll(m, 1);
}

static int setupIndex(ControlsModel& m, const char * name)
{
	const std::vector<SetupInfo> v = m.setups();
	for(size_t i = 0; i < v.size(); i++)
		if(v[i].name == name)
			return (int)i;
	return -1;
}

// Last: it saves setups, and the sample model keeps what it is given.
static void testSetupState()
{
	ControlsModel& m = fresh();
	applyAll(m, 1);
	CHECK(m.saveSetup("Keen", "KEEN4E.EXE"));
	CHECK_EQ(m.setupName(), "Keen");
	CHECK(!m.setupModified());					// just saved
	m.setBinding(0, INPUT_BTN_A, key("F2"));
	CHECK(m.setupModified());					// changed since
	CHECK(m.saveSetup("Keen", "KEEN4E.EXE"));
	CHECK(!m.setupModified());
	m.setBinding(0, INPUT_BTN_A, key("F2"));
	CHECK(!m.setupModified());					// the same value again is not a change
	m.setEmulated(2);
	CHECK(m.setupModified());
	m.setEmulated(0);							// back where it was: still counts (it is not compared with the file)
	CHECK(m.setupModified());
	CHECK(m.loadSetup(setupIndex(m, "Keen")));
	CHECK(!m.setupModified());
	CHECK_EQ(m.setupName(), "Keen");
	CHECK_EQ(m.setups()[(size_t)setupIndex(m, "Keen")].preset, "Arrow keys");
	m.applyPreset(1, 3);
	CHECK(m.setupModified());
	m.saveSetup("Mix", "");
	CHECK_EQ(m.setups()[(size_t)setupIndex(m, "Mix")].preset, "Mixed");	// the players started from different presets
	applyAll(m, 1);
}

static void testSetupNames()
{
	// a name becomes a file name: nothing a file system rejects, no dots, no padding, a sane length
	CHECK_EQ(Setup_CleanName("Keen 4"), "Keen 4");
	CHECK_EQ(Setup_CleanName("  Keen 4  "), "Keen 4");
	CHECK_EQ(Setup_CleanName("a/b\\c:d*e?f\"g<h>i|j"), "abcdefghij");
	CHECK_EQ(Setup_CleanName("keen4e.exe"), "keen4eexe");
	CHECK_EQ(Setup_CleanName(""), "");
	CHECK_EQ(Setup_CleanName("   "), "");
	CHECK_EQ(Setup_CleanName("///"), "");
	CHECK_EQ(Setup_CleanName("abcdefghijklmnopqrstuvwxyz"), "abcdefghijklmnopqrstuvwx");		// 24 characters
	CHECK_EQ(Setup_CleanName("abcdefghijklmnopqrstuvw yz"), "abcdefghijklmnopqrstuvw");		// no trailing space left by the cut
	CHECK_EQ(Setup_CleanName("Caf\x01x"), "Cafx");												// control characters go

	// a program name: the file alone, upper case
	CHECK_EQ(Setup_CleanProgram("keen4e.exe"), "KEEN4E.EXE");
	CHECK_EQ(Setup_CleanProgram("c:\\games\\keen4\\keen4e.exe"), "KEEN4E.EXE");
	CHECK_EQ(Setup_CleanProgram("/games/doom/doom.exe"), "DOOM.EXE");
	CHECK_EQ(Setup_CleanProgram(" keen4e.exe "), "KEEN4E.EXE");
	CHECK_EQ(Setup_CleanProgram("c:"), "");
	CHECK_EQ(Setup_CleanProgram(""), "");
	CHECK_EQ(Setup_CleanProgram("a b.exe"), "AB.EXE");

	// the name offered when saving
	CHECK_EQ(Setup_SuggestedName("KEEN4E.EXE"), "KEEN4E");
	CHECK_EQ(Setup_SuggestedName("c:\\x\\doom.exe"), "DOOM");
	CHECK_EQ(Setup_SuggestedName(""), "My setup");
	CHECK_EQ(Setup_SuggestedName(".EXE"), "My setup");
	CHECK_EQ(Setup_SuggestedName("GAME"), "GAME");
}

static void testSetups()
{
	ControlsModel& m = fresh();
	CHECK(m.setups().empty());
	CHECK(!m.saveSetup("", "X.EXE"));	// a setup needs a name
	CHECK(!m.loadSetup(0));				// and there is nothing to load yet
	CHECK_EQ(m.runningProgram(), "KEEN4E.EXE");

	m.setEmulated(1);
	applyAll(m, 2);
	m.setBinding(0, INPUT_BTN_A, key("F5"));
	CHECK(m.saveSetup("Keen", m.runningProgram()));
	CHECK(m.setups().size() == 1);
	CHECK_EQ(m.setups()[0].name, "Keen");
	CHECK_EQ(m.setups()[0].program, "KEEN4E.EXE");
	CHECK_EQ(m.setups()[0].emulated, "Two joysticks");	// what it holds, for the list
	CHECK_EQ(m.setups()[0].preset, "Arrows + shooter");

	// change everything, load, and it all comes back
	m.setEmulated(0);
	applyAll(m, 4);
	CHECK(m.loadSetup(0));
	CHECK(m.currentEmulated() == 1);
	CHECK(m.currentPreset(0) == 2);
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Key: F5");
	CHECK_EQ(m.describe(0, INPUT_BTN_B), "Key: Space"); // the preset's own default

	// saving under the same name replaces; a different name adds
	m.setBinding(0, INPUT_BTN_A, key("F6"));
	CHECK(m.saveSetup("Keen", ""));
	CHECK(m.setups().size() == 1 && m.setups()[0].program.empty());
	CHECK(m.saveSetup("Doom", "DOOM.EXE"));
	CHECK(m.setups().size() == 2);
	applyAll(m, 0);
	CHECK(m.loadSetup(0));
	CHECK_EQ(m.describe(0, INPUT_BTN_A), "Key: F6");

	CHECK(!m.loadSetup(-1) && !m.loadSetup(2));
	m.setEmulated(0);
	applyAll(m, 1);
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
		int currentPreset(int) const override { return 0; }
		void applyPreset(int, int) override {}
		BindingTarget presetBinding(int, int) const override { return BindingTarget(); }
		bool modified(int) const override { return false; }
		bool supported(int) const override { return false; }
		BindingTarget binding(int, int) const override { return BindingTarget(); }
		void setBinding(int, int, const BindingTarget&) override {}
		void resetBindings(int) override {}
		BindingTarget defaultBinding(int, int) const override { return BindingTarget(); }
		std::vector<SetupInfo> setups() const override { return {}; }
		bool saveSetup(const std::string&, const std::string&) override { return false; }
		bool loadSetup(int) override { return false; }
		std::string runningProgram() const override { return ""; }
		std::string setupName() const override { return ""; }
		bool setupModified() const override { return false; }
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

// ---- profiles: one builder behind every kind of hardware

static std::vector<const ProfileRow *> profileRows(const ControllerProfile& p)
{
	std::vector<const ProfileRow *> rows;
	for(size_t i = 0; i < p.left.size(); i++) rows.push_back(&p.left[i]);
	for(size_t i = 0; i < p.right.size(); i++) rows.push_back(&p.right[i]);
	for(size_t g = 0; g < p.groups.size(); g++)
		for(size_t i = 0; i < p.groups[g].rows.size(); i++)
			rows.push_back(&p.groups[g].rows[i]);
	return rows;
}

static const ProfileRow * rowFor(const ControllerProfile& p, int control)
{
	const std::vector<const ProfileRow *> rows = profileRows(p);
	for(size_t i = 0; i < rows.size(); i++)
		if(rows[i]->control == control && !(rows[i]->binding & BIND_STICK_CLICK))
			return rows[i];
	return nullptr;
}

static bool hasClickRows(const ControllerProfile& p)
{
	const std::vector<const ProfileRow *> rows = profileRows(p);
	for(size_t i = 0; i < rows.size(); i++)
		if(rows[i]->binding & BIND_STICK_CLICK)
			return true;
	return false;
}

static void testProfilesAreConsistent()
{
	ControlsModel& m = fresh();
	const PadType kinds[] = { PadType::NONE, PadType::WIIMOTE, PadType::WIIMOTE_NUNCHUK, PadType::CLASSIC, PadType::WIIU_PRO, PadType::GAMECUBE, PadType::GAMEPAD };
	for(size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++)
	for(int variant = 0; variant < 4; variant++)
	{
		const PadType kind = kinds[k];
		ProfileOptions o;
		o.sideways = (variant & 1) != 0;
		o.stickClicks = (variant & 2) != 0;
		const ControllerProfile p = Controllers_Profile(kind, o);
		const std::vector<const ProfileRow *> rows = profileRows(p);

		CHECK(p.layout != nullptr && p.scale > 0.0f && p.name != nullptr);
		CHECK(!rows.empty());

		std::set<int> bindings;
		std::set<std::string> names;
		for(size_t i = 0; i < rows.size(); i++)
		{
			const ProfileRow& r = *rows[i];
			CHECK(r.name != nullptr && r.letter != nullptr && r.name[0] != 0);
			CHECK(strcmp(r.name, "Control") != 0);		// a control with no name in any table
			CHECK(strlen(r.letter) <= 2);
			CHECK(bindings.insert(r.binding).second);	// no two rows edit the same thing
			CHECK(names.insert(r.name).second);			// names are what the hint line and the chooser show

			if(r.fixed)
			{
				CHECK(r.binding == INPUT_BTN_HOME);		// only the menu button cannot change
				CHECK(r.control != 0 || kind == PadType::GAMECUBE);	// a pad with no Home on its drawing lights nothing
			}
			else if(r.stick)
			{
				CHECK(r.stick == 1 || r.stick == 2);
				CHECK(r.control == (r.stick == 1 ? (int)INPUT_THUMB_L : (int)INPUT_THUMB_R));
				CHECK(!m.supported(r.binding));			// movement is the joystick's axes, not a mapping
			}
			else
			{
				CHECK(m.supported(r.binding));			// something the engine can bind
				CHECK(r.control != 0);
			}
			if(r.control != 0)
				CHECK(p.layout->find(r.control) != nullptr);	// and the drawing has a control to light
		}

		// click rows only on hardware that has the click and a driver that reports it
		const bool clickHardware = kind == PadType::NONE || kind == PadType::WIIU_PRO || kind == PadType::GAMEPAD;
		CHECK(hasClickRows(p) == (clickHardware && o.stickClicks));

		// the list covers the drawing: every control it shows has a row
		for(size_t i = 0; i < p.layout->spots.size(); i++)
		{
			const int id = p.layout->spots[i].id;
			if(id == 0)
				continue;
			bool listed = false;
			for(size_t r = 0; r < rows.size(); r++)
				listed = listed || rows[r]->control == id;
			CHECK(listed);
		}
	}
}

static void testProfilesPerHardware()
{
	ProfileOptions up, side, clicks;
	side.sideways = true;
	clicks.stickClicks = true;

	// each kind has its own drawing; an empty slot gets the Pro's
	CHECK(Controllers_Profile(PadType::WIIU_PRO).layout == &GuiWiiUProControllerLayout());
	CHECK(Controllers_Profile(PadType::NONE).layout == &GuiWiiUProControllerLayout());
	CHECK(Controllers_Profile(PadType::GAMEPAD).layout == &GuiGamePadControllerLayout());
	CHECK(Controllers_Profile(PadType::CLASSIC).layout == &GuiClassicControllerLayout());
	CHECK(Controllers_Profile(PadType::GAMECUBE).layout == &GuiGameCubeControllerLayout());
	CHECK(Controllers_Profile(PadType::WIIMOTE).layout == &GuiWiimoteControllerLayout());
	CHECK(Controllers_Profile(PadType::WIIMOTE, side).layout == &GuiWiimoteSidewaysControllerLayout());
	CHECK(Controllers_Profile(PadType::WIIMOTE_NUNCHUK).layout == &GuiWiimoteNunchukControllerLayout());

	// orientation only matters to a lone Wiimote (a Nunchuk means it is held upright)
	CHECK(Controllers_Profile(PadType::WIIMOTE_NUNCHUK, side).layout == &GuiWiimoteNunchukControllerLayout());
	CHECK(Controllers_Profile(PadType::CLASSIC, side).layout == &GuiClassicControllerLayout());
	CHECK(profileRows(Controllers_Profile(PadType::GAMECUBE, side)).size() == profileRows(Controllers_Profile(PadType::GAMECUBE)).size());

	// the Pro and the GamePad list the same controls; the Classic is the Pro without the clicks it does not have
	CHECK(profileRows(Controllers_Profile(PadType::WIIU_PRO, clicks)).size() == profileRows(Controllers_Profile(PadType::GAMEPAD, clicks)).size());
	CHECK(profileRows(Controllers_Profile(PadType::WIIU_PRO, clicks)).size() == profileRows(Controllers_Profile(PadType::CLASSIC, clicks)).size() + 2);
	CHECK(profileRows(Controllers_Profile(PadType::WIIU_PRO, up)).size() == profileRows(Controllers_Profile(PadType::CLASSIC)).size());

	// Wiimote: 1 and 2 are drawn as themselves but bind through the engine's X and Y sources
	for(int sideways = 0; sideways < 2; sideways++)
	{
		ProfileOptions o;
		o.sideways = sideways != 0;
		const ControllerProfile w = Controllers_Profile(PadType::WIIMOTE, o);
		CHECK(rowFor(w, INPUT_BTN_1) && rowFor(w, INPUT_BTN_1)->binding == INPUT_BTN_X);
		CHECK(rowFor(w, INPUT_BTN_2) && rowFor(w, INPUT_BTN_2)->binding == INPUT_BTN_Y);
		CHECK(rowFor(w, INPUT_BTN_X) == nullptr && rowFor(w, INPUT_BTN_Y) == nullptr);	// no X or Y to clash with
		CHECK(rowFor(w, INPUT_THUMB_L) == nullptr);											// and no stick
		CHECK(rowFor(w, INPUT_BTN_A) && rowFor(w, INPUT_BTN_B) && rowFor(w, INPUT_BTN_PLUS) && rowFor(w, INPUT_BTN_MINUS));
		CHECK(rowFor(w, INPUT_BTN_UP) && rowFor(w, INPUT_BTN_DOWN) && rowFor(w, INPUT_BTN_LEFT) && rowFor(w, INPUT_BTN_RIGHT));
		CHECK_EQ(rowFor(w, INPUT_BTN_MINUS)->name, "Minus");
	}

	// Nunchuk: its C and Z arrive as L and ZL, its stick is the joystick's first
	const ControllerProfile n = Controllers_Profile(PadType::WIIMOTE_NUNCHUK);
	CHECK(rowFor(n, INPUT_TRIGGER_L) && rowFor(n, INPUT_TRIGGER_L)->binding == INPUT_TRIGGER_L);
	CHECK_EQ(rowFor(n, INPUT_TRIGGER_L)->name, "C Button");
	CHECK_EQ(rowFor(n, INPUT_TRIGGER_ZL)->name, "Z Button");
	CHECK(rowFor(n, INPUT_THUMB_L) && rowFor(n, INPUT_THUMB_L)->stick == 1);
	CHECK(rowFor(n, INPUT_THUMB_R) == nullptr);
	CHECK(rowFor(n, INPUT_BTN_1) && rowFor(n, INPUT_BTN_A));				// the Wiimote's own controls are there too
	CHECK(!hasClickRows(Controllers_Profile(PadType::WIIMOTE_NUNCHUK, clicks)));

	// GameCube: Start is Plus, Z is ZR, no Home but the combo, and no click on either stick
	const ControllerProfile gc = Controllers_Profile(PadType::GAMECUBE, clicks);
	CHECK_EQ(rowFor(gc, INPUT_BTN_PLUS)->name, "Start");
	CHECK_EQ(rowFor(gc, INPUT_TRIGGER_ZR)->name, "Z Button");
	CHECK(rowFor(gc, INPUT_BTN_MINUS) == nullptr && rowFor(gc, INPUT_TRIGGER_ZL) == nullptr);
	CHECK(!hasClickRows(gc));
	bool menu = false;
	const std::vector<const ProfileRow *> gcRows = profileRows(gc);
	for(size_t i = 0; i < gcRows.size(); i++)
		if(gcRows[i]->binding == INPUT_BTN_HOME)
		{
			menu = true;
			CHECK_EQ(gcRows[i]->name, "Menu");
			CHECK_EQ(gcRows[i]->fixed, "Hold L + R + Start");
		}
	CHECK(menu);

	// every other kind's Home opens the menu
	const PadType homes[] = { PadType::WIIMOTE, PadType::WIIMOTE_NUNCHUK, PadType::CLASSIC, PadType::WIIU_PRO, PadType::GAMEPAD };
	for(size_t i = 0; i < sizeof(homes) / sizeof(homes[0]); i++)
	{
		const ProfileRow * home = rowFor(Controllers_Profile(homes[i]), INPUT_BTN_HOME);
		CHECK(home != nullptr && home->fixed != nullptr);
		if(home && home->fixed)
			CHECK_EQ(home->fixed, "Opens this menu");
	}

	// Classic names its triggers the way the pad does
	CHECK_EQ(rowFor(Controllers_Profile(PadType::CLASSIC), INPUT_TRIGGER_L)->name, "L Trigger");
	CHECK_EQ(rowFor(Controllers_Profile(PadType::WIIU_PRO), INPUT_TRIGGER_L)->name, "Left Bumper");

	// the model's player drives the options
	PlayerInfo pi;
	pi.type = PadType::WIIMOTE;
	CHECK(Controllers_Profile(pi).layout == &GuiWiimoteControllerLayout());
	pi.sideways = true;
	CHECK(Controllers_Profile(pi).layout == &GuiWiimoteSidewaysControllerLayout());
	pi.type = PadType::WIIU_PRO;
	CHECK(!hasClickRows(Controllers_Profile(pi)));
	pi.stickClicks = true;
	CHECK(hasClickRows(Controllers_Profile(pi)));

	// the sample model reports the hardware a test sets, and goes back
	SampleControls_SetHardware(0, PadType::WIIMOTE, true, false);
	CHECK(Controls().player(0).type == PadType::WIIMOTE && Controls().player(0).sideways);
	SampleControls_SetHardware(3, PadType::WIIU_PRO, false, true);
	CHECK(Controls().player(3).connected && Controls().player(3).stickClicks);
	SampleControls_ResetHardware();
	CHECK(Controls().player(0).type == PadType::WIIMOTE_NUNCHUK && !Controls().player(0).sideways);
	CHECK(!Controls().player(3).connected);
}

// ---- the hardware seam: what the HAL reports becomes players

static InputPadData padWith(std::initializer_list<int> hw)
{
	InputPadData pad;
	for(int h : hw)
		pad.hw_connected[h] = true;
	return pad;
}

static void testPadTypeFromHal()
{
	CHECK(PadType_FromPad(InputPadData()) == PadType::NONE);
	CHECK(PadType_FromPad(padWith({ INPUT_HW_WIIMOTE })) == PadType::WIIMOTE);
	CHECK(PadType_FromPad(padWith({ INPUT_HW_WIIMOTE, INPUT_HW_NUNCHUK })) == PadType::WIIMOTE_NUNCHUK);
	CHECK(PadType_FromPad(padWith({ INPUT_HW_WIIMOTE, INPUT_HW_CLASSIC })) == PadType::CLASSIC);
	CHECK(PadType_FromPad(padWith({ INPUT_HW_WUPC })) == PadType::WIIU_PRO);
	CHECK(PadType_FromPad(padWith({ INPUT_HW_GAMECUBE })) == PadType::GAMECUBE);
	CHECK(PadType_FromPad(padWith({ INPUT_HW_DRC })) == PadType::GAMEPAD);
	// a controller held alongside the GamePad is the one drawn
	CHECK(PadType_FromPad(padWith({ INPUT_HW_DRC, INPUT_HW_WUPC })) == PadType::WIIU_PRO);
	CHECK(PadType_FromPad(padWith({ INPUT_HW_DRC, INPUT_HW_WIIMOTE })) == PadType::WIIMOTE);

	InputPadData pad = padWith({ INPUT_HW_WIIMOTE, INPUT_HW_CLASSIC });
	pad.hw_battery_level[INPUT_HW_WIIMOTE] = 55;
	pad.hw_battery_level[INPUT_HW_DRC] = 30;
	CHECK(PadBattery(pad, PadType::CLASSIC) == 55);	// rides on the Wiimote
	CHECK(PadBattery(pad, PadType::WIIMOTE_NUNCHUK) == 55);
	CHECK(PadBattery(pad, PadType::GAMEPAD) == 30);
	CHECK(PadBattery(pad, PadType::GAMECUBE) == -1);
	CHECK(PadBattery(pad, PadType::NONE) == -1);
}

static void testDpadForDos()
{
	const uint32_t dpad = INPUT_BTN_UP | INPUT_BTN_DOWN | INPUT_BTN_LEFT | INPUT_BTN_RIGHT;
	InputPadData wm = padWith({ INPUT_HW_WIIMOTE });
	wm.buttons_h = INPUT_BTN_UP | INPUT_BTN_A;

	CHECK(PadIsLoneWiimote(wm));
	CHECK(PadIsLoneWiimote(padWith({ INPUT_HW_WIIMOTE, INPUT_HW_NUNCHUK })));
	CHECK(!PadIsLoneWiimote(padWith({ INPUT_HW_WIIMOTE, INPUT_HW_CLASSIC })));
	CHECK(!PadIsLoneWiimote(padWith({ INPUT_HW_WIIMOTE, INPUT_HW_GAMECUBE })));

	CHECK(PadButtonsForDos(wm, false) == (INPUT_BTN_UP | INPUT_BTN_A));		// held upright: untouched
	CHECK(PadButtonsForDos(wm, true) == (INPUT_BTN_LEFT | INPUT_BTN_A));		// sideways: a quarter turn
	// all four directions turn the same way, and other buttons never change
	const uint32_t from[4] = { INPUT_BTN_UP, INPUT_BTN_DOWN, INPUT_BTN_LEFT, INPUT_BTN_RIGHT };
	const uint32_t to[4] = { INPUT_BTN_LEFT, INPUT_BTN_RIGHT, INPUT_BTN_DOWN, INPUT_BTN_UP };
	for(int i = 0; i < 4; i++)
	{
		wm.buttons_h = from[i] | INPUT_BTN_PLUS | INPUT_BTN_1;
		CHECK(PadButtonsForDos(wm, true) == (to[i] | INPUT_BTN_PLUS | INPUT_BTN_1));
	}
	wm.buttons_h = dpad;
	CHECK(PadButtonsForDos(wm, true) == dpad);									// a full cross turns into itself

	// anything else on the channel keeps its D-pad, whatever the Wiimote's flag says
	InputPadData both = padWith({ INPUT_HW_WIIMOTE, INPUT_HW_GAMECUBE });
	both.buttons_h = INPUT_BTN_UP;
	CHECK(PadButtonsForDos(both, true) == INPUT_BTN_UP);
}

class FakeHardware : public ControlsHardware
{
	public:
		PlayerHardware player(int i) const override
		{
			PlayerHardware h;
			if(i == 1)
			{
				h.connected = true;
				h.type = PadType::WIIMOTE;
				h.sideways = true;
				h.battery = 25;
			}
			if(i == 2)
			{
				h.connected = true;
				h.type = PadType::GAMEPAD;
				h.stickClicks = true;
			}
			return h;
		}
		DeviceStatus devices() const override
		{
			DeviceStatus d;
			d.keyboard = false;
			d.mouse = true;
			d.wiimoteIr = true;
			d.gamepadPresent = true;
			d.gamepadTouch = false;
			return d;
		}
		uint32_t held(int i) const override { return i == 1 ? INPUT_BTN_2 : 0; }
};

static void testHardwareSeam()
{
	FakeHardware fake;
	Controls_SetModel(nullptr);
	Controls_SetHardware(&fake);

	// the model reports what the hardware does, and adds only what DOS does with it
	CHECK(!Controls().player(0).connected && Controls().player(0).type == PadType::NONE);
	const PlayerInfo wm = Controls().player(1);
	CHECK(wm.connected && wm.type == PadType::WIIMOTE && wm.sideways && !wm.stickClicks);
	CHECK((wm.battery) == (25));
	CHECK_EQ(wm.device, "Wiimote");;
	CHECK(wm.emulated != Emulated::NONE && !wm.role.empty());
	const PlayerInfo gp = Controls().player(2);
	CHECK(gp.type == PadType::GAMEPAD && gp.stickClicks && !gp.sideways);
	CHECK(Controls().player(0).role.empty());

	CHECK(!Controls().devices().keyboard && Controls().devices().mouse && Controls().devices().wiimoteIr);
	CHECK(Controls().devices().gamepadPresent);
	CHECK((Controls().held(1)) == ((int)INPUT_BTN_2));
	CHECK((Controls().held(0)) == (0));

	// and the setup screen's profile follows the hardware, end to end
	CHECK(Controllers_Profile(wm).layout == &GuiWiimoteSidewaysControllerLayout());
	CHECK(hasClickRows(Controllers_Profile(gp)));

	// pulling it out goes back to the sample players
	Controls_SetHardware(nullptr);
	CHECK(Controls().player(0).type == PadType::WIIMOTE_NUNCHUK);
	CHECK(Controls().devices().keyboard);
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
	testPerPlayerPresets();
	testSetupNames();
	testSetups();
	testHeldAndInstall();
	testProfilesAreConsistent();
	testProfilesPerHardware();
	testPadTypeFromHal();
	testDpadForDos();
	testHardwareSeam();
	testSetupState();
	printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
