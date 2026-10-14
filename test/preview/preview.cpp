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
	int scheme = 1;
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
			players[i].scheme = "Arrow keys";
		}
		dev.keyboard = true; dev.mouse = false; dev.wiimoteIr = true; dev.gamepadPresent = false; dev.gamepadTouch = false;
	}
	int playerCount() const override { return 4; }
	PlayerInfo player(int i) const override { return players[i]; }
	DeviceStatus devices() const override { return dev; }
	std::vector<std::string> schemes() const override
	{
		static const char * names[] = { "Compatible", "Arrow keys", "Arrows + shooter", "Point and click", "Stick mouse", "Menu keys" };
		return std::vector<std::string>(names, names + 6);
	}
	int currentScheme() const override { return scheme; }
	void setScheme(int i) override { scheme = i; }
	BindingTarget binding(PadType, int) const override { return BindingTarget(TARGET::KEY, 5); }
	void setBinding(PadType, int, const BindingTarget&) override {}
	void resetBindings(PadType) override {}
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
	{
		SceneModel one(1);
		Controls_SetModel(&one);
		Screen s;
		ControllerConfigView view(0, nullptr);
		s.page.append(&view);
		snap("config", s.root, 1);
	}
	{
		SceneModel one(1);
		Controls_SetModel(&one);
		Screen s;
		TargetChooserView view("A Button", false, BindingTarget(TARGET::KEY, 5), nullptr);
		s.page.append(&view);
		snap("chooser", s.root);
	}
	{
		SceneModel one(1);
		Controls_SetModel(&one);
		Screen s;
		TargetChooserView view("L Stick", true, BindingTarget(TARGET::STICK_MODE, 1), nullptr);
		s.page.append(&view);
		snap("chooser_stick", s.root);
	}
	return 0;
}
