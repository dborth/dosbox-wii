/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsui.cpp
 ***************************************************************************/
#include "controlsui.h"
#include "menu.h"
#include "menupage.h"
#include "menustyle.h"
#include "filelist.h"

static PixelColor rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
	PixelColor c = { r, g, b, a };
	return c;
}

GuiPaint UiBackdropPaint()
{
	const GuiTheme& t = GuiTheme::current();
	return GuiPaint().color(rgb(t.background.r, t.background.g, t.background.b, 224));
}

// ---- battery gauge

UiBatteryBar::UiBatteryBar(int w, int h) : pct(-1), dirty(true), meshFeather(-1.0f)
{
	width = w;
	height = h;
}

void UiBatteryBar::setLevel(int percent)
{
	if(percent > 100)
		percent = 100;
	if(percent != pct)
	{
		pct = percent;
		dirty = true;
	}
}

void UiBatteryBar::draw()
{
	if(!this->isVisible())
		return;

	const int a = this->getAlpha();
	if(a > 0 && pct >= 0)
	{
		VideoDriver * video = platform->getVideo();
		const float uiScale = video->getUIScale();
		const float feather = uiScale > 0.0f ? 1.0f / uiScale : 1.0f;
		if(dirty || feather != meshFeather)
		{
			const GuiTheme& t = GuiTheme::current();
			mesh = GuiMesh(feather);
			const float w = (float)width, h = (float)height;
			mesh.addRoundRect(GuiRect{ 0.0f, 0.0f, w, h }, GuiCorners(h * 0.5f), GuiFill::solid(t.outline));
			const float fw = pct == 0 ? 0.0f : std::max(h, w * (float)pct / 100.0f);
			if(fw > 0.0f)
			{
				const PixelColor c = pct >= 50 ? t.good : (pct >= 20 ? t.accent : rgb(218, 72, 60));
				mesh.addRoundRect(GuiRect{ 0.0f, 0.0f, fw, h }, GuiCorners(h * 0.5f), GuiFill::solid(c));
			}
			meshFeather = feather;
			dirty = false;
		}

		const float ox = (float)this->getLeft(), oy = (float)this->getTop();
		const std::vector<MeshVertex>& src = mesh.vertices();
		scratch.resize(src.size());
		for(size_t i = 0; i < src.size(); i++)
		{
			scratch[i] = src[i];
			scratch[i].x += ox;
			scratch[i].y += oy;
			if(a < 255)
				scratch[i].c.a = (uint8_t)((src[i].c.a * a + 127) / 255);
		}
		if(!scratch.empty())
			video->getImageRenderer()->drawMesh(scratch.data(), scratch.size());
	}

	this->updateEffects();
}

// ---- icons

