// Host tests for the mapper-backed controls model (controlsmapper.cpp), over a fake of the mapper's pad groups.
// The fake follows sdl_mapper.cpp as read (CPadBindGroup, MAPPER_PadSetEvent, the shipped binds); it is only as
// faithful as that reading. The text checks tie the names and the button order to the real sources.
#include "controlsmapper.h"
#include "controlspresets.h"
#include "controlssetupsfiles.h"
#include "drivers/InputData.h"
#include <algorithm>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0, checks = 0;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)
#define CHECK_EQ(a, b) do { checks++; const std::string x = (a), y = (b); if(x != y) { failures++; printf("FAIL %s:%d  %s\n      got  \"%s\"\n      want \"%s\"\n", __FILE__, __LINE__, #a, x.c_str(), y.c_str()); } } while(0)

static std::string readFile(const std::string& rel)
{
	std::ifstream f(std::string(SRC_DIR) + "/" + rel);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static bool quoted(const std::string& text, const std::string& word) { return text.find("\"" + word + "\"") != std::string::npos; }

// ---- fakes

class FakeHardware : public ControlsHardware
{
	public:
		PlayerHardware player(int i) const override
		{
			PlayerHardware h;
			h.connected = i < 3;
			h.type = i == 0 ? PadType::WIIMOTE : i == 1 ? PadType::CLASSIC : i == 2 ? PadType::GAMECUBE : PadType::NONE;
			return h;
		}
		DeviceStatus devices() const override { DeviceStatus d; d.keyboard = true; d.mouse = d.wiimoteIr = d.gamepadPresent = d.gamepadTouch = false; return d; }
		uint32_t held(int i) const override { return i == 1 ? INPUT_BTN_A : 0; }
		void beginCapture() override { began++; pending.clear(); }
		bool pollCapture(CaptureEvent& e) override
		{
			if(pending.empty())
				return false;
			e = pending.front();
			pending.pop_front();
			return true;
		}
		int began = 0;
		std::deque<CaptureEvent> pending;
};

// The pad groups as sdl_mapper.cpp has them. Events: the keys and handlers the real sources define (read from the
// text), jbutton_0_0..5 and jbutton_1_0..1 (the stick 0 and 1 events, hidden or not, are always created).
class FakeMapper : public PadMapper
{
	public:
		FakeMapper(const std::string& type = "4axis") { reset(type); }

		void reset(const std::string& t)
		{
			for(int p = 0; p < 4; p++)
				for(int s = 0; s < PAD_SOURCE_COUNT; s++)
					padBinds[p][s].clear();
			log.clear();
			saves = 0;
			setType(t);
		}

		// What MAPPER_RebuildJoysticks does: the stick groups are built again for the type, the pad groups keep their binds.
		// The stick group that reads pad N: 4axis one for pad 0 (the one for pad 1 is a dummy); 2axis one each for pads 0
		// and 1; 4axis_2 one for pad 1 (physical stick 1, emulated joystick 0; pad 0's is a dummy); fcs and ch one for
		// pad 0; none has none. Its shipped binds are what MAPPER_PadSetDefault gives back, named for the emulated joystick.
		void setType(const std::string& t)
		{
			type = t;
			for(int p = 0; p < 4; p++)
				for(int s = 0; s < PAD_SOURCE_COUNT; s++)
					stickBinds[p][s].clear();
			for(int p = 0; p < 2; p++)
			{
				int emu = -1;
				if(type == "2axis")
					emu = p;
				else if(type == "4axis_2")
					emu = p == 1 ? 0 : -1;
				else if(type == "4axis" || type == "fcs" || type == "ch")
					emu = p == 0 ? 0 : -1;
				if(emu < 0)
					continue;
				for(int s = 0; s < PAD_SOURCE_COUNT; s++)
				{
					char name[24];
					if(s < 12 && ((emu == 0 && s < 6) || (emu == 1 && s < 2)))
						snprintf(name, sizeof(name), "jbutton_%d_%d", emu, s);
					else if(s >= 12 && emu == 0)
						snprintf(name, sizeof(name), "jhat_0_0_%d", s - 12);
					else
						continue;
					stickBinds[p][s].push_back(name);
				}
			}
		}

		bool eventExists(const char * name) const override
		{
			const std::string n = name;
			if(n.compare(0, 4, "key_") == 0)
				return quoted(mapperSource(), n.substr(4)) || n == "key_esc";
			if(n.compare(0, 5, "hand_") == 0)
				return quoted(handlerSources(), n.substr(5));
			int a, b;
			if(sscanf(n.c_str(), "jbutton_%d_%d", &a, &b) == 2)
				return (a == 0 && b >= 0 && b < 6) || (a == 1 && b >= 0 && b < 2);
			if(sscanf(n.c_str(), "jhat_0_0_%d", &b) == 1)	// stick 0's hat, which CreateDefaultJoystickBinds binds
				return b >= 0 && b < 4;
			return false;
		}

		std::vector<std::string> events(int pad, int source) const override
		{
			std::vector<std::string> out = padBinds[pad][source];
			out.insert(out.end(), stickBinds[pad][source].begin(), stickBinds[pad][source].end());
			return out;
		}

		bool setEvent(int pad, int source, const char * event) override
		{
			if(pad < 0 || pad >= 4 || source < 0 || source >= PAD_SOURCE_COUNT)
				return false;
			if(*event && !eventExists(event))
				return false;
			log.push_back(std::to_string(pad) + ":" + std::to_string(source) + "=" + event);
			padBinds[pad][source].clear();
			stickBinds[pad][source].clear();
			if(*event)
				padBinds[pad][source].push_back(event);
			return true;
		}

		bool save() override { saves++; if(onSave) onSave(); return saveWorks; }
		bool setJoystickType(const std::string& t) override
		{
			static const char * const known[] = { "none", "2axis", "4axis", "4axis_2", "fcs", "ch" };
			bool ok = false;
			for(size_t i = 0; i < 6; i++)
				ok = ok || t == known[i];
			if(!ok)
				return false;
			setType(t);
			return true;
		}
		std::string joystickType() const override { return type; }

		void bind(int pad, int source, const char * event) { padBinds[pad][source].push_back(event); }

		std::string type;
		std::vector<std::string> padBinds[4][PAD_SOURCE_COUNT], stickBinds[4][PAD_SOURCE_COUNT];
		std::vector<std::string> log;
		int saves = 0;
		bool saveWorks = true;
		std::function<void()> onSave;	// what writing the mapper file does: the test points it at the fake file

	private:
		static const std::string& mapperSource() { static const std::string s = readFile("dosbox/gui/sdl_mapper.cpp"); return s; }
		static const std::string& handlerSources()
		{
			static const std::string s = readFile("dosbox/cpu/cpu.cpp") + readFile("dosbox/gui/render.cpp") + readFile("dosbox/ints/bios_disk.cpp") +
				readFile("dosbox/dosbox.cpp") + readFile("input.cpp");
			return s;
		}
};

static BindingTarget key(const char * n) { return BindingTarget(TARGET::KEY, Catalog_Find(TARGET::KEY, n)); }
static BindingTarget joy(int b) { return BindingTarget(TARGET::JOYSTICK, b); }

struct Rig
{
	FakeHardware hw;
	FakeMapper mapper;
	MapperControlsModel model;
	Rig(const char * type = "4axis") : mapper(type), model(hw, mapper) {}
};

// ---- the source order is the shim's

static void testSourceOrder()
{
	// the pad group reads the shim's SDL joystick by index, so source N must be the control the shim puts at button N
	struct { const char * name; int control; } known[] = {
		{ "INPUT_BTN_A", INPUT_BTN_A }, { "INPUT_BTN_B", INPUT_BTN_B }, { "INPUT_BTN_X", INPUT_BTN_X }, { "INPUT_BTN_Y", INPUT_BTN_Y },
		{ "INPUT_TRIGGER_L", INPUT_TRIGGER_L }, { "INPUT_TRIGGER_R", INPUT_TRIGGER_R }, { "INPUT_TRIGGER_ZL", INPUT_TRIGGER_ZL },
		{ "INPUT_TRIGGER_ZR", INPUT_TRIGGER_ZR }, { "INPUT_BTN_MINUS", INPUT_BTN_MINUS }, { "INPUT_BTN_PLUS", INPUT_BTN_PLUS },
		{ "INPUT_THUMB_L", INPUT_THUMB_L | BIND_STICK_CLICK }, { "INPUT_THUMB_R", INPUT_THUMB_R | BIND_STICK_CLICK },
	};
	const std::string shim = readFile("sdlshim/SDL_input.cpp");
	int found = 0;
	for(int n = 0; n < 12; n++)
	{
		const std::string head = "js.buttons[" + std::to_string(n) + "]";
		size_t at = shim.find(head);
		CHECK(at != std::string::npos);
		if(at == std::string::npos)
			continue;
		const size_t held = shim.find("Held(b, ", at);
		const size_t end = shim.find_first_of(" )", held + 8);
		const std::string first = shim.substr(held + 8, end - held - 8);
		for(size_t k = 0; k < sizeof(known) / sizeof(known[0]); k++)
			if(first == known[k].name)
			{
				CHECK(Pad_ControlOfSource(n) == known[k].control);
				CHECK(Pad_SourceOfControl(known[k].control) == n);
				found++;
			}
	}
	CHECK(found == 12);
	// the D-pad is the hat: up 1, right 2, down 4, left 8 (SDL_HAT_*), the mapper's source order up, right, down, left
	CHECK(Pad_ControlOfSource(12) == INPUT_BTN_UP && Pad_ControlOfSource(13) == INPUT_BTN_RIGHT);
	CHECK(Pad_ControlOfSource(14) == INPUT_BTN_DOWN && Pad_ControlOfSource(15) == INPUT_BTN_LEFT);
	CHECK(Pad_SourceOfControl(INPUT_THUMB_L) == -1 && Pad_SourceOfControl(INPUT_BTN_HOME) == -1);
	// the mapper has 16 pad sources (PAD_SOURCES in sdl_mapper.cpp)
	CHECK(readFile("dosbox/gui/sdl_mapper.cpp").find("#define PAD_SOURCES 16") != std::string::npos);
	CHECK(PAD_SOURCE_COUNT == 16);
}

// ---- event names

static void testEventNames()
{
	const std::string mapperText = readFile("dosbox/gui/sdl_mapper.cpp");
	const FakeMapper registered;	// answers from the sources that register the events
	std::set<std::string> seen;
	const std::vector<std::string>& keys = Catalog_Names(TARGET::KEY);
	for(size_t i = 0; i < keys.size(); i++)
	{
		std::string e;
		CHECK(Mapper_EventFor(0, BindingTarget(TARGET::KEY, (int)i), e));
		CHECK(e.compare(0, 4, "key_") == 0);
		CHECK(seen.insert(e).second);									// no two keys share an event
		CHECK(quoted(mapperText, e.substr(4)));							// the mapper has it
		BindingTarget back;
		CHECK(Mapper_TargetFor(0, e, back) && back == BindingTarget(TARGET::KEY, (int)i));
	}
	// the right-hand modifiers read as the one key the catalog has
	BindingTarget t;
	CHECK(Mapper_TargetFor(0, "key_rctrl", t) && t == key("Ctrl"));
	CHECK(Mapper_TargetFor(0, "key_ralt", t) && t == key("Alt"));
	CHECK(Mapper_TargetFor(0, "key_rshift", t) && t == key("Shift"));

	for(TARGET kind : { TARGET::MOUSE, TARGET::HOTKEY })
		for(size_t i = 0; i < Catalog_Names(kind).size(); i++)
		{
			std::string e;
			CHECK(Mapper_EventFor(0, BindingTarget(kind, (int)i), e) && e.compare(0, 5, "hand_") == 0);
			CHECK(registered.eventExists(e.c_str()));						// registered by MAPPER_AddHandler somewhere in the core or the app
			CHECK(seen.insert(e).second);
			BindingTarget back;
			CHECK(Mapper_TargetFor(0, e, back) && back == BindingTarget(kind, (int)i));
		}

	// a joystick button is the player's own: jbutton_N_B for player N
	std::string e;
	CHECK(Mapper_EventFor(0, joy(3), e) && e == "jbutton_0_3");
	CHECK(Mapper_EventFor(1, joy(1), e) && e == "jbutton_1_1");
	CHECK(!Mapper_EventFor(2, joy(0), e));								// only the emulated joysticks 0 and 1 have button events
	CHECK(!Mapper_EventFor(3, joy(0), e));
	CHECK(!Mapper_EventFor(0, joy(4), e));								// the catalog stops at four
	CHECK(Mapper_EventFor(3, BindingTarget(), e) && e.empty());
	CHECK(Mapper_TargetFor(1, "jbutton_1_0", t) && t == joy(0));
	CHECK(!Mapper_TargetFor(1, "jbutton_0_0", t));						// another player's joystick is not this player's button
	CHECK(!Mapper_TargetFor(0, "jbutton_0_4", t));						// joystick button 5: not in the catalog
	CHECK(!Mapper_TargetFor(0, "jhat_0_0_1", t) && t.kind == TARGET::NONE);
}

// ---- reading what the mapper ships

static void testShippedState()
{
	{
		Rig r("4axis");
		CHECK(r.model.currentEmulated() == 0);
		CHECK(r.model.joystickButtons(0) == 4 && r.model.joystickButtons(1) == 0);
		// player 1's shipped binds: A B X Y are joystick buttons 1-4
		CHECK(r.model.binding(0, INPUT_BTN_A) == joy(0) && r.model.binding(0, INPUT_BTN_B) == joy(1));
		CHECK(r.model.binding(0, INPUT_BTN_X) == joy(2) && r.model.binding(0, INPUT_BTN_Y) == joy(3));
		// L and R are jbutton_0_4 / _5: joystick buttons the catalog (and the 4axis joystick) does not have
		CHECK(r.model.binding(0, INPUT_TRIGGER_L).kind == TARGET::NONE);
		CHECK(r.model.binding(0, INPUT_BTN_UP).kind == TARGET::NONE);	// the hat bind
		// player 2 has no stick group under 4axis: nothing bound
		CHECK(r.model.binding(1, INPUT_BTN_A).kind == TARGET::NONE);
		// untouched, the buttons are Compatible and nothing is "modified"
		CHECK(r.model.currentPreset(0) == PRESET_COMPATIBLE);
		CHECK(!r.model.modified(0));
		CHECK(r.mapper.saves == 0 && r.mapper.log.empty());				// reading changes nothing
	}
	{
		Rig r("2axis");
		CHECK(r.model.currentEmulated() == 1);
		CHECK(r.model.joystickButtons(0) == 2 && r.model.joystickButtons(1) == 2 && r.model.joystickButtons(2) == 0);
		CHECK(r.model.binding(0, INPUT_BTN_A) == joy(0));
		CHECK(r.model.binding(1, INPUT_BTN_A) == joy(0) && r.model.binding(1, INPUT_BTN_B) == joy(1));	// player 2's own joystick
		CHECK(r.model.binding(1, INPUT_BTN_X).kind == TARGET::NONE);	// jbutton_1_2 does not exist
		// X is jbutton_0_2 on player 1: a button the 2axis joystick lacks, reported as it is
		CHECK(r.model.binding(0, INPUT_BTN_X) == joy(2));
		CHECK_EQ(r.model.describe(0, INPUT_BTN_X), "No button 3");
		CHECK(!r.model.modified(0) && !r.model.modified(1));			// but that is what Compatible gives it too, so not a change
		CHECK(r.model.currentPreset(1) == PRESET_COMPATIBLE);
	}
	{
		Rig r("none");
		CHECK(r.model.currentEmulated() == 2);
		CHECK(r.model.joystickButtons(0) == 0);
		CHECK(r.model.binding(0, INPUT_BTN_A).kind == TARGET::NONE);
		// no joystick: Compatible means nothing, so the closest preset a player can use is taken
		CHECK(!r.model.presets()[(size_t)r.model.currentPreset(0)].needsJoystick);
	}
	{
		Rig r("fcs");	// its own (advanced) mode, player 1's joystick with four buttons
		CHECK(r.model.currentEmulated() == 4 && r.model.joystickButtons(0) == 4);
	}
}

// ---- setting a button

static void testSetBinding()
{
	Rig r("4axis");
	r.model.setBinding(0, INPUT_BTN_A, key("Ctrl"));
	CHECK(r.mapper.log.size() == 1 && r.mapper.log[0] == "0:0=key_lctrl");		// player 1, source 0 (A)
	CHECK(r.mapper.saves == 1);												// written to the mapper file at once
	CHECK(r.model.binding(0, INPUT_BTN_A) == key("Ctrl"));
	CHECK(r.mapper.events(0, 0).size() == 1);								// the shipped joystick bind on that source went with it
	CHECK(r.model.modified(0));
	CHECK(r.model.setupModified());

	// setting what it already is changes nothing and writes nothing
	r.model.setBinding(0, INPUT_BTN_A, key("Ctrl"));
	CHECK(r.mapper.log.size() == 1 && r.mapper.saves == 1);

	// a key, the mouse and a hotkey, on the buttons they were asked for
	r.model.setBinding(0, INPUT_TRIGGER_L, BindingTarget(TARGET::MOUSE, 1));
	r.model.setBinding(0, INPUT_BTN_PLUS, BindingTarget(TARGET::HOTKEY, 0));
	r.model.setBinding(0, INPUT_THUMB_L | BIND_STICK_CLICK, key("F5"));
	r.model.setBinding(0, INPUT_BTN_LEFT, key("Left"));
	CHECK(r.mapper.log[1] == "0:4=hand_mouseright" && r.mapper.log[2] == "0:9=hand_cycleup");
	CHECK(r.mapper.log[3] == "0:10=key_f5" && r.mapper.log[4] == "0:15=key_left");
	CHECK(r.model.binding(0, INPUT_BTN_PLUS) == BindingTarget(TARGET::HOTKEY, 0));

	// not assigned removes it
	r.model.setBinding(0, INPUT_BTN_B, BindingTarget());
	CHECK(r.model.binding(0, INPUT_BTN_B).kind == TARGET::NONE && r.mapper.events(0, 1).empty());

	// a control with no source cannot be set
	const size_t before = r.mapper.log.size();
	r.model.setBinding(0, INPUT_THUMB_L, key("Esc"));
	r.model.setBinding(0, INPUT_BTN_HOME, key("Esc"));
	r.model.setBinding(7, INPUT_BTN_A, key("Esc"));
	r.model.setBinding(-1, INPUT_BTN_A, key("Esc"));
	CHECK(r.mapper.log.size() == before);
}

static void testPlayersAreSeparate()
{
	Rig r("2axis");
	r.model.setBinding(2, INPUT_BTN_A, key("Space"));
	CHECK(r.mapper.log.size() == 1 && r.mapper.log[0] == "2:0=key_space");
	CHECK(r.model.binding(2, INPUT_BTN_A) == key("Space"));
	CHECK(r.model.binding(0, INPUT_BTN_A) == joy(0));						// players 1 and 2 untouched
	CHECK(r.model.binding(1, INPUT_BTN_A) == joy(0));
	CHECK(r.model.binding(3, INPUT_BTN_A).kind == TARGET::NONE);
	CHECK(r.model.modified(2) && !r.model.modified(0) && !r.model.modified(1));

	// joystick buttons are the player's own jbutton_N_*
	r.model.setBinding(1, INPUT_BTN_Y, key("Esc"));
	r.model.setBinding(1, INPUT_BTN_Y, joy(1));
	CHECK(r.mapper.log.back() == "1:3=jbutton_1_1");
	CHECK(r.model.binding(1, INPUT_BTN_Y) == joy(1));
	r.model.setBinding(0, INPUT_BTN_Y, joy(0));
	CHECK(r.mapper.log.back() == "0:3=jbutton_0_0");
}

static void testJoystickLimit()
{
	{	// 2axis: two buttons each; a third is neither offered nor taken
		Rig r("2axis");
		const size_t before = r.mapper.log.size();
		r.model.setBinding(0, INPUT_TRIGGER_L, joy(2));
		r.model.setBinding(1, INPUT_TRIGGER_L, joy(3));
		CHECK(r.mapper.log.size() == before && r.mapper.saves == 0);
		CHECK(r.model.binding(0, INPUT_TRIGGER_L).kind == TARGET::NONE);
		r.model.setBinding(1, INPUT_TRIGGER_L, joy(1));
		CHECK(r.model.binding(1, INPUT_TRIGGER_L) == joy(1));
		CHECK(r.model.choices(0, TARGET::JOYSTICK).size() == 2);
		// players with no joystick are offered none
		CHECK(r.model.choices(2, TARGET::JOYSTICK).empty());
		r.model.setBinding(2, INPUT_TRIGGER_L, joy(0));
		CHECK(r.model.binding(2, INPUT_TRIGGER_L).kind == TARGET::NONE);
	}
	{	// 4axis: four for player 1, none for player 2
		Rig r("4axis");
		r.model.setBinding(0, INPUT_TRIGGER_L, joy(3));
		CHECK(r.model.binding(0, INPUT_TRIGGER_L) == joy(3));
		r.model.setBinding(1, INPUT_TRIGGER_L, joy(0));
		CHECK(r.model.binding(1, INPUT_TRIGGER_L).kind == TARGET::NONE);
	}
	{	// no joystick at all
		Rig r("none");
		r.model.setBinding(0, INPUT_TRIGGER_L, joy(0));
		CHECK(r.model.binding(0, INPUT_TRIGGER_L).kind == TARGET::NONE && r.mapper.log.empty());
		CHECK(!r.model.kindsOffered(0).empty());
		for(TARGET k : r.model.kindsOffered(0))
			CHECK(k != TARGET::JOYSTICK);
	}
}

// ---- presets

static void testPresets()
{
	Rig r("4axis");
	r.model.applyPreset(0, PRESET_ARROWS);
	CHECK(r.model.currentPreset(0) == PRESET_ARROWS);
	CHECK(r.mapper.saves == 1);
	// every button of player 1 has what the preset says, and no other player was touched
	for(int s = 0; s < PAD_SOURCE_COUNT; s++)
	{
		const int c = Pad_ControlOfSource(s);
		CHECK(r.model.binding(0, c) == Presets_Target(PRESET_ARROWS, c));
		CHECK(r.model.binding(1, c).kind == TARGET::NONE);
	}
	for(size_t i = 0; i < r.mapper.log.size(); i++)
		CHECK(r.mapper.log[i].compare(0, 2, "0:") == 0);
	CHECK(!r.model.modified(0));
	CHECK(r.model.defaultBinding(0, INPUT_BTN_A) == key("Ctrl"));

	// an edit makes it modified; reset puts the preset back, for that player only
	r.model.applyPreset(1, PRESET_SHOOTER);
	r.model.setBinding(0, INPUT_BTN_A, key("Z"));
	r.model.setBinding(1, INPUT_BTN_A, key("Z"));
	CHECK(r.model.modified(0) && r.model.modified(1));
	r.model.resetBindings(0);
	CHECK(!r.model.modified(0) && r.model.binding(0, INPUT_BTN_A) == key("Ctrl"));
	CHECK(r.model.modified(1) && r.model.binding(1, INPUT_BTN_A) == key("Z"));

	// reset with nothing changed writes nothing
	const size_t n = r.mapper.log.size();
	const int saves = r.mapper.saves;
	r.model.resetBindings(0);
	CHECK(r.mapper.log.size() == n && r.mapper.saves == saves);

	// applying a preset replaces edits
	r.model.applyPreset(1, PRESET_SHOOTER);
	CHECK(!r.model.modified(1) && r.model.binding(1, INPUT_BTN_A) == key("Ctrl"));
	CHECK_EQ(r.model.player(1).preset, "Arrows + shooter");
	CHECK_EQ(r.model.player(0).preset, "Arrow keys");

	CHECK(r.model.presetBinding(PRESET_POINT, INPUT_BTN_A) == BindingTarget(TARGET::MOUSE, 0));
	CHECK(r.model.presetBinding(99, INPUT_BTN_A).kind == TARGET::NONE);
	r.model.applyPreset(0, 99);
	r.model.applyPreset(9, 0);
	CHECK(r.model.currentPreset(0) == PRESET_ARROWS);
}

static void testCompatibleIsLimitedByTheJoystick()
{
	{	// 2axis: Compatible gives A and B joystick buttons 1 and 2 and cannot give X and Y a third and fourth
		Rig r("2axis");
		r.model.applyPreset(1, PRESET_ARROWS);
		r.model.applyPreset(1, PRESET_COMPATIBLE);
		CHECK(r.model.binding(1, INPUT_BTN_A) == joy(0) && r.model.binding(1, INPUT_BTN_B) == joy(1));
		CHECK(r.model.binding(1, INPUT_BTN_X).kind == TARGET::NONE && r.model.binding(1, INPUT_BTN_Y).kind == TARGET::NONE);
		CHECK(!r.model.modified(1));										// not "changed" because two buttons do not exist
		CHECK(r.model.defaultBinding(1, INPUT_BTN_X).kind == TARGET::NONE);	// the chooser's Default is what would be written
		CHECK(r.model.presetBinding(PRESET_COMPATIBLE, INPUT_BTN_X) == joy(2));	// the preset as listed is unchanged
	}
	{	// 4axis: a player with no joystick given Compatible gets no joystick buttons
		Rig r("4axis");
		r.model.applyPreset(2, PRESET_COMPATIBLE);
		CHECK(r.model.binding(2, INPUT_BTN_A).kind == TARGET::NONE);
		for(size_t i = 0; i < r.mapper.log.size(); i++)
			CHECK(r.mapper.log[i].find("jbutton") == std::string::npos);
	}
}

static void bindPreset(FakeMapper& m, int player, int preset)
{
	for(int s = 0; s < PAD_SOURCE_COUNT; s++)
	{
		std::string e;
		Mapper_EventFor(player, Presets_Target(preset, Pad_ControlOfSource(s)), e);
		if(!e.empty())
			m.bind(player, s, e.c_str());
	}
}

static void testPresetIsDerivedOnce()
{
	// the mapper file keeps binds, not the preset: a player whose buttons are a preset's reads as that preset
	Rig r("4axis");
	bindPreset(r.mapper, 3, PRESET_MENU);
	CHECK(r.model.currentPreset(3) == PRESET_MENU);
	CHECK(!r.model.modified(3));

	// one with a button changed is the closest preset, and modified
	Rig r2("4axis");
	bindPreset(r2.mapper, 2, PRESET_SHOOTER);
	r2.mapper.padBinds[2][1].clear();
	r2.mapper.bind(2, 1, "key_q");
	CHECK(r2.model.currentPreset(2) == PRESET_SHOOTER && r2.model.modified(2));

	// it stays what it was first taken to be, even as buttons change
	r2.model.setBinding(2, INPUT_BTN_A, key("A"));
	CHECK(r2.model.currentPreset(2) == PRESET_SHOOTER);
}

// ---- suggestions

static void testSuggestions()
{
	Rig r("2axis");
	std::vector<BindingTarget> s = r.model.suggestions(0, INPUT_BTN_A);
	CHECK(!s.empty());
	for(size_t i = 0; i < s.size(); i++)
		CHECK(r.model.usable(0, s[i]) && s[i].kind != TARGET::NONE);
	CHECK(r.model.suggestions(0, INPUT_BTN_HOME).empty());
	// a joystick button the player's joystick lacks is not suggested
	for(const BindingTarget& t : r.model.suggestions(0, INPUT_BTN_Y))
		CHECK(!(t == joy(3)));
	// a player with no joystick is suggested none
	for(const BindingTarget& t : r.model.suggestions(2, INPUT_BTN_A))
		CHECK(t.kind != TARGET::JOYSTICK);
}

// ---- saving

static void testSaving()
{
	Rig r("4axis");
	r.mapper.saveWorks = false;
	r.model.setBinding(0, INPUT_BTN_A, key("Ctrl"));
	CHECK(r.model.binding(0, INPUT_BTN_A) == key("Ctrl"));				// in effect
	CHECK(r.model.saveFailed());										// but the model says it was not kept
	r.mapper.saveWorks = true;
	r.model.setBinding(0, INPUT_BTN_B, key("Alt"));
	CHECK(!r.model.saveFailed());

	// a change the mapper refuses (no pad group yet) is not reported as a change
	Rig q("4axis");
	struct Refusing : FakeMapper { bool setEvent(int, int, const char *) override { return false; } } refusing;
	MapperControlsModel m(q.hw, refusing);
	m.setBinding(0, INPUT_BTN_A, key("Ctrl"));
	CHECK(!m.setupModified() && refusing.saves == 0);
	CHECK(m.binding(0, INPUT_BTN_A) == joy(0));
}


// ---- the emulated mode (D7c)

static std::string sourceEvent(Rig& r, int player, int control)
{
	const std::vector<std::string> e = r.mapper.events(player, Pad_SourceOfControl(control));
	return e.empty() ? std::string() : e[0];
}

static void testEmulatedMode()
{
	// each type is its own mode; auto reads as Joystick
	const struct { const char * type; int mode; } types[] = { { "4axis", 0 }, { "2axis", 1 }, { "none", 2 }, { "4axis_2", 3 }, { "fcs", 4 }, { "ch", 5 }, { "auto", 0 } };
	for(size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
	{
		Rig r(types[i].type);
		CHECK(r.model.currentEmulated() == types[i].mode);
	}

	// setEmulated sets the type through the mapper, and the model follows
	Rig r("4axis");
	r.model.setEmulated(1);
	CHECK_EQ(r.mapper.joystickType(), "2axis");
	CHECK(r.model.currentEmulated() == 1);
	CHECK(r.model.joystickSlot(0) == 0 && r.model.joystickSlot(1) == 1 && r.model.joystickSlot(2) == -1);
	CHECK_EQ(r.model.player(0).role, "Joystick 1");
	CHECK_EQ(r.model.player(1).role, "Joystick 2");
	CHECK_EQ(r.model.player(2).role, "Keys + mouse");
	CHECK(r.model.joystickButtons(1) == 2 && r.model.joystickAxes(1) == 2 && r.model.joystickButtons(2) == 0);
	r.model.setEmulated(2);
	CHECK_EQ(r.mapper.joystickType(), "none");
	CHECK_EQ(r.model.player(0).role, "Keys + mouse");
	r.model.setEmulated(0);
	CHECK_EQ(r.model.player(0).role, "Joystick 1");
	CHECK_EQ(r.model.player(1).role, "Keys + mouse");
	CHECK(r.model.joystickButtons(0) == 4 && r.model.joystickAxes(0) == 4);

	// an out of range index, or the mode already in effect, changes nothing
	const int saves = r.mapper.saves;
	r.model.setEmulated(0);
	r.model.setEmulated(9);
	r.model.setEmulated(-1);
	CHECK(r.model.currentEmulated() == 0 && r.mapper.saves == saves);

	// a type the mapper refuses leaves the mode as it was
	class Refuser : public FakeMapper { public: bool setJoystickType(const std::string&) override { return false; } };
	FakeHardware hw;
	Refuser refuser;
	MapperControlsModel m(hw, refuser);
	m.setEmulated(1);
	CHECK(m.currentEmulated() == 0 && refuser.saves == 0);
}

static void testAdvancedTypes()
{
	// 4axis_2: player 2 holds the one joystick, which is DOS joystick 1 (jbutton_0_*)
	Rig r("4axis_2");
	CHECK(r.model.currentEmulated() == 3);
	CHECK(r.model.joystickSlot(0) == -1 && r.model.joystickSlot(1) == 0);
	CHECK_EQ(r.model.player(0).role, "Keys + mouse");
	CHECK_EQ(r.model.player(1).role, "Joystick 1");
	CHECK(r.model.player(1).joystick == 1 && r.model.player(1).emulated == Emulated::JOYSTICK);
	CHECK(r.model.joystickButtons(1) == 4 && r.model.joystickButtons(0) == 0);
	// the shipped binds of player 2's stick group read as joystick buttons 1..4, player 1 has none
	CHECK(r.model.binding(1, INPUT_BTN_A) == joy(0) && r.model.binding(1, INPUT_BTN_Y) == joy(3));
	CHECK(r.model.binding(0, INPUT_BTN_A) == BindingTarget());
	// binding player 2's button writes jbutton_0_*, never jbutton_1_*
	r.model.setBinding(1, INPUT_TRIGGER_L, joy(2));
	CHECK_EQ(sourceEvent(r, 1, INPUT_TRIGGER_L), "jbutton_0_2");
	// player 1 has no joystick to offer
	r.model.setBinding(0, INPUT_TRIGGER_L, joy(0));
	CHECK(r.model.binding(0, INPUT_TRIGGER_L) == BindingTarget());

	// the flight sticks are player 1's, with a hat; CH has six buttons
	Rig f("fcs");
	CHECK(f.model.currentEmulated() == 4 && f.model.joystickSlot(0) == 0 && f.model.joystickSlot(1) == -1);
	CHECK(f.model.joystickButtons(0) == 4);
	Rig c("ch");
	CHECK(c.model.currentEmulated() == 5 && c.model.joystickButtons(0) == 6 && c.model.joystickAxes(0) == 4);
	CHECK(c.model.usable(0, joy(3)));
}

static void testMigrateBinds()
{
	// 2axis -> 4axis_2: player 2 had its own joystick (jbutton_1_*), now it is joystick 0
	Rig r("2axis");
	r.model.setBinding(1, INPUT_BTN_A, joy(1));
	CHECK_EQ(sourceEvent(r, 1, INPUT_BTN_A), "jbutton_1_1");
	r.model.setBinding(0, INPUT_BTN_B, joy(1));
	r.model.setBinding(0, INPUT_BTN_X, key("Ctrl"));
	r.model.setEmulated(3);
	CHECK_EQ(r.mapper.joystickType(), "4axis_2");
	CHECK_EQ(sourceEvent(r, 1, INPUT_BTN_A), "jbutton_0_1");			// moved to the player's own joystick
	CHECK(r.model.binding(1, INPUT_BTN_A) == joy(1));
	// player 1's bind would now fire player 2's joystick: removed
	CHECK(r.model.binding(0, INPUT_BTN_B) == BindingTarget());
	CHECK(r.model.binding(0, INPUT_BTN_X) == key("Ctrl"));			// keys are not touched
	CHECK(r.mapper.saves > 0);
	// nothing left to move
	CHECK(!Mapper_MigrateJoystickBinds(r.mapper));

	// 2axis -> 4axis: player 2's joystick buttons point at a joystick nobody drives: left alone, so 2axis finds them again
	Rig b("2axis");
	b.model.setBinding(1, INPUT_BTN_A, joy(1));
	b.model.setEmulated(0);
	CHECK_EQ(sourceEvent(b, 1, INPUT_BTN_A), "jbutton_1_1");
	CHECK(b.model.binding(1, INPUT_BTN_A) == BindingTarget());			// no joystick for player 2 now
	b.model.setEmulated(1);
	CHECK(b.model.binding(1, INPUT_BTN_A) == joy(1));

	// 4axis -> ch: a button the type has stays (six), 2axis -> 4axis: stays too
	Rig k("4axis");
	k.model.setBinding(0, INPUT_TRIGGER_L, joy(3));
	k.model.setEmulated(5);
	CHECK_EQ(sourceEvent(k, 0, INPUT_TRIGGER_L), "jbutton_0_3");

	// the Settings row path: the type changes behind the model's back, then the hook runs (what Apply does)
	Rig s("2axis");
	s.model.setBinding(1, INPUT_BTN_A, joy(1));
	s.mapper.setType("4axis_2");
	CHECK(s.model.currentEmulated() == 3);								// the model reads the type in effect, so roles follow
	CHECK_EQ(s.model.player(1).role, "Joystick 1");
	CHECK(Mapper_MigrateJoystickBinds(s.mapper));
	CHECK_EQ(sourceEvent(s, 1, INPUT_BTN_A), "jbutton_0_1");
}

static void testPresetFollowsType()
{
	// Compatible needs a joystick: with none it is not what the player has, so the closest usable preset is chosen again
	Rig r("4axis");
	CHECK(r.model.currentPreset(0) == PRESET_COMPATIBLE);
	r.model.setEmulated(2);
	CHECK(r.model.currentPreset(0) != PRESET_COMPATIBLE);
	CHECK(!Presets_All()[(size_t)r.model.currentPreset(0)].needsJoystick);
	// and from the Settings row
	Rig s("4axis");
	CHECK(s.model.currentPreset(0) == PRESET_COMPATIBLE);
	s.mapper.setType("none");
	CHECK(s.model.currentPreset(0) != PRESET_COMPATIBLE);
}

// ---- what is still a stand-in says so

static void testNoSetupHost()
{
	Rig r("2axis");
	CHECK(r.model.playerCount() == 4);
	CHECK(!r.model.setupModified());										// nothing changed since the start
	r.model.beginCapture();
	CaptureEvent e;
	CHECK(!r.model.pollCapture(e));
	CHECK(r.model.setups().empty());
	CHECK(!r.model.saveSetup("A", "") && !r.model.loadSetup(0));
	CHECK(r.model.runningProgram().empty() && r.model.setupName().empty());
}

// ---- players come from the hardware and the joystick type in effect

static void testPlayers()
{
	Rig r("2axis");
	PlayerInfo p = r.model.player(1);
	CHECK(p.connected && p.type == PadType::CLASSIC && p.emulated == Emulated::JOYSTICK && p.joystick == 2);
	CHECK_EQ(p.role, "Joystick 2");
	CHECK(r.model.player(2).emulated == Emulated::KEYS_MOUSE);
	CHECK(!r.model.player(3).connected && r.model.player(3).role.empty() && r.model.player(3).preset.empty());
	CHECK(r.model.held(1) == INPUT_BTN_A && r.model.held(0) == 0);
	CHECK(r.model.devices().keyboard);
	r.mapper.type = "4axis";
	CHECK(r.model.player(1).emulated == Emulated::KEYS_MOUSE);			// roles follow the joystick type, live
	r.mapper.type = "none";
	CHECK(r.model.player(0).emulated == Emulated::KEYS_MOUSE);
}

static void testInstall()
{
	Rig r("4axis");
	Controls_SetModel(&r.model);
	CHECK(&Controls() == &r.model);
	Controls().setBinding(0, INPUT_BTN_A, key("Ctrl"));
	CHECK_EQ(Controls().describe(0, INPUT_BTN_A), "Key: Ctrl");
	CHECK_EQ(Controls().describe(0, INPUT_BTN_B), "Joystick 2");
	Controls_SetModel(nullptr);
	CHECK(&Controls() != &r.model);
}


// ---- setups on disk

// A filesystem in memory: full path -> text. Fails on request, to see what a failed save leaves.
class FakeHost : public SetupHost
{
	public:
		std::string dir = "sd:/DOSBox/Setups";
		std::string mapPath = "sd:/DOSBox/mapper-0.74.map";
		std::string program;
		std::map<std::string, std::string> fs;
		bool madeDir = false, makeWorks = true;
		std::string failWriteOf;	// a write to a path with this in it fails

		std::string directory() const override { return dir; }
		bool makeDirectory() override { madeDir = makeWorks; return makeWorks; }
		std::vector<std::string> files() const override
		{
			std::vector<std::string> out;
			for(std::map<std::string, std::string>::const_iterator it = fs.begin(); it != fs.end(); ++it)
				if(it->first.compare(0, dir.size() + 1, dir + "/") == 0 && it->first.find('/', dir.size() + 1) == std::string::npos)
					out.push_back(it->first.substr(dir.size() + 1));
			return out;
		}
		bool read(const std::string& path, std::string& text) const override
		{
			std::map<std::string, std::string>::const_iterator it = fs.find(path);
			if(it == fs.end())
				return false;
			text = it->second;
			return true;
		}
		bool write(const std::string& path, const std::string& text) override
		{
			if(!failWriteOf.empty() && path.find(failWriteOf) != std::string::npos)
				return false;
			fs[path] = text;
			return true;
		}
		std::string mapperFile() const override { return mapPath; }
		std::string runningProgram() const override { return program; }
};

// The mapper file the fake groups would write (sdl_mapper.cpp's SaveBindsFile): a line per event, each bind quoted,
// the pad and stick binds of the sixteen sources among others the model does not manage.
static std::string mapText(const FakeMapper& m)
{
	std::map<std::string, std::vector<std::string> > byEvent;
	for(int p = 0; p < 4; p++)
		for(int s = 0; s < PAD_SOURCE_COUNT; s++)
			for(int g = 0; g < 2; g++)
			{
				const std::vector<std::string>& list = g == 0 ? m.padBinds[p][s] : m.stickBinds[p][s];
				for(size_t i = 0; i < list.size(); i++)
				{
					char bind[48];
					if(s < 12)
						snprintf(bind, sizeof(bind), "%s_%d button %d", g == 0 ? "pad" : "stick", p, s);
					else
						snprintf(bind, sizeof(bind), "%s_%d hat 0 %d", g == 0 ? "pad" : "stick", p, 1 << (s - 12));
					byEvent[list[i]].push_back(bind);
				}
			}
	std::string text = "key_a \"key 97\" \njaxis_0_0- \"stick_0 axis 0 -\" \n";
	for(std::map<std::string, std::vector<std::string> >::const_iterator it = byEvent.begin(); it != byEvent.end(); ++it)
	{
		text += it->first + " ";
		for(size_t i = 0; i < it->second.size(); i++)
			text += "\"" + it->second[i] + "\" ";
		text += "\n";
	}
	return text;
}

struct SRig
{
	FakeHardware hw;
	FakeMapper mapper;
	FakeHost host;
	MapperControlsModel model;
	SRig(const char * type = "4axis") : mapper(type), model(hw, mapper, &host)
	{
		mapper.onSave = [this] { host.fs[host.mapPath] = mapText(mapper); };
		mapper.onSave();	// the file exists from the start, as the mapper writes it when DOSBox first runs
		mapper.saves = 0;
	}
	std::string conf(const std::string& name) const { std::map<std::string, std::string>::const_iterator it = host.fs.find(host.dir + "/" + name + ".conf"); return it == host.fs.end() ? std::string("<missing>") : it->second; }
	bool has(const std::string& path) const { return host.fs.count(path) != 0; }
};

static void testConfFormat()
{
	SetupRecord r;
	r.name = "Keen 4";
	r.program = "KEEN4E.EXE";
	r.joystickType = "2axis";
	r.mapperFile = "Keen 4.map";
	r.presets[0] = "Arrow keys";
	r.presets[2] = "Doom";
	const std::string text = Setup_FormatConf(r);
	CHECK_EQ(text, "# DOSBox Wii setup\n# program=KEEN4E.EXE\n# preset.1=Arrow keys\n# preset.3=Doom\n[sdl]\nmapperfile=Keen 4.map\n[joystick]\njoysticktype=2axis\n");

	SetupRecord back;
	back.name = "Keen 4";
	CHECK(Setup_ParseConf(text, back));
	CHECK_EQ(back.name, "Keen 4");
	CHECK_EQ(back.program, "KEEN4E.EXE");
	CHECK_EQ(back.joystickType, "2axis");
	CHECK_EQ(back.mapperFile, "Keen 4.map");
	CHECK_EQ(back.presets[0], "Arrow keys");
	CHECK(back.presets[1].empty());
	CHECK_EQ(back.presets[2], "Doom");
	CHECK(back.presets[3].empty());

	// no program, no type: those lines are left out, and read back empty
	SetupRecord bare;
	bare.mapperFile = "b.map";
	const std::string t2 = Setup_FormatConf(bare);
	CHECK_EQ(t2, "# DOSBox Wii setup\n[sdl]\nmapperfile=b.map\n");
	SetupRecord b2;
	CHECK(Setup_ParseConf(t2, b2) && b2.program.empty() && b2.joystickType.empty() && b2.mapperFile == "b.map");

	// a conf that is not ours is not a setup, and leaves what was passed in alone
	SetupRecord keep;
	keep.name = "keep";
	keep.program = "X";
	CHECK(!Setup_ParseConf("[sdl]\nmapperfile=x.map\n", keep));
	CHECK(!Setup_ParseConf("", keep));
	CHECK(!Setup_ParseConf("# something else\n[sdl]\nmapperfile=x.map\n", keep));
	CHECK(keep.name == "keep" && keep.program == "X");

	// the card's editor may have made it CRLF, put blank lines first, quoted the value or changed the case
	SetupRecord edited;
	CHECK(Setup_ParseConf("\r\n# DOSBox Wii setup\r\n# Program = a.exe\r\n[SDL]\r\nMapperFile = \"my file.map\"\r\n[Joystick]\r\nJoystickType=4axis\r\n", edited));
	CHECK_EQ(edited.program, "a.exe");
	CHECK_EQ(edited.mapperFile, "my file.map");
	CHECK_EQ(edited.joystickType, "4axis");
	// a mapperfile line in another section is not the setting
	SetupRecord other;
	CHECK(Setup_ParseConf("# DOSBox Wii setup\n[cpu]\nmapperfile=no.map\n[sdl]\nmapperfile=yes.map\n", other) && other.mapperFile == "yes.map");

	// a value with a line break in it cannot start a setting of its own
	SetupRecord evil;
	evil.program = "A.EXE\n[cpu]\ncore=normal";
	evil.mapperFile = "e.map";
	SetupRecord evilBack;
	CHECK(Setup_ParseConf(Setup_FormatConf(evil), evilBack));
	CHECK(Setup_FormatConf(evil).find("\n[cpu]") == std::string::npos);

	// what DOSBox does with the file, from its sources: comment lines are skipped, and sdl.mapperfile and joystick.joysticktype exist
	const std::string setup = readFile("dosbox/misc/setup.cpp");
	const size_t parse = setup.find("bool Config::ParseConfigFile");
	CHECK(parse != std::string::npos && setup.find("case '#':", parse) != std::string::npos);
	CHECK(readFile("preferences.cpp").find("Add_path(\"mapperfile\"") != std::string::npos);
	CHECK(readFile("dosbox/hardware/joystick.cpp").find("\"joysticktype\"") != std::string::npos);
}

static void testPaths()
{
	CHECK(Setup_IsAbsolute("sd:/DOSBox/x.map") && Setup_IsAbsolute("usb2:/x.map") && Setup_IsAbsolute("dvd:x"));
	CHECK(!Setup_IsAbsolute("x.map") && !Setup_IsAbsolute("") && !Setup_IsAbsolute("a:/x") && !Setup_IsAbsolute("/x.map") && !Setup_IsAbsolute("sub/x:y"));
	CHECK_EQ(Setup_ResolveFile("sd:/DOSBox/Setups", "Keen 4.map"), "sd:/DOSBox/Setups/Keen 4.map");
	CHECK_EQ(Setup_ResolveFile("sd:/DOSBox/Setups", "usb:/maps/k.map"), "usb:/maps/k.map");
	CHECK_EQ(Setup_ResolveFile("sd:/DOSBox/Setups", ""), "");
	// Cross::IsPathAbsolute on this build is the same rule
	const std::string cross = readFile("dosbox/misc/cross.cpp");
	const size_t at = cross.find("bool Cross::IsPathAbsolute");
	CHECK(at != std::string::npos && cross.find("prefix > 1 && prefix < in.size() && in[prefix] == ':'", at) != std::string::npos);
	// and a relative mapperfile is looked for beside the conf (Prop_path::SetValue)
	const std::string setup = readFile("dosbox/misc/setup.cpp");
	CHECK(setup.find("realpath = current_config_dir + CROSS_FILESPLIT + workcopy;") != std::string::npos);
	CHECK(Setup_SameName("Keen", "KEEN") && !Setup_SameName("Keen", "Keen2") && Setup_SameName("", ""));
}

static void testProgramKey()
{
	CHECK_EQ(Setup_ProgramKey("KEEN4E.EXE"), "KEEN4E");
	CHECK_EQ(Setup_ProgramKey("c:\\games\\keen4e.exe"), "KEEN4E");
	CHECK_EQ(Setup_ProgramKey("Keen4E"), "KEEN4E");
	CHECK_EQ(Setup_ProgramKey("PRINCE.EXE"), "PRINCE");
	CHECK_EQ(Setup_ProgramKey("LONGNAME12.EXE"), "LONGNAME");	// DOS keeps 8
	CHECK_EQ(Setup_ProgramKey("A.B.EXE"), "A");					// the MCB stops at the first dot
	CHECK_EQ(Setup_ProgramKey(""), "");
	CHECK_EQ(Setup_ProgramKey("c:\\"), "");

	std::vector<SetupRecord> list(3);
	list[0].name = "A"; list[0].program = "DOOM.EXE";
	list[1].name = "B"; list[1].program = "keen4e.exe";
	list[2].name = "C"; list[2].program = "KEEN4E";
	CHECK(Setups_IndexForProgram(list, "KEEN4E.EXE") == 1);		// first by name wins
	CHECK(Setups_IndexForProgram(list, "Keen4e") == 1);
	CHECK(Setups_IndexForProgram(list, "doom") == 0);
	CHECK(Setups_IndexForProgram(list, "WOLF3D.EXE") == -1);
	CHECK(Setups_IndexForProgram(list, "") == -1);
	list[0].program.clear();
	CHECK(Setups_IndexForProgram(list, "") == -1);				// a setup with no program is not found by an empty one
}

static std::vector<unsigned char> bytes(const char * text, size_t n) { return std::vector<unsigned char>(text, text + n); }

static void testProgramFromEnvironment()
{
	// the layout DOS_Execute's MakeEnv writes: variables, an empty string, the word 1, the full path
	static const char withVars[] = "PATH=Z:\\\0COMSPEC=Z:\\COMMAND.COM\0\0\1\0C:\\GAMES\\Keen4e.exe\0";
	std::vector<unsigned char> env = bytes(withVars, sizeof(withVars));
	CHECK_EQ(Setup_ProgramFromEnvironment(&env[0], env.size()), "KEEN4E.EXE");
	// with room after it, as the block is bigger than what is written
	env.resize(env.size() + 40, 0);
	CHECK_EQ(Setup_ProgramFromEnvironment(&env[0], env.size()), "KEEN4E.EXE");
	// no variables at all: MakeEnv writes one zero, then the word
	static const char none[] = "\0\1\0D:\\X.COM\0";
	env = bytes(none, sizeof(none));
	CHECK_EQ(Setup_ProgramFromEnvironment(&env[0], env.size()), "X.COM");
	// a path with no end, a missing word, a block too short: nothing
	static const char cut[] = "A=1\0\0\1\0C:\\GAME.EXE";
	env = bytes(cut, sizeof(cut) - 1);
	CHECK_EQ(Setup_ProgramFromEnvironment(&env[0], env.size()), "");
	static const char noWord[] = "A=1\0\0\2\0C:\\GAME.EXE\0";
	env = bytes(noWord, sizeof(noWord));
	CHECK_EQ(Setup_ProgramFromEnvironment(&env[0], env.size()), "");
	static const char noEnd[] = "A=1\0B=2";
	env = bytes(noEnd, sizeof(noEnd) - 1);
	CHECK_EQ(Setup_ProgramFromEnvironment(&env[0], env.size()), "");
	CHECK_EQ(Setup_ProgramFromEnvironment(nullptr, 10), "");
	CHECK_EQ(Setup_ProgramFromEnvironment(&env[0], 2), "");
	// the empty path is no program
	static const char emptyPath[] = "\0\1\0\0";
	env = bytes(emptyPath, sizeof(emptyPath));
	CHECK_EQ(Setup_ProgramFromEnvironment(&env[0], env.size()), "");

	// MakeEnv in the tree is what writes it, in this order
	const std::string exec = readFile("dosbox/dos/dos_execute.cpp");
	const size_t make = exec.find("static bool MakeEnv");
	const size_t word = exec.find("mem_writew(envwrite,1);", make);
	const size_t path = exec.find("DOS_Canonicalize(name,namebuf)", make);
	CHECK(make != std::string::npos && word != std::string::npos && path != std::string::npos && word < path);
	CHECK(exec.find("MakeEnv(name,&envseg)") != std::string::npos);
}

static void testParsePadBinds()
{
	const std::string text =
		"key_a \"key 97\" \"pad_0 button 4\" \n"
		"key_b \"stick_1 button 4\" \n"								// the stick group's bind for player 2's fifth source
		"jbutton_0_0 \"stick_0 button 0\" \"key 306 mod1\" \"pad_2 button 0 mod2\" \n"	// flags and other groups around it
		"hand_cycleup \"pad_0 hat 0 1\" \n"							// D-pad up
		"hand_cycledown \"pad_3 hat 0 8\" \n"						// D-pad left
		"jhat_0_0_1 \"stick_0 hat 0 2\" \n"
		"jaxis_0_0- \"stick_0 axis 0 -\" \n"							// not a source
		"key_c \"pad_4 button 1\" \"pad_0 button 12\" \"pad_0 hat 1 1\" \"pad_0 button x\" \n"	// player 5, button 12, hat 1, junk
		"\r\n"
		"nonsense\n"
		"key_d \"pad_0 button 4\" \r\n";
	MapPadBinds b;
	Setup_ParsePadBinds(text, b);
	CHECK(b.events[0][4].size() == 2 && b.events[0][4][0] == "key_a" && b.events[0][4][1] == "key_d");	// file order
	CHECK(b.events[1][4].size() == 1 && b.events[1][4][0] == "key_b");
	CHECK(b.events[0][0].size() == 1 && b.events[0][0][0] == "jbutton_0_0");
	CHECK(b.events[2][0].size() == 1 && b.events[2][0][0] == "jbutton_0_0");
	CHECK(b.events[0][12].size() == 1 && b.events[0][12][0] == "hand_cycleup");
	CHECK(b.events[3][15].size() == 1 && b.events[3][15][0] == "hand_cycledown");
	CHECK(b.events[0][13].size() == 1 && b.events[0][13][0] == "jhat_0_0_1");
	size_t total = 0;
	for(int p = 0; p < 4; p++)
		for(int s = 0; s < PAD_SOURCE_COUNT; s++)
			total += b.events[p][s].size();
	CHECK(total == 8);

	// parsing again starts from nothing
	Setup_ParsePadBinds("", b);
	total = 0;
	for(int p = 0; p < 4; p++)
		for(int s = 0; s < PAD_SOURCE_COUNT; s++)
			total += b.events[p][s].size();
	CHECK(total == 0);

	// what the fake writes is what is read back: every pad and stick bind, on its own source
	FakeMapper m("2axis");
	m.bind(0, 3, "key_x");
	m.bind(1, 15, "hand_incfskip");
	m.bind(2, 11, "key_z");
	Setup_ParsePadBinds(mapText(m), b);
	for(int p = 0; p < 4; p++)
		for(int s = 0; s < PAD_SOURCE_COUNT; s++)
		{
			std::vector<std::string> want = m.events(p, s), got = b.events[p][s];
			std::sort(want.begin(), want.end());
			std::sort(got.begin(), got.end());
			CHECK(want == got);
		}

	// the grammar is the mapper's: CPadBindGroup and the stick group write "<group> button B" and "<group> hat 0 <mask>"
	const std::string mapper = readFile("dosbox/gui/sdl_mapper.cpp");
	CHECK(mapper.find("sprintf(line,\"%s \\\"pad_%d button %d\\\" \",event,pad,source)") != std::string::npos);
	CHECK(mapper.find("sprintf(line,\"%s \\\"pad_%d hat 0 %d\\\" \",event,pad,(int)CPadBindGroup::DirMask(source-PAD_BUTTONS))") != std::string::npos);
	CHECK(mapper.find("sprintf(buf,\"%s button %d\",group->ConfigStart(),(int)button);") != std::string::npos);
	CHECK(mapper.find("sprintf(buf,\"%s hat 0 %d\",group->ConfigStart(),dir);") != std::string::npos);
	CHECK(mapper.find("static Bit8u DirMask(Bitu d) { return (Bit8u)(1<<d); }") != std::string::npos);
	// the shipped binds the fake gives stick 0's hat, which a load must recognise as the D-pad sources
	CHECK(mapper.find("jhat_0_0_0 \\\"stick_0 hat 0 1\\\"") != std::string::npos);
	CHECK(mapper.find("jhat_0_0_3 \\\"stick_0 hat 0 8\\\"") != std::string::npos);
}

static void testNothingListedWithoutOurs()
{
	SRig r;
	r.host.fs[r.host.dir + "/Foreign.conf"] = "[sdl]\nmapperfile=x.map\n";
	r.host.fs[r.host.dir + "/Foreign.map"] = "key_a \"key 97\"\n";
	r.host.fs[r.host.dir + "/Old.conf.bak"] = Setup_FormatConf(SetupRecord());
	r.host.fs[r.host.dir + "/Old.conf.tmp"] = Setup_FormatConf(SetupRecord());
	r.host.fs[r.host.dir + "/notes.txt"] = "hello";
	r.host.fs[r.host.dir + "/sub/Deep.conf"] = Setup_FormatConf(SetupRecord());
	r.host.fs["sd:/DOSBox/dosbox.conf"] = Setup_FormatConf(SetupRecord());	// next to the folder, not in it
	CHECK(r.model.setups().empty());
}

static void testSaveSetup()
{
	SRig r("4axis");
	r.hw.began = 0;
	r.model.setBinding(0, INPUT_BTN_A, key("Ctrl"));
	r.model.setBinding(1, INPUT_BTN_X, key("Space"));
	CHECK(r.model.setupModified());
	const int savesBefore = r.mapper.saves;

	CHECK(r.model.saveSetup("Keen 4", "c:\\games\\keen4e.exe") == true);
	CHECK(r.host.madeDir);
	CHECK(r.mapper.saves == savesBefore + 1);											// the mapper file was written first
	const std::string mapPath = r.host.dir + "/Keen 4.map";
	CHECK(r.has(mapPath) && r.host.fs[mapPath] == mapText(r.mapper));					// and is what is in the .map
	CHECK(r.host.fs[mapPath].find("\"pad_0 button 0\"") != std::string::npos);		// with player 1's A in it
	SetupRecord rec;
	CHECK(Setup_ParseConf(r.conf("Keen 4"), rec));
	CHECK_EQ(rec.mapperFile, "Keen 4.map");											// by name: DOSBox looks beside the conf
	CHECK_EQ(rec.joystickType, "4axis");
	CHECK_EQ(rec.program, "KEEN4E.EXE");											// the file name, upper case
	CHECK(!rec.presets[0].empty() && !rec.presets[1].empty() && !rec.presets[2].empty());	// the three connected players
	CHECK(rec.presets[3].empty());												// the fourth is not
	CHECK_EQ(rec.presets[0], r.model.presets()[(size_t)r.model.currentPreset(0)].name);
	CHECK_EQ(r.model.setupName(), "Keen 4");
	CHECK(!r.model.setupModified());
	CHECK(r.host.fs.size() == 3);													// the mapper file, .map, .conf: nothing else left behind

	// the list shows it
	const std::vector<SetupInfo> list = r.model.setups();
	CHECK(list.size() == 1);
	CHECK_EQ(list[0].name, "Keen 4");
	CHECK_EQ(list[0].program, "KEEN4E.EXE");
	CHECK_EQ(list[0].emulated, "Joystick");

	// the same name again replaces it (not a second setup), and with a different program
	r.model.setBinding(0, INPUT_BTN_B, key("Alt"));
	CHECK(r.model.setupModified());
	CHECK(r.model.saveSetup("Keen 4", ""));
	CHECK(r.model.setups().size() == 1 && r.model.setups()[0].program.empty());
	CHECK(r.host.fs[mapPath].find("key_lalt") != std::string::npos);
	CHECK(!r.model.setupModified());
	// no program: the line is not written
	CHECK(r.conf("Keen 4").find("program") == std::string::npos);

	// the type is the one in effect, which is not always the first
	r.model.setEmulated(1);
	CHECK(r.model.saveSetup("Two", ""));
	SetupRecord two;
	CHECK(Setup_ParseConf(r.conf("Two"), two) && two.joystickType == "2axis");
	CHECK_EQ(r.model.setups()[1].emulated, "Two joysticks");
}

static void testSaveSetupFails()
{
	{	// a name the file system cannot hold, or none
		SRig r;
		CHECK(!r.model.saveSetup("", "") && !r.model.saveSetup("a/b", "") && !r.model.saveSetup("a.b", "") && !r.model.saveSetup(" x", ""));
		CHECK(r.host.fs.size() == 1 && r.model.setupName().empty());
	}
	{	// no storage
		SRig r;
		r.host.dir.clear();
		CHECK(!r.model.saveSetup("A", ""));
		SRig q;
		q.host.makeWorks = false;
		CHECK(!q.model.saveSetup("A", "") && q.host.fs.size() == 1);
	}
	{	// the mapper cannot write its file: nothing is saved, and the model says so
		SRig r;
		r.mapper.saveWorks = false;
		CHECK(!r.model.saveSetup("A", ""));
		CHECK(r.model.saveFailed());
		CHECK(r.host.fs.size() == 1 && r.model.setupName().empty());
	}
	{	// the mapper file cannot be read back
		SRig r;
		r.host.fs.erase(r.host.mapPath);
		r.mapper.onSave = nullptr;
		CHECK(!r.model.saveSetup("A", "") && r.host.fs.empty());
		SRig q;
		q.host.mapPath.clear();
		CHECK(!q.model.saveSetup("A", ""));
	}
	{	// the .map is written, the .conf is not: no setup is listed, and the model is as it was
		SRig r;
		r.model.setBinding(0, INPUT_BTN_A, key("Ctrl"));
		r.host.failWriteOf = ".conf";
		CHECK(!r.model.saveSetup("A", ""));
		CHECK(r.model.setups().empty() && r.model.setupName().empty() && r.model.setupModified());
		r.host.failWriteOf = ".map";
		CHECK(!r.model.saveSetup("B", ""));
		CHECK(r.model.setups().empty());
	}
	{	// no host at all
		FakeHardware hw;
		FakeMapper m;
		MapperControlsModel model(hw, m);
		CHECK(!model.saveSetup("A", "") && model.setups().empty() && !model.loadSetup(0) && model.runningProgram().empty());
	}
}

static void testListSetups()
{
	SRig r;
	SetupRecord a;
	a.mapperFile = "a.map";
	a.joystickType = "4axis";
	a.presets[0] = "Arrow keys"; a.presets[1] = "Arrow keys";
	r.host.fs[r.host.dir + "/beta.conf"] = Setup_FormatConf(a);
	a.joystickType = "2axis";
	a.presets[1] = "Doom";
	a.program = "WOLF3D.EXE";
	r.host.fs[r.host.dir + "/Alpha.conf"] = Setup_FormatConf(a);
	a.joystickType = "";
	a.presets[0].clear(); a.presets[1].clear();
	r.host.fs[r.host.dir + "/Charlie.CONF"] = Setup_FormatConf(a);
	a.joystickType = "hovercraft";
	r.host.fs[r.host.dir + "/delta.conf"] = Setup_FormatConf(a);
	const std::vector<SetupInfo> list = r.model.setups();
	CHECK(list.size() == 4);
	CHECK_EQ(list[0].name, "Alpha");	// by name, ignoring case
	CHECK_EQ(list[1].name, "beta");
	CHECK_EQ(list[2].name, "Charlie");
	CHECK_EQ(list[3].name, "delta");
	CHECK_EQ(list[0].emulated, "Two joysticks");
	CHECK_EQ(list[0].preset, "Mixed");
	CHECK_EQ(list[0].program, "WOLF3D.EXE");
	CHECK_EQ(list[1].emulated, "Joystick");
	CHECK_EQ(list[1].preset, "Arrow keys");
	CHECK_EQ(list[2].emulated, "As now");
	CHECK_EQ(list[2].preset, "Not recorded");
	CHECK_EQ(list[3].emulated, "hovercraft");
}

// A player's pad button by event
static std::string ev(const SRig& r, int pad, int source)
{
	const std::vector<std::string> e = r.mapper.events(pad, source);
	return e.empty() ? std::string("-") : e[0];
}

static void testLoadSetup()
{
	SRig r("4axis");
	const std::string shippedHat = ev(r, 0, 12);
	CHECK_EQ(shippedHat, "jhat_0_0_0");
	r.model.setBinding(0, INPUT_BTN_A, key("Ctrl"));
	r.model.setBinding(0, INPUT_BTN_Y, BindingTarget(TARGET::MOUSE, 1));
	r.model.setBinding(1, INPUT_BTN_B, key("Enter"));
	r.model.applyPreset(2, PRESET_ARROWS);
	const int presetOne = r.model.currentPreset(0);
	CHECK(r.model.saveSetup("Mine", "KEEN4E.EXE"));

	// change everything: another type, other buttons, another preset
	r.model.setEmulated(1);
	r.model.setBinding(0, INPUT_BTN_A, key("Esc"));
	r.model.setBinding(0, INPUT_BTN_X, key("Tab"));
	r.model.setBinding(1, INPUT_BTN_B, BindingTarget());
	r.model.applyPreset(0, PRESET_COMPATIBLE);
	CHECK(r.model.setupModified() && r.model.currentEmulated() == 1);

	r.mapper.log.clear();
	const int saves = r.mapper.saves;
	CHECK(r.model.loadSetup(0));
	CHECK_EQ(r.mapper.joystickType(), "4axis");											// the type first, then the buttons
	CHECK(r.model.currentEmulated() == 0);
	CHECK(r.model.binding(0, INPUT_BTN_A) == key("Ctrl"));
	CHECK(r.model.binding(0, INPUT_BTN_Y) == BindingTarget(TARGET::MOUSE, 1));
	CHECK(r.model.binding(1, INPUT_BTN_B) == key("Enter"));
	CHECK(r.model.binding(0, INPUT_BTN_X) != key("Tab"));
	CHECK_EQ(r.model.setupName(), "Mine");
	CHECK(!r.model.setupModified());
	CHECK(r.mapper.saves == saves + 1);													// written to the mapper file as any change is
	CHECK(!r.model.saveFailed());
	CHECK(r.model.currentPreset(0) == presetOne);											// back as it was saved, not recomputed
	CHECK(r.model.currentPreset(2) == PRESET_ARROWS);
	// a source that already had what the map gives it is not touched: the shipped joystick bind stays the stick group's
	CHECK_EQ(ev(r, 0, 12), shippedHat);
	bool touchedHat = false;
	for(size_t i = 0; i < r.mapper.log.size(); i++)
		touchedHat = touchedHat || r.mapper.log[i].compare(0, 5, "0:12=") == 0;
	CHECK(!touchedHat);

	// loading it again changes nothing at all
	r.mapper.log.clear();
	CHECK(r.model.loadSetup(0));
	CHECK(r.mapper.log.empty());
}

static void testLoadTypeAndJoystickBinds()
{
	// saved under two joysticks: player 2's button is jbutton_1_0
	SRig r("2axis");
	r.model.setBinding(1, INPUT_BTN_A, joy(0));
	r.model.setBinding(1, INPUT_BTN_B, joy(1));
	CHECK_EQ(ev(r, 1, 0), "jbutton_1_0");
	CHECK(r.model.saveSetup("Duel", ""));
	r.model.setEmulated(0);		// back to one joystick
	CHECK_EQ(r.mapper.joystickType(), "4axis");
	CHECK(r.model.binding(1, INPUT_BTN_A).kind != TARGET::JOYSTICK);
	CHECK(r.model.loadSetup(0));
	CHECK_EQ(r.mapper.joystickType(), "2axis");
	CHECK(r.model.binding(1, INPUT_BTN_A) == joy(0) && r.model.binding(1, INPUT_BTN_B) == joy(1));
	CHECK_EQ(ev(r, 1, 0), "jbutton_1_0");

	// saved under 4axis_2 (player 2 holds the one joystick), loaded under 4axis: the binds follow the type that comes with the setup
	SRig q("4axis_2");
	q.model.setBinding(1, INPUT_BTN_A, joy(2));
	CHECK(q.model.saveSetup("Second", ""));
	q.model.setEmulated(0);
	CHECK(q.model.loadSetup(0));
	CHECK_EQ(q.mapper.joystickType(), "4axis_2");
	CHECK(q.model.binding(1, INPUT_BTN_A) == joy(2));
	CHECK(q.model.joystickSlot(1) == 0 && q.model.joystickSlot(0) == -1);

	// a setup with no type keeps the one in effect
	SRig n("2axis");
	SetupRecord rec;
	rec.mapperFile = "n.map";
	n.host.fs[n.host.dir + "/n.conf"] = Setup_FormatConf(rec);
	n.host.fs[n.host.dir + "/n.map"] = "key_a \"pad_0 button 0\" \n";
	CHECK(n.model.loadSetup(0));
	CHECK_EQ(n.mapper.joystickType(), "2axis");
	CHECK(n.model.binding(0, INPUT_BTN_A) == key("A"));
}

static void testLoadSetupFails()
{
	SetupRecord rec;
	rec.mapperFile = "x.map";
	rec.joystickType = "2axis";
	const std::string conf = Setup_FormatConf(rec);
	const std::string map = "key_a \"pad_0 button 0\" \n";
	{	// the map is missing: nothing changes, not even the type
		SRig r("4axis");
		r.host.fs[r.host.dir + "/x.conf"] = conf;
		r.mapper.log.clear();
		CHECK(!r.model.loadSetup(0));
		CHECK(r.mapper.log.empty() && r.mapper.joystickType() == "4axis" && r.model.setupName().empty());
	}
	{	// no map named in the conf
		SRig r;
		r.host.fs[r.host.dir + "/x.conf"] = Setup_FormatConf(SetupRecord());
		CHECK(!r.model.loadSetup(0));
	}
	{	// a type that is not one of ours (DOSBox would take a bad value as its default): refused before anything is set
		SRig r("4axis");
		for(const char * bad : { "auto", "bogus", "2AXIS" })
		{
			rec.joystickType = bad;
			r.host.fs[r.host.dir + "/x.conf"] = Setup_FormatConf(rec);
			r.host.fs[r.host.dir + "/x.map"] = map;
			r.mapper.log.clear();
			CHECK(!r.model.loadSetup(0));
			CHECK(r.mapper.log.empty() && r.mapper.joystickType() == "4axis");
		}
		rec.joystickType = "2axis";
	}
	{	// DOSBox does not take the type
		SRig r("4axis");
		r.host.fs[r.host.dir + "/x.conf"] = conf;
		r.host.fs[r.host.dir + "/x.map"] = map;
		struct Refuser : FakeMapper { bool setJoystickType(const std::string&) override { return false; } };
		Refuser refuser;
		refuser.onSave = r.mapper.onSave;
		MapperControlsModel m(r.hw, refuser, &r.host);
		refuser.log.clear();
		CHECK(!m.loadSetup(0) && refuser.log.empty() && m.setupName().empty());
	}
	{	// out of range
		SRig r;
		CHECK(!r.model.loadSetup(0) && !r.model.loadSetup(-1));
		r.host.fs[r.host.dir + "/x.conf"] = conf;
		r.host.fs[r.host.dir + "/x.map"] = map;
		CHECK(!r.model.loadSetup(1) && !r.model.loadSetup(-1));
	}
	{	// the mapper does not take a bind: the load says so, and the setup is not the one loaded
		SRig r("4axis");
		SetupRecord plain;
		plain.mapperFile = "x.map";
		r.host.fs[r.host.dir + "/x.conf"] = Setup_FormatConf(plain);
		r.host.fs[r.host.dir + "/x.map"] = map;
		struct Picky : FakeMapper { bool setEvent(int p, int s, const char * e) override { return p == 0 && s == 0 ? false : FakeMapper::setEvent(p, s, e); } };
		Picky picky;
		picky.onSave = r.mapper.onSave;
		MapperControlsModel m(r.hw, picky, &r.host);
		CHECK(!m.loadSetup(0));
		CHECK(m.setupName().empty() && m.setupModified());
	}
}

static void testLoadReplacesWhatTheMapDoesNotSay()
{
	// a source the map has nothing for is left unbound, not kept: the setup is the whole of the buttons
	SRig r("4axis");
	r.model.setBinding(0, INPUT_BTN_A, key("Ctrl"));
	r.model.setBinding(3, INPUT_BTN_X, key("Space"));
	SetupRecord rec;
	rec.mapperFile = "m.map";
	r.host.fs[r.host.dir + "/m.conf"] = Setup_FormatConf(rec);
	r.host.fs[r.host.dir + "/m.map"] =
		"key_enter \"pad_0 button 1\" \n"
		"key_nosuchkey \"pad_0 button 2\" \n"				// the mapper has no such event: the source is left unbound
		"key_a \"pad_0 hat 0 2\" \"pad_0 button 3\" \n"
		"hand_cycleup \"pad_1 button 3\" \"pad_1 button 4\" \n"
		"jhat_0_0_0 \"pad_0 button 5\" \n"					// an event the catalogs cannot hold, but the mapper has: kept as it is
		"key_space \"pad_1 button 5\" \"pad_0 button 6\" \n"
		"jbutton_0_0 \"pad_1 button 6\" \"key_b\" \n"		// player 1's joystick button on player 2, who has no joystick under 4axis: removed
		"jhat_0_0_1 \"pad_0 button 7\" \n"					// two events on one source: the one the catalogs can hold wins over an earlier one they cannot
		"key_enter \"pad_0 button 7\" \n";
	CHECK(r.model.loadSetup(0));
	CHECK(r.model.binding(0, INPUT_BTN_A) == BindingTarget());
	CHECK(r.model.binding(3, INPUT_BTN_X) == BindingTarget());
	CHECK(r.model.binding(0, INPUT_BTN_B) == key("Enter"));
	CHECK(r.model.binding(0, INPUT_BTN_X) == BindingTarget());
	CHECK_EQ(ev(r, 0, 2), "-");
	CHECK_EQ(ev(r, 0, 13), "key_a");
	CHECK(r.model.binding(1, INPUT_BTN_Y) == BindingTarget(TARGET::HOTKEY, 0));
	CHECK_EQ(ev(r, 0, 5), "jhat_0_0_0");
	CHECK(r.model.binding(0, INPUT_TRIGGER_R) == BindingTarget());			// shown as Not assigned, left alone
	CHECK_EQ(ev(r, 1, 6), "-");
	CHECK_EQ(ev(r, 0, 7), "key_enter");
}

static void testRunningProgram()
{
	SRig r;
	CHECK(r.model.runningProgram().empty());
	r.host.program = "KEEN4E.EXE";
	CHECK_EQ(r.model.runningProgram(), "KEEN4E.EXE");
	// only DOS's short name: nothing saved for it, so that is all there is
	r.host.program = "KEEN4E";
	CHECK_EQ(r.model.runningProgram(), "KEEN4E");
	// a setup saved for it knows the full name
	CHECK(r.model.saveSetup("Keen", "keen4e.exe"));
	CHECK_EQ(r.model.runningProgram(), "KEEN4E.EXE");
	r.host.program = "KEEN4E.COM";			// a name that has an extension is taken as it is
	CHECK_EQ(r.model.runningProgram(), "KEEN4E.COM");
	r.host.program = "DOOM";
	CHECK_EQ(r.model.runningProgram(), "DOOM");
	r.host.program.clear();
	CHECK(r.model.runningProgram().empty());
	// the Save flow's suggested name follows it
	CHECK_EQ(Setup_SuggestedName("KEEN4E.EXE"), "KEEN4E");
}

static void testSetupFileOwnMapper()
{
	// DOSBox was started with a setup's conf: its mapper file is one of the setups, and nothing writes into it behind the user's back
	SRig r("4axis");
	r.model.setBinding(0, INPUT_BTN_A, key("Ctrl"));
	CHECK(r.model.saveSetup("A", ""));
	CHECK(r.model.saveSetup("B", ""));
	r.host.mapPath = r.host.dir + "/A.map";
	const std::string aMap = r.host.fs[r.host.mapPath];

	int saves = r.mapper.saves;
	r.model.setBinding(0, INPUT_BTN_B, key("Alt"));
	CHECK(r.mapper.saves == saves);													// held in the running mapper
	CHECK(r.model.setupModified() && !r.model.saveFailed());
	CHECK(r.host.fs[r.host.mapPath] == aMap);

	CHECK(r.model.loadSetup(1));													// loading B does not overwrite A's file
	CHECK(r.mapper.saves == saves);
	CHECK(r.host.fs[r.host.mapPath] == aMap && !r.model.saveFailed());
	CHECK_EQ(r.model.setupName(), "B");

	// saving does write the file: it is the only way to get the running binds out
	r.model.setBinding(0, INPUT_BTN_X, key("Tab"));
	CHECK(r.model.saveSetup("C", ""));
	CHECK(r.mapper.saves == saves + 1);

	// a mapper file beside the setups, or one whose folder merely starts the same, is not one of them
	r.host.mapPath = "sd:/DOSBox/Setups2/A.map";
	saves = r.mapper.saves;
	r.model.setBinding(0, INPUT_BTN_Y, key("Enter"));
	CHECK(r.mapper.saves == saves + 1);
	r.host.mapPath = r.host.dir;
	r.model.setBinding(0, INPUT_BTN_Y, key("Esc"));
	CHECK(r.mapper.saves == saves + 2);
	r.host.mapPath = "SD:/dosbox/setups/A.map";									// FAT ignores case
	saves = r.mapper.saves;
	r.model.setBinding(0, INPUT_BTN_Y, key("Tab"));
	CHECK(r.mapper.saves == saves);
}

static void testCaptureThroughHardware()
{
	SRig r;
	r.model.beginCapture();
	CHECK(r.hw.began == 1);
	CaptureEvent e;
	CHECK(!r.model.pollCapture(e));
	CaptureEvent keyPress = { TARGET::KEY, Catalog_Find(TARGET::KEY, "Ctrl") };
	CaptureEvent mouse = { TARGET::MOUSE, 1 };
	r.hw.pending.push_back(keyPress);
	r.hw.pending.push_back(mouse);
	CHECK(r.model.pollCapture(e) && e.kind == TARGET::KEY && e.code == keyPress.code);
	CHECK(r.model.pollCapture(e) && e.kind == TARGET::MOUSE && e.code == 1);
	CHECK(!r.model.pollCapture(e));
	r.hw.pending.push_back(mouse);
	r.model.beginCapture();															// what was pending before is dropped
	CHECK(r.hw.began == 2 && !r.model.pollCapture(e));

	// hardware that does not capture (the default) never reports a press
	struct Deaf : ControlsHardware
	{
		PlayerHardware player(int) const override { return PlayerHardware(); }
		DeviceStatus devices() const override { return DeviceStatus(); }
		uint32_t held(int) const override { return 0; }
	} deaf;
	FakeMapper m;
	MapperControlsModel model(deaf, m);
	model.beginCapture();
	CHECK(!model.pollCapture(e));
}

// The files half of a host, on a real folder
class TempHost : public FileSetupHost
{
	public:
		explicit TempHost(const std::string& d) : FileSetupHost(d) {}
		std::string mapperFile() const override { return std::string(); }
		std::string runningProgram() const override { return std::string(); }
};

static void testFileHost()
{
	char tmpl[] = "/tmp/setupsXXXXXX";
	const std::string root = mkdtemp(tmpl) ? tmpl : "/tmp";
	{
		// the folder is made, with its parent when that is missing, and not further
		TempHost host(root + "/DOSBox/Setups");
		CHECK(host.makeDirectory());
		CHECK(host.makeDirectory());												// again: already there
		TempHost deep(root + "/a/b/c");
		CHECK(!deep.makeDirectory());
		TempHost none((std::string()));
		CHECK(!none.makeDirectory() && none.files().empty());

		const std::string path = root + "/DOSBox/Setups/One.conf";
		std::string text;
		CHECK(!host.read(path, text));
		CHECK(host.write(path, "first\nline"));
		CHECK(host.read(path, text) && text == "first\nline");
		CHECK(host.write(path, "second"));											// replaced
		CHECK(host.read(path, text) && text == "second");
		CHECK(host.write(root + "/DOSBox/Setups/Two.map", std::string(5000, 'x')));	// more than one read's worth
		CHECK(host.read(root + "/DOSBox/Setups/Two.map", text) && text.size() == 5000);
		CHECK(host.write(root + "/DOSBox/Setups/Empty.map", "") && host.read(root + "/DOSBox/Setups/Empty.map", text) && text.empty());

		// a write cut short left a .tmp: it does not get in the way, and nothing is left behind
		{ FILE * f = fopen((path + ".tmp").c_str(), "wb"); fputs("junk", f); fclose(f); }
		CHECK(host.write(path, "third") && host.read(path, text) && text == "third");
		std::vector<std::string> names = host.files();
		std::sort(names.begin(), names.end());
		CHECK(names.size() == 3 && names[0] == "Empty.map" && names[1] == "One.conf" && names[2] == "Two.map");

		// the folder is not there: no write, and no file made elsewhere
		TempHost gone(root + "/nowhere");
		CHECK(!gone.write(root + "/nowhere/x.conf", "x") && gone.files().empty());

		// listing and loading over real files, with a host that knows the mapper file
		struct Real : TempHost
		{
			Real(const std::string& d, const std::string& m) : TempHost(d), mapPath(m) {}
			std::string mapperFile() const override { return mapPath; }
			std::string mapPath;
		} real(root + "/DOSBox/Setups", root + "/DOSBox/mapper.map");
		FakeHardware hw;
		FakeMapper mapper("4axis");
		MapperControlsModel model(hw, mapper, &real);
		mapper.onSave = [&] { real.write(real.mapPath, mapText(mapper)); };
		model.setBinding(0, INPUT_BTN_A, key("Ctrl"));
		CHECK(model.saveSetup("Real one", "GAME.EXE"));
		CHECK(real.read(root + "/DOSBox/Setups/Real one.conf", text) && text.find("mapperfile=Real one.map") != std::string::npos);
		model.setBinding(0, INPUT_BTN_A, key("Esc"));
		CHECK(model.setups().size() == 1);
		CHECK(model.loadSetup(0) && model.binding(0, INPUT_BTN_A) == key("Ctrl"));
	}
	std::string cleanup = "rm -rf '" + root + "'";
	CHECK(system(cleanup.c_str()) == 0);

	// the live host keeps the folder beside dosbox.conf, where Cross::GetPlatformConfigDir puts the config
	const std::string live = readFile("menu/controlssetupslive.cpp"), cross = readFile("dosbox/misc/cross.cpp");
	CHECK(live.find("std::string(appDrive) + \"/\" DOSBOX_DIR_NAME \"/Setups\"") != std::string::npos);
	CHECK(cross.find("in = std::string(appDrive) + \"/\" DOSBOX_DIR_NAME;") != std::string::npos);
}

int main()
{
	testSourceOrder();
	testEventNames();
	testShippedState();
	testSetBinding();
	testPlayersAreSeparate();
	testJoystickLimit();
	testPresets();
	testCompatibleIsLimitedByTheJoystick();
	testPresetIsDerivedOnce();
	testSuggestions();
	testSaving();
	testEmulatedMode();
	testAdvancedTypes();
	testMigrateBinds();
	testPresetFollowsType();
	testNoSetupHost();
	testPlayers();
	testInstall();
	testConfFormat();
	testPaths();
	testProgramKey();
	testProgramFromEnvironment();
	testParsePadBinds();
	testNothingListedWithoutOurs();
	testSaveSetup();
	testSaveSetupFails();
	testListSetups();
	testLoadSetup();
	testLoadTypeAndJoystickBinds();
	testLoadSetupFails();
	testLoadReplacesWhatTheMapDoesNotSay();
	testRunningProgram();
	testSetupFileOwnMapper();
	testCaptureThroughHardware();
	testFileHost();
	printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
