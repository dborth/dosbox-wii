/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlsui.cpp
 ***************************************************************************/
#include "controlsui.h"
#include <algorithm>
#include "menu.h"
#include "menupage.h"
#include "menustyle.h"
#include "menuart.h"
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

// ---- helpers

namespace {

// The setup screen's fixed geometry (design pixels). The card holds the same block whatever controller is drawn:
// kContentH is what the GameCube drawing, the tallest with groups, needs; kGroupsH is the three columns under the
// drawing; kPlayerNameW holds "Player 1" and "Player 4" alike, so the switcher's arrows never move.
const int kContentH = 262;
const int kGroupsH = 124;
const int kPlayerNameW = 74;

// How many controllers are connected
int ConnectedCount()
{
	int n = 0;
	for(int i = 0; i < Controls().playerCount(); i++)
		n += Controls().player(i).connected ? 1 : 0;
	return n;
}

// The next connected player after `from`, going `dir` (+1 or -1) and wrapping; `from` when there is no other
int NextPlayer(int from, int dir)
{
	const int count = Controls().playerCount();
	for(int step = 1; step < count; step++)
	{
		const int n = ((from + dir * step) % count + count) % count;
		if(Controls().player(n).connected)
			return n;
	}
	return from;
}

// The player to show for a slot: itself when a controller is in it, else the first connected one (the slot itself when none is)
int ResolvePlayer(int slot)
{
	if(slot >= 0 && slot < Controls().playerCount() && Controls().player(slot).connected)
		return slot;
	for(int i = 0; i < Controls().playerCount(); i++)
		if(Controls().player(i).connected)
			return i;
	return slot < 0 ? 0 : slot;
}

// A name as it fits on a button: long ones are cut
std::string ClipName(const std::string& name, size_t most)
{
	return name.size() <= most ? name : name.substr(0, most - 2) + "..";
}

// ---- device icons for the rail

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
		default: return icon_wiiu_pro_png;
	}
}


} // namespace

// ---- controller setup

int ControllerConfigView::highlighted() const
{
	if(!view)
		return 0;
	const std::vector<int>& h = view->getHighlight();
	return h.empty() ? 0 : h[0];
}

void ControllerConfigView::selectHeader(Pick which)
{
	GuiTextButton * b = which == Pick::EMULATED ? emulatedButton : (which == Pick::PRESET ? presetButton : (which == Pick::SETUP ? setupButton : nullptr));
	if(b)
		b->setState(STATE::SELECTED);
}

PopoverAnchor ControllerConfigView::anchorFor(Pick which)
{
	GuiTextButton * b = which == Pick::EMULATED ? emulatedButton : (which == Pick::PRESET ? presetButton : (which == Pick::SETUP ? setupButton : nullptr));
	// A preset previews on the rows and the drawing behind it, so its popover keeps to the left (the two left-hand
	// rows and the sticks), leaving the right-hand rows, the D-pad and the face buttons in view
	return b ? PopoverAnchor(b->getLeft(), b->getTop(), b->getWidth(), b->getHeight(), which == Pick::PRESET ? PopoverAnchor::LEFT_MARGIN : PopoverAnchor::AUTO) : PopoverAnchor();
}

PopoverAnchor ControllerConfigView::anchorForRow(int binding)
{
	for(size_t i = 0; i < rowsById.size(); i++)
		if(rowsById[i].first == binding)
		{
			GuiCard * r = rowsById[i].second;
			// the popover is wide: it goes to the other side of the screen, so the row being changed (and the line to its control) stay in view
			const bool leftHalf = r->getLeft() + r->getWidth() / 2 < platform->getVideo()->getScreenWidth() / 2;
			return PopoverAnchor(r->getLeft(), r->getTop(), r->getWidth(), r->getHeight(), leftHalf ? PopoverAnchor::RIGHT_MARGIN : PopoverAnchor::LEFT_MARGIN);
		}
	return PopoverAnchor();
}

bool ControllerConfigView::takeRequest(ProfileRow& row)
{
	if(!hasRequest)
		return false;
	row = request;
	hasRequest = false;
	return true;
}

bool ControllerConfigView::outOfDate() const
{
	const PlayerInfo now = Controls().player(playerSlot);
	// a different controller, a Wiimote turned on its side (another drawing), or a different number of players (the switcher)
	// (a device joining or leaving the channel changes the rail, and a flipped-to device leaving the drawing)
	return (now.connected ? now.type : PadType::NONE) != padType || (now.connected && now.sideways) != sideways || ConnectedCount() != shownPlayers ||
		(now.connected ? now.devices : std::vector<PadType>()) != deviceList;
}

// A list row: the control's icon and what it does. The control's name is on the hint line while the row is
// highlighted, along with a line drawn to the control on the picture. Clicking asks to change it.
GuiCard& ControllerConfigView::bindingRow(const ProfileRow& r, Side side, int width)
{
	const int pl = playerSlot;
	const int id = r.binding;
	const int stick = r.stick;
	const int padX = 10;	// room between the highlight box and what is in it
	const char * fixed = r.fixed;
	const char * name = r.name;
	const bool info = fixed || stick; // shown, not changed

	// what the row says: a button's target, or for a stick what the DOS side does with it
	// (while a preset popover is open, a button shows what that preset would give it)
	const std::function<std::string()> text = [this, pl, id, fixed, stick]() -> std::string {
		if(stick)
			return Controls().joystickAxes(pl) >= (stick == 1 ? 2 : 4) ? "Joystick axes" : "Not used";
		if(fixed)
			return std::string(fixed);
		return previewPreset >= 0 ? Controls().describeTarget(pl, Controls().presetBinding(previewPreset, id)) : Controls().describe(pl, id);
	};

	GuiLabel& target = ui.label().fontSize(UI_FONT_SMALL + 1);
	target.setMaxWidth(width - 2 * padX - 19 - 8); // long choices are cut rather than spilling out of the row
	target.bind(text);

	GuiLayout& content = ui.row().size(width - 2 * padX, 0).gap(8).align(LAYOUT_ALIGN::CENTRE);
	GuiGlyph& icon = ui.glyph(r.glyph, r.letter);
	if(side == Side::LEFT)
		content.addSpace().add(target).add(icon);
	else
		content.add(icon).add(target);

	GuiCard& row = ui.card();
	row.size(width, 0).style(MenuRowHighlightStyle()).pad(padX, 4);
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
		// the hint holds about 30 characters beside the footer buttons (it scrolls when longer), so just the facts
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
	else
	{
		// Hovering shows what it is, but clicking does nothing. It still needs a handler: a card with none is never
		// told its click was seen, so it stays in the clicked state and ignores the pointer from then on.
		row.onClick([]{});
	}
	row.add(content);
	rowCards.push_back(&row);
	if(!info)
		rowsById.push_back(std::make_pair(id, &row));
	return row;
}