namespace {

GuiImage& iconImage(UiScope& ui, const unsigned char * png)
{
	GuiImageData& data = ui.own<GuiImageData>(png);
	return ui.make<GuiImage>(&data);
}

const unsigned char * padIcon(PadType t)
{
	switch(t)
	{
		case PadType::WIIMOTE: return icon_wiimote_png;
		case PadType::WIIMOTE_NUNCHUK: return icon_wiimote_nunchuk_png;
		case PadType::CLASSIC: return icon_classic_png;
		case PadType::WIIU_PRO: return icon_wiiu_pro_png;
		case PadType::GAMECUBE: return icon_gamecube_png;
		case PadType::GAMEPAD: return icon_gamepad_png;
		default: return nullptr;
	}
}

// The art of whichever controller is in a slot: one icon per kind, shown while the slot holds that kind
GuiLayout& padArt(UiScope& ui, int n)
{
	GuiLayout& art = ui.overlay().size(40, 40);
	static const PadType kinds[] = { PadType::WIIMOTE, PadType::WIIMOTE_NUNCHUK, PadType::CLASSIC, PadType::WIIU_PRO, PadType::GAMECUBE, PadType::GAMEPAD };
	for(size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
	{
		GuiImage& img = iconImage(ui, padIcon(kinds[i]));
		const PadType kind = kinds[i];
		ui.visibleWhen(img, [n, kind]{ return Controls().player(n).connected && Controls().player(n).type == kind; });
		art.add(img);
	}
	return art;
}

// What DOS sees: a joystick (with its number) or a keyboard and mouse
GuiLayout& emulatedArt(UiScope& ui, int n)
{
	GuiChip& number = ui.chip().bind([n]{ return GuiFormat("%d", Controls().player(n).joystick); });
	GuiLayout& joystick = ui.overlay().size(40, 40).add(iconImage(ui, icon_joystick_png)).add(number, GuiItem().at(LAYOUT_ALIGN::END, LAYOUT_ALIGN::END));
	ui.visibleWhen(joystick, [n]{ return Controls().player(n).emulated == Emulated::JOYSTICK; });

	GuiLayout& keysMouse = ui.row().gap(4).add(iconImage(ui, icon_keyboard_png)).add(iconImage(ui, icon_mouse_png));
	ui.visibleWhen(keysMouse, [n]{ return Controls().player(n).emulated == Emulated::KEYS_MOUSE; });

	return ui.row().size(100, 0).justifyContent(LAYOUT_JUSTIFY::CENTRE).add(joystick).add(keysMouse);
}

// A keyboard or mouse mark for the header: its icon, faded while it is not there, with a status dot
GuiLayout& deviceMark(UiScope& ui, const unsigned char * png, std::function<bool()> present)
{
	GuiImage& img = iconImage(ui, png);
	img.setScale(0.8f);
	GuiShape& dot = ui.make<GuiShape>(9, 9);
	dot.setShape(SHAPE::ELLIPSE);
	GuiLayout& art = ui.overlay().size(34, 34);
	art.add(img, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE)).add(dot, GuiItem().at(LAYOUT_ALIGN::END, LAYOUT_ALIGN::END));

	int last = -1;
	ui.bind([&img, &dot, present, last]() mutable {
		const int s = present() ? 1 : 0;
		if(s != last)
		{
			last = s;
			img.setAlpha(s ? 255 : 90);
			dot.setPaint(GuiPaint().color(s ? GuiTheme::current().good : GuiTheme::current().outlineStrong).stroke(1.5f, GuiTheme::current().surfaceRaised));
		}
	});
	return art;
}

const int kZoneW = 300;		// the controller half of a player's row
const int kDosW = 116;		// the DOS half
const int kHeroH = 104;		// a row when there is only one player
const int kCompactH = 50;	// a row when there are several

} // namespace

// ---- hub

// What the DOS mode button says it does, for the hint line
std::string ControlsHubView::modeHint() const
{
	const ControlsModel& m = Controls();
	const std::vector<EmulatedMode> modes = m.emulatedModes();
	const EmulatedMode& cur = modes[(size_t)m.currentEmulated()];
	return GuiFormat("%s: %s.  A changes it", cur.name.c_str(), cur.description.c_str());
}

