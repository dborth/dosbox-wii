// Renders the menu screens to PNG on a host. usage: preview <out dir> [scale]
// Each PNG is the 640x480 canvas with the 5% overscan frame drawn in red; <name>_wide.png is the same frame
// stretched to 16:9 the way a widescreen TV shows it.
#include "libgui/Gui.h"
#include "hostgfx.h"
#include "menu/controlsui.h"
#include "menu/controlssetupsui.h"
#include "menu/homeview.h"
#include "menu/controlsmodel.h"
#include "samplecontrols.h"
#include "menu/menupage.h"
#include "menu/menustyle.h"
#include "menu/menuart.h"
#include "menu/menupopover.h"
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
void Host_SetPressMask(int mask);
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

// The setup screen's card as drawn (read back from the frame): where it starts and ends, and where the first non-card
// pixel of the rail of device icons is, at the rail's x, below the card's top
struct CardGeometry { int top, bottom, railY; };
static std::vector<CardGeometry> g_cards, g_rails;
static std::vector<int> g_modalBg;	// the red of the dimmed background over the black margin, per modal scene
static CardGeometry MeasureCard()
{
	int w = 0, h = 0;
	const std::vector<uint8_t>& px = HostPixels(w, h);
	CardGeometry g = { -1, -1, -1 };
	const int x = (int)(w * 0.075f);
	for(int y = (int)(h * 0.19f); y < h; y++)
	{
		const uint8_t * p = &px[((size_t)y * w + x) * 4];
		if(p[0] + p[1] + p[2] > 650)
		{
			if(g.top < 0)
				g.top = y;
			g.bottom = y;
		}
	}
	for(int y = g.top + 4; g.top >= 0 && y < g.bottom && g.railY < 0; y++)
		for(int xx = (int)(w * 0.095f); xx < (int)(w * 0.115f); xx++)
		{
			const uint8_t * p = &px[((size_t)y * w + xx) * 4];
			if(p[0] + p[1] + p[2] < 600)
			{
				g.railY = y;
				break;
			}
		}
	return g;
}

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


// The quick settings of the home screen, with values the scene picks
struct SceneSettings : HomeSettings
{
	int cycles = 3000, skip = 0;
	bool corrected = true, sharp = false;
	std::string speed() const override { return std::to_string(cycles); }
	std::string speedNote() const override { return "Dynamic core"; }
	void stepSpeed(bool up) override { cycles += up ? 500 : -500; }
	std::string frameskip() const override { return std::to_string(skip); }
	void stepFrameskip(bool up) override { skip += up ? 1 : -1; }
	bool aspectCorrected() const override { return corrected; }
	void setAspectCorrected(bool c) override { corrected = c; }
	bool filterSharp() const override { return sharp; }
	void setFilterSharp(bool s) override { sharp = s; }
};

