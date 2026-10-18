/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2026
 * homeview.cpp
 ***************************************************************************/
#include "homeview.h"
#include "controlsui.h"
#include "controlsmodel.h"
#include "menu.h"
#include "menustyle.h"
#include "filelist.h"

namespace {

// ---- icons drawn from shapes, in a 24 x 24 box scaled to the element's size

enum { ICON_NONE = -1, ICON_GAUGE, ICON_FRAMES, ICON_ASPECT, ICON_FILTER, ICON_KEYBOARD, ICON_GEAR, ICON_PULSE, ICON_DISK, ICON_POWER, ICON_INFO, ICON_PLAY, ICON_PAD };

class HomeIcon : public GuiElement
{
	public:
		HomeIcon(int k, int size, PixelColor c) : kind(k), color(c), meshFeather(-1.0f)
		{
			width = size;
			height = size;
		}

		void setColor(PixelColor c)
		{
			if(c.r != color.r || c.g != color.g || c.b != color.b || c.a != color.a)
			{
				color = c;
				meshFeather = -1.0f;
			}
		}

		void draw() override
		{
			if(!this->isVisible())
				return;
			const int a = this->getAlpha();
			if(a > 0)
			{
				VideoDriver * video = platform->getVideo();
				const float uiScale = video->getUIScale();
				const float feather = uiScale > 0.0f ? 1.0f / uiScale : 1.0f;
				if(feather != meshFeather)
				{
					mesh = GuiMesh(feather);
					build();
					meshFeather = feather;
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

	private:
		float u;
		GuiFill fill;

		GuiPoint P(float x, float y) const { return GuiPoint{ x * u, y * u }; }
		void line(float x1, float y1, float x2, float y2, float w = 2.0f)
		{
			mesh.addLine(P(x1, y1), P(x2, y2), w * u, fill);
			// round the ends, so joined lines have no notch at the corner
			mesh.addEllipse(GuiRect{ (x1 - w / 2) * u, (y1 - w / 2) * u, w * u, w * u }, fill);
			mesh.addEllipse(GuiRect{ (x2 - w / 2) * u, (y2 - w / 2) * u, w * u, w * u }, fill);
		}
		void box(float x, float y, float w, float h, float r, float lw = 2.0f)
		{
			mesh.strokeRoundRect(GuiRect{ x * u, y * u, w * u, h * u }, GuiCorners(r * u), lw * u, STROKE::INSIDE, fill);
		}
		void ring(float cx, float cy, float r, float lw = 2.0f)
		{
			mesh.strokeEllipse(GuiRect{ (cx - r) * u, (cy - r) * u, 2 * r * u, 2 * r * u }, lw * u, STROKE::CENTRE, fill);
		}
		void dot(float cx, float cy, float r)
		{
			mesh.addEllipse(GuiRect{ (cx - r) * u, (cy - r) * u, 2 * r * u, 2 * r * u }, fill);
		}

		void build()
		{
			u = (float)width / 24.0f;
			fill = GuiFill::solid(color);
			switch(kind)
			{
				case ICON_GAUGE:
					mesh.strokeArc(P(12, 13), 9 * u, 150.0f, 240.0f, 2.2f * u, color);
					line(12, 13, 16.5f, 8.5f);
					dot(12, 13, 1.8f);
					break;
				case ICON_FRAMES:
					// a strip of film: the frame in the middle, a column of sprocket holes down each side. The rails stop inside
					// the outline (not past it) and the holes sit clear of both the outline and the rails.
					box(2, 4, 20, 16, 2.5f);
					line(8.8f, 5.5f, 8.8f, 18.5f, 1.6f);
					line(15.2f, 5.5f, 15.2f, 18.5f, 1.6f);
					for(int k = 0; k < 3; k++)
					{
						dot(5.9f, 8.5f + 3.5f * k, 1.0f);
						dot(18.1f, 8.5f + 3.5f * k, 1.0f);
					}
					break;
				case ICON_ASPECT:
					box(3, 4, 18, 13, 2.5f);
					line(12, 17, 12, 20.5f);
					line(8, 20.5f, 16, 20.5f);
					break;
				case ICON_FILTER:
					box(3.5f, 3.5f, 7.5f, 7.5f, 1.5f);
					box(13, 13, 7.5f, 7.5f, 1.5f);
					mesh.addRoundRect(GuiRect{ 13 * u, 3.5f * u, 7.5f * u, 7.5f * u }, GuiCorners(1.5f * u), fill);
					mesh.addRoundRect(GuiRect{ 3.5f * u, 13 * u, 7.5f * u, 7.5f * u }, GuiCorners(1.5f * u), fill);
					break;
				case ICON_KEYBOARD:
					box(2, 6, 20, 12, 2.5f);
					dot(6.5f, 10, 1.0f); dot(10.5f, 10, 1.0f); dot(14.5f, 10, 1.0f); dot(18, 10, 1.0f);
					line(7, 14.2f, 17, 14.2f, 1.8f);
					break;
				case ICON_GEAR:
					ring(12, 12, 6.2f);
					ring(12, 12, 1.8f);
					for(int k = 0; k < 8; k++)
					{
						const float t = (float)k * 3.14159265f / 4.0f;
						line(12 + 7.4f * cosf(t), 12 + 7.4f * sinf(t), 12 + 10 * cosf(t), 12 + 10 * sinf(t), 3.0f);
					}
					break;
				case ICON_PULSE:
					line(2.5f, 12, 7, 12);
					line(7, 12, 10, 5);
					line(10, 5, 14, 19);
					line(14, 19, 17, 12);
					line(17, 12, 21.5f, 12);
					break;
				case ICON_DISK:
					line(4, 4, 17, 4); line(17, 4, 20, 7); line(20, 7, 20, 20); line(20, 20, 4, 20); line(4, 20, 4, 4);
					line(8, 4, 8, 9.5f, 1.6f); line(8, 9.5f, 16, 9.5f, 1.6f); line(16, 9.5f, 16, 4, 1.6f);
					line(7.5f, 20, 7.5f, 14, 1.6f); line(7.5f, 14, 16.5f, 14, 1.6f); line(16.5f, 14, 16.5f, 20, 1.6f);
					break;
				case ICON_POWER:
					mesh.strokeArc(P(12, 13), 7.8f * u, 310.0f, 280.0f, 2.2f * u, color);
					line(12, 3.5f, 12, 12.5f);
					break;
				case ICON_INFO:
					ring(12, 12, 9);
					dot(12, 7.6f, 1.3f);
					line(12, 11, 12, 17);
					break;
				case ICON_PAD:
					// a gamepad: the body (as wide as the keyboard and monitor glyphs), a D-pad at the left, two buttons at the right,
					// all well inside the outline
					box(0.5f, 4.5f, 23, 15, 7.0f);
					line(7.5f, 10.5f, 7.5f, 14.5f, 1.8f);
					line(5.5f, 12.5f, 9.5f, 12.5f, 1.8f);
					dot(16.5f, 11.0f, 1.2f);
					dot(19.0f, 13.8f, 1.2f);
					break;
				case ICON_PLAY:
				{
					std::vector<GuiPoint> tri;
					tri.push_back(P(6.5f, 3.5f)); tri.push_back(P(20, 12)); tri.push_back(P(6.5f, 20.5f));
					mesh.addPolygon(tri, fill);
					break;
				}
				default:
					break;
			}
		}

		int kind;
		PixelColor color;
		GuiMesh mesh;
		float meshFeather;
		std::vector<MeshVertex> scratch;
};

// The tiles under the controllers: as short as they can be, to leave the controllers their room
const int TILE_W = 128;
const int TILE_H = 48;
// The controllers card: its inner width, a player's slot, and the row of shared devices under the players
const int CONTROLLERS_INNER_W = 244;
const int PLAYER_SLOT_W = 58;
const int PLAYER_SLOT_H = 80;
const int PLAYER_SLOT_PAD = 4;	// air between a slot's highlight and the icon and label inside it
const int DEVICE_ROW_H = 28;
const int DEVICE_ROW_H_CHARGE = 36;	// the row when a GamePad is there: its battery goes under it, as a player's does

const PixelColor Dark() { return MenuPaletteCurrent().text; }
const PixelColor Light() { return MenuChromeText(); }

// the device icons, by PadType
const unsigned char * ControllerIcon(PadType t) { return Controls_PadIcon(t); }

} // namespace

// ---- building blocks

// A pill for the bars: the bar buttons' look, an icon and a word. The icon and the word go white when it is highlighted
// (the face goes dark), as GuiTextButton does for its label.
GuiCard& HomeView::pill(const char * text, int icon, int w, int h, int font, UiCallback onClick, UiCallback onFocus)
{
	GuiCard& c = ui.card().size(w, h).style(MenuBarButtonStyle()).corners(GuiCorners(h / 2.0f)).pad(0, 0).onClick(onClick).onFocus(onFocus);
	const int iconSize = font + 2;
	GuiLayout& row = ui.row().gap(6).align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE).size(w, h);
	HomeIcon * ic = nullptr;
	if(icon != ICON_NONE)
	{
		ic = &ui.make<HomeIcon>(icon, iconSize, Dark());
		row.add(*ic);
	}
	GuiLabel& label = ui.label(text).fontSize(font).textColor(Dark());
	row.add(label);
	c.add(row, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));
	int last = -1;
	ui.bind([&c, &label, ic, last]() mutable {
		const int lit = c.isSelected() ? 1 : 0;
		if(lit == last)
			return;
		last = lit;
		const PixelColor col = lit ? Light() : Dark();
		label.textColor(col);
		if(ic)
			ic->setColor(col);
	});
	return c;
}

// A square-ish tile: an icon, a title and a line under it
GuiCard& HomeView::tile(const char * title, const char * note, int icon, Action action, const char * hintText)
{
	const std::string h = hintText;
	GuiCard& c = ui.card().size(TILE_W, TILE_H).style(MenuTileStyle()).corners(GuiCorners(12.0f)).pad(8, 6)
		.onClick([this, action]{ pending = action; actionPlayer = -1; })
		.onFocus([this, h]{ hint = h; });
	GuiLayout& row = ui.row().gap(6).align(LAYOUT_ALIGN::CENTRE).size(TILE_W - 16, TILE_H - 12);
	row.add(ui.make<HomeIcon>(icon, 24, Dark()));
	GuiLayout& text = ui.column().gap(4).align(LAYOUT_ALIGN::START);
	text.add(ui.label(title).fontSize(UI_FONT_HEADING - 4));
	text.add(ui.label(note).fontSize(12).muted());
	row.add(text);
	c.add(row, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));
	return c;
}