GuiCard& ControlsHubView::playerRow(int n)
{
	UiBatteryBar& battery = ui.make<UiBatteryBar>(56, 10);
	ui.bind([&battery, n]{
		const int b = Controls().player(n).battery;
		battery.setLevel(b);
		battery.setVisible(b >= 0 && Controls().player(n).connected);
	});

	const std::function<bool()> connected = [n]{ return Controls().player(n).connected; };
	const std::function<bool()> absent = [n]{ return !Controls().player(n).connected; };
	const GuiTheme& t = ui.theme();

	GuiLabel& name = ui.label().fontSize(UI_FONT_HEADING).bind([n]{ return Controls().player(n).device; });
	GuiLayout& who = ui.column().gap(4).add(name).add(battery);
	ui.visibleWhen(who, connected);
	GuiLabel& waiting = ui.label("Press a button on a controller").fontSize(UI_FONT_BODY).muted().visibleWhen(absent);

	// left: the controller. A opens its button setup
	GuiLayout& hardware = ui.row().size(kZoneW - 20, 0).gap(12).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.label(GuiFormat("P%d", n + 1).c_str()).fontSize(UI_FONT_HEADING).accent().visibleWhen(connected))
		.add(padArt(ui, n))
		.add(who)
		.add(waiting);
	hw[n] = &ui.card().style(MenuRowHighlightStyle()).corners(GuiCorners(10.0f)).pad(10, 2).size(kZoneW, 0)
		.onFocus([this, n]{
			const PlayerInfo p = Controls().player(n);
			status = p.connected ? GuiFormat("P%d  %s.  A: set up buttons", n + 1, p.device.c_str())
				: std::string("Press a button on a controller to connect it");
		})
		.onClick([this, n]{ if(Controls().player(n).connected) request = n; })
		.add(hardware);

	// right: what DOS sees it as. Joystick n is player n; how many joysticks there are comes from the DOS mode
	// above, so this is only shown
	GuiArrow& arrow = ui.make<GuiArrow>(ARROW::RIGHT, 16, 24, t.outlineStrong, t.outlineStrong);
	ui.visibleWhen(arrow, connected);
	dos[n] = &ui.card().selectable(false).style(MenuTileStyle()).corners(GuiCorners(8.0f)).pad(8, 2).size(kDosW, 0).visibleWhen(connected)
		.add(emulatedArt(ui, n));

	GuiLayout& line = ui.row().size(MENU_PAGE_CONTENT_W - 16, 0).align(LAYOUT_ALIGN::CENTRE)
		.add(*hw[n]).addSpace().add(arrow).addSpace().add(*dos[n]);
	return MenuPage_Section(ui, MENU_PAGE_CONTENT_W).add(line);
}

ControlsHubView::ControlsHubView(GuiSound * soundOver) :
	request(-1), status("A controller's buttons on the left, what DOS sees it as on the right"), backBtn(nullptr), heroMode(true)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();
	const GuiTheme& t = ui.theme();

	MenuPageFrame f = MenuPage_Build(ui, *this, "Controllers", [this]{ return status; });

	// beside the title: the preset, which applies to every player. At the right: the keyboard and mouse,
	// which need no setup, in one capsule
	GuiTextButton& presetBtn = ui.button("").size(196, 38).fontSize(UI_FONT_BODY).style(MenuBarButtonStyle())
		.bind([]{ ControlsModel& m = Controls(); return GuiFormat("Preset: %s", m.presets()[(size_t)m.currentPreset()].name.c_str()); })
		.onClick([this]{
			ControlsModel& m = Controls();
			m.applyPreset((m.currentPreset() + 1) % (int)m.presets().size());
			status = GuiFormat("Preset: %s", m.presets()[(size_t)m.currentPreset()].name.c_str());
		})
		.onFocus([this]{ status = "A changes the preset for every player"; });
	GuiLayout& marks = ui.row().gap(8).align(LAYOUT_ALIGN::CENTRE)
		.add(deviceMark(ui, icon_keyboard_png, []{ return Controls().devices().keyboard; }))
		.add(deviceMark(ui, icon_mouse_png, []{ return Controls().devices().mouse; }));
	GuiCard& capsule = ui.card().selectable(false).style(GuiShapeStyle(GuiPaint().color(t.surfaceRaised).stroke(1.5f, t.outline)))
		.corners(GuiCorners(19.0f)).pad(12, 6).add(marks);
	f.headerRight->add(capsule);
	f.headerLeft->add(presetBtn);

	// column headings, then a row for each player. The DOS heading carries the mode button: how many joysticks DOS
	// has (player n drives joystick n), or none
	GuiTextButton& modeBtn = ui.button("").size(150, 26).fontSize(UI_FONT_SMALL + 1).style(MenuBarButtonStyle())
		.bind([]{ ControlsModel& m = Controls(); return m.emulatedModes()[(size_t)m.currentEmulated()].name; })
		.onClick([this]{
			ControlsModel& m = Controls();
			m.setEmulated((m.currentEmulated() + 1) % (int)m.emulatedModes().size());
			status = modeHint();
		})
		.onFocus([this]{ status = modeHint(); });
	GuiLayout& heads = ui.row().size(MENU_PAGE_CONTENT_W, 0).pad(18, 0, 8, 0).gap(8).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.label("Controller").fontSize(UI_FONT_SMALL).muted())
		.addSpace()
		.add(ui.label("In DOS:").fontSize(UI_FONT_SMALL).muted())
		.add(modeBtn);
	f.content->gap(4);
	f.content->add(heads);

	// A slot that has nothing in it takes no room (the first one always shows, to say what to do), and the rows
	// are roomier when there is only one player, which is the usual case. This runs before the rows' own
	// bindings: showing a window shows everything inside it, and those bindings then hide what should be hidden.
	for(int n = 0; n < 4; n++)
		rows[n] = nullptr;
	ui.bind([this]{
		if(!rows[3])
			return;
		int shown = 0;
		for(int n = 0; n < 4; n++)
		{
			const bool v = n == 0 || Controls().player(n).connected;
			if(rows[n]->isVisible() != v)
				rows[n]->setVisible(v);
			shown += v ? 1 : 0;
		}
		const bool hero = shown <= 1;
		if(hero != heroMode || rows[0]->getHeight() == 0)
		{
			heroMode = hero;
			for(int n = 0; n < 4; n++)
			{
				rows[n]->setFixedSize(MENU_PAGE_CONTENT_W, hero ? kHeroH : kCompactH);
				hw[n]->setFixedSize(kZoneW, hero ? kHeroH - 12 : kCompactH - 8);
				dos[n]->setFixedSize(kDosW, hero ? kHeroH - 12 : kCompactH - 8);
			}
		}
	});
	for(int n = 0; n < 4; n++)
	{
		rows[n] = &playerRow(n);
		f.content->add(*rows[n]);
	}
	ui.runBindings();

	// When the page takes focus its window moves the selection one step down from whatever is selected (or to the
	// first thing in reading order when nothing is). Starting from the preset button, which is just above the first
	// controller, lands that step on the first player rather than on the header.
	presetBtn.setState(STATE::SELECTED);

	GuiTextButton& back = MenuPage_Button(ui, "Back");
	back.setTrigger(1, &cancel);
	backBtn = &back;
	f.footerRight->add(back);

	ui.attachTo(*this);
}

