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
	// the setup screen on the sample model, so rows show real targets: (player, emulated mode, preset, held buttons)
	struct SampleScene { const char * name; int player, mode, preset; uint32_t held; };
	const SampleScene scenes[] = {
		{ "config", 0, 0, 1, 0 },
		{ "config_pressed", 0, 0, 1, INPUT_BTN_A | INPUT_TRIGGER_ZL | INPUT_BTN_UP },
		{ "config_twojoy", 0, 1, 0, 0 },
		{ "config_keys_mouse", 0, 2, 0, 0 },
		{ "config_gamecube", 2, 0, 1, 0 },
	};
	for(size_t i = 0; i < sizeof(scenes) / sizeof(scenes[0]); i++)
	{
		Controls_SetModel(nullptr);
		Controls().setEmulated(scenes[i].mode);
		Controls().applyPreset(scenes[i].preset);
		SampleControls_SetHeld(scenes[i].held);
		Screen s;
		ControllerConfigView view(scenes[i].player, nullptr);
		s.page.append(&view);
		snap(scenes[i].name, s.root, 0, 0, 0);
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
		Controls().applyPreset(choosers[i].preset);
		Screen s;
		TargetChooserView view("A Button", 0, choosers[i].control, choosers[i].now, BindingTarget(TARGET::KEY, 4), nullptr);
		s.page.append(&view);
		snap(choosers[i].name, s.root);
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
	if(failures)
		return 1;
	printf("behaviour checks passed\n");
	return 0;
}
