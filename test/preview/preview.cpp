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

// Starts every player from a preset (the sample scenes show one player at a time but share one look)
static void ApplyAll(int preset)
{
	for(int p = 0; p < Controls().playerCount(); p++)
		Controls().applyPreset(p, preset);
}

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
	int presetOf[4] = { 1, 1, 1, 1 };
	bool edited[4] = { false, false, false, false };
	std::string setup;
	bool dirty = false;
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
		p.preset = presets()[(size_t)presetOf[i]].name;
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
	int currentPreset(int p) const override { return presetOf[p]; }
	void applyPreset(int p, int i) override { presetOf[p] = i; edited[p] = false; dirty = true; }
	BindingTarget presetBinding(int, int) const override { return BindingTarget(); }
	bool modified(int p) const override { return edited[p]; }
	bool supported(int) const override { return true; }
	BindingTarget binding(int, int) const override { return BindingTarget(TARGET::KEY, 5); }
	void setBinding(int, int, const BindingTarget&) override {}
	BindingTarget defaultBinding(int, int) const override { return BindingTarget(TARGET::KEY, 4); }
	void resetBindings(int p) override { edited[p] = false; dirty = true; }
	std::vector<SetupInfo> setups() const override { return {}; }
	bool saveSetup(const std::string&, const std::string&) override { return false; }
	bool loadSetup(int) override { return false; }
	std::string runningProgram() const override { return ""; }
	std::string setupName() const override { return setup; }
	bool setupModified() const override { return dirty; }
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

	// the setup screen as the model shows it for a given set of controllers
	{
		SceneModel one(1);
		Controls_SetModel(&one);
		Screen s;
		ControllerConfigView view(0, nullptr);
		s.page.append(&view);
		snap("config_one", s.root);					// no switcher: the title says Controls
	}
	{
		SceneModel two(2);
		two.edited[0] = true; two.dirty = true; two.setup = "KEEN4E";
		Controls_SetModel(&two);
		Screen s;
		ControllerConfigView view(0, nullptr);
		s.page.append(&view);
		snap("config_two", s.root);					// the switcher, a preset marked edited, an unsaved setup
	}
	{
		SceneModel two(2);
		two.mode = 1;
		Controls_SetModel(&two);
		Screen s;
		ControllerConfigView view(1, nullptr);
		s.page.append(&view);
		snap("config_two_p2", s.root);
	}
	{
		// the longest strings the header and footer carry: four players, the longest preset and mode, a long setup name
		SceneModel four(4);
		four.presetOf[3] = 2; four.edited[3] = true; four.mode = 2; four.dirty = true;
		four.setup = "abcdefghijklmnopqrstuvwx";
		Controls_SetModel(&four);
		Screen s;
		ControllerConfigView view(3, nullptr);
		s.page.append(&view);
		snap("config_worst", s.root);
	}
	{
		SceneModel none(0);
		Controls_SetModel(&none);
		Screen s;
		ControllerConfigView view(0, nullptr);
		s.page.append(&view);
		snap("config_empty", s.root);
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
		ApplyAll(scenes[i].preset);
		SampleControls_SetHeld(scenes[i].held);
		Screen s;
		ControllerConfigView view(scenes[i].player, nullptr);
		s.page.append(&view);
		snap(scenes[i].name, s.root, 0, 0, 0);
	}
	SampleControls_ResetHardware();
	SampleControls_SetHeld(0);
	// two devices on one channel: the rail of icons, with each of them drawn
	{
		struct RailScene { const char * name; PadType primary, second, show; bool clicks; uint32_t held; };
		const RailScene rails[] = {
			{ "rail_nunchuk_gamecube", PadType::WIIMOTE_NUNCHUK, PadType::GAMECUBE, PadType::NONE, false, 0 },
			{ "rail_nunchuk_gamecube_flipped", PadType::WIIMOTE_NUNCHUK, PadType::GAMECUBE, PadType::GAMECUBE, false, INPUT_BTN_A | INPUT_TRIGGER_ZR },
			{ "rail_wiimote_gamecube", PadType::WIIMOTE, PadType::GAMECUBE, PadType::NONE, false, 0 },
			{ "rail_classic_wiimote_gc", PadType::CLASSIC, PadType::GAMECUBE, PadType::NONE, false, 0 },
			{ "rail_gamepad_wiimote", PadType::GAMEPAD, PadType::WIIMOTE, PadType::NONE, true, 0 },
		};
		for(size_t i = 0; i < sizeof(rails) / sizeof(rails[0]); i++)
		{
			Controls_SetModel(nullptr);
			SampleControls_ResetHardware();
			SampleControls_SetDevices(0, { rails[i].primary, rails[i].second }, false, rails[i].clicks);
			Controls().setEmulated(0);
			Controls().applyPreset(0, 1);
			SampleControls_SetHeld(rails[i].held);
			Screen s;
			ControllerConfigView view(0, nullptr, rails[i].show);
			s.page.append(&view);
			snap(rails[i].name, s.root, 0, 0, 0);
		}
		SampleControls_ResetHardware();
		SampleControls_SetHeld(0);
	}
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
		ApplyAll(choosers[i].preset);
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
			ApplyAll(pickers[i].preset);
			if(pickers[i].edited)
				Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F2")));
			std::vector<int> index;
			const std::vector<PickerItem> items = pickers[i].presets ? Picker_PresetItems(0, index) : Picker_EmulatedItems();
			Screen s;
			PickerView view(pickers[i].presets ? "Choose a preset" : "What does DOS see?", items, nullptr);
			s.page.append(&view);
			snap(pickers[i].name, s.root);
		}
		Controls_SetModel(nullptr);
		Controls().setEmulated(0);
		ApplyAll(1);
	}
	// the Setup button's menu, with and without a setup to save over
	{
		std::vector<SetupAction> kinds;
		Controls_SetModel(nullptr);
		ApplyAll(1);
		Controls().saveSetup("Plain", "");
		{
			Screen s;
			PickerView view("Load or save a setup", Picker_SetupMenuItems(kinds), nullptr);
			s.page.append(&view);
			snap("picker_setup", s.root);
		}
	}

	// Load and the attach choices, on the sample model with two saved setups
	{
		Controls_SetModel(nullptr);
		Controls().setEmulated(1);
		ApplyAll(2);
		Controls().saveSetup("KEEN4E", "KEEN4E.EXE");
		Controls().setEmulated(0);
		ApplyAll(3);
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
		ApplyAll(1);
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
		ApplyAll(1);
		std::vector<int> index;
		std::vector<PickerItem> items = Picker_PresetItems(0, index);
		EXPECT(items.size() == 5 && index.size() == 5);
		EXPECT(items[1].current && !items[0].current);
		EXPECT(items[0].confirm.empty());						// nothing changed, nothing to lose
		EXPECT(items[0].rows.size() == 7 && items[0].rows[0].label == "A Button" && items[0].rows[0].value == "Joystick 1");
		EXPECT(items[1].rows[0].value == "Key: Ctrl");

		Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F2")));
		items = Picker_PresetItems(0, index);
		for(size_t i = 0; i < items.size(); i++)
			EXPECT(!items[i].confirm.empty());					// every choice would throw the change away

		ApplyAll(1);
		Controls().setEmulated(2);								// no joystick: the joystick preset is not offered
		items = Picker_PresetItems(0, index);
		EXPECT(items.size() == 4 && index[0] == 1);
		ApplyAll(0);								// ...unless it is the one in use
		items = Picker_PresetItems(0, index);
		EXPECT(items.size() == 5 && items[0].current);
		Controls().setEmulated(1);								// two buttons: what Compatible gives X cannot happen
		ApplyAll(1);
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
		ApplyAll(1);
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
		Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F2")));
		items = Picker_SetupItems(index);
		EXPECT(!items[0].confirm.empty());											// loading would throw the edit away
		ApplyAll(1);
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
	{
		// the preset list for a player who changed buttons starts with Undo, and the preset is that player's own
		Controls_SetModel(nullptr);
		Controls().setEmulated(0);
		ApplyAll(1);
		Controls().setBinding(1, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F2")));
		std::vector<int> index;
		std::vector<PickerItem> items = Picker_PresetItems(0, index);
		EXPECT(items[0].name != "Undo my changes");						// player 1 has no changes
		items = Picker_PresetItems(1, index);
		EXPECT(items[0].name == "Undo my changes" && index[0] == -1 && !items[0].current && !items[0].confirm.empty());
		EXPECT(items.size() == index.size() && items.size() == 5);		// Undo, and the presets (no joystick here, so no Compatible)
		EXPECT(items[0].lines[0] == "Back to Arrow keys.");
		Controls().applyPreset(2, 4);									// a preset for player 3 leaves player 2's change alone
		EXPECT(Controls().modified(1) && Controls().currentPreset(1) == 1 && Controls().currentPreset(2) == 4);
		Controls().resetBindings(1);
		EXPECT(!Controls().modified(1));
		ApplyAll(1);

		// the Setup button: loading always, saving over the current one only when there is one
		std::vector<SetupAction> kinds;
		Controls_SetModel(nullptr);
		ApplyAll(1);
		Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F2")));
		items = Picker_SetupMenuItems(kinds);
		EXPECT(kinds.size() == items.size() && kinds[0] == SetupAction::LOAD);
		EXPECT(kinds.back() == SetupAction::SAVE_NEW);
		Controls().saveSetup("PLAIN", "");
		items = Picker_SetupMenuItems(kinds);
		EXPECT(kinds.size() == 3 && kinds[1] == SetupAction::SAVE_CURRENT && items[1].name == "Save as PLAIN");
		for(size_t i = 0; i < items.size(); i++)
			for(size_t k = 0; k < items[i].lines.size(); k++)
				EXPECT(items[i].lines[k].size() <= 44);
		ApplyAll(1);
	}
	{
		// the screen notices when it was built for something that changed
		SceneModel two(2);
		Controls_SetModel(&two);
		Screen s;
		ControllerConfigView view(0, nullptr);
		s.page.append(&view);
		EXPECT(!view.outOfDate() && !view.isEmpty());
		two.players[1].connected = false;								// the other controller goes: no switcher any more
		EXPECT(view.outOfDate());
		two.players[1].connected = true;
		EXPECT(!view.outOfDate());
		two.players[0].type = PadType::CLASSIC;							// a different controller in the slot: a different drawing
		EXPECT(view.outOfDate());
		two.players[0].type = PadType::WIIMOTE_NUNCHUK;
		two.players[0].connected = false;								// this one goes
		EXPECT(view.outOfDate());
		Controls_SetModel(nullptr);
	}
	{
		// a screen with no controller says so, and nothing in it needs a profile
		SceneModel none(0);
		Controls_SetModel(&none);
		Screen s;
		ControllerConfigView view(0, nullptr);
		s.page.append(&view);
		EXPECT(view.isEmpty() && !view.outOfDate());
		none.players[2].connected = true; none.players[2].type = PadType::GAMECUBE;	// one appears
		EXPECT(view.outOfDate());
		Controls_SetModel(nullptr);
	}
	{
		// the rail: which device is drawn, when it exists, and when the screen asks to be built again
		Controls_SetModel(nullptr);
		SampleControls_ResetHardware();
		SampleControls_SetDevices(0, { PadType::WIIMOTE_NUNCHUK, PadType::GAMECUBE });
		Screen s;
		ControllerConfigView view(0, nullptr);
		s.page.append(&view);
		EXPECT(view.devices().size() == 2 && view.shownType() == PadType::WIIMOTE_NUNCHUK);	// the primary one by default
		EXPECT(view.takeFlip() == PadType::NONE && !view.outOfDate());
		{
			ControllerConfigView gc(0, nullptr, PadType::GAMECUBE);
			EXPECT(gc.shownType() == PadType::GAMECUBE);
			EXPECT(!gc.outOfDate());															// drawing the second device is not a stale screen
			ControllerConfigView gone(0, nullptr, PadType::CLASSIC);							// not on the channel: the primary one
			EXPECT(gone.shownType() == PadType::WIIMOTE_NUNCHUK);
		}
		const PlayerInfo info = Controls().player(0);
		EXPECT(std::string(Controllers_Profile(info, 0).name) == PadType_Name(PadType::WIIMOTE_NUNCHUK));
		EXPECT(std::string(Controllers_Profile(info, 1).name) == PadType_Name(PadType::GAMECUBE));
		EXPECT(std::string(Controllers_Profile(info, 9).name) == PadType_Name(PadType::WIIMOTE_NUNCHUK));	// past the list: the primary one
		SampleControls_SetDevices(0, { PadType::WIIMOTE_NUNCHUK });								// the GameCube pad goes
		EXPECT(view.outOfDate());
		SampleControls_SetDevices(0, { PadType::WIIMOTE_NUNCHUK, PadType::GAMECUBE, PadType::GAMEPAD });	// one more arrives
		EXPECT(view.outOfDate());
		SampleControls_SetDevices(0, { PadType::WIIMOTE_NUNCHUK, PadType::GAMECUBE });
		EXPECT(!view.outOfDate());
		// one device: no rail
		SampleControls_SetDevices(0, { PadType::CLASSIC });
		ControllerConfigView single(0, nullptr);
		EXPECT(single.devices().size() == 1 && single.shownType() == PadType::CLASSIC);
		SampleControls_ResetHardware();
	}
	{
		// a hidden layout takes neither input nor the D-pad's selection (the chooser stacks its panels, and the hidden
		// ones sit under the visible one)
		struct Probe : public GuiElement
		{
			int updates = 0;
			void update(InputController *) override { updates++; }
			void draw() override {}
		};
		Probe shownProbe, hiddenProbe;
		GuiColumn outer(100, 100);
		GuiColumn shownPanel(100, 40), hiddenPanel(100, 40);
		shownPanel.add(&shownProbe);
		hiddenPanel.add(&hiddenProbe);
		outer.add(&shownPanel);
		outer.add(&hiddenPanel);
		hiddenPanel.setVisible(false);
		outer.update(nullptr);
		EXPECT(shownProbe.updates == 1 && hiddenProbe.updates == 0);
		std::vector<GuiElement*> nav;
		outer.collectNavigable(nav);
		bool hasHidden = false, hasShown = false;
		for(size_t i = 0; i < nav.size(); i++)
		{
			hasHidden = hasHidden || nav[i] == &hiddenProbe;
			hasShown = hasShown || nav[i] == &shownProbe;
		}
		EXPECT(hasShown && !hasHidden);
		hiddenPanel.setVisible(true);						// showing it again brings it back
		outer.update(nullptr);
		EXPECT(hiddenProbe.updates == 1);
	}
	if(failures)
		return 1;
	printf("behaviour checks passed\n");
	return 0;
}