// - value +
GuiLayout& HomeView::stepper(std::function<std::string()> value, std::function<void(bool)> step, const char * hintText)
{
	const std::string h = hintText;
	GuiTextButton& less = ui.button("-").size(30, 30).fontSize(22).style(MenuStepperStyle())
		.onClick([step]{ step(false); }).onFocus([this, h]{ hint = h; });
	GuiTextButton& more = ui.button("+").size(30, 30).fontSize(22).style(MenuStepperStyle())
		.onClick([step]{ step(true); }).onFocus([this, h]{ hint = h; });
	// the label does not reserve more room when its text grows, so it sits in a box of its own
	GuiLayout& valueBox = ui.row().size(50, 30).align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE)
		.add(ui.label().fontSize(UI_FONT_HEADING).bind(value));
	return ui.row().gap(2).align(LAYOUT_ALIGN::CENTRE).add(less).add(valueBox).add(more);
}

// Two choices side by side; the one in use is outlined
GuiLayout& HomeView::choice(const char * firstText, const char * secondText, std::function<bool()> firstOn, std::function<void(bool)> set, const char * hintText)
{
	const std::string h = hintText;
	GuiTextButton& a = ui.button(firstText).size(78, 28).fontSize(13).style(MenuBarButtonStyle(true))
		.onClick([set]{ set(true); }).onFocus([this, h]{ hint = h; });
	GuiTextButton& b = ui.button(secondText).size(58, 28).fontSize(13).style(MenuBarButtonStyle(false))
		.onClick([set]{ set(false); }).onFocus([this, h]{ hint = h; });
	int last = -1;
	ui.bind([&a, &b, firstOn, last]() mutable {
		const int on = firstOn() ? 1 : 0;
		if(on == last)
			return;
		last = on;
		a.style(MenuBarButtonStyle(on == 1));
		b.style(MenuBarButtonStyle(on == 0));
	});
	return ui.row().gap(4).align(LAYOUT_ALIGN::CENTRE).add(a).add(b);
}

