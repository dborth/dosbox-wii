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
	const int stick = r.stick;
	const int padX = 10;	// room between the highlight box and what is in it
	// a GameCube pad has no Home button: the menu is the L + R + Start combo (the same combo works on any pad)
	const bool gcMenu = r.binding == INPUT_BTN_HOME && padType == PadType::GAMECUBE;
	const char * fixed = gcMenu ? "Hold L + R + Start" : r.fixed;
	const char * name = gcMenu ? "Menu" : r.name;
	const bool info = fixed || stick; // shown, not changed

	// what the row says: a button's target, or for a stick what the DOS side does with it
	const std::function<std::string()> text = [pl, id, fixed, stick]() -> std::string {
		if(stick)
			return Controls().joystickAxes(pl) >= (stick == 1 ? 2 : 4) ? "Joystick axes" : "Not used";
		return fixed ? std::string(fixed) : Controls().describe(pl, id);
	};

	GuiLabel& target = ui.label().fontSize(UI_FONT_SMALL + 1);
	target.setMaxWidth(width - 2 * padX - 22 - 8); // long choices are cut rather than spilling out of the row
	target.bind(text);

	GuiLayout& content = ui.row().size(width - 2 * padX, 0).gap(8).align(LAYOUT_ALIGN::CENTRE);
	GuiGlyph& icon = ui.glyph(r.glyph, r.letter);
	if(side == Side::LEFT)
		content.addSpace().add(target).add(icon);
	else
		content.add(icon).add(target);

	GuiCard& row = ui.card();
	row.size(width, 0).style(MenuRowHighlightStyle()).pad(padX, 2);
	row.corners(GuiCorners(9.0f));
	const int control = r.control;
	row.onFocus([this, &row, control, side, name, info, text]{
		view->setHighlight(control);
		const float midY = (float)(row.getTop() + row.getHeight() / 2);
		if(side == Side::LEFT)
			view->setLeader(true, (float)(row.getLeft() + row.getWidth()), midY);
		else if(side == Side::RIGHT)
			view->setLeader(true, (float)row.getLeft(), midY);
		else
			view->setLeader(true, (float)(row.getLeft() + row.getWidth() / 2), (float)row.getTop());
		// the hint holds about 26 characters beside the four footer buttons (it scrolls when longer), so just the facts
		status = GuiFormat("%s: %s", name, text().c_str());
	});
	if(!info)
	{
		const ProfileRow copy = r;
		row.onClick([this, copy]{
			request = copy;
			hasRequest = true;
		});
	}
	row.add(content);
	rowCards.push_back(&row);
	return row;
}