ControllerConfigView::ControllerConfigView(int player, GuiSound * soundOver, PadType show) :
	playerSlot(player), padType(PadType::NONE), shownIndex(0), flipRequest(PadType::NONE), sideways(false), view(nullptr), status("Move over a control to see what it does"), hasRequest(false), pickRequest(Pick::NONE), switchRequest(0), shownPlayers(ConnectedCount()), backBtn(nullptr), emulatedButton(nullptr), presetButton(nullptr), setupButton(nullptr), previewPreset(-1)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	const PlayerInfo info = Controls().player(player);
	padType = info.connected ? info.type : PadType::NONE;
	if(info.connected)
		deviceList = info.devices;
	for(size_t i = 0; i < deviceList.size(); i++)
		if(deviceList[i] == show)
			shownIndex = i;
	sideways = info.connected && info.sideways;
	const bool empty = padType == PadType::NONE;
	const bool several = shownPlayers >= 2;
	if(empty)
		status = "Press a button on a controller to connect it";

	// Header: who (this player: the switcher, only with two or more controllers; else just the page name), then what
	// the buttons start from (Preset: this player) and what DOS sees (In DOS: the whole game). The footer has Setup
	// (whole game) and Back. Left of the middle is about the player, right of it about the game.
	MenuPageFrame f = MenuPage_Build(ui, *this, "Controls", [this]{ return status; }, 250);
	if(!empty)
	{
		// "Player N" with, under it, what the DOS side does with this player (the caption belongs to the name above it)
		f.title->setVisible(false);
		const int slot = player;
		GuiLabel& who = ui.label(GuiFormat("Player %d", player + 1).c_str()).fontSize(UI_FONT_HEADING).textColor(MenuChromeText());
		GuiLabel& caption = ui.label(info.role.c_str()).fontSize(UI_FONT_SMALL).textColor(MenuChromeTextMuted())
			.bind([slot]{ return Controls().player(slot).role; });
		caption.setMaxWidth(110);
		// a fixed width, so the arrows stay put whatever the name and caption say ("Player 1" is narrower than "Player 4")
		GuiLayout& nameBlock = ui.column().gap(4).size(kPlayerNameW, 0).align(LAYOUT_ALIGN::START).add(who).add(caption);
		if(several)
		{
			// a small page-turning arrow, centred in its button (dark on the white face, white once it is highlighted)
			const auto arrowIcon = [this](ARROW dir) -> GuiArrow& {
				GuiArrow& a = ui.own<GuiArrow>(dir, 9, 14, MenuBarArrowRest(), MenuBarArrowLit());
				a.setAlignment(ALIGN_H::CENTRE, ALIGN_V::MIDDLE);
				return a;
			};
			// drawn arrows: the font has no clean < and >
			GuiTextButton& prev = ui.button("").size(34, 34).style(MenuStepperStyle())
				.onClick([this]{ switchRequest = -1; })
				.onFocus([this]{ status = "A shows the previous controller"; });
			prev.setIcon(&arrowIcon(ARROW::LEFT));
			GuiTextButton& next = ui.button("").size(34, 34).style(MenuStepperStyle())
				.onClick([this]{ switchRequest = 1; })
				.onFocus([this]{ status = "A shows the next controller"; });
			next.setIcon(&arrowIcon(ARROW::RIGHT));
			// room between the buttons and the name, so a highlighted one never touches the text
			f.headerLeft->gap(14).add(prev).add(nameBlock).add(next);
		}
		else
			f.headerLeft->add(nameBlock);
	}

	const int pl = playerSlot;
	GuiTextButton& emulatedBtn = ui.button("").size(174, 32).fontSize(UI_FONT_SMALL).style(MenuBarButtonStyle())
		.bind([]{ ControlsModel& m = Controls(); return GuiFormat("In DOS: %s", m.emulatedModes()[(size_t)m.currentEmulated()].name.c_str()); })
		.onClick([this]{ pickRequest = Pick::EMULATED; })
		.onFocus([this]{ status = "A chooses what DOS sees: a joystick, two, or none (the whole game)"; });
	if(!empty)
	{
		GuiTextButton& presetBtn = ui.button("").size(178, 32).fontSize(UI_FONT_SMALL).style(MenuBarButtonStyle())
			.bind([pl]{ ControlsModel& m = Controls(); return GuiFormat("Preset: %s%s", m.presets()[(size_t)m.currentPreset(pl)].name.c_str(), m.modified(pl) ? "*" : ""); })
			.onClick([this]{ pickRequest = Pick::PRESET; })
			.onFocus([this]{ status = "A chooses a preset for this player's buttons"; });
		presetButton = &presetBtn;
		f.headerRight->add(presetBtn);
	}
	emulatedButton = &emulatedBtn;
	f.headerRight->add(emulatedBtn);

	if(empty)
	{
		// nothing to set up yet: say so, and what to do
		GuiLayout& message = ui.column().size(MENU_PAGE_CONTENT_W, 220).gap(6).align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE)
			.add(ui.label("No controller connected").fontSize(UI_FONT_HEADING))
			.add(ui.label("Press a button on a Wiimote, Classic, GameCube or Wii U controller.").fontSize(UI_FONT_SMALL).muted())
			.add(ui.label("The keyboard and mouse keep working.").fontSize(UI_FONT_SMALL).muted());
		f.content->add(message);
	}
	else
	{
		const ControllerProfile profile = Controllers_Profile(info, (int)shownIndex);
		view = &ui.controllerView(*profile.layout, profile.scale);
		// no text on the drawing: the buttons are too small to carry it legibly, and the rows beside it name each control
		view->setLabelFilter([](int){ return false; });
		// lit while held, so pressing a button on the controller shows which control it is
		view->setLit([pl](int id){ return id > 0 && id < GUI_CTL_CAPTURE && (Controls().held(pl) & (uint32_t)id) != 0; });
		const int sideW = 168;
		// Two or more devices on the channel (a Wiimote and a GameCube pad): a rail of their icons down the left, the
		// one drawn lit, A on another draws that one. It takes room from the left rows only where the drawing leaves none.
		const bool rail = deviceList.size() > 1;
		const int railW = 56;
		int leftW = sideW;
		if(rail)
		{
			const int drawW = (int)(profile.layout->width * profile.scale);
			const int over = railW + 4 + 2 * sideW + 8 + drawW - MENU_PAGE_CONTENT_W;
			leftW = std::max(110, sideW - std::max(0, over));
		}
		const int groupSectionW = (MENU_PAGE_CONTENT_W - 12) / 3;

		GuiLayout& left = ui.column().gap(1).align(LAYOUT_ALIGN::END);
		for(size_t i = 0; i < profile.left.size(); i++)
			left.add(bindingRow(profile.left[i], Side::LEFT, leftW));
		GuiLayout * railColumn = nullptr;
		if(rail)
		{
			railColumn = &ui.column().gap(6);
			for(size_t i = 0; i < deviceList.size(); i++)
			{
				const PadType type = deviceList[i];
				const bool active = i == shownIndex;
				GuiImage& img = iconImage(ui, padIcon(type));
				if(!active)
					img.setAlpha(150);
				GuiCard& tile = ui.card();
				tile.size(railW, railW).style(MenuRailTileStyle(active)).corners(GuiCorners(10.0f)).pad(8, 8);
				tile.onFocus([this, type, active]{ status = GuiFormat(active ? "Showing %s" : "A shows the %s", PadType_Name(type)); });
				if(!active)
					tile.onClick([this, type]{ flipRequest = type; });
				tile.add(ui.overlay().size(railW - 16, railW - 16).add(img));
				railColumn->add(tile);
			}
		}
		GuiLayout& right = ui.column().gap(1);
		for(size_t i = 0; i < profile.right.size(); i++)
			right.add(bindingRow(profile.right[i], Side::RIGHT, sideW));

		// The picture and the groups sit straight on the page card: no tinted boxes behind them. A heading is just
		// small muted capitals, lined up with the text of the rows under it.
		GuiLayout& groups = ui.row().gap(6);
		for(size_t g = 0; g < profile.groups.size(); g++)
		{
			GuiLayout& column = ui.column().gap(1).size(groupSectionW, 0);
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

		// The card is always the same height, whatever is drawn in it, and the rail always sits in the same place: the
		// card's content is a fixed block, the rail is pinned to its top left, and the drawing with its rows is centred
		// in what is left. A drawing with groups under it gets the stage above them; one without (an upright Wiimote)
		// gets all of it.
		const bool hasGroups = !profile.groups.empty();
		const int stageH = hasGroups ? kContentH - kGroupsH - 6 : kContentH;
		const int railSlotW = rail ? railW + 4 : 0;
		f.content->gap(4).size(MENU_PAGE_CONTENT_W, kContentH);
		GuiLayout& top = ui.row().size(MENU_PAGE_CONTENT_W, stageH).gap(4).align(LAYOUT_ALIGN::CENTRE);
		if(railColumn)
			top.add(*railColumn, GuiItem().alignStart());
		GuiLayout& stage = ui.row().size(MENU_PAGE_CONTENT_W - railSlotW, stageH).gap(4).align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE);
		top.add(stage.add(left).add(*view).add(right));
		f.content->add(top);
		if(hasGroups)
			f.content->add(groups.size(MENU_PAGE_CONTENT_W, kGroupsH));
	}

	// Footer: Setup is the whole game (every player's buttons and what DOS sees), so it sits under In DOS; Back is last.
	// The pill says which setup is loaded, and marks changes not saved yet.
	GuiTextButton& setupBtn = MenuPage_Button(ui, "", 190).fontSize(UI_FONT_BODY)
		.bind([]{
			ControlsModel& m = Controls();
			return GuiFormat("Setup: %s%s", m.setupName().empty() ? "none" : ClipName(m.setupName(), 14).c_str(), m.setupModified() ? "*" : "");
		})
		.onClick([this]{ pickRequest = Pick::SETUP; })
		.onFocus([this]{ status = "A loads or saves a setup: every player's buttons and what DOS sees"; });
	setupButton = &setupBtn;
	GuiTextButton& back = MenuPage_Button(ui, "Back", 76);
	back.setTrigger(1, &cancel);
	backBtn = &back;
	f.footerRight->add(setupBtn).add(back);

	ui.attachTo(*this);
	// When the page takes focus its window steps the selection down from whatever is selected, or goes to the first
	// thing in reading order when nothing is, which would be a header button. Starting from the In DOS button, which
	// sits over the right-hand column, steps onto a control.
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
		const char * shownName = full.c_str();
		if(kind == TARGET::KEY)
		{
			static const char * const caps[][2] = { { "Backspace", "Bksp" }, { "Minus", "-" }, { "Equals", "=" }, { "Comma", "," },
				{ "Period", "." }, { "Slash", "/" }, { "Semicolon", ";" }, { "Quote", "'" }, { "Backslash", "\\" }, { "Grave", "`" } };
			for(size_t c = 0; c < sizeof(caps) / sizeof(caps[0]); c++)
				if(full == caps[c][0])
					shownName = caps[c][1];
		}
		const bool now = current.kind == kind && current.code == code;
		GuiTextButton& b = ui.button(shownName).size(w, h).fontSize(fontPx).style(MenuBarButtonStyle(now));
		b.onClick([this, kind, code]{
			picked = BindingTarget(kind, code);
			done = true;
		});
		if(kind == TARGET::KEY)
			b.onFocus([this, full]{ if(!listen) hintText = GuiFormat("Key: %s", full.c_str()); });
		grid.add(b);
	}
}