// ---- controller setup

int ControllerConfigView::highlighted() const
{
	const std::vector<int>& h = view->getHighlight();
	return h.empty() ? 0 : h[0];
}

bool ControllerConfigView::takeRequest(ProfileRow& row)
{
	if(!hasRequest)
		return false;
	row = request;
	hasRequest = false;
	return true;
}

// A list row: the control's icon and what it does. The control's name is on the hint line while the row is
// highlighted, along with a line drawn to the control on the picture. Clicking asks to change it.
GuiCard& ControllerConfigView::bindingRow(const ProfileRow& r, Side side, int width)
{
	const int pl = playerSlot;
	const int id = r.binding;
	const char * fixed = r.fixed;
	const char * name = r.name;

	GuiLabel& target = ui.label().fontSize(UI_FONT_SMALL + 1);
	target.setMaxWidth(width - 12 - 22 - 8); // long choices are cut rather than spilling out of the row
	target.bind([pl, id, fixed]{ return fixed ? std::string(fixed) : Controls().describe(pl, id); });

	GuiLayout& content = ui.row().size(width - 12, 0).gap(8).align(LAYOUT_ALIGN::CENTRE);
	GuiGlyph& icon = ui.glyph(r.glyph, r.letter);
	if(side == Side::LEFT)
		content.addSpace().add(target).add(icon);
	else
		content.add(icon).add(target);

	GuiCard& row = ui.card();
	row.size(width, 0).style(MenuRowHighlightStyle()).pad(6, 1);
	row.corners(GuiCorners(6.0f));
	const int control = r.control;
	row.onFocus([this, &row, control, side, name, pl, id, fixed]{
		view->setHighlight(control);
		const float midY = (float)(row.getTop() + row.getHeight() / 2);
		if(side == Side::LEFT)
			view->setLeader(true, (float)(row.getLeft() + row.getWidth()), midY);
		else if(side == Side::RIGHT)
			view->setLeader(true, (float)row.getLeft(), midY);
		else
			view->setLeader(true, (float)(row.getLeft() + row.getWidth() / 2), (float)row.getTop());
		status = fixed ? GuiFormat("%s: %s", name, fixed) : GuiFormat("%s: %s.  A changes it", name, Controls().describe(pl, id).c_str());
	});
	if(!fixed)
	{
		const ProfileRow copy = r;
		row.onClick([this, copy]{
			request = copy;
			hasRequest = true;
		});
	}
	row.add(content);
	return row;
}

