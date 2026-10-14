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

// An icon from the asset list, optionally faded
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
	GuiLayout& art = ui.overlay().size(48, 48);
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
	GuiLayout& joystick = ui.overlay().size(48, 48).add(iconImage(ui, icon_joystick_png)).add(number, GuiItem().at(LAYOUT_ALIGN::END, LAYOUT_ALIGN::END));
	ui.visibleWhen(joystick, [n]{ return Controls().player(n).emulated == Emulated::JOYSTICK; });

	GuiLayout& keysMouse = ui.row().gap(2).add(iconImage(ui, icon_keyboard_png)).add(iconImage(ui, icon_mouse_png));
	ui.visibleWhen(keysMouse, [n]{ return Controls().player(n).emulated == Emulated::KEYS_MOUSE; });

	return ui.row().add(joystick).add(keysMouse);
}

} // namespace

// ---- hub

// A small tile in the header for one shared device: its icon, faded while it is not there, with a dot
GuiCard& ControlsHubView::deviceChip(const unsigned char * png, std::function<std::string()> tip, std::function<bool()> present)
{
	GuiImage& img = iconImage(ui, png);
	img.setScale(0.667f);
	GuiShape& dot = ui.make<GuiShape>(9, 9);
	dot.setShape(SHAPE::ELLIPSE);
	GuiLayout& art = ui.overlay().size(34, 34);
	art.add(img).add(dot, GuiItem().at(LAYOUT_ALIGN::END, LAYOUT_ALIGN::END));

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

	return ui.card().pad(5).corners(GuiCorners(10.0f))
		.onFocus([this, tip]{ status = tip(); })
		.add(art);
}

GuiCard& ControlsHubView::playerCard(int n)
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

	GuiArrow& arrow = ui.make<GuiArrow>(ARROW::RIGHT, 16, 24, t.outlineStrong, t.outlineStrong);
	ui.visibleWhen(arrow, connected);

	GuiCard& dos = ui.card().selectable(false).style(t.panelStyle()).pad(10, 2).visibleWhen(connected).add(emulatedArt(ui, n));

	GuiLabel& waiting = ui.label("Press a button on a controller").fontSize(UI_FONT_BODY + 2).muted().visibleWhen(absent);

	// the controller on the left and what DOS sees on the right, with the arrow in the same place on every card
	GuiLayout& hardware = ui.row().size(318, 0).gap(14).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.label(GuiFormat("P%d", n + 1).c_str()).fontSize(UI_FONT_HEADING).accent().visibleWhen(connected))
		.add(padArt(ui, n))
		.add(who)
		.add(waiting);
	GuiLayout& content = ui.row().size(MENU_PAGE_BODY_W - 32, 0).align(LAYOUT_ALIGN::CENTRE)
		.add(hardware)
		.add(arrow)
		.addSpace()
		.add(dos);

	GuiCard& card = ui.card().size(MENU_PAGE_BODY_W, 0).pad(16, 2)
		.onFocus([this, n]{
			const PlayerInfo p = Controls().player(n);
			if(!p.connected)
			{
				status = "Press a button on a controller to connect it";
				return;
			}
			status = GuiFormat("P%d  %s plays as %s.  A sets up its buttons", n + 1, p.device.c_str(), p.role.c_str());
		})
		.onClick([this, n]{ if(Controls().player(n).connected) request = n; })
		.add(content);
	return card;
}

