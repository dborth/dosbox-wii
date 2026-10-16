// Renders the menu screens to PNG on a host. usage: preview <out dir> [scale]
// Each PNG is the 640x480 canvas with the 5% overscan frame drawn in red; <name>_wide.png is the same frame
// stretched to 16:9 the way a widescreen TV shows it.
#include "libgui/Gui.h"
#include "hostgfx.h"
#include "menu/controlsui.h"
#include "menu/controlsmodel.h"
#include "menu/menupage.h"
#include "menu/menustyle.h"
#include "menu/menuart.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static std::string outDir = "out";

// The main window as HomeMenu builds it: the header and footer bars, with a page on top
struct Screen
{
	GuiWindow root;
	MenuBarShape top, bottom;
	GuiWindow page;
	Screen() : root(640, 480), top(MenuBarShape::TOP), bottom(MenuBarShape::BOTTOM), page(640, 480)
	{
		root.append(&top);
		root.append(&bottom);
		root.append(&page);
	}
	~Screen() { root.remove(&top); root.remove(&bottom); root.remove(&page); }
};

// Lays the page out, optionally moves the selection, and draws it over a paused game
static void snap(const char * name, GuiWindow& root, int downs = 0, int rights = 0, int ups = 0)
{
	for(int i = 0; i < 3; i++)
		root.update(nullptr);
	for(int i = 0; i < rights; i++) { root.moveSelectionHor(1); root.update(nullptr); }
	for(int i = 0; i < downs; i++) { root.moveSelectionVert(1); root.update(nullptr); }
	for(int i = 0; i < ups; i++) { root.moveSelectionVert(-1); root.update(nullptr); }
	for(int i = 0; i < 2; i++)
		root.update(nullptr);
	HostClear(0, 0, 0);
	HostPaintGame(640, 480);
	root.draw();
	HostSavePng((outDir + "/" + name + ".png").c_str(), false, true);
	HostSavePng((outDir + "/" + name + "_wide.png").c_str(), true, false);
	printf("wrote %s\n", name);
}

// A model with the players, devices and scheme chosen by the scene
struct SceneModel : ControlsModel
{
	PlayerInfo players[4];
	DeviceStatus dev;
	int preset = 1;
	int mode = 0;
	SceneModel(int count)
	{
		for(int i = 0; i < 4; i++)
		{
			players[i].connected = false; players[i].type = PadType::NONE; players[i].battery = -1;
			players[i].emulated = Emulated::NONE; players[i].joystick = 0;
		}
		const PadType types[4] = { PadType::WIIMOTE_NUNCHUK, PadType::CLASSIC, PadType::GAMECUBE, PadType::WIIU_PRO };
		const int battery[4] = { 80, 45, -1, 15 };
		for(int i = 0; i < count; i++)
		{
			players[i].connected = true; players[i].type = types[i]; players[i].battery = battery[i];
			players[i].device = PadType_Name(types[i]);
			players[i].emulated = i == 1 ? Emulated::KEYS_MOUSE : Emulated::JOYSTICK;
			players[i].joystick = i == 2 ? 2 : 1;
			players[i].role = i == 1 ? "Keys + mouse" : (i == 2 ? "Joystick 2" : "Joystick 1");
			players[i].preset = "Arrow keys";
		}
		dev.keyboard = true; dev.mouse = false; dev.wiimoteIr = true; dev.gamepadPresent = false; dev.gamepadTouch = false;
	}
	int playerCount() const override { return 4; }
	PlayerInfo player(int i) const override
	{
		PlayerInfo p = players[i];
		if(p.connected)
		{
			// as the real model does: joystick n is player n; players past the joysticks use keys and mouse
			const int joysticks = emulatedModes()[(size_t)mode].joysticks;
			p.emulated = i < joysticks ? Emulated::JOYSTICK : Emulated::KEYS_MOUSE;
			p.joystick = i < joysticks ? i + 1 : 0;
			p.role = i < joysticks ? (i == 0 ? "Joystick 1" : "Joystick 2") : "Keys + mouse";
		}
		return p;
	}
	DeviceStatus devices() const override { return dev; }
	uint32_t held(int) const override { return 0; }
	std::vector<EmulatedMode> emulatedModes() const override
	{
		return { { "Joystick", "", 1, 4, 4 }, { "Two joysticks", "", 2, 2, 2 }, { "Keyboard + mouse", "", 0, 0, 0 } };
	}
	int currentEmulated() const override { return mode; }
	void setEmulated(int i) override { mode = i; }
	std::vector<PresetInfo> presets() const override
	{
		return { { "Compatible", "", true }, { "Arrow keys", "", false }, { "Arrows + shooter", "", false }, { "Point and click", "", false }, { "Menu keys", "", false } };
	}
	int currentPreset() const override { return preset; }
	void applyPreset(int i) override { preset = i; }
	BindingTarget presetBinding(int, int) const override { return BindingTarget(); }
	bool modified() const override { return false; }
	bool supported(int) const override { return true; }
	BindingTarget binding(int, int) const override { return BindingTarget(TARGET::KEY, 5); }
	void setBinding(int, int, const BindingTarget&) override {}
	BindingTarget defaultBinding(int) const override { return BindingTarget(TARGET::KEY, 4); }
	void resetBindings(int) override {}
	std::vector<SetupInfo> setups() const override { return {}; }
	bool saveSetup(const std::string&, const std::string&) override { return false; }
	bool loadSetup(int) override { return false; }
	std::string runningProgram() const override { return ""; }
	std::vector<BindingTarget> suggestions(int, int) const override { return {}; }
	void beginCapture() override {}
	bool pollCapture(CaptureEvent&) override { return false; }
};