ControllerConfigView::ControllerConfigView(int player, GuiSound * soundOver) :
	playerSlot(player), padType(Controls().player(player).type), view(nullptr), status("Move over a control to see what it does"), hasRequest(false), backBtn(nullptr)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	const PlayerInfo info = Controls().player(player);
	const ControllerProfile& profile = Controllers_Profile(padType);
	MenuPageFrame f = MenuPage_Build(ui, *this, GuiFormat("P%d  %s", player + 1, info.device.c_str()).c_str(), [this]{ return status; });
	f.headerRight->add(ui.chip().bind([]{ ControlsModel& m = Controls(); return m.presets()[(size_t)m.currentPreset()].name; }));

	view = &ui.controllerView(*profile.layout, profile.scale);
	const int sideW = 168;
	const int groupSectionW = (MENU_PAGE_CONTENT_W - 12) / 3;

	GuiLayout& left = ui.column().gap(2).align(LAYOUT_ALIGN::END);
	for(size_t i = 0; i < profile.left.size(); i++)
		left.add(bindingRow(profile.left[i], Side::LEFT, sideW));
	GuiLayout& right = ui.column().gap(2);
	for(size_t i = 0; i < profile.right.size(); i++)
		right.add(bindingRow(profile.right[i], Side::RIGHT, sideW));

	GuiLayout& groups = ui.row().gap(6);
	for(size_t g = 0; g < profile.groups.size(); g++)
	{
		GuiLayout& column = ui.column().gap(1).add(ui.label(profile.groups[g].title).fontSize(UI_FONT_SMALL - 1).muted());
		for(size_t i = 0; i < profile.groups[g].rows.size(); i++)
			column.add(bindingRow(profile.groups[g].rows[i], Side::GROUP, groupSectionW - 16));
		groups.add(MenuPage_Section(ui, groupSectionW).pad(8, 4).add(column));
	}

	f.content->gap(6);
	f.content->add(MenuPage_Section(ui, MENU_PAGE_CONTENT_W).pad(8, 4).add(ui.row().gap(4).align(LAYOUT_ALIGN::CENTRE).add(left).add(*view).add(right)));
	f.content->add(groups);

	const int pl = playerSlot;
	GuiTextButton& reset = MenuPage_Button(ui, "Reset")
		.onClick([this, pl]{
			Controls().resetBindings(pl);
			status = "Buttons restored to the preset's defaults";
		})
		.onFocus([this]{ status = "A restores every button to the preset's defaults"; });
	GuiTextButton& back = MenuPage_Button(ui, "Back");
	back.setTrigger(1, &cancel);
	backBtn = &back;
	f.footerRight->add(reset).add(back);

	ui.attachTo(*this);
}

// ---- target chooser

void TargetChooserView::optionButtons(GuiLayout& grid, TARGET kind, int w, int h, int fontPx)
{
	const std::vector<std::string>& names = Catalog_Names(kind);
	for(size_t i = 0; i < names.size(); i++)
	{
		// the longest key names do not fit a key
		const char * shown = names[i] == "Backspace" ? "Bksp" : names[i].c_str();
		const bool now = current.kind == kind && current.code == (int)i;
		GuiTextButton& b = ui.button(shown).size(w, h).fontSize(fontPx).style(MenuBarButtonStyle(now));
		const int code = (int)i;
		b.onClick([this, kind, code]{
			picked = BindingTarget(kind, code);
			done = true;
		});
		grid.add(b);
	}
}