ControllerConfigView::ControllerConfigView(int player, GuiSound * soundOver) :
	playerSlot(player), padType(Controls().player(player).type), view(nullptr), status("Move over a control to see what it does"), hasRequest(false), pickRequest(Pick::NONE), backBtn(nullptr)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	const PlayerInfo info = Controls().player(player);
	const ControllerProfile& profile = Controllers_Profile(padType);
	// four buttons in the footer leave the hint about 220 wide (it scrolls when longer)
	MenuPageFrame f = MenuPage_Build(ui, *this, GuiFormat("Player %d", player + 1).c_str(), [this]{ return status; }, 214);
	// what DOS sees, and the preset the buttons started from: each opens a list of the choices
	GuiTextButton& emulatedBtn = ui.button("").size(150, 32).fontSize(UI_FONT_SMALL + 1).style(MenuBarButtonStyle())
		.bind([]{ ControlsModel& m = Controls(); return m.emulatedModes()[(size_t)m.currentEmulated()].name; })
		.onClick([this]{ pickRequest = Pick::EMULATED; })
		.onFocus([this]{ status = "A chooses what DOS sees: a joystick, two, or none"; });
	GuiTextButton& presetBtn = ui.button("").size(150, 32).fontSize(UI_FONT_SMALL + 1).style(MenuBarButtonStyle())
		.bind([]{ ControlsModel& m = Controls(); return m.presets()[(size_t)m.currentPreset()].name; })
		.onClick([this]{ pickRequest = Pick::PRESET; })
		.onFocus([this]{ status = "A chooses a preset for the buttons"; });
	f.headerRight->add(emulatedBtn);
	f.headerRight->add(presetBtn);

	view = &ui.controllerView(*profile.layout, profile.scale);
	// the shoulder buttons are too small to carry legible text; their rows name them
	view->setLabelFilter([](int id){ return id != INPUT_TRIGGER_L && id != INPUT_TRIGGER_R && id != INPUT_TRIGGER_ZL && id != INPUT_TRIGGER_ZR; });
	// lit while held, so pressing a button on the controller shows which control it is
	view->setLit([pl = player](int id){ return id > 0 && id < GUI_CTL_CAPTURE && (Controls().held(pl) & (uint32_t)id) != 0; });
	const int sideW = 168;
	const int groupSectionW = (MENU_PAGE_CONTENT_W - 12) / 3;

	GuiLayout& left = ui.column().gap(2).align(LAYOUT_ALIGN::END);
	// which controller this is (the title has room for the player only, beside the two controls)
	GuiLabel& deviceName = ui.label(info.device.c_str()).fontSize(UI_FONT_SMALL).muted();
	deviceName.setMaxWidth(sideW);
	left.add(deviceName);
	for(size_t i = 0; i < profile.left.size(); i++)
		left.add(bindingRow(profile.left[i], Side::LEFT, sideW));
	GuiLayout& right = ui.column().gap(2);
	for(size_t i = 0; i < profile.right.size(); i++)
		right.add(bindingRow(profile.right[i], Side::RIGHT, sideW));

	// The picture and the groups sit straight on the page card: no tinted boxes behind them. A heading is just
	// small muted capitals, lined up with the text of the rows under it.
	GuiLayout& groups = ui.row().gap(6);
	for(size_t g = 0; g < profile.groups.size(); g++)
	{
		GuiLayout& column = ui.column().gap(2).size(groupSectionW, 0);
		column.add(ui.row().size(groupSectionW, 0).pad(10, 0, 0, 2).add(ui.label(profile.groups[g].title).fontSize(UI_FONT_SMALL - 1).muted()));
		for(size_t i = 0; i < profile.groups[g].rows.size(); i++)
			column.add(bindingRow(profile.groups[g].rows[i], Side::GROUP, groupSectionW - 8));
		groups.add(column);
	}

	// the picture marks a control, and draws the line to it, only while its row has the selection
	ui.bind([this]{
		for(size_t i = 0; i < rowCards.size(); i++)
			if(rowCards[i]->isSelected())
				return;
		if(!view->getHighlight().empty())
		{
			view->setHighlight(0);
			view->setLeader(false);
		}
	});

	f.content->gap(6);
	f.content->add(ui.row().gap(4).align(LAYOUT_ALIGN::CENTRE).add(left).add(*view).add(right));
	f.content->add(groups);

	const int pl = playerSlot;
	// Load and Save hold whole setups (names, a program to load for); Reset puts this player's buttons back
	GuiTextButton& load = MenuPage_Button(ui, "Load", 76)
		.onClick([this]{
			if(Controls().setups().empty())
				status = "No saved setups yet.  Use Save first";
			else
				pickRequest = Pick::LOAD;
		})
		.onFocus([this]{ status = "A loads a saved setup"; });
	GuiTextButton& save = MenuPage_Button(ui, "Save", 76)
		.onClick([this]{ pickRequest = Pick::SAVE; })
		.onFocus([this]{ status = "A saves this setup under a name"; });
	GuiTextButton& reset = MenuPage_Button(ui, "Reset", 76)
		.onClick([this, pl]{
			Controls().resetBindings(pl);
			status = "Buttons restored to the preset's defaults";
		})
		.onFocus([this]{ status = "A restores every button to the preset's defaults"; });
	GuiTextButton& back = MenuPage_Button(ui, "Back", 76);
	back.setTrigger(1, &cancel);
	backBtn = &back;
	f.footerRight->add(load).add(save).add(reset).add(back);

	ui.attachTo(*this);
	// When the page takes focus its window steps the selection down from whatever is selected, or goes to the first
	// thing in reading order when nothing is, which would be this header button. Starting from it steps onto a control.
	emulatedBtn.setState(STATE::SELECTED);
}