void TargetChooserView::optionRows(GuiLayout& list, TARGET kind, const std::vector<int>& codes, int w, int h)
{
	const std::vector<std::string>& names = Catalog_Names(kind);
	for(size_t i = 0; i < codes.size(); i++)
	{
		const int code = codes[i];
		std::string label = names[(size_t)code];
		if(!label.empty())
			label[0] = (char)toupper((unsigned char)label[0]);
		GuiCard& row = Popover_Row(ui, w, h, label, current.kind == kind && current.code == code);
		row.onClick([this, kind, code]{
			picked = BindingTarget(kind, code);
			done = true;
		});
		list.add(row);
	}
}

void TargetChooserView::targetRows(GuiLayout& list, const std::vector<BindingTarget>& targets, int w, int h)
{
	for(size_t i = 0; i < targets.size(); i++)
	{
		const BindingTarget t = targets[i];
		GuiCard& row = Popover_Row(ui, w, h, t.describe(), t == current);
		row.onClick([this, t]{
			picked = t;
			done = true;
		});
		list.add(row);
	}
}

// The key picker: "Press a key" for a USB keyboard, then the keys most DOS games want, and the rest in groups behind
// tabs, laid out like a keyboard (Letters in QWERTY rows). Only keys the mapper can bind for this player are drawn.
void TargetChooserView::keyPane(GuiLayout& pane, int player, int w)
{
	const std::vector<int> allowed = Controls().choices(player, TARGET::KEY);
	const std::vector<std::string>& names = Catalog_Names(TARGET::KEY);
	static const char * const caps[][2] = { { "Backspace", "Bksp" }, { "Minus", "-" }, { "Equals", "=" }, { "Comma", "," },
		{ "Period", "." }, { "Slash", "/" }, { "Semicolon", ";" }, { "Quote", "'" }, { "Backslash", "\\" }, { "Grave", "`" } };

	// a key by name, as a cap `kw` wide; nothing when the build or player cannot bind it
	auto cap = [&](GuiLayout& row, const char * name, int kw, int kh) -> bool {
		const int code = Catalog_Find(TARGET::KEY, name);
		if(code < 0 || std::find(allowed.begin(), allowed.end(), code) == allowed.end())
			return false;
		const std::string full = names[(size_t)code];
		const char * label = full.c_str();
		for(size_t c = 0; c < sizeof(caps) / sizeof(caps[0]); c++)
			if(full == caps[c][0])
				label = caps[c][1];
		const bool inUse = current.kind == TARGET::KEY && current.code == code;
		GuiTextButton& b = ui.button(label).size(kw, kh).fontSize(UI_FONT_SMALL).style(MenuKeycapStyle(inUse)).rounded(6.0f).litTextColor(MenuPaletteCurrent().text);
		b.onClick([this, code]{
			picked = BindingTarget(TARGET::KEY, code);
			done = true;
		});
		b.onFocus([this, full]{ if(!listen) hintText = GuiFormat("Key: %s", full.c_str()); });
		row.add(b);
		return true;
	};

	// each tab: its rows of keys (a row is a list of names, with a left offset for the stagger)
	struct Row { std::vector<const char *> keys; int indent; };
	struct Tab { const char * label; int width; int capW; int capH; std::vector<Row> rows; };
	const std::vector<Tab> tabs = {
		{ "Common", 54, 58, 28, { { { "Enter", "Esc", "Space", "Tab" }, 0 }, { { "Backspace", "Ctrl", "Alt", "Shift" }, 0 }, { { "Up", "Down", "Left", "Right" }, 0 } } },
		{ "A-Z", 30, 20, 26, { { { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" }, 0 }, { { "A", "S", "D", "F", "G", "H", "J", "K", "L" }, 10 }, { { "Z", "X", "C", "V", "B", "N", "M" }, 21 } } },
		{ "0-9", 30, 20, 26, { { { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" }, 0 } } },
		{ "F-keys", 42, 36, 26, { { { "F1", "F2", "F3", "F4", "F5", "F6" }, 0 }, { { "F7", "F8", "F9", "F10", "F11", "F12" }, 0 } } },
		{ "Edit", 32, 72, 28, { { { "PgUp", "PgDn", "Home" }, 0 }, { { "End", "Ins", "Del" }, 0 } } },
		{ "Symbols", 46, 36, 26, { { { "Minus", "Equals", "Comma", "Period", "Slash", "Semicolon" }, 0 }, { { "Quote", "[", "]", "Backslash", "Grave" }, 0 } } },
	};

	// open on the tab with the key in use
	keyTab = 0;
	if(current.kind == TARGET::KEY && current.code >= 0)
		for(size_t t = 0; t < tabs.size(); t++)
			for(size_t r = 0; r < tabs[t].rows.size(); r++)
				for(size_t k = 0; k < tabs[t].rows[r].keys.size(); k++)
					if(Catalog_Find(TARGET::KEY, tabs[t].rows[r].keys[k]) == current.code)
						keyTab = (int)t;

	GuiTextButton& press = ui.button("").size(w, 24).fontSize(UI_FONT_SMALL).style(MenuKeycapStyle()).rounded(8.0f).litTextColor(MenuPaletteCurrent().text)
		.bind([this]{ return listen && listenKind == TARGET::KEY ? std::string("Press a key...") : std::string("Press a key on the keyboard"); })
		.visibleWhen([]{ return Controls().devices().keyboard; })
		.onClick([this]{ startListening(TARGET::KEY); });

	GuiLayout& tabRow = ui.row().gap(2).size(w, 22);
	std::vector<GuiTextButton*>& tabButtons = ui.own<std::vector<GuiTextButton*> >();
	GuiLayout& stack = ui.overlay().size(w, 92);
	for(size_t t = 0; t < tabs.size(); t++)
	{
		const int idx = (int)t;
		GuiTextButton& tb = ui.button(tabs[t].label).size(tabs[t].width, 22).fontSize(UI_FONT_SMALL - 1).style(MenuRowHighlightStyle()).rounded(6.0f).litTextColor(MenuPaletteCurrent().text);
		tb.onFocus([this, idx]{ keyTab = idx; });
		tb.onClick([this, idx]{ keyTab = idx; });
		tabButtons.push_back(&tb);
		tabRow.add(tb);

		GuiLayout& body = ui.column().gap(2);
		for(size_t r = 0; r < tabs[t].rows.size(); r++)
		{
			GuiLayout& row = ui.row().gap(2);
			if(tabs[t].rows[r].indent > 0)
				row.addGap((float)tabs[t].rows[r].indent);
			for(size_t k = 0; k < tabs[t].rows[r].keys.size(); k++)
				cap(row, tabs[t].rows[r].keys[k], tabs[t].capW, tabs[t].capH);
			body.add(row);
		}
		stack.add(body, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
		ui.visibleWhen(body, [this, idx]{ return keyTab == idx; });
	}
	// the tab that is showing keeps a fill
	int styled = -1;
	ui.bind([&tabButtons, styled, this]() mutable {
		if(styled == keyTab)
			return;
		styled = keyTab;
		for(size_t k = 0; k < tabButtons.size(); k++)
			tabButtons[k]->style((int)k == keyTab ? MenuPopoverShownStyle() : MenuRowHighlightStyle());
	});

	pane.add(press).add(tabRow).add(stack);
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
	hintText = "Move over a kind to see its choices";
}

void TargetChooserView::showCategory(int index)
{
	if(index < 0 || index >= kinds || index == shown)
		return;
	stopListening();
	shown = index;
}

void TargetChooserView::selectCategory()
{
	if(kindCards.empty())
		return;
	for(size_t i = 0; i < kindCards.size(); i++)
		kindCards[i]->deselect();
	kindCards[(size_t)shown < kindCards.size() ? (size_t)shown : 0]->select();
}

TargetChooserView::TargetChooserView(const char * controlName, int player, int control, const BindingTarget& now, const BindingTarget& defaultTarget,
	GuiSound * soundOver, const PopoverAnchor& anchor) :
	current(now), picked(), shown(0), done(false), backBtn(nullptr), hintText("Move over a kind to see its choices"),
	listen(false), listenKind(TARGET::KEY), kinds(0), keyTab(0), panelWidth(380), panelHeight(0)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	const int sw = platform->getVideo()->getScreenWidth();
	const int sh = platform->getVideo()->getScreenHeight();
	setFixedSize(sw, sh);
	setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	setPosition(0, 0);

	ControlsModel& m = Controls();
	const std::vector<BindingTarget> suggested = m.suggestions(player, control);

	const int pad = 10, inner = panelWidth - 2 * pad;
	const int leftW = 104, gap = 8, rightW = inner - leftW - gap;
	const int rowH = 28, rowGap = 2, kindH = 30;

	// the kinds down the left (a joystick only when the player has one), each with the choices it shows
	struct Panel { const char * name; GuiLayout * layout; TARGET kind; int height; };
	std::vector<Panel> panels;

	if(!suggested.empty())
	{
		GuiLayout& g = ui.column().gap(rowGap);
		targetRows(g, suggested, rightW, rowH);
		panels.push_back({ "Suggested", &g, TARGET::NONE, (int)suggested.size() * (rowH + rowGap) });
	}

	// Key: a row that waits for a real key press, then the common keys and groups of the rest behind tabs
	{
		GuiLayout& pane = ui.column().gap(4);
		keyPane(pane, player, rightW);
		panels.push_back({ "Key", &pane, TARGET::KEY, 24 + 4 + 22 + 4 + 92 });
	}

	if(m.joystickButtons(player) > 0)
	{
		GuiLayout& g = ui.column().gap(rowGap);
		const std::vector<int> codes = m.choices(player, TARGET::JOYSTICK);
		optionRows(g, TARGET::JOYSTICK, codes, rightW, rowH);
		panels.push_back({ "Joystick", &g, TARGET::JOYSTICK, (int)codes.size() * (rowH + rowGap) });
	}

	{
		GuiLayout& g = ui.column().gap(rowGap);
		const std::vector<int> codes = m.choices(player, TARGET::MOUSE);
		GuiTextButton& click = ui.button("").size(rightW, 24).fontSize(UI_FONT_SMALL).style(MenuBarButtonStyle())
			.bind([this]{ return listen && listenKind == TARGET::MOUSE ? std::string("Click a button...") : std::string("Click a mouse button"); })
			.visibleWhen([]{ return Controls().devices().mouse; })
			.onClick([this]{ startListening(TARGET::MOUSE); });
		g.add(click);
		optionRows(g, TARGET::MOUSE, codes, rightW, rowH);
		panels.push_back({ "Mouse", &g, TARGET::MOUSE, 26 + (int)codes.size() * (rowH + rowGap) });
	}

	{
		GuiLayout& g = ui.column().gap(rowGap);
		const std::vector<int> codes = m.choices(player, TARGET::HOTKEY);
		optionRows(g, TARGET::HOTKEY, codes, rightW, rowH);
		panels.push_back({ "DOSBox", &g, TARGET::HOTKEY, (int)codes.size() * (rowH + rowGap) });
	}

	{
		GuiCard& clear = Popover_Row(ui, rightW, rowH, "Leave unassigned", now.kind == TARGET::NONE);
		clear.onClick([this]{
			picked = BindingTarget();
			done = true;
		});
		GuiLayout& g = ui.column().gap(6).add(clear).add(ui.label("This control will do nothing.").fontSize(UI_FONT_SMALL).muted());
		panels.push_back({ "None", &g, TARGET::NONE, rowH + 6 + 18 });
	}

	// open on the kind the control uses now; failing that, the suggestions (the first panel) when there are any
	shown = 0;
	for(size_t i = 0; i < panels.size(); i++)
		if(now.kind != TARGET::NONE && panels[i].kind == now.kind)
			shown = (int)i;
	kinds = (int)panels.size();

	// one fixed size for every kind, so the panel does not jump as the cursor moves down the left
	int bodyH = kinds * (kindH + rowGap);
	for(size_t i = 0; i < panels.size(); i++)
		bodyH = std::max(bodyH, panels[i].height);
	const int headH = 18, footH = 26;
	panelHeight = 2 * pad + headH + 7 + bodyH + 8 + footH;
	place = Popover_Place(anchor, panelWidth, panelHeight, sw, sh);

	// the kinds: plain rows, the one showing is filled
	GuiLayout& list = ui.column().gap(rowGap).size(leftW, bodyH);
	for(size_t i = 0; i < panels.size(); i++)
	{
		const int idx = (int)i;
		GuiCard& row = Popover_Row(ui, leftW, kindH, panels[i].name, false, false);
		row.onFocus([this, idx]{ showCategory(idx); });
		row.onClick([this, idx]{ showCategory(idx); });
		kindCards.push_back(&row);
		list.add(row);
	}
	// the kind that is showing keeps a fill, apart from the highlight, while the cursor is over its choices
	{
		std::vector<GuiCard*>& cards = kindCards;
		int styled = -1;
		ui.bind([&cards, styled, this]() mutable {
			if(styled == shown)
				return;
			styled = shown;
			for(size_t k = 0; k < cards.size(); k++)
				cards[k]->style((int)k == shown ? MenuPopoverShownStyle() : MenuRowHighlightStyle());
		});
	}

	GuiLayout& stack = ui.overlay().size(rightW, bodyH);
	for(size_t i = 0; i < panels.size(); i++)
	{
		stack.add(*panels[i].layout, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
		const int idx = (int)i;
		ui.visibleWhen(*panels[i].layout, [this, idx]{ return shown == idx; });
	}

	// the head says what is being set and what it does now; the foot says what the row under the cursor is, and offers the preset's default
	GuiLabel& title = ui.label(GuiFormat("What should %s do?", controlName).c_str()).fontSize(UI_FONT_SMALL).muted();
	GuiLabel& nowLabel = ui.label(GuiFormat("Now: %s", m.describe(player, control).c_str()).c_str()).fontSize(UI_FONT_SMALL).muted();
	GuiLabel& hint = ui.label().fontSize(UI_FONT_SMALL).muted().bind([this]{ return hintText; });
	hint.setMaxWidth(inner - 74);
	GuiTextButton& useDefault = ui.button("Default").size(70, footH).fontSize(UI_FONT_SMALL).style(MenuBarButtonStyle())
		.onClick([this, defaultTarget]{
			picked = defaultTarget;
			done = true;
		})
		.onFocus([this, defaultTarget]{ hintText = GuiFormat("A picks the preset's default: %s", defaultTarget.describe().c_str()); });

	GuiLayout& content = ui.column().size(inner, panelHeight - 2 * pad)
		.add(ui.row().size(inner, headH).align(LAYOUT_ALIGN::CENTRE).add(title).addSpace().add(nowLabel))
		.add(Popover_Divider(ui, inner))
		.addGap(6)
		.add(ui.row().size(inner, bodyH).gap(gap).align(LAYOUT_ALIGN::START).add(list).add(stack))
		.addGap(8)
		.add(ui.row().size(inner, footH).align(LAYOUT_ALIGN::CENTRE).add(hint).addSpace().add(useDefault));

	GuiCard& panel = ui.card().selectable(false).style(MenuPopoverStyle()).corners(GuiCorners(12.0f)).pad(pad, pad).size(panelWidth, panelHeight);
	panel.add(content);
	backBtn = &Popover_Mount(ui, *this, cancel, panel, place, panelWidth, panelHeight);

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

	ui.attachTo(*this);
}

// ---- list pickers

static void addWrapped(std::vector<std::string>& lines, const std::string& text, size_t max);

namespace {

const int kPopoverW = 280;
const int kPopoverPad = 10;
const int kPopoverTitleH = 18;
const int kPopoverCaptionLineH = 16;
const int kPopoverDividerH = 9;
const int kPopoverRowGap = 2;
const int kPopoverCaptionChars = 38;	// at the caption's size, in the width inside the padding

}

void PickerView::choose(int index)
{
	if(index < 0 || (size_t)index >= items.size())
		return;
	if(!items[(size_t)index].confirm.empty() && armedIdx != index)
	{
		focus = index;
		armedIdx = index;
		refreshCaption();
		return;
	}
	picked = index;
	done = true;
}

void PickerView::focusItem(int index)
{
	if(index < 0 || (size_t)index >= items.size())
		return;
	const bool moved = focus != index;
	focus = index;
	if(armedIdx != index)
		armedIdx = -1;
	refreshCaption();
	if(moved && focusCb)
		focusCb(index);
}

void PickerView::onFocusChange(std::function<void(int)> fn)
{
	focusCb = fn;
	refreshCaption();
	if(focusCb && !items.empty())
		focusCb(focus);
}

// The caption under the list: what the row under the cursor is, or (once armed) what the second press does
void PickerView::refreshCaption()
{
	for(size_t i = 0; i < captionLines.size(); i++)
		captionLines[i].clear();
	if(items.empty() || focus < 0 || (size_t)focus >= items.size())
		return;
	const PickerItem& it = items[(size_t)focus];
	std::string text;
	if(armedIdx == focus && !it.confirm.empty())
		text = it.confirm;
	else
	{
		for(size_t i = 0; i < it.lines.size(); i++)
			text += (text.empty() ? "" : " ") + it.lines[i];
		// the rows say what the choice does for each button; a screen that previews the choice shows that itself
		if(!focusCb)
			for(size_t i = 0; i < it.rows.size(); i++)
				text += (text.empty() ? "" : (i == 0 ? "  " : ", ")) + it.rows[i].label + ": " + it.rows[i].value;
	}
	std::vector<std::string> wrapped;
	addWrapped(wrapped, text, kPopoverCaptionChars);
	for(size_t i = 0; i < captionLines.size() && i < wrapped.size(); i++)
		captionLines[i] = wrapped[i];
	if(wrapped.size() > captionLines.size())
	{
		std::string& last = captionLines.back();
		if(last.size() > 2)
			last.resize(last.size() - 2);
		last += "..";
	}
}

PickerView::PickerView(const char * title, const std::vector<PickerItem>& itemList, GuiSound * soundOver, const PopoverAnchor& anchor) :
	items(itemList), picked(-1), done(false), focus(0), armedIdx(-1), backBtn(nullptr), inUse(0), panelWidth(kPopoverW), panelHeight(0), rowHeight(0)
{
	ui.setSounds(soundOver, nullptr);
	GuiTrigger& cancel = ui.own<GuiTrigger>();
	cancel.setSecondaryTrigger();

	const int sw = platform->getVideo()->getScreenWidth();
	const int sh = platform->getVideo()->getScreenHeight();
	setFixedSize(sw, sh);
	setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	setPosition(0, 0);

	int current = 0;
	for(size_t i = 0; i < items.size(); i++)
		if(items[i].current)
			current = (int)i;
	focus = current;
	inUse = current;

	// Size: a short list gets roomy rows; a long one shrinks its rows (down to 28 px) to stay between the bars
	const int n = (int)items.size();
	int dividers = 0;
	for(size_t i = 0; i < items.size(); i++)
		dividers += items[i].divider ? 1 : 0;
	const int captionLinesN = n <= 5 ? 3 : 2;
	captionLines.assign((size_t)captionLinesN, std::string());
	const int captionH = captionLinesN * kPopoverCaptionLineH + 4;
	const int fixedH = 2 * kPopoverPad + kPopoverTitleH + 7 + 8 + captionH;
	const int spare = Popover_MaxHeight(sh) - fixedH - dividers * kPopoverDividerH - std::max(0, n - 1) * kPopoverRowGap;
	rowHeight = std::max(28, std::min(36, n > 0 ? spare / n : 36));
	const int listH = n * rowHeight + std::max(0, n - 1) * kPopoverRowGap + dividers * kPopoverDividerH;
	panelHeight = fixedH + listH;
	const int innerW = kPopoverW - 2 * kPopoverPad;
	place = Popover_Place(anchor, kPopoverW, panelHeight, sw, sh);

	// the list: plain rows, a dot on the one in use, the highlight tint on the one under the cursor
	GuiLayout& list = ui.column().gap(kPopoverRowGap).size(innerW, listH);
	for(size_t i = 0; i < items.size(); i++)
	{
		const int idx = (int)i;
		GuiCard& row = Popover_Row(ui, innerW, rowHeight, items[i].name, items[i].current);
		row.onClick([this, idx]{ choose(idx); });
		row.onFocus([this, idx]{ focusItem(idx); });
		itemCards.push_back(&row);
		list.add(row);
		if(items[i].divider)
			list.add(ui.row().size(innerW, kPopoverDividerH).align(LAYOUT_ALIGN::CENTRE).add(Popover_Divider(ui, innerW)));
	}

	// what the popover is about, then a rule; the caption goes under the list
	GuiLabel& heading = ui.label(title).fontSize(UI_FONT_SMALL).muted();
	GuiLayout& captionColumn = ui.column().size(innerW, captionH).pad(0, 0);
	for(int k = 0; k < captionLinesN; k++)
	{
		GuiLabel& line = ui.label().fontSize(UI_FONT_SMALL).muted().bind([this, k]{ return captionLines[(size_t)k]; });
		captionColumn.add(ui.row().size(innerW, kPopoverCaptionLineH).align(LAYOUT_ALIGN::CENTRE).add(line));
	}
	GuiLayout& content = ui.column().size(innerW, panelHeight - 2 * kPopoverPad)
		.add(ui.row().size(innerW, kPopoverTitleH).align(LAYOUT_ALIGN::CENTRE).add(heading))
		.add(Popover_Divider(ui, innerW))
		.addGap(6)
		.add(list)
		.addGap(8)
		.add(captionColumn);

	GuiCard& panel = ui.card().selectable(false).style(MenuPopoverStyle()).corners(GuiCorners(12.0f)).pad(kPopoverPad, kPopoverPad).size(kPopoverW, panelHeight);
	panel.add(content);

	backBtn = &Popover_Mount(ui, *this, cancel, panel, place, kPopoverW, panelHeight);

	refreshCaption();
	ui.attachTo(*this);
	// the window steps the selection down from what is selected (or to the first control when nothing is): start just
	// above the item in use, so the step lands on it
	if(current > 0)
		itemCards[(size_t)current - 1]->select();
}

void PickerView::selectItem(int index)
{
	if(index < 0 || (size_t)index >= itemCards.size())
		return;
	for(size_t i = 0; i < itemCards.size(); i++)
		itemCards[i]->deselect();
	itemCards[(size_t)index]->select();
	focusItem(index);
}

void PickerView::selectInUse()
{
	if(itemCards.empty())
		return;
	for(size_t i = 0; i < itemCards.size(); i++)
		itemCards[i]->deselect();
	itemCards[(size_t)inUse < itemCards.size() ? (size_t)inUse : 0]->select();
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

std::vector<PickerItem> Picker_EmulatedItems(std::vector<int> * index)
{
	ControlsModel& m = Controls();
	const std::vector<EmulatedMode> modes = m.emulatedModes();
	std::vector<PickerItem> items;
	if(index)
		index->clear();
	for(size_t i = 0; i < modes.size(); i++)
	{
		// the advanced types are set from Settings: listed only while one is in effect, so it can be left
		if(modes[i].advanced && (int)i != m.currentEmulated())
			continue;
		if(index)
			index->push_back((int)i);
		PickerItem it;
		it.name = modes[i].name;
		it.current = (int)i == m.currentEmulated();
		addWrapped(it.lines, modes[i].description, 44);
		// (most descriptions already give the buttons: do not say it twice)
		if(modes[i].joysticks > 0 && modes[i].description.find("button") == std::string::npos)
			it.lines.push_back(GuiFormat("%d buttons, %d axes each.", modes[i].buttons, modes[i].axes));
		it.lines.push_back(modes[i].joysticks > 0 ? "Players without one use keys and the mouse." : "Keyboard and mouse stay available.");
		for(int p = 0; p < m.playerCount(); p++)
		{
			if(!m.player(p).connected)
				continue;
			PickerRow r;
			r.label = GuiFormat("Player %d", p + 1);
			const int slot = p - modes[i].firstPlayer;
			r.value = slot >= 0 && slot < modes[i].joysticks ? GuiFormat("Joystick %d", slot + 1) : std::string("Keys + mouse");
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
	const int now = m.currentPreset(player);
	const bool edited = m.modified(player);

	// what a preset gives the buttons that matter most, for the right-hand pane
	const auto describe = [&](PickerItem& it, int preset){
		for(size_t c = 0; c < sizeof(shown) / sizeof(shown[0]); c++)
		{
			PickerRow r;
			r.label = shown[c].name;
			r.value = m.describeTarget(player, m.presetBinding(preset, shown[c].control));
			it.rows.push_back(r);
		}
	};

	std::vector<PickerItem> items;
	index.clear();
	// first, when this player has changed buttons: go back to the preset they started from. This is what Reset was.
	if(edited && now >= 0 && now < (int)presets.size())
	{
		PickerItem it;
		it.name = "Undo my changes";
		it.current = false;
		it.lines.push_back(GuiFormat("Back to %s.", presets[(size_t)now].name.c_str()));
		describe(it, now);
		it.confirm = "This discards your changes.  Press A again to confirm";
		it.divider = true;
		items.push_back(it);
		index.push_back(-1);
	}
	for(size_t i = 0; i < presets.size(); i++)
	{
		const bool inUse = (int)i == now;
		// a preset that fires the joystick means nothing without one (unless it is the one in use, which must stay visible)
		if(presets[i].needsJoystick && m.joystickButtons(player) == 0 && !inUse)
			continue;
		PickerItem it;
		it.name = presets[i].name;
		it.current = inUse;
		addWrapped(it.lines, presets[i].description, 44);
		describe(it, (int)i);
		if(edited)
			it.confirm = "This replaces your changes.  Press A again to confirm";
		items.push_back(it);
		index.push_back((int)i);
	}
	return items;
}

std::vector<PickerItem> Picker_SetupMenuItems(std::vector<SetupAction>& kinds)
{
	ControlsModel& m = Controls();
	std::vector<PickerItem> items;
	kinds.clear();
	const size_t saved = m.setups().size();
	{
		PickerItem it;
		it.name = "Load a setup...";
		it.current = false;
		it.lines.push_back(saved == 0 ? std::string("No saved setups yet.") : saved == 1 ? std::string("1 setup saved.") : GuiFormat("%d setups saved.", (int)saved));
		it.lines.push_back("Replaces every player's buttons");
		it.lines.push_back("and what DOS sees.");
		items.push_back(it);
		kinds.push_back(SetupAction::LOAD);
	}
	if(!m.setupName().empty())
	{
		PickerItem it;
		it.name = GuiFormat("Save as %s", ClipName(m.setupName(), 16).c_str());
		it.current = false;
		it.lines.push_back("Saves every player's buttons and what");
		it.lines.push_back("DOS sees, replacing what was saved.");
		items.push_back(it);
		kinds.push_back(SetupAction::SAVE_CURRENT);
	}
	{
		PickerItem it;
		it.name = "Save as a new setup...";
		it.current = false;
		it.lines.push_back("Asks for a name, then the program it");
		it.lines.push_back("loads for. Saves every player's buttons");
		it.lines.push_back("and what DOS sees.");
		items.push_back(it);
		kinds.push_back(SetupAction::SAVE_NEW);
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
		if(m.setupModified())
			it.confirm = "This replaces your changes.  Press A again to confirm";
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
		GuiWindow& window() { return w; }
		// A disabled screen stays drawn, dimmed (the menu's one look for "something is open over this"), and takes no
		// input, so a modal put over it is the only thing the D-pad and pointer reach
		void setEnabled(bool on) { w.setState(on ? STATE::DEFAULT : STATE::DISABLED); }
	private:
		GuiWindow w;
		bool shown;
};

// The chooser is a popover over the setup screen, which the caller has disabled and which stays drawn.
bool ChooserScreen(const char * name, int player, int control, BindingTarget& target, const BindingTarget& defaultTarget, const PopoverAnchor& anchor)
{
	GuiSound over(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	TargetChooserView view(name, player, control, target, defaultTarget, &over, anchor);
	ScreenWindow screen(view);
	UpdateGui();
	mainWindow->changeFocus(&screen.window());
	view.selectCategory();

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

// Runs a picker as a popover over whatever screen is running (which stays on the window, disabled by the caller: a
// question asked in place, not a trip to another page). The index chosen, or -1 on Back. `preview` hears the index of
// the row under the cursor; `perFrame` runs after each frame (to let a screen that is not being updated follow it).
int PickerScreen(const char * title, const std::vector<PickerItem>& items, const PopoverAnchor& anchor,
	std::function<void(int)> preview = nullptr, std::function<void()> perFrame = nullptr)
{
	GuiSound over(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	PickerView view(title, items, &over, anchor);
	if(preview)
		view.onFocusChange(preview);
	ScreenWindow screen(view);
	// The screen under it still holds the focus, which only moves by itself when that screen is removed. Hand it over
	// once the popover has been laid out: taking it steps the selection down, and that needs to know where things are.
	UpdateGui();
	mainWindow->changeFocus(&screen.window());
	view.selectInUse(); // (taking the focus stepped the selection, from wherever it was)

	while(true)
	{
		UpdateGui();
		if(perFrame)
			perFrame();
		if(view.chosen())
			return view.result();
		if(view.backButton().getState() == STATE::CLICKED)
			return -1;
	}
}

void EmulatedPicker(const PopoverAnchor& anchor)
{
	std::vector<int> index;
	const int chosen = PickerScreen("What does DOS see?", Picker_EmulatedItems(&index), anchor);
	if(chosen >= 0 && (size_t)chosen < index.size())
		Controls().setEmulated(index[(size_t)chosen]);
}

// What the hint line should say after. The setup screen behind it previews the preset under the cursor.
std::string PresetPicker(int player, ControllerConfigView& behind)
{
	std::vector<int> index;
	ControlsModel& m = Controls();
	const std::string title = ConnectedCount() >= 2 ? GuiFormat("Preset for Player %d", player + 1) : std::string("Choose a preset");
	const int chosen = PickerScreen(title.c_str(), Picker_PresetItems(player, index), behind.anchorFor(ControllerConfigView::Pick::PRESET),
		[&](int i){
			const int preset = i >= 0 && (size_t)i < index.size() ? index[(size_t)i] : -1;
			behind.setPreview(preset < 0 ? m.currentPreset(player) : preset); // "Undo my changes" shows the preset they started from
		},
		[&]{ behind.refresh(); });
	behind.setPreview(-1);
	behind.refresh();
	if(chosen < 0)
		return std::string();
	const std::vector<PresetInfo> presets = m.presets();
	if(index[(size_t)chosen] < 0)
	{
		m.resetBindings(player);
		return GuiFormat("Buttons back to %s", presets[(size_t)m.currentPreset(player)].name.c_str());
	}
	m.applyPreset(player, index[(size_t)chosen]);
	return GuiFormat("Preset: %s", presets[(size_t)index[(size_t)chosen]].name.c_str());
}

// Asks something with a few answers; the index chosen, or -1.
int AskScreen(const char * title, const std::vector<std::string>& answers, const std::string& detail, const PopoverAnchor& anchor)
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
	return PickerScreen(title, items, anchor);
}

// Load: choose a saved setup. What the hint line should say after.
std::string LoadFlow(const PopoverAnchor& anchor)
{
	std::vector<int> index;
	const int chosen = PickerScreen("Load a setup", Picker_SetupItems(index), anchor);
	if(chosen < 0)
		return std::string();
	const std::vector<SetupInfo> setups = Controls().setups();
	if(!Controls().loadSetup(index[(size_t)chosen]))
		return "That setup could not be loaded";
	return "Loaded " + setups[(size_t)index[(size_t)chosen]].name;
}

// Save: a name, what program it is for, and a check before replacing one of the same name.
// What a flow needs of the screen it runs over: to take it away while the on-screen keyboard has the display, and bring it back
struct ScreenHooks {
	std::function<void()> hide;
	std::function<void()> show;
};

std::string SaveFlow(const ScreenHooks& screen, const PopoverAnchor& anchor)
{
	ControlsModel& m = Controls();
	const std::string running = Setup_CleanProgram(m.runningProgram());

	char typed[64];
	snprintf(typed, sizeof(typed), "%s", Setup_SuggestedName(running).c_str());
	screen.hide();	// the on-screen keyboard is a screen of its own
	const bool named = OnScreenKeyboard(typed, sizeof(typed));
	screen.show();
	if(!named)
		return std::string();
	const std::string name = Setup_CleanName(typed);
	if(name.empty())
		return "That name can't be used";

	std::vector<AttachChoice> kinds;
	const int attach = PickerScreen("Load these buttons for", Picker_AttachItems(running, kinds), anchor);
	if(attach < 0)
		return std::string();
	std::string program;
	if(kinds[(size_t)attach] == AttachChoice::RUNNING)
		program = running;
	else if(kinds[(size_t)attach] == AttachChoice::TYPED)
	{
		char prog[64] = { 0 };
		screen.hide();
		const bool typedOk = OnScreenKeyboard(prog, sizeof(prog));
		screen.show();
		if(!typedOk)
			return std::string();
		program = Setup_CleanProgram(prog);
	}

	const std::vector<SetupInfo> existing = m.setups();
	for(size_t i = 0; i < existing.size(); i++)
	{
		if(Setup_SameName(existing[i].name, name))
		{
			std::vector<std::string> answers;
			answers.push_back("Replace it");
			answers.push_back("Cancel");
			if(AskScreen(GuiFormat("%s already exists", name.c_str()).c_str(), answers, "Replacing it loses what was saved under that name.", anchor) != 0)
				return std::string();
			break;
		}
	}
	return m.saveSetup(name, program) ? "Saved " + name : std::string("Could not save the setup");
}

// Saves over the setup that is loaded: no questions, it is the same name and the same program.
std::string SaveCurrentFlow()
{
	ControlsModel& m = Controls();
	const std::string name = m.setupName();
	std::string program;
	const std::vector<SetupInfo> existing = m.setups();
	for(size_t i = 0; i < existing.size(); i++)
		if(Setup_SameName(existing[i].name, name))
			program = existing[i].program;
	return m.saveSetup(name, program) ? "Saved " + name : std::string("Could not save the setup");
}

// The Setup button: load a setup, or save this one. What the hint line should say after.
std::string SetupFlow(const ScreenHooks& screen, const PopoverAnchor& anchor)
{
	std::vector<SetupAction> kinds;
	const int chosen = PickerScreen("Load or save a setup", Picker_SetupMenuItems(kinds), anchor);
	if(chosen < 0)
		return std::string();
	switch(kinds[(size_t)chosen])
	{
		case SetupAction::LOAD:
			return Controls().setups().empty() ? std::string("No saved setups yet.  Save one first") : LoadFlow(anchor);
		case SetupAction::SAVE_CURRENT:
			return SaveCurrentFlow();
		default:
			return SaveFlow(screen, anchor);
	}
}

// The device each slot's setup screen last drew, so a choice on the rail survives a rebuild or coming back to the screen
PadType shownDevice[4] = { PadType::NONE, PadType::NONE, PadType::NONE, PadType::NONE };

void ConfigScreen(int player)
{
	GuiSound over(button_over_pcm, button_over_pcm_size, SOUND::PCM);
	player = ResolvePlayer(player);
	bool leave = false;

	// The screen is built for one controller. When the player is switched, or a controller comes or goes, it is built again.
	while(!leave)
	{
		ControllerConfigView view(player, &over, shownDevice[player & 3]);
		ScreenWindow screen(view);

		ProfileRow row;
		while(true)
		{
			UpdateGui();
			if(view.backButton().getState() == STATE::CLICKED)
			{
				leave = true;
				break;
			}
			const int sw = view.takeSwitch();
			if(sw != 0)
			{
				player = NextPlayer(view.player(), sw);
				break;
			}
			const PadType flip = view.takeFlip();
			if(flip != PadType::NONE)
			{
				shownDevice[view.player() & 3] = flip;
				break;
			}
			if(view.outOfDate())
			{
				player = ResolvePlayer(view.player());
				break;
			}
			if(view.takeRequest(row))
			{
				BindingTarget target = Controls().binding(view.player(), row.binding);
				screen.setEnabled(false);
				if(ChooserScreen(row.name, view.player(), row.binding, target, Controls().defaultBinding(view.player(), row.binding), view.anchorForRow(row.binding)))
					Controls().setBinding(view.player(), row.binding, target);
				screen.setEnabled(true);
				mainWindow->changeFocus(&screen.window());
			}
			const ControllerConfigView::Pick pick = view.takePick();
			if(pick != ControllerConfigView::Pick::NONE)
			{
				// All three are popovers over this screen, which stays drawn (dimmed) where it is. The on-screen keyboard
				// that a Save asks for is the one trip to another screen.
				std::string said;
				const PopoverAnchor anchor = view.anchorFor(pick);
				screen.setEnabled(false);
				if(pick == ControllerConfigView::Pick::SETUP)
					said = SetupFlow(ScreenHooks{ [&]{ screen.hide(); }, [&]{ screen.show(); } }, anchor);
				else if(pick == ControllerConfigView::Pick::EMULATED)
					EmulatedPicker(anchor);
				else
					said = PresetPicker(view.player(), view);
				screen.setEnabled(true);
				// the screen takes the focus back, and the selection returns to the button that was pressed
				mainWindow->changeFocus(&screen.window());
				screen.window().resetState();
				view.selectHeader(pick);
				if(!said.empty())
					view.setStatus(said);
			}
		}
	}
}

} // namespace

const unsigned char * Controls_PadIcon(PadType type)
{
	return padIcon(type);
}

std::string Controls_SetupMenu(GuiWindow * home, const PopoverAnchor& anchor)
{
	// the home screen stays drawn (dimmed) under the popover; only the on-screen keyboard takes it away
	home->setState(STATE::DISABLED);
	const ScreenHooks hooks = { [home]{ mainWindow->remove(home); }, [home]{ mainWindow->append(home); } };
	const std::string said = SetupFlow(hooks, anchor);
	home->setState(STATE::DEFAULT);
	mainWindow->changeFocus(home);
	home->resetState();
	return said;
}

void Controls_Menu(GuiWindow * home, int player)
{
	mainWindow->remove(home);
	ConfigScreen(player);
	mainWindow->append(home);
}
