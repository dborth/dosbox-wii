/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsui.cpp
 ***************************************************************************/
#include "controlsui.h"
#include "menu.h"
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

// ---- hub

void ControlsHubView::deviceRow(GuiLayout& grid, const char * name, std::function<bool()> present)
{
	GuiShape& dot = ui.make<GuiShape>(10, 10);
	dot.setShape(SHAPE::ELLIPSE);
	int last = -1;
	ui.bind([&dot, present, last]() mutable {
		const int s = present() ? 1 : 0;
		if(s != last)
		{
			last = s;
			dot.setPaint(GuiPaint().color(s ? GuiTheme::current().good : GuiTheme::current().outline));
		}
	});

	const std::string label = name;
	GuiLabel& text = ui.label().fontSize(UI_FONT_BODY - 2).muted();
	text.bind([label, present]{ return GuiFormat("%s: %s", label.c_str(), present() ? "connected" : "none"); });

	grid.add(ui.row().gap(8).align(LAYOUT_ALIGN::CENTRE).add(dot).add(text));
}

GuiCard& ControlsHubView::playerCard(int n)
{
	UiBatteryBar& battery = ui.make<UiBatteryBar>(48, 10);
	ui.bind([&battery, n]{
		const int b = Controls().player(n).battery;
		battery.setLevel(b);
		battery.setVisible(b >= 0);
	});

	const std::function<bool()> connected = [n]{ return Controls().player(n).connected; };
	const std::function<bool()> absent = [n]{ return !Controls().player(n).connected; };

	return ui.card().size(308, 0).enabledWhen(connected)
		.onFocus([this, n]{
			const PlayerInfo p = Controls().player(n);
			status = GuiFormat("P%d  %s  -  %s", n + 1, p.device.c_str(), p.role.c_str());
		})
		.onClick([this, n]{ request = n; })
		.add(ui.column().gap(6)
			.add(ui.row().size(288, 0).gap(8).align(LAYOUT_ALIGN::CENTRE)
				.add(ui.label(GuiFormat("P%d", n + 1).c_str()).fontSize(UI_FONT_HEADING).accent())
				.add(ui.label().bind([n]{ return Controls().player(n).device; }).fontSize(UI_FONT_BODY + 2).visibleWhen(connected), GuiItem().grow(1))
				.add(ui.label("Not connected").fontSize(UI_FONT_BODY + 2).muted().visibleWhen(absent), GuiItem().grow(1))
				.add(battery))
			.add(ui.row().gap(8).align(LAYOUT_ALIGN::CENTRE)
				.add(ui.chip().bind([n]{ return Controls().player(n).role; }).visibleWhen(connected))
				.add(ui.label().bind([n]{ return Controls().player(n).scheme; }).fontSize(UI_FONT_BODY - 2).muted().visibleWhen(connected)))
			.add(ui.label().bind([n]{ return Controls().player(n).connected ? "Set up this controller's buttons" : "Press a button on a controller to connect it"; })
				.fontSize(UI_FONT_SMALL + 1).muted()));
}

ControlsHubView::ControlsHubView(GuiSound * soundOver) :
	request(-1), status("Choose a controller to set up its buttons"), backBtn(nullptr)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();
	const GuiTheme& t = ui.theme();

	// title and the control scheme, which applies to every player
	GuiTextButton& scheme = ui.button("").size(240, 30).fontSize(UI_FONT_BODY)
		.bind([]{ ControlsModel& m = Controls(); return GuiFormat("Scheme: %s", m.schemes()[(size_t)m.currentScheme()].c_str()); })
		.onClick([this]{
			ControlsModel& m = Controls();
			m.setScheme(m.currentScheme() + 1);
			status = GuiFormat("Scheme: %s", m.schemes()[(size_t)m.currentScheme()].c_str());
		})
		.onFocus([this]{ status = "A changes the control scheme for every player"; });

	GuiLayout& devices = ui.grid(2).size(584, 0).gap(0, 8);
	deviceRow(devices, "USB keyboard", []{ return Controls().devices().keyboard; });
	deviceRow(devices, "USB mouse", []{ return Controls().devices().mouse; });
	deviceRow(devices, "Wiimote pointer", []{ return Controls().devices().wiimoteIr; });
	if(Controls().devices().gamepadPresent)
		deviceRow(devices, "GamePad touch", []{ return Controls().devices().gamepadTouch; });

	GuiTextButton& back = ui.button("Back").size(90, 30).fontSize(UI_FONT_BODY);
	back.setTrigger(1, &cancel);
	backBtn = &back;

	this->gap(10);
	this->add(ui.row().size(624, 0).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.label("Controllers").fontSize(UI_FONT_TITLE))
		.addSpace()
		.add(scheme));
	this->add(ui.grid(2).gap(8, 8)
		.add(playerCard(0)).add(playerCard(1)).add(playerCard(2)).add(playerCard(3)));
	this->add(ui.card().selectable(false).style(t.panelStyle()).size(624, 0).pad(14, 10).add(devices));
	this->add(ui.card().selectable(false).style(t.barStyle()).size(624, 0).pad(10, 6)
		.add(ui.row().size(604, 0).align(LAYOUT_ALIGN::CENTRE)
			.add(ui.label().bind([this]{ return status; }).fontSize(UI_FONT_BODY - 1).muted())
			.addSpace()
			.add(back)));

	this->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	this->setPosition(8, 8);
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