// One line of the quick settings: icon, what it is and a note, the control at the right
void HomeView::quickRow(GuiLayout& column, int icon, const char * labelText, std::function<std::string()> note, GuiLayout& control)
{
	GuiLayout& text = ui.column().gap(4).align(LAYOUT_ALIGN::START);
	text.add(ui.label(labelText).fontSize(UI_FONT_HEADING - 4));
	text.add(ui.label().fontSize(12).muted().bind(note));
	column.add(ui.row().size(256, 68).gap(8).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.make<HomeIcon>(icon, 24, Dark())).add(text).addSpace().add(control));
}

// A battery: a 1px casing, four bars inside it and the terminal, on whole pixels
struct BatteryShapes {
	GuiShape * body = nullptr;
	GuiShape * nub = nullptr;
	GuiShape * seg[4] = { nullptr, nullptr, nullptr, nullptr };
};

// How many of the four bars a charge in percent lights; -1 for no battery to show
static int BatterySegments(int percent)
{
	if(percent < 0)
		return -1;
	return percent * 4 / 100 > 4 ? 4 : percent * 4 / 100;
}

// The four players (device, battery), then under a rule the shared devices: the GamePad (Wii U), keyboard and mouse.
// A player's slot opens that player's setup; the devices are only shown.
GuiLayout& HomeView::playersStrip()
{
	struct Slot {
		GuiImage * icon = nullptr; GuiShape * ring = nullptr; BatteryShapes battery;
		GuiLabel * label = nullptr; GuiCard * card = nullptr; int key = -1;
	};
	const PadType kinds[] = { PadType::WIIMOTE, PadType::WIIMOTE_NUNCHUK, PadType::CLASSIC, PadType::WIIU_PRO, PadType::GAMECUBE, PadType::GAMEPAD };
	GuiImageData * images[7] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	for(size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++)
		images[(int)kinds[k]] = &ui.own<GuiImageData>(ControllerIcon(kinds[k]));

	const MenuPalette& pal = MenuPaletteCurrent();
	auto makeBattery = [this](BatteryShapes& b) -> GuiLayout& {
		b.body = &ui.make<GuiShape>(23, 9);
		b.nub = &ui.make<GuiShape>(2, 3);
		GuiLayout& bars = ui.row().size(23, 9).pad(2, 2).gap(1);
		for(int k = 0; k < 4; k++)
		{
			b.seg[k] = &ui.make<GuiShape>(4, 5);
			bars.add(*b.seg[k]);
		}
		GuiLayout& battery = ui.overlay().size(25, 9);
		battery.add(*b.body, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::CENTRE));
		battery.add(bars, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::CENTRE));
		battery.add(*b.nub, GuiItem().at(LAYOUT_ALIGN::END, LAYOUT_ALIGN::CENTRE));
		return battery;
	};
	auto showBattery = [pal](BatteryShapes& b, int segs) {
		const bool shown = segs >= 0;
		const PixelColor tint = segs <= 1 ? pal.mark : pal.text;
		b.body->setVisible(shown);
		b.nub->setVisible(shown);
		if(shown)
		{
			b.body->setPaint(GuiPaint().stroke(1.0f, tint));
			b.nub->setPaint(GuiPaint().color(tint));
		}
		for(int k = 0; k < 4; k++)
		{
			b.seg[k]->setVisible(shown && k < segs);
			if(shown && k < segs)
				b.seg[k]->setPaint(GuiPaint().color(tint));
		}
	};

	// each player: the device, its battery and P1..P4, in a slot tall enough for the tallest device (the Wiimote's 40)
	GuiLayout& players = ui.row().size(CONTROLLERS_INNER_W, PLAYER_SLOT_H).align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::SPACE_BETWEEN);
	for(int i = 0; i < 4; i++)
	{
		Slot& s = ui.own<Slot>();
		const std::string h = "Set up this player's buttons";
		s.card = &ui.card().size(PLAYER_SLOT_W, PLAYER_SLOT_H).style(MenuRowHighlightStyle()).corners(GuiCorners(8.0f)).pad(0, 0)
			.onClick([this, i]{ pending = Action::CONTROLLERS; actionPlayer = i; })
			.onFocus([this, h]{ hint = h; });

		s.icon = &ui.make<GuiImage>();
		s.ring = &ui.make<GuiShape>(10, 14);
		s.ring->setShape(SHAPE::ELLIPSE);
		s.ring->setPaint(GuiPaint().stroke(1.5f, pal.outlineStrong));
		GuiLayout& iconBox = ui.overlay().size(PLAYER_SLOT_W, 40);
		iconBox.add(*s.icon, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));
		iconBox.add(*s.ring, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));

		GuiLayout& battery = makeBattery(s.battery);
		s.label = &ui.label(GuiFormat("P%d", i + 1).c_str()).fontSize(13);
		GuiLayout& col = ui.column().gap(3).align(LAYOUT_ALIGN::CENTRE).size(PLAYER_SLOT_W, PLAYER_SLOT_H).pad(0, PLAYER_SLOT_PAD).add(iconBox).add(battery).add(*s.label);
		s.card->add(col, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));
		players.add(*s.card);

		ui.bind([&s, i, images, showBattery]{
			const PlayerInfo p = Controls().player(i);
			const bool present = p.connected;
			const int segs = present ? BatterySegments(p.battery) : -1;
			const int key = (present ? 1 + (int)p.type : 0) | ((segs + 1) << 4);
			if(key == s.key)
				return;
			s.key = key;
			s.icon->setVisible(present);
			s.ring->setVisible(!present);
			if(present)
				s.icon->setImage(images[(int)p.type]);
			s.label->setAlpha(present ? 255 : 110);
			s.card->disabled(!present);
			showBattery(s.battery, segs);
		});
	}

	// the shared devices, dimmed until one is attached: the GamePad with its charge (only where there is one), the
	// keyboard and the mouse
	GuiShape& rule = ui.make<GuiShape>(CONTROLLERS_INNER_W, 1);
	rule.setPaint(GuiPaint().color(pal.outline));
	const bool hasPad = Controls().devices().gamepadPresent;
	GuiLayout& devices = ui.row().size(CONTROLLERS_INNER_W, hasPad ? DEVICE_ROW_H_CHARGE : DEVICE_ROW_H).gap(18).align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE);
	GuiImage * padImage = nullptr;
	BatteryShapes * padBattery = nullptr;
	if(hasPad)
	{
		GuiImageData& padData = ui.own<GuiImageData>(icon_gamepad_png);
		padImage = &ui.make<GuiImage>(&padData);
		padBattery = &ui.own<BatteryShapes>();
		GuiLayout& padCharge = makeBattery(*padBattery);
		devices.add(ui.column().gap(3).align(LAYOUT_ALIGN::CENTRE).add(*padImage).add(padCharge));
	}
	GuiImageData& kbdData = ui.own<GuiImageData>(icon_keyboard_png);
	GuiImageData& mouseData = ui.own<GuiImageData>(icon_mouse_png);
	GuiImage& kbd = ui.make<GuiImage>(&kbdData);
	GuiImage& mouse = ui.make<GuiImage>(&mouseData);
	if(hasPad)
	{
		// the keyboard and mouse stand level with the GamePad, over the room its battery takes
		devices.add(ui.column().align(LAYOUT_ALIGN::CENTRE).add(kbd).addGap(12));
		devices.add(ui.column().align(LAYOUT_ALIGN::CENTRE).add(mouse).addGap(12));
	}
	else
	{
		devices.add(kbd);
		devices.add(mouse);
	}
	int padKey = -2;
	ui.bind([&kbd, &mouse, padImage, padBattery, showBattery, padKey]() mutable {
		const DeviceStatus d = Controls().devices();
		kbd.setAlpha(d.keyboard ? 255 : 90);
		mouse.setAlpha(d.mouse ? 255 : 90);
		if(padImage)
			padImage->setAlpha(d.gamepadPresent ? 255 : 90);
		if(padBattery)
		{
			const int segs = d.gamepadPresent ? BatterySegments(d.gamepadBattery) : -1;
			if(segs != padKey)
			{
				padKey = segs;
				showBattery(*padBattery, segs);
			}
		}
	});

	return ui.column().size(CONTROLLERS_INNER_W, 0).gap(6).align(LAYOUT_ALIGN::CENTRE).add(players).add(rule).add(devices);
}