TargetChooserView::TargetChooserView(const char * controlName, bool stickMode, const BindingTarget& now, const BindingTarget& defaultTarget, GuiSound * soundOver) :
	current(now), picked(), shown(0), done(false), backBtn(nullptr), hintText(stickMode ? "A picks what the stick does" : "Pick a kind on the left, then what it does")
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	MenuPageFrame f = MenuPage_Build(ui, *this, GuiFormat("What should %s do?", controlName).c_str(), [this]{ return hintText; });
	f.headerRight->add(ui.chip(GuiFormat("Now: %s", now.describe().c_str()).c_str()));

	if(stickMode)
	{
		GuiLayout& modes = ui.column().gap(6);
		for(size_t i = 0; i < Catalog_Names(TARGET::STICK_MODE).size(); i++)
		{
			GuiTextButton& b = ui.button(Catalog_Names(TARGET::STICK_MODE)[i].c_str()).size(300, 38).fontSize(UI_FONT_BODY + 2)
				.style(MenuBarButtonStyle(now.kind == TARGET::STICK_MODE && now.code == (int)i));
			const int code = (int)i;
			b.onClick([this, code]{ picked = BindingTarget(TARGET::STICK_MODE, code); done = true; });
			modes.add(b);
		}
		f.content->add(MenuPage_Section(ui, MENU_PAGE_CONTENT_W).pad(8, 10).add(ui.column().align(LAYOUT_ALIGN::CENTRE).add(modes)));
	}
	else
	{
		// kinds down the left; a click shows that kind's choices on the right
		static const char * const categories[5] = { "Key", "Joystick", "Mouse", "DOSBox", "None" };
		static const char * const headings[5] = { "Keyboard keys", "Joystick buttons", "Mouse", "DOSBox functions", "Nothing" };
		static const TARGET kinds[4] = { TARGET::KEY, TARGET::JOYSTICK, TARGET::MOUSE, TARGET::HOTKEY };
		for(int k = 0; k < 4; k++)
			if(now.kind == kinds[k])
				shown = k;

		GuiTextButton * tabs[5];
		GuiLayout& list = ui.column().gap(4);
		for(int k = 0; k < 5; k++)
		{
			tabs[k] = &ui.button(categories[k]).size(104, 34).fontSize(UI_FONT_BODY).style(MenuBarButtonStyle());
			tabs[k]->onClick([this, k]{ shown = k; });
			list.add(*tabs[k]);
		}
		// the tab that is showing looks pressed in, apart from the highlight
		int styled = -1;
		ui.bind([tabs, styled, this]() mutable {
			if(styled == shown)
				return;
			styled = shown;
			for(int k = 0; k < 5; k++)
				tabs[k]->style(k == shown ? MenuTabActiveStyle() : MenuBarButtonStyle());
		});

		const int choicesW = MENU_PAGE_CONTENT_W - 120 - 6;
		const int inner = choicesW - 16;
		GuiLayout& keys = ui.grid(9).gap(2, 2);
		optionButtons(keys, TARGET::KEY, 41, 24, UI_FONT_SMALL);
		const int wide = (inner - 6) / 2;
		GuiLayout& joystick = ui.grid(2).gap(6, 6);
		optionButtons(joystick, TARGET::JOYSTICK, wide, 30, UI_FONT_BODY);
		GuiLayout& mouse = ui.grid(2).gap(6, 6);
		optionButtons(mouse, TARGET::MOUSE, wide, 30, UI_FONT_BODY);
		GuiLayout& hotkeys = ui.grid(2).gap(6, 6);
		optionButtons(hotkeys, TARGET::HOTKEY, wide, 30, UI_FONT_BODY);
		GuiTextButton& clear = ui.button("Leave unassigned").size(wide, 30).fontSize(UI_FONT_BODY)
			.style(MenuBarButtonStyle(now.kind == TARGET::NONE))
			.onClick([this]{
				picked = BindingTarget();
				done = true;
			});
		GuiLayout& nothing = ui.column().gap(8)
			.add(ui.label("This control will do nothing.").fontSize(UI_FONT_BODY).muted())
			.add(clear);

		GuiLayout& stack = ui.overlay();
		GuiLayout * panels[5] = { &keys, &joystick, &mouse, &hotkeys, &nothing };
		for(int k = 0; k < 5; k++)
		{
			stack.add(*panels[k], GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
			ui.visibleWhen(*panels[k], [this, k]{ return shown == k; });
		}
		GuiLabel& heading = ui.label().fontSize(UI_FONT_SMALL - 1).muted().bind([this]{ return std::string(headings[shown]); });

		f.content->add(ui.row().gap(6).align(LAYOUT_ALIGN::START)
			.add(MenuPage_Section(ui, 120).pad(8, 8).add(list))
			.add(MenuPage_Section(ui, choicesW).pad(8, 6).add(ui.column().gap(4).add(heading).add(stack))));
	}

	// the control's own default, from the current preset
	GuiTextButton& useDefault = MenuPage_Button(ui, "Default")
		.onClick([this, defaultTarget]{
			picked = defaultTarget;
			done = true;
		})
		.onFocus([this, defaultTarget]{ hintText = GuiFormat("A picks the preset's default: %s", defaultTarget.describe().c_str()); });
	GuiTextButton& back = MenuPage_Button(ui, "Back");
	back.setTrigger(1, &cancel);
	backBtn = &back;
	f.footerRight->add(useDefault).add(back);

	ui.attachTo(*this);
}