// A list row: an icon, the control's name, and below it what the control does. Selecting it (pointer or
// D-pad) highlights the control on the drawing and draws a line to it; clicking asks to change it.
GuiCard& ControllerConfigView::bindingRow(const ProfileRow& r, Side side, int width)
{
	const GuiTheme& t = ui.theme();
	const PadType pt = padType;
	const int id = r.binding;
	const char * fixed = r.fixed;
	const char * name = r.name;

	GuiLabel& nameLabel = ui.label(name).fontSize(UI_FONT_BODY - 1);
	GuiLabel& target = ui.label().fontSize(UI_FONT_SMALL - 1).muted();
	target.setMaxWidth(width - 16 - 30 - 8); // long choices are cut rather than spilling out of the row
	target.bind([pt, id, fixed]{ return fixed ? std::string(fixed) : Controls().binding(pt, id).describe(); });

	GuiLayout& text = ui.column().gap(0).align(side == Side::LEFT ? LAYOUT_ALIGN::END : LAYOUT_ALIGN::START).add(nameLabel).add(target);
	GuiLayout& content = ui.row().size(width - 16, 0).gap(8).align(LAYOUT_ALIGN::CENTRE);
	GuiGlyph& icon = ui.glyph(r.glyph, r.letter);
	if(side == Side::LEFT)
		content.addSpace().add(text).add(icon);
	else
		content.add(icon).add(text);

	GuiCard& row = ui.card();
	row.size(width, 0).style(t.rowStyle()).pad(8, 3);
	row.corners(GuiCorners(4.0f));
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
		status = fixed ? GuiFormat("%s: %s", name, fixed) : GuiFormat("%s: %s   (A to change)", name, Controls().binding(pt, id).describe().c_str());
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
	view = &ui.controllerView(*profile.layout, profile.scale);
	const int sideW = 188, groupW = 200;

	GuiLayout& left = ui.column().gap(4).align(LAYOUT_ALIGN::END).pad(0, 8, 0, 0);
	for(size_t i = 0; i < profile.left.size(); i++)
		left.add(bindingRow(profile.left[i], Side::LEFT, sideW));
	GuiLayout& right = ui.column().gap(4).pad(0, 8, 0, 0);
	for(size_t i = 0; i < profile.right.size(); i++)
		right.add(bindingRow(profile.right[i], Side::RIGHT, sideW));

	GuiLayout& groups = ui.row().gap(12);
	for(size_t g = 0; g < profile.groups.size(); g++)
	{
		GuiLayout& column = ui.column().gap(2).add(ui.label(profile.groups[g].title).fontSize(UI_FONT_SMALL).muted());
		for(size_t i = 0; i < profile.groups[g].rows.size(); i++)
			column.add(bindingRow(profile.groups[g].rows[i], Side::GROUP, groupW));
		groups.add(column);
	}

	const PadType pt = padType;
	GuiTextButton& reset = ui.button("Reset").size(90, 30).fontSize(UI_FONT_BODY)
		.onClick([this, pt]{
			Controls().resetBindings(pt);
			status = "Buttons restored to the scheme's defaults";
		})
		.onFocus([this]{ status = "A restores every button to the scheme's defaults"; });
	GuiTextButton& back = ui.button("Back").size(90, 30).fontSize(UI_FONT_BODY);
	back.setTrigger(1, &cancel);
	backBtn = &back;

	this->gap(8);
	this->add(ui.row().size(624, 0).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.label(GuiFormat("P%d  %s", player + 1, info.device.c_str()).c_str()).fontSize(UI_FONT_HEADING + 2))
		.addSpace()
		.add(ui.chip().bind([]{ ControlsModel& m = Controls(); return m.schemes()[(size_t)m.currentScheme()]; })));
	this->add(ui.row().gap(4).add(left).add(*view).add(right));
	this->add(groups);
	this->add(ui.card().selectable(false).style(t.barStyle()).size(624, 0).pad(10, 6)
		.add(ui.row().size(604, 0).gap(8).align(LAYOUT_ALIGN::CENTRE)
			.add(ui.label().bind([this]{ return status; }).fontSize(UI_FONT_BODY - 1).muted())
			.addSpace()
			.add(reset)
			.add(back)));

	this->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	this->setPosition(8, 4);
	ui.attachTo(*this);
}