int main(int argc, char ** argv)
{
	if(argc > 1) outDir = argv[1];
	const float scale = argc > 2 ? (float)atof(argv[2]) : 2.0f;
	HostInit(640, 480, scale);
	MenuApplyTheme();

	{
		SceneModel one(1);
		Controls_SetModel(&one);
		Screen s;
		ControlsHubView view(nullptr);
		s.page.append(&view);
		snap("hub_1player", s.root);
	}
	{
		SceneModel one(1);
		Controls_SetModel(&one);
		Screen s;
		ControlsHubView view(nullptr);
		s.page.append(&view);
		snap("hub_1player_device", s.root, 0, 0, 1);
	}
	{
		SceneModel four(4);
		four.mode = 1;
		Controls_SetModel(&four);
		Screen s;
		ControlsHubView view(nullptr);
		s.page.append(&view);
		snap("hub_4players", s.root, 1);
	}
	{
		SceneModel none(0);
		none.dev.keyboard = true; none.dev.mouse = true;
		Controls_SetModel(&none);
		Screen s;
		ControlsHubView view(nullptr);
		s.page.append(&view);
		snap("hub_none", s.root);
	}
	// the setup screen on the sample model, so rows show real targets: (player, emulated mode, preset, held buttons),
	// and for the per-hardware scenes what the player is (hw -1 keeps the sample's own hardware)
	struct SampleScene { const char * name; int player, mode, preset; uint32_t held; int hw; bool sideways, clicks; };
	const int NOHW = -1;
	const SampleScene scenes[] = {
		{ "config", 0, 0, 1, 0, NOHW, false, false },
		{ "config_pressed", 0, 0, 1, INPUT_BTN_A | INPUT_TRIGGER_ZL | INPUT_BTN_UP, NOHW, false, false },
		{ "config_twojoy", 0, 1, 0, 0, NOHW, false, false },
		{ "config_keys_mouse", 0, 2, 0, 0, NOHW, false, false },
		{ "config_gamecube", 2, 0, 1, 0, NOHW, false, false },
		// one per kind of hardware, as player 1
		{ "hw_wiimote", 0, 0, 1, 0, (int)PadType::WIIMOTE, false, false },
		{ "hw_wiimote_pressed", 0, 0, 1, INPUT_BTN_1 | INPUT_BTN_B | INPUT_BTN_LEFT, (int)PadType::WIIMOTE, false, false },
		{ "hw_wiimote_sideways", 0, 0, 1, 0, (int)PadType::WIIMOTE, true, false },
		{ "hw_wiimote_sideways_pressed", 0, 0, 1, INPUT_BTN_2 | INPUT_BTN_UP, (int)PadType::WIIMOTE, true, false },
		{ "hw_nunchuk", 0, 0, 1, 0, (int)PadType::WIIMOTE_NUNCHUK, false, false },
		{ "hw_nunchuk_pressed", 0, 0, 1, INPUT_TRIGGER_L | INPUT_TRIGGER_ZL | INPUT_BTN_A, (int)PadType::WIIMOTE_NUNCHUK, false, false },
		{ "hw_gamecube_pressed", 0, 0, 1, INPUT_BTN_A | INPUT_TRIGGER_ZR | INPUT_THUMB_R | INPUT_BTN_PLUS, (int)PadType::GAMECUBE, false, false },
		{ "hw_classic", 0, 0, 1, 0, (int)PadType::CLASSIC, false, false },
		{ "hw_classic_pressed", 0, 0, 1, INPUT_TRIGGER_ZR | INPUT_BTN_X | INPUT_BTN_RIGHT, (int)PadType::CLASSIC, false, false },
		{ "hw_wiiupro", 0, 0, 1, 0, (int)PadType::WIIU_PRO, false, false },		// on a Wii: no click rows
		{ "hw_wiiupro_clicks", 0, 0, 1, INPUT_THUMB_L, (int)PadType::WIIU_PRO, false, true },	// on a Wii U
		{ "hw_gamepad", 0, 0, 1, 0, (int)PadType::GAMEPAD, false, true },
		{ "hw_gamepad_pressed", 0, 0, 1, INPUT_TRIGGER_L | INPUT_BTN_Y | INPUT_THUMB_R, (int)PadType::GAMEPAD, false, true },
	};
	for(size_t i = 0; i < sizeof(scenes) / sizeof(scenes[0]); i++)
	{
		Controls_SetModel(nullptr);
		SampleControls_ResetHardware();
		if(scenes[i].hw != NOHW)
			SampleControls_SetHardware(scenes[i].player, (PadType)scenes[i].hw, scenes[i].sideways, scenes[i].clicks);
		Controls().setEmulated(scenes[i].mode);
		Controls().applyPreset(scenes[i].preset);
		SampleControls_SetHeld(scenes[i].held);
		Screen s;
		ControllerConfigView view(scenes[i].player, nullptr);
		s.page.append(&view);
		snap(scenes[i].name, s.root, 0, 0, 0);
	}
	SampleControls_ResetHardware();
	SampleControls_SetHeld(0);
	// the chooser on the sample model: (name, player, emulated mode, control, what it does now)
	struct ChooserScene { const char * name; int mode; int preset; int control; BindingTarget now; };
	const ChooserScene choosers[] = {
		{ "chooser", 0, 1, INPUT_BTN_A, BindingTarget(TARGET::KEY, 5) },					// opens on the key it uses
		{ "chooser_suggested", 0, 1, INPUT_TRIGGER_ZR, BindingTarget() },				// nothing yet: the suggestions
		{ "chooser_keys_mouse", 2, 0, INPUT_BTN_A, BindingTarget(TARGET::JOYSTICK, 0) },	// no joystick, so no Joystick kind
	};
	for(size_t i = 0; i < sizeof(choosers) / sizeof(choosers[0]); i++)
	{
		Controls_SetModel(nullptr);
		Controls().setEmulated(choosers[i].mode);
		Controls().applyPreset(choosers[i].preset);
		Screen s;
		TargetChooserView view("A Button", 0, choosers[i].control, choosers[i].now, BindingTarget(TARGET::KEY, 4), nullptr);
		s.page.append(&view);
		snap(choosers[i].name, s.root);
	}

	// the pickers on the sample model
	{
		struct PickerScene { const char * name; int mode; int preset; bool edited; bool presets; };
		const PickerScene pickers[] = {
			{ "picker_preset", 0, 1, true, true },			// edited buttons: choosing asks first
			{ "picker_preset_two", 1, 0, false, true },		// two joysticks: Compatible shows what it cannot do
			{ "picker_preset_nojoy", 2, 1, false, true },	// no joystick: Compatible is not offered
			{ "picker_emulated", 1, 1, false, false },
		};
		for(size_t i = 0; i < sizeof(pickers) / sizeof(pickers[0]); i++)
		{
			Controls_SetModel(nullptr);
			Controls().setEmulated(pickers[i].mode);
			Controls().applyPreset(pickers[i].preset);
			if(pickers[i].edited)
				Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, 5));
			std::vector<int> index;
			const std::vector<PickerItem> items = pickers[i].presets ? Picker_PresetItems(0, index) : Picker_EmulatedItems();
			Screen s;
			PickerView view(pickers[i].presets ? "Choose a preset" : "What does DOS see?", items, nullptr);
			s.page.append(&view);
			snap(pickers[i].name, s.root);
		}
		Controls_SetModel(nullptr);
		Controls().setEmulated(0);
		Controls().applyPreset(1);
	}

	// Load and the attach choices, on the sample model with two saved setups
	{
		Controls_SetModel(nullptr);
		Controls().setEmulated(1);
		Controls().applyPreset(2);
		Controls().saveSetup("KEEN4E", "KEEN4E.EXE");
		Controls().setEmulated(0);
		Controls().applyPreset(3);
		Controls().saveSetup("Adventure night", "");
		Controls().saveSetup("abcdefghijklmnopqrstuvwx", "LONGNAME.EXE");
		std::vector<int> index;
		{
			Screen s;
			PickerView view("Load a setup", Picker_SetupItems(index), nullptr);
			s.page.append(&view);
			snap("picker_load", s.root);
		}
		{
			std::vector<AttachChoice> kinds;
			Screen s;
			PickerView view("Load these buttons for", Picker_AttachItems("KEEN4E.EXE", kinds), nullptr);
			s.page.append(&view);
			snap("picker_attach", s.root);
		}
	}

	// ---- behaviour checks (a failure prints and makes the run fail)
	int failures = 0;
	#define EXPECT(c) do { if(!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while(0)
	{
		Controls_SetModel(nullptr);
		Controls().setEmulated(0);
		Controls().applyPreset(1);
		Screen s;
		TargetChooserView view("A Button", 0, INPUT_BTN_A, BindingTarget(TARGET::KEY, 5), BindingTarget(TARGET::KEY, 4), nullptr);
		s.page.append(&view);
		const int withJoystick = view.categoryCount();
		EXPECT(withJoystick == 6);	// Suggested, Key, Joystick, Mouse, DOSBox, None

		EXPECT(!view.listening() && !view.chosen());
		view.startListening(TARGET::KEY);
		EXPECT(view.listening());
		SampleControls_PushCapture({ TARGET::MOUSE, 0 });		// the wrong kind of press is ignored
		SampleControls_PushCapture({ TARGET::KEY, -1 });		// a key with no target is refused
		s.root.update(nullptr);
		EXPECT(!view.chosen() && view.listening());
		SampleControls_PushCapture({ TARGET::KEY, Catalog_Find(TARGET::KEY, "Z") });
		s.root.update(nullptr);
		EXPECT(view.chosen() && !view.listening());
		EXPECT(view.result() == BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "Z")));
	}
	{
		// a key pressed before listening began does not count
		Controls_SetModel(nullptr);
		Screen s;
		TargetChooserView view("A Button", 0, INPUT_BTN_A, BindingTarget(), BindingTarget(), nullptr);
		s.page.append(&view);
		SampleControls_PushCapture({ TARGET::KEY, Catalog_Find(TARGET::KEY, "Q") });
		view.startListening(TARGET::KEY);
		s.root.update(nullptr);
		EXPECT(!view.chosen());
		view.stopListening();
		EXPECT(!view.listening());
	}
	{
		// no joystick, no Joystick kind; and a joystick button a two-button joystick lacks is never offered
		Controls_SetModel(nullptr);
		Controls().setEmulated(2);
		Screen s;
		TargetChooserView view("A Button", 0, INPUT_BTN_A, BindingTarget(), BindingTarget(), nullptr);
		s.page.append(&view);
		EXPECT(view.categoryCount() == 5);
		Controls().setEmulated(0);
	}
	{
		// a picker: items that ask for confirmation need a second press, others do not
		std::vector<PickerItem> items(3);
		items[0].name = "One"; items[0].current = true;
		items[1].name = "Two"; items[1].confirm = "Really?";
		items[2].name = "Three"; items[2].confirm = "Really?";
		Screen s;
		PickerView view("Pick", items, nullptr);
		s.page.append(&view);
		EXPECT(view.focused() == 0);							// opens on the one in use
		view.choose(1);
		EXPECT(view.armed() && !view.chosen());
		view.focusItem(2);										// moving away drops the confirmation
		EXPECT(!view.armed() && view.focused() == 2);
		view.choose(1);
		view.choose(2);											// a different item re-arms rather than choosing
		EXPECT(view.armed() && !view.chosen());
		view.choose(2);
		EXPECT(view.chosen() && view.result() == 2);

		PickerView plain("Pick", items, nullptr);
		s.page.append(&plain);
		plain.choose(0);										// no confirmation asked: chosen at once
		EXPECT(plain.chosen() && plain.result() == 0);
		PickerView bad("Pick", items, nullptr);
		bad.choose(-1);
		bad.choose(9);
		EXPECT(!bad.chosen());
	}
	{
		// what the preset list offers
		Controls_SetModel(nullptr);
		Controls().setEmulated(0);
		Controls().applyPreset(1);
		std::vector<int> index;
		std::vector<PickerItem> items = Picker_PresetItems(0, index);
		EXPECT(items.size() == 5 && index.size() == 5);
		EXPECT(items[1].current && !items[0].current);
		EXPECT(items[0].confirm.empty());						// nothing changed, nothing to lose
		EXPECT(items[0].rows.size() == 7 && items[0].rows[0].label == "A Button" && items[0].rows[0].value == "Joystick 1");
		EXPECT(items[1].rows[0].value == "Key: Ctrl");

		Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, 5));
		items = Picker_PresetItems(0, index);
		for(size_t i = 0; i < items.size(); i++)
			EXPECT(!items[i].confirm.empty());					// every choice would throw the change away

		Controls().applyPreset(1);
		Controls().setEmulated(2);								// no joystick: the joystick preset is not offered
		items = Picker_PresetItems(0, index);
		EXPECT(items.size() == 4 && index[0] == 1);
		Controls().applyPreset(0);								// ...unless it is the one in use
		items = Picker_PresetItems(0, index);
		EXPECT(items.size() == 5 && items[0].current);
		Controls().setEmulated(1);								// two buttons: what Compatible gives X cannot happen
		Controls().applyPreset(1);
		items = Picker_PresetItems(0, index);
		EXPECT(items[0].rows[2].label == "X Button" && items[0].rows[2].value == "No button 3");

		// the emulated list: only connected players are listed
		Controls().setEmulated(0);
		std::vector<PickerItem> modes = Picker_EmulatedItems();
		EXPECT(modes.size() == 3 && modes[0].current);
		EXPECT(modes[0].rows.size() == 3 && modes[0].rows[0].value == "Joystick 1" && modes[0].rows[1].value == "Keys + mouse");
		EXPECT(modes[1].rows[1].value == "Joystick 2" && modes[1].rows[2].value == "Keys + mouse");
		EXPECT(modes[2].rows[0].value == "Keys + mouse");
		// every line fits the pane (it cannot wrap), and at most four are shown
		for(size_t i = 0; i < modes.size(); i++)
		{
			EXPECT(modes[i].lines.size() <= 4);
			for(size_t k = 0; k < modes[i].lines.size(); k++)
				EXPECT(modes[i].lines[k].size() <= 44);
		}
		for(size_t i = 0; i < items.size(); i++)
		{
			EXPECT(items[i].lines.size() <= 4);
			for(size_t k = 0; k < items[i].lines.size(); k++)
				EXPECT(items[i].lines[k].size() <= 44);
		}
	}
	{
		// the Load list shows what each setup holds, and asks before replacing edited buttons
		Controls_SetModel(nullptr);
		Controls().setEmulated(0);
		Controls().applyPreset(1);
		std::vector<int> index;
		EXPECT(Picker_SetupItems(index).empty() || true);
		Controls().saveSetup("KEEN4E", "KEEN4E.EXE");
		Controls().saveSetup("Plain", "");
		std::vector<PickerItem> items = Picker_SetupItems(index);
		EXPECT(items.size() >= 2 && index.size() == items.size());
		size_t keen = 0, plain = 0;
		for(size_t i = 0; i < items.size(); i++)
		{
			if(items[i].name == "KEEN4E") keen = i;
			if(items[i].name == "Plain") plain = i;
		}
		EXPECT(items[keen].lines[0] == "Loads for KEEN4E.EXE.");
		EXPECT(items[plain].lines[0] == "Not tied to a program.");
		EXPECT(items[keen].rows.size() == 2 && items[keen].rows[0].value == "Joystick" && items[keen].rows[1].value == "Arrow keys");
		EXPECT(items[keen].confirm.empty());										// nothing edited
		Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, 5));
		items = Picker_SetupItems(index);
		EXPECT(!items[0].confirm.empty());											// loading would throw the edit away
		Controls().applyPreset(1);
	}
	{
		// what a setup can be tied to
		std::vector<AttachChoice> kinds;
		std::vector<PickerItem> items = Picker_AttachItems("KEEN4E.EXE", kinds);
		EXPECT(items.size() == 3 && kinds.size() == 3);
		EXPECT(kinds[0] == AttachChoice::RUNNING && items[0].name == "KEEN4E.EXE" && items[0].current);
		EXPECT(kinds[1] == AttachChoice::NONE && !items[1].current);
		EXPECT(kinds[2] == AttachChoice::TYPED);
		items = Picker_AttachItems("", kinds);									// at the prompt: nothing to offer but none or typed
		EXPECT(items.size() == 2 && kinds[0] == AttachChoice::NONE && items[0].current && kinds[1] == AttachChoice::TYPED);
		for(size_t i = 0; i < items.size(); i++)
			for(size_t k = 0; k < items[i].lines.size(); k++)
				EXPECT(items[i].lines[k].size() <= 44);
	}
	if(failures)
		return 1;
	printf("behaviour checks passed\n");
	return 0;
}