// ---- target chooser

void TargetChooserView::optionButtons(GuiLayout& grid, TARGET kind, const std::vector<int>& codes, int w, int h, int fontPx)
{
	const std::vector<std::string>& names = Catalog_Names(kind);
	for(size_t i = 0; i < codes.size(); i++)
	{
		const int code = codes[i];
		// the longest names do not fit a key: draw the symbol, and name it on the hint line when it is selected
		const std::string& full = names[(size_t)code];
		const char * shown = full.c_str();
		if(kind == TARGET::KEY)
		{
			static const char * const caps[][2] = { { "Backspace", "Bksp" }, { "Minus", "-" }, { "Equals", "=" }, { "Comma", "," },
				{ "Period", "." }, { "Slash", "/" }, { "Semicolon", ";" }, { "Quote", "'" }, { "Backslash", "\\" }, { "Grave", "`" } };
			for(size_t c = 0; c < sizeof(caps) / sizeof(caps[0]); c++)
				if(full == caps[c][0])
					shown = caps[c][1];
		}
		const bool now = current.kind == kind && current.code == code;
		GuiTextButton& b = ui.button(shown).size(w, h).fontSize(fontPx).style(MenuBarButtonStyle(now));
		b.onClick([this, kind, code]{
			picked = BindingTarget(kind, code);
			done = true;
		});
		if(kind == TARGET::KEY)
			b.onFocus([this, full]{ if(!listen) hintText = GuiFormat("Key: %s", full.c_str()); });
		grid.add(b);
	}
}

void TargetChooserView::targetButtons(GuiLayout& grid, const std::vector<BindingTarget>& targets, int w, int h, int fontPx)
{
	for(size_t i = 0; i < targets.size(); i++)
	{
		const BindingTarget t = targets[i];
		GuiTextButton& b = ui.button(t.describe().c_str()).size(w, h).fontSize(fontPx).style(MenuBarButtonStyle(t == current));
		b.onClick([this, t]{
			picked = t;
			done = true;
		});
		grid.add(b);
	}
}

void TargetChooserView::startListening(TARGET kind)
{
	Controls().beginCapture();
	listen = true;
	listenKind = kind;
	hintText = kind == TARGET::KEY ? "Press a key on the keyboard.  B cancels" : "Click a mouse button.  B cancels";
}

void TargetChooserView::stopListening()
{
	listen = false;
	hintText = "Pick a kind on the left, then what it does";
}