ControlsHubView::ControlsHubView(GuiSound * soundOver) :
	request(-1), status("Controllers on the left, what DOS sees on the right"), backBtn(nullptr), heroMode(true)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	MenuPageFrame f = MenuPage_Build(ui, *this, "Controllers", [this]{ return status; });

	// the shared devices and the scheme, which apply to every player
	GuiTextButton& scheme = ui.button("").size(204, 38).fontSize(UI_FONT_BODY).style(MenuBarButtonStyle())
		.bind([]{ ControlsModel& m = Controls(); return GuiFormat("Scheme: %s", m.schemes()[(size_t)m.currentScheme()].c_str()); })
		.onClick([this]{
			ControlsModel& m = Controls();
			m.setScheme(m.currentScheme() + 1);
			status = GuiFormat("Scheme: %s", m.schemes()[(size_t)m.currentScheme()].c_str());
		})
		.onFocus([this]{ status = "A changes the control scheme for every player"; });

	f.headerRight->add(deviceChip(icon_keyboard_png,
		[]{ return Controls().devices().keyboard ? "USB keyboard: typing goes to DOS" : "USB keyboard: not connected"; },
		[]{ return Controls().devices().keyboard; }));
	f.headerRight->add(deviceChip(icon_mouse_png,
		[]{ return Controls().devices().mouse ? "USB mouse: moves the DOS mouse" : "USB mouse: not connected"; },
		[]{ return Controls().devices().mouse; }));
	f.headerRight->add(deviceChip(icon_pointer_png,
		[]{ return Controls().devices().wiimoteIr ? "Wiimote pointer: moves the DOS mouse" : "Wiimote pointer: not pointing at the screen"; },
		[]{ return Controls().devices().wiimoteIr; }));
	f.headerRight->add(scheme);

	// A slot that has nothing in it takes no room (the first one always shows, to say what to do), and the cards
	// are roomier when there is only one player, which is the usual case. This runs before the cards' own
	// bindings: showing a window shows everything inside it, and those bindings then hide what should be hidden.
	for(int n = 0; n < 4; n++)
		cards[n] = nullptr;
	ui.bind([this]{
		if(!cards[3])
			return;
		int shown = 0;
		for(int n = 0; n < 4; n++)
		{
			const bool v = n == 0 || Controls().player(n).connected;
			if(cards[n]->isVisible() != v)
				cards[n]->setVisible(v);
			shown += v ? 1 : 0;
		}
		const bool hero = shown <= 1;
		if(hero != heroMode || cards[0]->getHeight() == 0)
		{
			heroMode = hero;
			for(int n = 0; n < 4; n++)
				cards[n]->setFixedSize(MENU_PAGE_BODY_W, hero ? 112 : 56);
		}
	});

	f.body->gap(6);
	for(int n = 0; n < 4; n++)
	{
		cards[n] = &playerCard(n);
		f.body->add(*cards[n]);
	}
	ui.runBindings();
	// When the page takes focus its window moves the selection one step down from whatever is selected (or to the
	// first thing in reading order when nothing is). Starting from the scheme button, which is just above the first
	// card, lands that step on the first player rather than on the header.
	scheme.setState(STATE::SELECTED);

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
	const GuiTheme& t = ui.theme();
	const PadType pt = padType;
	const int id = r.binding;
	const char * fixed = r.fixed;
	const char * name = r.name;

	GuiLabel& target = ui.label().fontSize(UI_FONT_SMALL + 1);
	target.setMaxWidth(width - 12 - 22 - 8); // long choices are cut rather than spilling out of the row
	target.bind([pt, id, fixed]{ return fixed ? std::string(fixed) : Controls().binding(pt, id).describe(); });

	GuiLayout& content = ui.row().size(width - 12, 0).gap(8).align(LAYOUT_ALIGN::CENTRE);
	GuiGlyph& icon = ui.glyph(r.glyph, r.letter);
	if(side == Side::LEFT)
		content.addSpace().add(target).add(icon);
	else
		content.add(icon).add(target);

	GuiCard& row = ui.card();
	row.size(width, 0).style(t.rowStyle()).pad(6, 1);
	row.corners(GuiCorners(6.0f));
	const int control = r.control;
	row.onFocus([this, &row, control, side, name, pt, id, fixed]{
		view->setHighlight(control);
		const float midY = (float)(row.getTop() + row.getHeight() / 2);
		if(side == Side::LEFT)
			view->setLeader(true, (float)(row.getLeft() + row.getWidth()), midY);
		else if(side == Side::RIGHT)
			view->setLeader(true, (float)row.getLeft(), midY);
		else
			view->setLeader(true, (float)(row.getLeft() + row.getWidth() / 2), (float)row.getTop());
		status = fixed ? GuiFormat("%s: %s", name, fixed) : GuiFormat("%s: %s.  A changes it", name, Controls().binding(pt, id).describe().c_str());
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
	padType(Controls().player(player).type), view(nullptr), status("Move over a control to see what it does"), hasRequest(false), backBtn(nullptr)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();
	const GuiTheme& t = ui.theme();

	const PlayerInfo info = Controls().player(player);
	const ControllerProfile& profile = Controllers_Profile(padType);
	MenuPageFrame f = MenuPage_Build(ui, *this, GuiFormat("P%d  %s", player + 1, info.device.c_str()).c_str(), [this]{ return status; });
	f.headerRight->add(ui.chip().bind([]{ ControlsModel& m = Controls(); return m.schemes()[(size_t)m.currentScheme()]; }));

	view = &ui.controllerView(*profile.layout, profile.scale);
	const int sideW = 168, groupW = 152;

	GuiLayout& left = ui.column().gap(2).align(LAYOUT_ALIGN::END);
	for(size_t i = 0; i < profile.left.size(); i++)
		left.add(bindingRow(profile.left[i], Side::LEFT, sideW));
	GuiLayout& right = ui.column().gap(2);
	for(size_t i = 0; i < profile.right.size(); i++)
		right.add(bindingRow(profile.right[i], Side::RIGHT, sideW));

	GuiLayout& groups = ui.row().gap(8);
	for(size_t g = 0; g < profile.groups.size(); g++)
	{
		GuiLayout& column = ui.column().gap(1).pad(0, 0, 0, 0).add(ui.label(profile.groups[g].title).fontSize(UI_FONT_SMALL - 1).muted());
		for(size_t i = 0; i < profile.groups[g].rows.size(); i++)
			column.add(bindingRow(profile.groups[g].rows[i], Side::GROUP, groupW));
		groups.add(ui.card().selectable(false).style(t.panelStyle()).pad(8, 5).add(column));
	}

	f.body->gap(6);
	f.body->add(ui.card().selectable(false).style(t.panelStyle()).pad(8, 6).add(ui.row().gap(4).align(LAYOUT_ALIGN::CENTRE).add(left).add(*view).add(right)));
	f.body->add(groups);

	const PadType pt = padType;
	GuiTextButton& reset = MenuPage_Button(ui, "Reset")
		.onClick([this, pt]{
			Controls().resetBindings(pt);
			status = "Buttons restored to the scheme's defaults";
		})
		.onFocus([this]{ status = "A restores every button to the scheme's defaults"; });
	GuiTextButton& back = MenuPage_Button(ui, "Back");
	back.setTrigger(1, &cancel);
	backBtn = &back;
	f.footerRight->add(reset).add(back);

	ui.attachTo(*this);
}

// ---- target chooser

void TargetChooserView::optionButtons(GuiLayout& grid, TARGET kind, int w, int h, int fontPx)
{
	const GuiTheme& t = ui.theme();
	const std::vector<std::string>& names = Catalog_Names(kind);
	for(size_t i = 0; i < names.size(); i++)
	{
		// the longest key names do not fit a key
		const char * shown = names[i] == "Backspace" ? "Bksp" : names[i].c_str();
		GuiTextButton& b = ui.button(shown).size(w, h).fontSize(fontPx);
		if(current.kind == kind && current.code == (int)i)
		{
			// what the control does now, marked with an accent outline
			GuiShapeStyle s = t.buttonStyle();
			s.normal.stroke(3.0f, t.accent);
			b.style(s);
		}
		const int code = (int)i;
		b.onClick([this, kind, code]{
			picked = BindingTarget(kind, code);
			done = true;
		});
		grid.add(b);
	}
}

TargetChooserView::TargetChooserView(const char * controlName, bool stickMode, const BindingTarget& now, GuiSound * soundOver) :
	current(now), shown(0), done(false), backBtn(nullptr), hintText(stickMode ? "A picks what the stick does" : "Move over a category, then pick a choice")
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();
	const GuiTheme& t = ui.theme();

	MenuPageFrame f = MenuPage_Build(ui, *this, GuiFormat("What should %s do?", controlName).c_str(), [this]{ return hintText; });
	f.headerRight->add(ui.chip(GuiFormat("Now: %s", now.describe().c_str()).c_str()));

	if(stickMode)
	{
		GuiLayout& modes = ui.grid(1).gap(0, 8);
		optionButtons(modes, TARGET::STICK_MODE, 300, 40, UI_FONT_BODY + 2);
		f.body->add(ui.card().selectable(false).style(t.panelStyle()).pad(14, 10).add(modes));
	}
	else
	{
		// categories on the left; moving over one shows its choices on the right
		static const char * const categories[5] = { "Key", "Joystick", "Mouse", "DOSBox", "None" };
		static const TARGET kinds[4] = { TARGET::KEY, TARGET::JOYSTICK, TARGET::MOUSE, TARGET::HOTKEY };
		for(int k = 0; k < 4; k++)
			if(now.kind == kinds[k])
				shown = k;
		if(now.kind == TARGET::NONE)
			shown = 0;

		GuiLayout& list = ui.column().gap(4);
		for(int k = 0; k < 5; k++)
		{
			GuiCard& entry = ui.card();
			entry.size(100, 0).style(t.rowStyle()).pad(8, 8);
			entry.corners(GuiCorners(6.0f));
			entry.onFocus([this, k]{ shown = k; });
			entry.add(ui.label(categories[k]).fontSize(UI_FONT_BODY));
			list.add(entry);
		}

		GuiLayout& keys = ui.grid(9).size(420, 0).gap(3, 3);
		optionButtons(keys, TARGET::KEY, 44, 26, UI_FONT_SMALL);
		GuiLayout& joystick = ui.grid(2).size(424, 0).gap(8, 8);
		optionButtons(joystick, TARGET::JOYSTICK, 208, 34, UI_FONT_BODY);
		GuiLayout& mouse = ui.grid(2).size(424, 0).gap(8, 8);
		optionButtons(mouse, TARGET::MOUSE, 208, 34, UI_FONT_BODY);
		GuiLayout& hotkeys = ui.grid(2).size(424, 0).gap(8, 8);
		optionButtons(hotkeys, TARGET::HOTKEY, 208, 34, UI_FONT_BODY);
		GuiTextButton& clear = ui.button("Leave unassigned").size(208, 34).fontSize(UI_FONT_BODY)
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

		f.body->add(ui.row().gap(4).align(LAYOUT_ALIGN::START).add(ui.card().selectable(false).style(t.panelStyle()).pad(6, 6).add(list))
			.add(ui.card().selectable(false).style(t.panelStyle()).size(436, 256).pad(8, 8).add(stack)));
	}

	GuiTextButton& back = MenuPage_Button(ui, "Back");
	back.setTrigger(1, &cancel);
	backBtn = &back;
	f.footerRight->add(back);

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

bool ChooserScreen(const char * name, bool stickMode, BindingTarget& target)
{
	GuiSound over(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	TargetChooserView view(name, stickMode, target, &over);
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
			BindingTarget target = Controls().binding(view.type(), row.binding);
			if(ChooserScreen(row.name, row.stickMode, target))
				Controls().setBinding(view.type(), row.binding, target);
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
