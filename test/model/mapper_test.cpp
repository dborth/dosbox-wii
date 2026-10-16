// Host tests for the mapper-backed controls model (controlsmapper.cpp), over a fake of the mapper's pad groups.
// The fake follows sdl_mapper.cpp as read (CPadBindGroup, MAPPER_PadSetEvent, the shipped binds); it is only as
// faithful as that reading. The text checks tie the names and the button order to the real sources.
#include "controlsmapper.h"
#include "controlspresets.h"
#include "drivers/InputData.h"
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdio.h>
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
};

// The pad groups as sdl_mapper.cpp has them. Events: the keys and handlers the real sources define (read from the
// text), jbutton_0_0..5 and jbutton_1_0..1 (the stick 0 and 1 events, hidden or not, are always created).
class FakeMapper : public PadMapper
{
	public:
		FakeMapper(const std::string& type = "4axis") { reset(type); }

		void reset(const std::string& t)
		{
			type = t;
			for(int p = 0; p < 4; p++)
				for(int s = 0; s < PAD_SOURCE_COUNT; s++)
					padBinds[p][s].clear(), stickBinds[p][s].clear();
			log.clear();
			saves = 0;
			// the stick group that reads pad N: 4axis has one for pad 0 (the one for pad 1 is a dummy); 2axis one each
			// for pads 0 and 1; none has no stick groups. Shipped binds are what MAPPER_PadSetDefault gives back.
			for(int p = 0; p < 2; p++)
			{
				if(type == "none" || (p == 1 && type != "2axis"))
					continue;
				for(int s = 0; s < PAD_SOURCE_COUNT; s++)
				{
					char name[24];
					if(s < 12 && ((p == 0 && s < 6) || (p == 1 && s < 2)))
						snprintf(name, sizeof(name), "jbutton_%d_%d", p, s);
					else if(s >= 12 && p == 0)
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

		bool save() override { saves++; return saveWorks; }
		std::string joystickType() const override { return type; }

		void bind(int pad, int source, const char * event) { padBinds[pad][source].push_back(event); }

		std::string type;
		std::vector<std::string> padBinds[4][PAD_SOURCE_COUNT], stickBinds[4][PAD_SOURCE_COUNT];
		std::vector<std::string> log;
		int saves = 0;
		bool saveWorks = true;

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
	const std::string mapperText = readFile("dosbox/gui/sdl_mapper.cpp"), buttonsText = readFile("menu/buttons.cpp");
	std::set<std::string> seen;
	const std::vector<std::string>& keys = Catalog_Names(TARGET::KEY);
	for(size_t i = 0; i < keys.size(); i++)
	{
		std::string e;
		CHECK(Mapper_EventFor(0, BindingTarget(TARGET::KEY, (int)i), e));
		CHECK(e.compare(0, 4, "key_") == 0);
		CHECK(seen.insert(e).second);									// no two keys share an event
		CHECK(quoted(mapperText, e.substr(4)));							// the mapper has it
		CHECK(quoted(buttonsText, e));									// and so does the page that already worked on a console
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
			CHECK(quoted(buttonsText, e));
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
		Rig r("fcs");	// until D7c: shown as Joystick, four buttons
		CHECK(r.model.currentEmulated() == 0);
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

// ---- what is still a stand-in says so

static void testStandIns()
{
	Rig r("2axis");
	CHECK(r.model.playerCount() == 4);
	CHECK(r.model.emulatedModes().size() == 3);
	r.model.setEmulated(2);													// does nothing until D7c: the mapper decides
	CHECK(r.model.currentEmulated() == 1 && r.mapper.log.empty());
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
	testStandIns();
	testPlayers();
	testInstall();
	printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