// ---- the screen

HomeView::HomeView(HomeSettings& s, const char * version) :
	settings(s), pending(Action::NONE), actionPlayer(-1), resumeCard(nullptr), exitCard(nullptr), creditsCard(nullptr), setupsCard(nullptr),
	configureCard(nullptr), keyboardCard(nullptr), settingsCard(nullptr), statusCard(nullptr), hint("Pick something, or Resume to go back to the game")
{
	const int sw = platform->getVideo()->getScreenWidth();
	const int sh = platform->getVideo()->getScreenHeight();
	const int bodyH = sh - MENU_BAR_TOP_H - MENU_BAR_BOTTOM_H - 8;

	// header: the logo, then what is running, and Resume at the right
	GuiImageData& logoData = ui.own<GuiImageData>(logo_png);
	GuiImage& logo = ui.make<GuiImage>(&logoData);
	GuiLayout& versionBox = ui.column().size(34, 48).justifyContent(LAYOUT_JUSTIFY::END)
		.add(ui.label(version).fontSize(14).textColor(MenuChromeTextMuted()));
	GuiShape& rule = ui.make<GuiShape>(1, 38);
	rule.setPaint(GuiPaint().color(MenuRGBA(255, 255, 255, 80)));
	GuiLabel& program = ui.label().fontSize(UI_FONT_HEADING - 2).textColor(MenuChromeText())
		.bind([]{ const std::string p = Controls().runningProgram(); return p.empty() ? std::string("DOS prompt") : p; });
	program.setMaxWidth(150);
	GuiLabel& setupName = ui.label().fontSize(13).textColor(MenuChromeTextMuted())
		.bind([]{ const std::string n = Controls().setupName(); return n.empty() ? std::string("No setup loaded") : "Setup: " + n; });
	setupName.setMaxWidth(150);
	GuiLayout& running = ui.column().gap(4).align(LAYOUT_ALIGN::START).add(program).add(setupName);
	resumeCard = &pill("Resume", ICON_PLAY, 118, 40, 20, [this]{ pending = Action::RESUME; }, [this]{ hint = "Go back to the game"; });
	GuiLayout& header = ui.row().size(sw, MENU_BAR_TOP_H).pad(MENU_PAGE_MARGIN, 0).gap(10).align(LAYOUT_ALIGN::CENTRE)
		.add(logo).add(versionBox).add(rule).add(running).addSpace().add(*resumeCard);

	// body, left: quick settings, one card
	GuiLayout& quick = ui.column().size(256, 0).gap(0);
	HomeSettings * hs = &settings;
	quickRow(quick, ICON_GAUGE, "Speed", [hs]{ return hs->speedNote(); },
		stepper([hs]{ return hs->speed(); }, [hs](bool up){ hs->stepSpeed(up); }, "- slows DOS down, + speeds it up (CPU cycles)"));
	quick.add(Popover_Divider(ui, 256));
	quickRow(quick, ICON_FRAMES, "Frameskip", []{ return std::string("Skips frames"); },
		stepper([hs]{ return hs->frameskip(); }, [hs](bool up){ hs->stepFrameskip(up); }, "More skipped frames runs heavy games faster, but choppier"));
	quick.add(Popover_Divider(ui, 256));
	quickRow(quick, ICON_ASPECT, "Aspect", []{ return std::string("4:3 monitor"); },
		choice("Corrected", "Square", [hs]{ return hs->aspectCorrected(); }, [hs](bool c){ hs->setAspectCorrected(c); }, "Corrected draws the picture 4:3 like a real monitor. Square keeps pixels square"));
	quick.add(Popover_Divider(ui, 256));
	quickRow(quick, ICON_FILTER, "Filter", []{ return std::string("Pixel edges"); },
		choice("Sharp", "Smooth", [hs]{ return hs->filterSharp(); }, [hs](bool sharp){ hs->setFilterSharp(sharp); }, "Sharp keeps pixel edges crisp. Smooth blends them"));
	GuiCard& quickCard = ui.card().selectable(false).style(MenuPageCardStyle()).corners(GuiCorners(14.0f)).pad(10, 6).size(276, bodyH);
	quickCard.add(quick, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));

	// body, right: controllers, then four tiles. The tiles are short, and the controllers take what they leave.
	configureCard = &pill("Configure", ICON_NONE, 76, 26, 13, [this]{ pending = Action::CONTROLLERS; actionPlayer = -1; }, [this]{ hint = "Set up the buttons of each controller"; });
	GuiLayout& controllersHead = ui.row().size(CONTROLLERS_INNER_W, 28).gap(6).align(LAYOUT_ALIGN::CENTRE)
		.add(ui.make<HomeIcon>(ICON_PAD, 24, Dark())).add(ui.label("Controllers").fontSize(UI_FONT_HEADING - 4)).addSpace().add(*configureCard);
	GuiLayout& controllersBody = ui.column().size(CONTROLLERS_INNER_W, 0).gap(6).align(LAYOUT_ALIGN::CENTRE).add(controllersHead).add(playersStrip());
	const int controllersH = bodyH - 2 * TILE_H - 2 * 8;
	GuiCard& controllersCard = ui.card().selectable(false).style(MenuPageCardStyle()).corners(GuiCorners(14.0f)).pad(10, 8).size(264, controllersH);
	controllersCard.add(controllersBody, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));

	keyboardCard = &tile("Keyboard", "On screen", ICON_KEYBOARD, Action::KEYBOARD, "Type into DOS with an on-screen keyboard");
	settingsCard = &tile("Settings", "All options", ICON_GEAR, Action::SETTINGS, "Video, audio, DOS, drives and everything else");
	statusCard = &tile("Status", "Live info", ICON_PULSE, Action::STATUS, "What DOSBox is set to and doing right now");
	setupsCard = &tile("Setups", "Save, load", ICON_DISK, Action::SETUPS, "Save or load the buttons and settings for this game");
	GuiLayout& right = ui.column().size(264, bodyH).gap(8).align(LAYOUT_ALIGN::CENTRE).add(controllersCard)
		.add(ui.row().gap(8).add(*keyboardCard).add(*settingsCard))
		.add(ui.row().gap(8).add(*statusCard).add(*setupsCard));

	GuiLayout& body = ui.row().size(sw, sh).pad(MENU_PAGE_MARGIN, MENU_BAR_TOP_H + 4, MENU_PAGE_MARGIN, MENU_BAR_BOTTOM_H + 4).gap(12)
		.align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE).add(quickCard).add(right);

	// footer: Exit, what the highlighted thing does, Credits
	exitCard = &pill("Exit", ICON_POWER, 76, 34, 15, [this]{ pending = Action::EXIT; }, [this]{ hint = "Exit DOSBox"; });
	creditsCard = &pill("Credits", ICON_INFO, 96, 34, 15, [this]{ pending = Action::CREDITS; }, [this]{ hint = "Who made this"; });
	GuiLabel& hintLabel = ui.label().fontSize(MENU_PAGE_HINT_FONT).textColor(MenuChromeTextMuted())
		.bind([this]{ return status.empty() ? hint : status; });
	hintLabel.setMaxWidth(sw - 2 * MENU_PAGE_MARGIN - 132 - 96 - 36);
	hintLabel.setScroll(SCROLL::HORIZONTAL);
	GuiLayout& footer = ui.row().size(sw, MENU_BAR_BOTTOM_H).pad(MENU_PAGE_MARGIN, 0).gap(12).align(LAYOUT_ALIGN::CENTRE)
		.add(*exitCard).add(hintLabel).addSpace().add(*creditsCard);

	add(header, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
	add(body, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
	add(footer, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::END));
	setFixedSize(sw, sh);
	setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	setPosition(0, 0);
	ui.attachTo(*this);
}

HomeView::Action HomeView::takeAction()
{
	const Action a = pending;
	pending = Action::NONE;
	return a;
}

PopoverAnchor HomeView::setupsAnchor()
{
	return PopoverAnchor(setupsCard->getLeft(), setupsCard->getTop(), setupsCard->getWidth(), setupsCard->getHeight());
}

void HomeView::select(Action which)
{
	GuiCard * c = nullptr;
	switch(which)
	{
		case Action::RESUME: c = resumeCard; break;
		case Action::EXIT: c = exitCard; break;
		case Action::KEYBOARD: c = keyboardCard; break;
		case Action::SETTINGS: c = settingsCard; break;
		case Action::STATUS: c = statusCard; break;
		case Action::SETUPS: c = setupsCard; break;
		case Action::CONTROLLERS: c = configureCard; break;
		case Action::CREDITS: c = creditsCard; break;
		default: break;
	}
	if(c)
		c->select();
}