// ---- running the screens on the menu's main window

namespace {

// A full-screen page on the menu's main window, above the header and footer bars HomeMenu keeps there. It can be
// taken off the window while another screen runs on top (a window only hands the D-pad to the topmost screen that way).
class ScreenWindow
{
	public:
		ScreenWindow(GuiElement& view) :
			w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight()),
			shown(false)
		{
			w.append(&view);
			show();
		}
		~ScreenWindow() { hide(); }
		void show()
		{
			if(!shown)
			{
				mainWindow->append(&w);
				shown = true;
			}
		}
		void hide()
		{
			if(shown)
			{
				mainWindow->remove(&w);
				shown = false;
			}
		}
	private:
		GuiWindow w;
		bool shown;
};

bool ChooserScreen(const char * name, bool stickMode, BindingTarget& target, const BindingTarget& defaultTarget)
{
	GuiSound over(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	TargetChooserView view(name, stickMode, target, defaultTarget, &over);
	ScreenWindow screen(view);

	while(true)
	{
		UpdateGui();
		if(view.chosen())
		{
			target = view.result();
			return true;
		}
		if(view.backButton().getState() == STATE::CLICKED)
			return false;
	}
}

void ConfigScreen(int player)
{
	GuiSound over(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	ControllerConfigView view(player, &over);
	ScreenWindow screen(view);

	ProfileRow row;
	while(true)
	{
		UpdateGui();
		if(view.backButton().getState() == STATE::CLICKED)
			return;
		if(view.takeRequest(row))
		{
			screen.hide();
			BindingTarget target = Controls().binding(view.player(), row.binding);
			if(ChooserScreen(row.name, row.stickMode, target, Controls().defaultBinding(row.binding)))
				Controls().setBinding(view.player(), row.binding, target);
			screen.show();
		}
	}
}

void HubScreen()
{
	GuiSound over(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	ControlsHubView view(&over);
	ScreenWindow screen(view);

	while(true)
	{
		UpdateGui();
		if(view.backButton().getState() == STATE::CLICKED)
			return;
		const int player = view.takeRequest();
		if(player >= 0)
		{
			screen.hide();
			ConfigScreen(player);
			screen.show();
		}
	}
}

} // namespace

void Controls_Menu(GuiWindow * home)
{
	mainWindow->remove(home);
	HubScreen();
	mainWindow->append(home);
}