// ---- target chooser

void TargetChooserView::optionButtons(GuiLayout& grid, TARGET kind, int w, int h, int fontPx)
{
	const GuiTheme& t = ui.theme();
	const std::vector<std::string>& names = Catalog_Names(kind);
	for(size_t i = 0; i < names.size(); i++)
	{
		GuiTextButton& b = ui.button(names[i].c_str()).size(w, h).fontSize(fontPx);
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
	current(now), shown(0), done(false), backBtn(nullptr)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();
	const GuiTheme& t = ui.theme();

	GuiTextButton& back = ui.button("Back").size(90, 30).fontSize(UI_FONT_BODY);
	back.setTrigger(1, &cancel);
	backBtn = &back;

	GuiLayout& titleRow = ui.row().size(624, 0).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.label(GuiFormat("What should %s do?", controlName).c_str()).fontSize(UI_FONT_HEADING + 2))
		.addSpace()
		.add(ui.label(GuiFormat("Now: %s", now.describe().c_str()).c_str()).fontSize(UI_FONT_BODY - 1).muted());

	GuiLayout& body = ui.row().gap(8);
	if(stickMode)
	{
		GuiLayout& modes = ui.grid(1).gap(0, 8);
		optionButtons(modes, TARGET::STICK_MODE, 300, 42, UI_FONT_BODY + 2);
		body.add(ui.card().selectable(false).style(t.panelStyle()).size(624, 0).pad(14, 12).add(modes));
	}
	else
	{
		// categories on the left; moving over one shows its choices on the right
		static const char * const categories[5] = { "Key", "Joystick button", "Mouse", "DOSBox function", "Not assigned" };
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
			entry.size(150, 0).style(t.rowStyle()).pad(10, 7);
			entry.corners(GuiCorners(4.0f));
			entry.onFocus([this, k]{ shown = k; });
			entry.add(ui.label(categories[k]).fontSize(UI_FONT_BODY));
			list.add(entry);
		}

		GuiLayout& keys = ui.grid(9).size(432, 0).gap(4, 4);
		optionButtons(keys, TARGET::KEY, 44, 26, UI_FONT_SMALL);
		GuiLayout& joystick = ui.grid(2).size(432, 0).gap(8, 8);
		optionButtons(joystick, TARGET::JOYSTICK, 208, 34, UI_FONT_BODY);
		GuiLayout& mouse = ui.grid(2).size(432, 0).gap(8, 8);
		optionButtons(mouse, TARGET::MOUSE, 208, 34, UI_FONT_BODY);
		GuiLayout& hotkeys = ui.grid(2).size(432, 0).gap(8, 8);
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

		body.add(list);
		body.add(ui.card().selectable(false).style(t.panelStyle()).size(462, 266).pad(14, 12).add(stack));
	}

	this->gap(10);
	this->add(titleRow);
	this->add(body);
	this->add(ui.card().selectable(false).style(t.barStyle()).size(624, 0).pad(10, 6)
		.add(ui.row().size(604, 0).align(LAYOUT_ALIGN::CENTRE)
			.add(ui.label(stickMode ? "A picks what the stick does" : "Move over a category, then pick a choice").fontSize(UI_FONT_BODY - 1).muted())
			.addSpace()
			.add(back)));

	this->setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	this->setPosition(8, 8);
	ui.attachTo(*this);
}

// ---- running the screens on the menu's main window

namespace {

// A full-screen page on the menu's main window: a dimmed backdrop with a view on it. It can be taken off the
// window while another screen runs on top (a window only hands the D-pad to the topmost screen that way).
class ScreenWindow
{
	public:
		ScreenWindow(GuiElement& view) :
			backdrop(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight()),
			w(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight()),
			shown(false)
		{
			backdrop.setPaint(UiBackdropPaint());
			w.append(&backdrop);
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
		GuiShape backdrop;	// declared before the window, so the window lets go of it before it is destroyed
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