TargetChooserView::TargetChooserView(const char * controlName, int player, int control, const BindingTarget& now, const BindingTarget& defaultTarget, GuiSound * soundOver) :
	current(now), picked(), shown(0), done(false), backBtn(nullptr), hintText("Pick a kind on the left, then what it does"),
	listen(false), listenKind(TARGET::KEY), kinds(0)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	MenuPageFrame f = MenuPage_Build(ui, *this, GuiFormat("What should %s do?", controlName).c_str(), [this]{ return hintText; });
	f.headerRight->add(ui.chip(GuiFormat("Now: %s", Controls().describe(player, control).c_str()).c_str()));

	{
		ControlsModel& m = Controls();
		const std::vector<BindingTarget> suggested = m.suggestions(player, control);

		const int choicesW = MENU_PAGE_CONTENT_W - 120 - 6;
		const int inner = choicesW - 16;
		const int wide = (inner - 6) / 2;
		const int keyW = (inner - 9 * 2) / 10; // ten keys across

		// the kinds down the left (a joystick only when the player has one), each with the panel it shows
		struct Panel { const char * tab; const char * heading; GuiLayout * layout; TARGET kind; };
		std::vector<Panel> panels;

		if(!suggested.empty())
		{
			GuiLayout& g = ui.grid(2).gap(6, 6);
			targetButtons(g, suggested, wide, 30, UI_FONT_BODY);
			panels.push_back({ "Suggested", "Common choices for this button", &g, TARGET::NONE });
		}

		// Key: a button that waits for a real key press, above the keys for a pad-only user
		{
			GuiLayout& keys = ui.grid(10).gap(2, 2);
			optionButtons(keys, TARGET::KEY, m.choices(player, TARGET::KEY), keyW, 24, UI_FONT_SMALL);
			GuiTextButton& press = ui.button("").size(wide, 24).fontSize(UI_FONT_SMALL).style(MenuBarButtonStyle())
				.bind([this]{ return listen && listenKind == TARGET::KEY ? std::string("Press a key...") : std::string("Press a key on the keyboard"); })
				.visibleWhen([]{ return Controls().devices().keyboard; })
				.onClick([this]{ startListening(TARGET::KEY); });
			panels.push_back({ "Key", "Keyboard keys", &ui.column().gap(4).add(press).add(keys), TARGET::KEY });
		}

		if(m.joystickButtons(player) > 0)
		{
			GuiLayout& g = ui.grid(2).gap(6, 6);
			optionButtons(g, TARGET::JOYSTICK, m.choices(player, TARGET::JOYSTICK), wide, 30, UI_FONT_BODY);
			panels.push_back({ "Joystick", "Joystick buttons", &g, TARGET::JOYSTICK });
		}

		{
			GuiLayout& g = ui.grid(2).gap(6, 6);
			optionButtons(g, TARGET::MOUSE, m.choices(player, TARGET::MOUSE), wide, 30, UI_FONT_BODY);
			GuiTextButton& click = ui.button("").size(wide, 30).fontSize(UI_FONT_BODY).style(MenuBarButtonStyle())
				.bind([this]{ return listen && listenKind == TARGET::MOUSE ? std::string("Click a button...") : std::string("Click a mouse button"); })
				.visibleWhen([]{ return Controls().devices().mouse; })
				.onClick([this]{ startListening(TARGET::MOUSE); });
			panels.push_back({ "Mouse", "Mouse buttons", &ui.column().gap(6).add(click).add(g), TARGET::MOUSE });
		}

		{
			GuiLayout& g = ui.grid(2).gap(6, 6);
			optionButtons(g, TARGET::HOTKEY, m.choices(player, TARGET::HOTKEY), wide, 30, UI_FONT_BODY);
			panels.push_back({ "DOSBox", "DOSBox functions", &g, TARGET::HOTKEY });
		}

		{
			GuiTextButton& clear = ui.button("Leave unassigned").size(wide, 30).fontSize(UI_FONT_BODY)
				.style(MenuBarButtonStyle(now.kind == TARGET::NONE))
				.onClick([this]{
					picked = BindingTarget();
					done = true;
				});
			panels.push_back({ "None", "Nothing", &ui.column().gap(8)
				.add(ui.label("This control will do nothing.").fontSize(UI_FONT_BODY).muted())
				.add(clear), TARGET::NONE });
		}

		// open on the kind the control uses now; failing that, the suggestions (the first panel) when there are any
		shown = 0;
		for(size_t i = 0; i < panels.size(); i++)
			if(now.kind != TARGET::NONE && panels[i].kind == now.kind)
				shown = (int)i;

		kinds = (int)panels.size();
		std::vector<GuiTextButton*>& tabs = ui.own<std::vector<GuiTextButton*>>();
		std::vector<const char *>& headings = ui.own<std::vector<const char *>>();
		GuiLayout& list = ui.column().gap(4);
		for(size_t i = 0; i < panels.size(); i++)
		{
			GuiTextButton& tab = ui.button(panels[i].tab).size(104, 34).fontSize(UI_FONT_BODY).style(MenuBarButtonStyle());
			const int idx = (int)i;
			tab.onClick([this, idx]{
				stopListening();
				shown = idx;
			});
			tabs.push_back(&tab);
			headings.push_back(panels[i].heading);
			list.add(tab);
		}
		// the tab that is showing looks pressed in, apart from the highlight
		int styled = -1;
		ui.bind([&tabs, styled, this]() mutable {
			if(styled == shown)
				return;
			styled = shown;
			for(size_t k = 0; k < tabs.size(); k++)
				tabs[k]->style((int)k == shown ? MenuTabActiveStyle() : MenuBarButtonStyle());
		});

		GuiLayout& stack = ui.overlay();
		for(size_t i = 0; i < panels.size(); i++)
		{
			stack.add(*panels[i].layout, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
			const int idx = (int)i;
			ui.visibleWhen(*panels[i].layout, [this, idx]{ return shown == idx; });
		}
		GuiLabel& heading = ui.label().fontSize(UI_FONT_SMALL - 1).muted().bind([this, &headings]{ return std::string(headings[(size_t)shown]); });

		f.content->add(ui.row().gap(6).align(LAYOUT_ALIGN::START)
			.add(MenuPage_Section(ui, 120).pad(8, 8).add(list))
			.add(MenuPage_Section(ui, choicesW).pad(8, 6).add(ui.column().gap(4).add(heading).add(stack))));

		// while listening: the first press that is a key (or mouse button) the mapper can bind is the answer
		ui.bind([this, player]{
			if(!listen)
				return;
			CaptureEvent e;
			while(Controls().pollCapture(e))
			{
				if(e.kind != listenKind)
					continue;
				if(e.code < 0 || !Controls().usable(player, BindingTarget(e.kind, e.code)))
				{
					hintText = "That one can't be used here.  Press another, or B";
					continue;
				}
				picked = BindingTarget(e.kind, e.code);
				done = true;
				listen = false;
				return;
			}
		});
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

// ---- list pickers

void PickerView::choose(int index)
{
	if(index < 0 || (size_t)index >= items.size())
		return;
	if(!items[(size_t)index].confirm.empty() && armedIdx != index)
	{
		armedIdx = index;
		hintText = items[(size_t)index].confirm;
		return;
	}
	picked = index;
	done = true;
}

void PickerView::focusItem(int index)
{
	if(index < 0 || (size_t)index >= items.size())
		return;
	focus = index;
	if(armedIdx != index)
		armedIdx = -1;
	hintText = "A chooses, B goes back";
}

PickerView::PickerView(const char * title, const std::vector<PickerItem>& itemList, GuiSound * soundOver) :
	items(itemList), picked(-1), done(false), focus(0), armedIdx(-1), backBtn(nullptr), hintText("A chooses, B goes back")
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	MenuPageFrame f = MenuPage_Build(ui, *this, title, [this]{ return hintText; });

	int current = 0;
	for(size_t i = 0; i < items.size(); i++)
		if(items[i].current)
			current = (int)i;
	focus = current;

	// Both panels are one fixed size whatever is chosen, and everything in the right one has a slot of its own
	// (an empty slot keeps its height), so moving the cursor changes the words and nothing else
	const int listW = 200; // wide enough for a saved setup's name of 24 characters
	const int paneW = MENU_PAGE_CONTENT_W - listW - 6;
	const int panelH = 240;
	const int innerW = paneW - 24;
	const int headingH = 24, lineH = 17, rowH = 17, labelW = 112;
	const int maxLines = 4, maxRows = 7;

	std::vector<GuiTextButton*>& buttons = ui.own<std::vector<GuiTextButton*>>();
	GuiLayout& list = ui.column().gap(4).size(listW - 16, panelH - 16);
	for(size_t i = 0; i < items.size(); i++)
	{
		const int idx = (int)i;
		// a long name (a saved setup can have 24 characters) is set smaller to fit its button
		GuiTextButton& b = ui.button(items[i].name.c_str()).size(listW - 16, 34).fontSize(items[i].name.size() > 14 ? UI_FONT_SMALL + 2 : UI_FONT_BODY)
			.style(MenuBarButtonStyle(items[i].current))
			.onClick([this, idx]{ choose(idx); })
			.onFocus([this, idx]{ focusItem(idx); });
		buttons.push_back(&b);
		list.add(b);
	}

	// the right pane follows the cursor
	// a slot: a label in a box of fixed height, so the line's height never depends on the letters in it
	auto slot = [&](GuiLabel& l, int w, int h) -> GuiLayout& { return ui.row().size(w, h).align(LAYOUT_ALIGN::CENTRE).add(l); };
	GuiLabel& heading = ui.label().fontSize(UI_FONT_BODY + 2).bind([this]{
		return items.empty() ? std::string() : items[(size_t)focus].name + (items[(size_t)focus].current ? "  (in use)" : "");
	});
	GuiLayout& pane = ui.column().size(innerW, panelH - 20).add(slot(heading, innerW, headingH));
	for(int k = 0; k < maxLines; k++)
	{
		GuiLabel& line = ui.label().fontSize(UI_FONT_SMALL).muted()
			.bind([this, k]{ return items.empty() || (size_t)k >= items[(size_t)focus].lines.size() ? std::string() : items[(size_t)focus].lines[(size_t)k]; });
		pane.add(slot(line, innerW, lineH)); // (a label does not wrap: Picker_* split sentences into lines that fit)
	}
	pane.addGap(8);
	for(int k = 0; k < maxRows; k++)
	{
		GuiLabel& name = ui.label().fontSize(UI_FONT_SMALL).muted()
			.bind([this, k]{ return items.empty() || (size_t)k >= items[(size_t)focus].rows.size() ? std::string() : items[(size_t)focus].rows[(size_t)k].label; });
		GuiLabel& value = ui.label().fontSize(UI_FONT_SMALL + 1)
			.bind([this, k]{ return items.empty() || (size_t)k >= items[(size_t)focus].rows.size() ? std::string() : items[(size_t)focus].rows[(size_t)k].value; });
		pane.add(ui.row().size(innerW, rowH).align(LAYOUT_ALIGN::CENTRE).add(slot(name, labelW, rowH)).add(slot(value, innerW - labelW, rowH)));
	}

	f.content->add(ui.row().gap(6).align(LAYOUT_ALIGN::START)
		.add(MenuPage_Section(ui, listW).pad(8, 8).size(listW, panelH).add(list))
		.add(MenuPage_Section(ui, paneW).pad(12, 10).size(paneW, panelH).add(pane)));

	GuiTextButton& back = MenuPage_Button(ui, "Back");
	back.setTrigger(1, &cancel);
	backBtn = &back;
	f.footerRight->add(back);

	ui.attachTo(*this);
	// a window steps the selection down from what is selected, or goes to the first control when nothing is: start
	// just above the item in use so the step lands on it
	if(current > 0)
		buttons[(size_t)current - 1]->setState(STATE::SELECTED);
}

// Splits a sentence into lines of at most `max` characters, at spaces, so it fits a pane that cannot wrap text.
static void addWrapped(std::vector<std::string>& lines, const std::string& text, size_t max)
{
	size_t start = 0;
	while(start < text.size())
	{
		size_t end = start + max;
		if(end >= text.size())
			end = text.size();
		else
		{
			const size_t space = text.rfind(' ', end);
			if(space != std::string::npos && space > start)
				end = space;
		}
		lines.push_back(text.substr(start, end - start));
		start = end < text.size() && text[end] == ' ' ? end + 1 : end;
	}
}

std::vector<PickerItem> Picker_EmulatedItems()
{
	ControlsModel& m = Controls();
	const std::vector<EmulatedMode> modes = m.emulatedModes();
	std::vector<PickerItem> items;
	for(size_t i = 0; i < modes.size(); i++)
	{
		PickerItem it;
		it.name = modes[i].name;
		it.current = (int)i == m.currentEmulated();
		addWrapped(it.lines, modes[i].description, 44);
		if(modes[i].joysticks > 0)
			it.lines.push_back(GuiFormat("%d buttons, %d axes each.", modes[i].buttons, modes[i].axes));
		it.lines.push_back(modes[i].joysticks > 0 ? "Players without one use keys and the mouse." : "Keyboard and mouse stay available.");
		for(int p = 0; p < m.playerCount(); p++)
		{
			if(!m.player(p).connected)
				continue;
			PickerRow r;
			r.label = GuiFormat("Player %d", p + 1);
			r.value = p < modes[i].joysticks ? GuiFormat("Joystick %d", p + 1) : std::string("Keys + mouse");
			it.rows.push_back(r);
		}
		items.push_back(it);
	}
	return items;
}

std::vector<PickerItem> Picker_PresetItems(int player, std::vector<int>& index)
{
	ControlsModel& m = Controls();
	const std::vector<PresetInfo> presets = m.presets();
	static const struct { int control; const char * name; } shown[] = {
		{ INPUT_BTN_A, "A Button" }, { INPUT_BTN_B, "B Button" }, { INPUT_BTN_X, "X Button" }, { INPUT_BTN_Y, "Y Button" },
		{ INPUT_BTN_UP, "D-pad Up" }, { INPUT_BTN_PLUS, "Plus" }, { INPUT_BTN_MINUS, "Minus" } };

	std::vector<PickerItem> items;
	index.clear();
	for(size_t i = 0; i < presets.size(); i++)
	{
		const bool inUse = (int)i == m.currentPreset();
		// a preset that fires the joystick means nothing without one (unless it is the one in use, which must stay visible)
		if(presets[i].needsJoystick && m.joystickButtons(player) == 0 && !inUse)
			continue;
		PickerItem it;
		it.name = presets[i].name;
		it.current = inUse;
		addWrapped(it.lines, presets[i].description, 44);
		it.lines.push_back("Sets the buttons of every player.");
		for(size_t c = 0; c < sizeof(shown) / sizeof(shown[0]); c++)
		{
			PickerRow r;
			r.label = shown[c].name;
			r.value = m.describeTarget(player, m.presetBinding((int)i, shown[c].control));
			it.rows.push_back(r);
		}
		if(m.modified())
			it.confirm = "This replaces your changes.  Press A again to confirm";
		items.push_back(it);
		index.push_back((int)i);
	}
	return items;
}

std::vector<PickerItem> Picker_SetupItems(std::vector<int>& index)
{
	ControlsModel& m = Controls();
	const std::vector<SetupInfo> setups = m.setups();
	std::vector<PickerItem> items;
	index.clear();
	for(size_t i = 0; i < setups.size(); i++)
	{
		PickerItem it;
		it.name = setups[i].name;
		it.current = false;
		it.lines.push_back(setups[i].program.empty() ? std::string("Not tied to a program.") : "Loads for " + setups[i].program + ".");
		PickerRow dos; dos.label = "DOS sees"; dos.value = setups[i].emulated;
		PickerRow preset; preset.label = "Preset"; preset.value = setups[i].preset;
		it.rows.push_back(dos);
		it.rows.push_back(preset);
		if(m.modified())
			it.confirm = "This replaces your buttons.  Press A again to confirm";
		items.push_back(it);
		index.push_back((int)i);
	}
	return items;
}

std::vector<PickerItem> Picker_AttachItems(const std::string& running, std::vector<AttachChoice>& kinds)
{
	std::vector<PickerItem> items;
	kinds.clear();
	if(!running.empty())
	{
		PickerItem it;
		it.name = running;
		it.current = true;
		addWrapped(it.lines, "These buttons load by themselves when " + running + " starts.", 44);
		items.push_back(it);
		kinds.push_back(AttachChoice::RUNNING);
	}
	{
		PickerItem it;
		it.name = "No program";
		it.current = running.empty();
		addWrapped(it.lines, "Load it yourself from the Load button.", 44);
		items.push_back(it);
		kinds.push_back(AttachChoice::NONE);
	}
	{
		PickerItem it;
		it.name = "Type a name";
		it.current = false;
		addWrapped(it.lines, "For a program that is not running now.", 44);
		items.push_back(it);
		kinds.push_back(AttachChoice::TYPED);
	}
	return items;
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

bool ChooserScreen(const char * name, int player, int control, BindingTarget& target, const BindingTarget& defaultTarget)
{
	GuiSound over(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	TargetChooserView view(name, player, control, target, defaultTarget, &over);
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
		{
			if(!view.listening())
				return false;
			view.stopListening(); // B while waiting for a key only stops waiting
			view.backButton().setState(STATE::DEFAULT);
		}
	}
}

// Runs a picker; the index chosen, or -1 on Back.
int PickerScreen(const char * title, const std::vector<PickerItem>& items)
{
	GuiSound over(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	PickerView view(title, items, &over);
	ScreenWindow screen(view);

	while(true)
	{
		UpdateGui();
		if(view.chosen())
			return view.result();
		if(view.backButton().getState() == STATE::CLICKED)
			return -1;
	}
}

void EmulatedPicker()
{
	const int chosen = PickerScreen("What does DOS see?", Picker_EmulatedItems());
	if(chosen >= 0)
		Controls().setEmulated(chosen);
}

void PresetPicker(int player)
{
	std::vector<int> index;
	const int chosen = PickerScreen("Choose a preset", Picker_PresetItems(player, index));
	if(chosen >= 0)
		Controls().applyPreset(index[(size_t)chosen]);
}

// Asks something with a few answers; the index chosen, or -1.
int AskScreen(const char * title, const std::vector<std::string>& answers, const std::string& detail)
{
	std::vector<PickerItem> items;
	for(size_t i = 0; i < answers.size(); i++)
	{
		PickerItem it;
		it.name = answers[i];
		it.current = false;
		addWrapped(it.lines, detail, 44);
		items.push_back(it);
	}
	return PickerScreen(title, items);
}

// Load: choose a saved setup. What the hint line should say after.
std::string LoadFlow()
{
	std::vector<int> index;
	const int chosen = PickerScreen("Load a setup", Picker_SetupItems(index));
	if(chosen < 0)
		return std::string();
	const std::vector<SetupInfo> setups = Controls().setups();
	if(!Controls().loadSetup(index[(size_t)chosen]))
		return "That setup could not be loaded";
	return "Loaded " + setups[(size_t)index[(size_t)chosen]].name;
}

// Save: a name, what program it is for, and a check before replacing one of the same name.
std::string SaveFlow()
{
	ControlsModel& m = Controls();
	const std::string running = Setup_CleanProgram(m.runningProgram());

	char typed[64];
	snprintf(typed, sizeof(typed), "%s", Setup_SuggestedName(running).c_str());
	if(!OnScreenKeyboard(typed, sizeof(typed)))
		return std::string();
	const std::string name = Setup_CleanName(typed);
	if(name.empty())
		return "That name can't be used";

	std::vector<AttachChoice> kinds;
	const int attach = PickerScreen("Load these buttons for", Picker_AttachItems(running, kinds));
	if(attach < 0)
		return std::string();
	std::string program;
	if(kinds[(size_t)attach] == AttachChoice::RUNNING)
		program = running;
	else if(kinds[(size_t)attach] == AttachChoice::TYPED)
	{
		char prog[64] = { 0 };
		if(!OnScreenKeyboard(prog, sizeof(prog)))
			return std::string();
		program = Setup_CleanProgram(prog);
	}

	const std::vector<SetupInfo> existing = m.setups();
	for(size_t i = 0; i < existing.size(); i++)
	{
		if(existing[i].name == name)
		{
			std::vector<std::string> answers;
			answers.push_back("Replace it");
			answers.push_back("Cancel");
			if(AskScreen(GuiFormat("%s already exists", name.c_str()).c_str(), answers, "Replacing it loses what was saved under that name.") != 0)
				return std::string();
			break;
		}
	}
	return m.saveSetup(name, program) ? "Saved " + name : std::string("Could not save the setup");
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
			if(ChooserScreen(row.name, view.player(), row.binding, target, Controls().defaultBinding(row.binding)))
				Controls().setBinding(view.player(), row.binding, target);
			screen.show();
		}
		const ControllerConfigView::Pick pick = view.takePick();
		if(pick != ControllerConfigView::Pick::NONE)
		{
			screen.hide();
			std::string said;
			if(pick == ControllerConfigView::Pick::EMULATED)
				EmulatedPicker();
			else if(pick == ControllerConfigView::Pick::PRESET)
				PresetPicker(view.player());
			else if(pick == ControllerConfigView::Pick::SAVE)
				said = SaveFlow();
			else
				said = LoadFlow();
			screen.show();
			if(!said.empty())
				view.setStatus(said);
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

void Controls_Menu(GuiWindow * home, int player)
{
	mainWindow->remove(home);
	if(player >= 0)
		ConfigScreen(player);
	else
		HubScreen();
	mainWindow->append(home);
}