int main(int argc, char ** argv)
{
	if(argc > 1) outDir = argv[1];
	const float scale = argc > 2 ? (float)atof(argv[2]) : 2.0f;
	HostInit(640, 480, scale);
	SampleControls_Use();
	MenuApplyTheme();

	// the setup screen as the model shows it for a given set of controllers
	{
		SceneModel one(1);
		Controls_SetModel(&one);
		Screen s;
		ControllerConfigView view(0);
		s.page.append(&view);
		snap("config_one", s.root);					// no switcher: the title says Controls
	}
	{
		SceneModel two(2);
		two.edited[0] = true; two.dirty = true; two.setup = "KEEN4E";
		Controls_SetModel(&two);
		Screen s;
		ControllerConfigView view(0);
		s.page.append(&view);
		snap("config_two", s.root);					// the switcher, a preset marked edited, an unsaved setup
	}
	{
		SceneModel two(2);
		two.mode = 1;
		Controls_SetModel(&two);
		Screen s;
		ControllerConfigView view(1);
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
		ControllerConfigView view(3);
		s.page.append(&view);
		snap("config_worst", s.root);
	}
	{
		SceneModel none(0);
		Controls_SetModel(&none);
		Screen s;
		ControllerConfigView view(0);
		s.page.append(&view);
		snap("config_empty", s.root);
	}

	// the home screen: two controllers, four, none; and with a button highlighted
	{
		const int counts[] = { 2, 4, 0 };
		const char * names[] = { "home", "home_four", "home_none" };
		for(int i = 0; i < 3; i++)
		{
			SceneModel m(counts[i]);
			m.dev.mouse = i == 1;
			m.setup = i == 0 ? "Keen 4" : "";
			Controls_SetModel(&m);
			SceneSettings settings;
			Screen s;
			HomeView view(settings, "2.0");
			s.page.append(&view);
			snap(names[i], s.root);
		}
		{
			// a Wii U: the GamePad shown beside the keyboard and mouse, once charged and once low
			const int charges[] = { 70, 12 };
			const char * gpNames[] = { "home_gamepad", "home_gamepad_low" };
			for(int i = 0; i < 2; i++)
			{
				SceneModel m(3);
				m.dev.gamepadPresent = true;
				m.dev.gamepadBattery = charges[i];
				m.dev.mouse = true;
				Controls_SetModel(&m);
				SceneSettings settings;
				Screen s;
				HomeView view(settings, "2.0");
				s.page.append(&view);
				snap(gpNames[i], s.root);
			}
		}
		SceneModel m(2);
		Controls_SetModel(&m);
		SceneSettings settings;
		settings.sharp = true; settings.corrected = false; settings.cycles = 12000; settings.skip = 2;
		Screen s;
		HomeView view(settings, "2.0");
		view.select(HomeView::Action::SETTINGS);
		s.page.append(&view);
		snap("home_selected", s.root);
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
		ControllerConfigView view(scenes[i].player);
		s.page.append(&view);
		snap(scenes[i].name, s.root, 0, 0, 0);
		if(scenes[i].hw != NOHW)
			g_cards.push_back(MeasureCard());
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
			ControllerConfigView view(0, rails[i].show);
			s.page.append(&view);
			snap(rails[i].name, s.root, 0, 0, 0);
			g_rails.push_back(MeasureCard());
		}
		SampleControls_ResetHardware();
		SampleControls_SetHeld(0);
	}
	// the chooser on the sample model, as a popover beside the row that was pressed: (name, hardware, emulated mode, preset, control, what it does now, kind to show)
	struct ChooserScene { const char * name; int hw; int mode; int preset; int control; BindingTarget now; int show; };
	const ChooserScene choosers[] = {
		{ "chooser", (int)PadType::CLASSIC, 0, 1, INPUT_BTN_A, BindingTarget(TARGET::KEY, 5), -1 },				// opens on the key it uses
		{ "chooser_suggested", (int)PadType::CLASSIC, 0, 1, INPUT_TRIGGER_ZR, BindingTarget(), -1 },			// nothing yet: the suggestions
		{ "chooser_keys_mouse", (int)PadType::GAMECUBE, 2, 0, INPUT_BTN_A, BindingTarget(TARGET::JOYSTICK, 0), -1 },	// no joystick, so no Joystick kind
		{ "chooser_left", (int)PadType::CLASSIC, 0, 1, INPUT_BTN_UP, BindingTarget(TARGET::KEY, 5), 3 },		// a left-hand row: the popover on the right
		{ "chooser_dosbox", (int)PadType::CLASSIC, 0, 1, INPUT_BTN_Y, BindingTarget(), 4 },
		{ "chooser_letters", (int)PadType::CLASSIC, 0, 1, INPUT_BTN_X, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "G")), -1 },
		{ "chooser_fkeys", (int)PadType::CLASSIC, 0, 1, INPUT_BTN_X, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F3")), -1 },	// opens on the tab with the key in use
	};
	for(size_t i = 0; i < sizeof(choosers) / sizeof(choosers[0]); i++)
	{
		Controls_SetModel(nullptr);
		SampleControls_ResetHardware();
		SampleControls_SetHardware(0, (PadType)choosers[i].hw, false, false);
		Controls().setEmulated(choosers[i].mode);
		ApplyAll(choosers[i].preset);
		Screen s;
		ControllerConfigView config(0);
		s.page.append(&config);
		for(int u = 0; u < 3; u++)
			s.root.update(nullptr);
		TargetChooserView view("A Button", 0, choosers[i].control, choosers[i].now, BindingTarget(TARGET::KEY, 4), config.anchorForRow(choosers[i].control));
		GuiWindow popWindow(640, 480);
		popWindow.append(&view);
		s.root.append(&popWindow);
		s.page.setState(STATE::DISABLED);
		for(int u = 0; u < 3; u++)
			s.root.update(nullptr);
		s.root.changeFocus(&popWindow);
		if(choosers[i].show >= 0)
			view.showCategory(choosers[i].show);
		popWindow.resetState();
		view.selectCategory();
		snap(choosers[i].name, s.root);
		s.root.remove(&popWindow);
	}
	Controls_SetModel(nullptr);
	SampleControls_ResetHardware();

	// Every list is a popover over the setup screen, beside the button that opened it; the screen under it is dimmed
	// and, for presets, previews the row under the cursor
	{
		enum Kind { PRESET, EMULATED, SETUP, LOAD, ATTACH };
		struct PopScene { const char * name; int hw; Kind kind; bool edited; int mode; int preset; int pick; };
		const PopScene scenes[] = {
			{ "modal_preset", (int)PadType::CLASSIC, PRESET, false, 0, 1, 2 },
			{ "modal_preset_edited", (int)PadType::GAMECUBE, PRESET, true, 0, 1, 4 },
			{ "modal_emulated", (int)PadType::WIIMOTE, EMULATED, false, 0, 1, -1 },
			{ "picker_preset_two", (int)PadType::CLASSIC, PRESET, false, 1, 0, 1 },
			{ "picker_preset_nojoy", (int)PadType::CLASSIC, PRESET, false, 2, 1, -1 },
			{ "picker_emulated", (int)PadType::GAMECUBE, EMULATED, false, 1, 1, 1 },
			{ "picker_setup", (int)PadType::CLASSIC, SETUP, false, 0, 1, -1 },
			{ "picker_load", (int)PadType::CLASSIC, LOAD, false, 0, 1, 1 },
			{ "picker_attach", (int)PadType::CLASSIC, ATTACH, false, 0, 1, -1 },
		};
		for(size_t i = 0; i < sizeof(scenes) / sizeof(scenes[0]); i++)
		{
			const PopScene& sc = scenes[i];
			Controls_SetModel(nullptr);
			SampleControls_ResetHardware();
			SampleControls_SetHardware(0, (PadType)sc.hw, false, false);
			Controls().setEmulated(sc.mode);
			ApplyAll(sc.preset);
			if(sc.kind == SETUP || sc.kind == LOAD)
			{
				Controls().saveSetup("Plain", "");
				Controls().saveSetup("KEEN4E", "KEEN4E.EXE");
				Controls().saveSetup("abcdefghijklmnopqrstuvwx", "LONGNAME.EXE");
			}
			if(sc.edited)
				Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F2")));
			Screen s;
			ControllerConfigView config(0);
			s.page.append(&config);
			for(int u = 0; u < 3; u++)
				s.root.update(nullptr);
			std::vector<int> index;
			std::vector<SetupAction> setupKinds;
			std::vector<AttachChoice> attachKinds;
			std::vector<PickerItem> items;
			const char * title = "";
			ControllerConfigView::Pick which = ControllerConfigView::Pick::PRESET;
			switch(sc.kind)
			{
				case PRESET: items = Picker_PresetItems(0, index); title = "Choose a preset"; break;
				case EMULATED: items = Picker_EmulatedItems(); title = "What does DOS see?"; which = ControllerConfigView::Pick::EMULATED; break;
				case SETUP: items = Picker_SetupMenuItems(setupKinds); title = "Load or save a setup"; which = ControllerConfigView::Pick::SETUP; break;
				case LOAD: items = Picker_SetupItems(index); title = "Load a setup"; which = ControllerConfigView::Pick::SETUP; break;
				case ATTACH: items = Picker_AttachItems("KEEN4E.EXE", attachKinds); title = "Load these buttons for"; which = ControllerConfigView::Pick::SETUP; break;
			}
			PickerView picker(title, items, config.anchorFor(which));
			if(sc.kind == PRESET)
				picker.onFocusChange([&](int k){ const int pr = k >= 0 && (size_t)k < index.size() ? index[(size_t)k] : -1; config.setPreview(pr < 0 ? Controls().currentPreset(0) : pr); });
			GuiWindow popWindow(640, 480);
			popWindow.append(&picker);
			s.root.append(&popWindow);
			s.page.setState(STATE::DISABLED);		// what the app does: the screen under a popover is dimmed and takes no input
			for(int u = 0; u < 3; u++)
				s.root.update(nullptr);				// laid out first, as the app's UpdateGui does
			s.root.changeFocus(&popWindow);
			picker.selectInUse();					// as the app does once the popover has the focus
			if(sc.pick >= 0)
				picker.selectItem(sc.pick);
			config.refresh();
			snap(sc.name, s.root);
			if(i < 3)
			{
				int w = 0, h = 0;
				const std::vector<uint8_t>& px = HostPixels(w, h);
				g_modalBg.push_back(px[((size_t)(h / 2) * w + (size_t)(w * 0.02f)) * 4]);
			}
			s.root.remove(&popWindow);
		}
		Controls_SetModel(nullptr);
		SampleControls_ResetHardware();
		Controls().setEmulated(0);
		ApplyAll(1);
	}

	// ---- behaviour checks (a failure prints and makes the run fail)
	int failures = 0;
	#define EXPECT(c) do { if(!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while(0)

	// ---- popovers: placement, size and what they show
	{
		// below its anchor when it fits, lined up with the nearer edge, inside the margins
		PopoverPlace p = Popover_Place(PopoverAnchor(400, 60, 150, 32), 280, 200, 640, 480, MenuPopover_Insets());
		EXPECT(p.y == 60 + 32 + 6);
		EXPECT(p.x == 400 + 150 - 280);				// right half: right edges line up
		p = Popover_Place(PopoverAnchor(60, 40, 100, 32), 280, 200, 640, 480, MenuPopover_Insets());
		EXPECT(p.x == 60);								// left half: left edges line up
		p = Popover_Place(PopoverAnchor(60, 40, 100, 32, PopoverAnchor::RIGHT_MARGIN), 280, 200, 640, 480, MenuPopover_Insets());
		EXPECT(p.x == 640 - MENU_PAGE_MARGIN - 280);
		p = Popover_Place(PopoverAnchor(300, 40, 100, 32, PopoverAnchor::LEFT_MARGIN), 280, 200, 640, 480, MenuPopover_Insets());
		EXPECT(p.x == MENU_PAGE_MARGIN);
		p = Popover_Place(PopoverAnchor(560, 400, 100, 32), 280, 200, 640, 480, MenuPopover_Insets());
		EXPECT(p.x == 640 - MENU_PAGE_MARGIN - 280);	// kept inside the margin
		EXPECT(p.y == 400 - 200 - 6);						// no room below a footer chip: it comes out of the top of it
		p = Popover_Place(PopoverAnchor(100, 200, 120, 30), 280, 280, 640, 480, MenuPopover_Insets());
		EXPECT(p.y >= MENU_BAR_TOP_H && p.y + 280 <= 480 - MENU_BAR_BOTTOM_H);	// a row in the middle of the page: kept in the band
		// the tail points at the anchor: up from a panel under it, down from one over it, sideways from one beside it
		{
			PopoverPlace under = Popover_Place(PopoverAnchor(400, 20, 150, 40), 280, 200, 640, 480, MenuPopover_Insets());
			PopoverTail t = Popover_Tail(PopoverAnchor(400, 20, 150, 40), under, 280, 200);
			EXPECT(t.shown && t.direction == POPOVER_TAIL_UP && t.y + t.h == under.y + 1);
			EXPECT(t.x >= under.x && t.x + t.w <= under.x + 280);
			PopoverPlace over = Popover_Place(PopoverAnchor(400, 420, 150, 40), 280, 200, 640, 480, MenuPopover_Insets());
			t = Popover_Tail(PopoverAnchor(400, 420, 150, 40), over, 280, 200);
			EXPECT(t.shown && t.direction == POPOVER_TAIL_DOWN && t.y == over.y + 200 - 1);
			PopoverAnchor row(60, 200, 170, 30, PopoverAnchor::RIGHT_MARGIN);
			PopoverPlace beside = Popover_Place(row, 380, 270, 640, 480, MenuPopover_Insets());
			t = Popover_Tail(row, beside, 380, 270);
			EXPECT(t.shown && t.direction == POPOVER_TAIL_LEFT);
			EXPECT(t.y >= beside.y && t.y + t.h <= beside.y + 270);
			PopoverAnchor covered(300, 200, 170, 30, PopoverAnchor::RIGHT_MARGIN);
			EXPECT(!Popover_Tail(covered, Popover_Place(covered, 380, 270, 640, 480, MenuPopover_Insets()), 380, 270).shown);	// the panel covers the anchor: no tail
			EXPECT(!Popover_Tail(PopoverAnchor(), under, 280, 200).shown);
		}
		p = Popover_Place(PopoverAnchor(), 280, 200, 640, 480, MenuPopover_Insets());
		EXPECT(p.x == 180 && p.y == 140);				// no anchor: centred

		// every list fits the band between the bars, with rows at least 28 px tall, and its caption fits its lines
		Controls_SetModel(nullptr);
		SampleControls_ResetHardware();
		SampleControls_SetHardware(0, PadType::CLASSIC, false, false);
		ApplyAll(1);
		Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F2")));
		std::vector<int> index;
		std::vector<SetupAction> sk;
		Controls().saveSetup("Plain", "");
		const std::vector<PickerItem> lists[] = { Picker_PresetItems(0, index), Picker_EmulatedItems(), Picker_SetupMenuItems(sk) };
		for(size_t l = 0; l < sizeof(lists) / sizeof(lists[0]); l++)
		{
			Screen s;
			PickerView pv("Title", lists[l], PopoverAnchor(300, 40, 170, 32, PopoverAnchor::LEFT_MARGIN));
			EXPECT(pv.panelY() >= 0);
			EXPECT(pv.panelY() + pv.panelH() <= 480 - MENU_BAR_BOTTOM_H);
			EXPECT(pv.rowH() >= 28 && pv.rowH() <= 36);
			EXPECT(pv.caption().size() >= 2 && pv.caption().size() <= 3);
			for(size_t k = 0; k < pv.caption().size(); k++)
				EXPECT(pv.caption()[k].size() <= 40);
			// moving the cursor changes the caption
			pv.focusItem(1);
			EXPECT(pv.focused() == 1);
			EXPECT(!pv.caption()[0].empty());
		}
		{
			PickerView pv("Title", lists[0]);
			EXPECT(lists[0][0].divider && lists[0][0].name == "Undo my changes");	// the one different kind of choice is set apart
			pv.choose(0);
			EXPECT(pv.chosen() && pv.result() == 0);								// one press chooses: nothing asks twice
		}
		// the preview hook hears the cursor, once at once
		{
			PickerView pv("Title", lists[0]);
			int heard = -99, calls = 0;
			pv.onFocusChange([&](int k){ heard = k; calls++; });
			EXPECT(calls == 1);
			pv.focusItem(3);
			EXPECT(calls == 2 && heard == 3);
			pv.focusItem(3);
			EXPECT(calls == 2);														// the same row again says nothing
		}
		Controls_SetModel(nullptr);
		SampleControls_ResetHardware();
	}
	// a disabled screen is washed over once (0xbe at 0x70 over black is 83), however many rows and columns it holds; each
	// extra layer would lift this toward 0xbe
	EXPECT(g_modalBg.size() == 3);
	for(size_t i = 0; i < g_modalBg.size(); i++)
		EXPECT(g_modalBg[i] >= 80 && g_modalBg[i] <= 86);
	// the card is the same height whatever controller is drawn in it, and the rail of icons never moves
	EXPECT(g_cards.size() >= 8 && g_rails.size() >= 4);
	for(size_t i = 0; i < g_cards.size(); i++)
		EXPECT(g_cards[i].top >= 0 && g_cards[i].top == g_cards[0].top && g_cards[i].bottom == g_cards[0].bottom);
	// (rail scene 1 has a lit icon first, whose pale edge this reading can't see, so only its card is compared)
	for(size_t i = 0; i < g_rails.size(); i++)
		EXPECT(g_rails[i].top == g_cards[0].top && g_rails[i].bottom == g_cards[0].bottom && (i == 1 || (g_rails[i].railY >= 0 && g_rails[i].railY == g_rails[0].railY)));
	{
		Controls_SetModel(nullptr);
		Controls().setEmulated(0);
		ApplyAll(1);
		Screen s;
		TargetChooserView view("A Button", 0, INPUT_BTN_A, BindingTarget(TARGET::KEY, 5), BindingTarget(TARGET::KEY, 4));
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
		// the key picker: a cap for every key the player can bind, L and R step the tabs (wrapping) only while Key is showing
		Controls_SetModel(nullptr);
		Controls().setEmulated(0);
		Screen s;
		TargetChooserView view("A Button", 0, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F3")), BindingTarget());
		s.page.append(&view);
		EXPECT(view.keyCapCount() == (int)Controls().choices(0, TARGET::KEY).size());
		EXPECT(view.keyTabCount() == 6);
		EXPECT(view.category() == 1);								// opens on Key, the kind the control uses
		EXPECT(view.keyTabIndex() == 3);							// ...and on the F-keys tab, which has F3
		view.stepKeyTab(1);
		EXPECT(view.keyTabIndex() == 4);
		view.stepKeyTab(1); view.stepKeyTab(1);
		EXPECT(view.keyTabIndex() == 0);							// wraps forward
		view.stepKeyTab(-1);
		EXPECT(view.keyTabIndex() == 5);							// and back
	}
	{
		// a key pressed before listening began does not count
		Controls_SetModel(nullptr);
		Screen s;
		TargetChooserView view("A Button", 0, INPUT_BTN_A, BindingTarget(), BindingTarget());
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
		TargetChooserView view("A Button", 0, INPUT_BTN_A, BindingTarget(), BindingTarget());
		s.page.append(&view);
		EXPECT(view.categoryCount() == 5);
		Controls().setEmulated(0);
	}
	{
		// a picker: one press chooses, whatever the item
		std::vector<PickerItem> items(3);
		items[0].name = "One"; items[0].current = true;
		items[1].name = "Two";
		items[2].name = "Three";
		Screen s;
		PickerView view("Pick", items);
		s.page.append(&view);
		EXPECT(view.focused() == 0);							// opens on the one in use
		view.choose(1);
		EXPECT(view.chosen() && view.result() == 1);
		PickerView bad("Pick", items);
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
		EXPECT(items[0].rows.size() == 7 && items[0].rows[0].label == "A Button" && items[0].rows[0].value == "Joystick 1");
		EXPECT(items[1].rows[0].value == "Key: Ctrl");

		Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F2")));
		items = Picker_PresetItems(0, index);

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
		Controls().setBinding(0, INPUT_BTN_A, BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, "F2")));
		items = Picker_SetupItems(index);
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
		EXPECT(items[0].name == "Undo my changes" && index[0] == -1 && !items[0].current);
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
		EXPECT(kinds.size() == items.size() && kinds.back() == SetupAction::SAVE_NEW);
		// Load is only offered when something is saved
		EXPECT((kinds[0] == SetupAction::LOAD) == !Controls().setups().empty());
		Controls().saveSetup("PLAIN", "");
		items = Picker_SetupMenuItems(kinds);
		EXPECT(kinds.size() == 3 && kinds[0] == SetupAction::LOAD && kinds[1] == SetupAction::SAVE_CURRENT && items[1].name == "Save as PLAIN");
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
		ControllerConfigView view(0);
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
		ControllerConfigView view(0);
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
		ControllerConfigView view(0);
		s.page.append(&view);
		EXPECT(view.devices().size() == 2 && view.shownType() == PadType::WIIMOTE_NUNCHUK);	// the primary one by default
		EXPECT(view.takeFlip() == PadType::NONE && !view.outOfDate());
		{
			ControllerConfigView gc(0, PadType::GAMECUBE);
			EXPECT(gc.shownType() == PadType::GAMECUBE);
			EXPECT(!gc.outOfDate());															// drawing the second device is not a stale screen
			ControllerConfigView gone(0, PadType::CLASSIC);							// not on the channel: the primary one
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
		ControllerConfigView single(0);
		EXPECT(single.devices().size() == 1 && single.shownType() == PadType::CLASSIC);
		SampleControls_ResetHardware();
	}
	{
		// the popover backdrop covers the whole screen, so every pointer is always over it. With two pointers either
		// player can click away, whichever controller the screen happens to update first (player 4 down to player 1)
		PopoverBackdrop backdrop(640, 480);
		struct Box : public GuiElement
		{
			Box() { width = 100; height = 100; }
			void draw() override {}
		};
		Box panel;
		panel.setPosition(200, 100);
		backdrop.setPanel(&panel);
		backdrop.setCancel(nullptr);
		GuiTrigger click;
		backdrop.setPrimary(&click);
		InputController p1(0), p2(1);
		InputPadData off, on;
		off.validPointer = true; off.cursor_x = 40; off.cursor_y = 40;			// outside the panel
		on.validPointer = true; on.cursor_x = 250; on.cursor_y = 150;			// over the panel
		const auto frame = [&](const InputPadData& a, const InputPadData& b, int pressed) {
			p1.update(a, 0.016f);
			p2.update(b, 0.016f);
			Host_SetPressMask(pressed);
			backdrop.update(&p2);												// the screen updates the later players first
			backdrop.update(&p1);
			Host_SetPressMask(0);
		};
		frame(off, off, 1);														// player 1 clicks, both pointers off the panel
		EXPECT(backdrop.getState() == STATE::CLICKED);
		backdrop.resetState();
		frame(off, off, 2);														// player 2 clicks
		EXPECT(backdrop.getState() == STATE::CLICKED);
		backdrop.resetState();
		frame(on, off, 1);														// player 1 clicks on the panel: not a click away
		EXPECT(backdrop.getState() != STATE::CLICKED);
		frame(off, on, 1);														// player 1 clicks away while player 2 points at the panel
		EXPECT(backdrop.getState() == STATE::CLICKED);
		backdrop.resetState();
		InputPadData lost = off; lost.validPointer = false;
		frame(lost, off, 1);													// no pointer: a press is not a click away
		EXPECT(backdrop.getState() != STATE::CLICKED);
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
