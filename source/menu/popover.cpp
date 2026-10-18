/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * popover.cpp
 ***************************************************************************/
#include "popover.h"
#include "menustyle.h"
#include "menuart.h"
#include "menupage.h"
#include <algorithm>

static const int kGap = 6;	// between the anchor and the panel

int Popover_MaxHeight(int screenHeight)
{
	return screenHeight - MENU_BAR_TOP_H - MENU_BAR_BOTTOM_H - 2 * kGap;
}

PopoverPlace Popover_Place(const PopoverAnchor& a, int w, int h, int screenW, int screenH)
{
	const int top = MENU_BAR_TOP_H + kGap;
	const int bottom = screenH - MENU_BAR_BOTTOM_H - kGap;
	PopoverPlace p;
	if(!a.valid)
	{
		p.x = (screenW - w) / 2;
		p.y = (screenH - h) / 2;
	}
	else
	{
		// Straight under its anchor if that fits above the footer bar, else straight over it; a chip in a bar
		// gets a panel that comes right out of it. Otherwise (a row in the middle of the page) it keeps to the band.
		const int below = a.y + a.h + kGap, above = a.y - h - kGap;
		if(below + h <= bottom)
			p.y = below;
		else if(above >= top)
			p.y = above;
		else
			p.y = std::max(top, std::min(below, bottom - h));
		if(a.edge == PopoverAnchor::LEFT_MARGIN)
			p.x = MENU_PAGE_MARGIN;
		else if(a.edge == PopoverAnchor::RIGHT_MARGIN)
			p.x = screenW - MENU_PAGE_MARGIN - w;
		else
			p.x = (a.x + a.w / 2 > screenW / 2) ? a.x + a.w - w : a.x;
	}
	p.x = std::max(MENU_PAGE_MARGIN, std::min(p.x, screenW - MENU_PAGE_MARGIN - w));
	return p;
}

GuiShape& Popover_Divider(UiScope& ui, int w)
{
	GuiShape& line = ui.make<GuiShape>(w, 1);
	line.setPaint(GuiPaint().color(MenuPaletteCurrent().outline));
	return line;
}

PopoverBackdrop::PopoverBackdrop(int w, int h) : GuiButton(w, h), panel(nullptr), cancel(nullptr), primary(nullptr), closing(false)
{
	setSelectable(false);
	setRumble(false);
}

void PopoverBackdrop::dismiss()
{
	closing = true;
	setState(STATE::CLICKED);
}

void PopoverBackdrop::update(InputController * c)
{
	if(closing)
	{
		if(getState() == STATE::CLICKED)
			return;
		closing = false;	// the screen took the click (or ignored it) and reset the button
	}
	if(!c || getState() == STATE::CLICKED || getState() == STATE::DISABLED)
		return;
	if(parentElement && parentElement->getState() == STATE::DISABLED)
		return;

	// A click anywhere outside the panel, or B, closes it.
	// Each controller can click for itself here.
	const InputPadData& pad = c->getPadData();
	const int chan = c->getChannel();
	bool leave = cancel && cancel->isClicked(c);
	if(!leave && primary && primary->isClicked(c) && pad.validPointer && chan >= 0)
		leave = !panel || !panel->isInside(pad.cursor_x, pad.cursor_y);
	if(leave)
		setState(STATE::CLICKED, chan);
}

namespace {

// The X of a close button: two crossed strokes, drawn from shapes like the home screen's icons
class PopoverCross : public MenuMeshElement
{
	public:
		PopoverCross(int size) : MenuMeshElement(size, size) {}

	private:
		void build() override
		{
			const GuiFill fill = GuiFill::solid(MenuPaletteCurrent().text);
			const float s = (float)width, m = s * 0.3f, w = 1.8f;
			mesh.addLine(GuiPoint{ m, m }, GuiPoint{ s - m, s - m }, w, fill);
			mesh.addLine(GuiPoint{ s - m, m }, GuiPoint{ m, s - m }, w, fill);
			mesh.addEllipse(GuiRect{ m - w / 2, m - w / 2, w, w }, fill);
			mesh.addEllipse(GuiRect{ s - m - w / 2, s - m - w / 2, w, w }, fill);
			mesh.addEllipse(GuiRect{ s - m - w / 2, m - w / 2, w, w }, fill);
			mesh.addEllipse(GuiRect{ m - w / 2, s - m - w / 2, w, w }, fill);
		}
};

}

GuiCard& Popover_CloseButton(UiScope& ui, std::function<void()> onClick)
{
	const int size = 22;
	GuiCard& close = ui.card().size(size, size).style(MenuRowHighlightStyle()).corners(GuiCorners(size / 2.0f)).pad(0, 0).onClick(onClick);
	close.add(ui.make<PopoverCross>(size), GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));
	return close;
}

GuiCard& Popover_Row(UiScope& ui, int width, int height, const std::string& title, bool marked, bool withDotSlot)
{
	const int padX = 10, dotSlot = withDotSlot ? 14 : 0;
	GuiLabel& name = ui.label(title.c_str()).fontSize(title.size() > 18 ? 14 : 16);
	name.setMaxWidth(width - 2 * padX - (withDotSlot ? dotSlot + 8 : 0));
	GuiLayout& dotBox = ui.row().size(dotSlot, dotSlot).align(LAYOUT_ALIGN::CENTRE);
	if(marked)
	{
		GuiShape& dot = ui.make<GuiShape>(10, 10);
		dot.setShape(SHAPE::ELLIPSE);
		dot.setPaint(GuiPaint().color(MenuPaletteCurrent().accent));
		dotBox.add(dot);
	}
	GuiLayout& content = ui.row().size(width - 2 * padX, height).gap(8).align(LAYOUT_ALIGN::CENTRE).add(name).addSpace();
	if(withDotSlot)
		content.add(dotBox);
	GuiCard& row = ui.card();
	row.size(width, height).style(MenuRowHighlightStyle()).corners(GuiCorners(8.0f)).pad(padX, 0);
	row.add(content);
	return row;
}

PopoverTail Popover_Tail(const PopoverAnchor& a, const PopoverPlace& at, int panelW, int panelH)
{
	const int len = 8, base = 16, edge = 18;	// how far it sticks out, how wide it is, how far from a corner it stays
	PopoverTail t = { false, POPOVER_TAIL_UP, 0, 0, 0, 0 };
	if(!a.valid)
		return t;
	const int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
	if(at.y >= a.y + a.h)			// below it: pointing up, under the anchor's middle
	{
		t = { true, POPOVER_TAIL_UP, std::max(at.x + edge, std::min(ax - base / 2, at.x + panelW - edge - base)), at.y - len + 1, base, len };
	}
	else if(at.y + panelH <= a.y)	// above it: pointing down
	{
		t = { true, POPOVER_TAIL_DOWN, std::max(at.x + edge, std::min(ax - base / 2, at.x + panelW - edge - base)), at.y + panelH - 1, base, len };
	}
	else if(at.x >= ax)				// beside it, on its right (it may cover the anchor's end): pointing left, level with the anchor
	{
		t = { true, POPOVER_TAIL_LEFT, at.x - len + 1, std::max(at.y + edge, std::min(ay - base / 2, at.y + panelH - edge - base)), len, base };
	}
	else if(at.x + panelW <= ax)	// on its left: pointing right
	{
		t = { true, POPOVER_TAIL_RIGHT, at.x + panelW - 1, std::max(at.y + edge, std::min(ay - base / 2, at.y + panelH - edge - base)), len, base };
	}
	return t;
}

GuiButton& Popover_Mount(UiScope& ui, GuiLayout& view, GuiTrigger& cancel, GuiCard& panel, const PopoverPlace& at, int panelW, int panelH, const PopoverAnchor& anchor)
{
	const int sw = platform->getVideo()->getScreenWidth();
	const int sh = platform->getVideo()->getScreenHeight();
	PopoverBackdrop& backdrop = ui.make<PopoverBackdrop>(sw, sh);
	GuiTrigger * click = ui.trigger();
	backdrop.setTrigger(0, click);
	backdrop.setPrimary(click);
	backdrop.setTrigger(1, &cancel);
	backdrop.setCancel(&cancel);
	backdrop.setPanel(&panel);
	view.add(&backdrop, GuiItem().background());
	GuiLayout& placed = ui.column().size(sw, sh).addGap((float)at.y).add(ui.row().size(sw, panelH).addGap((float)at.x).add(panel));
	view.add(&placed);
	// the tail, drawn over the panel's edge in the panel's color, pointing at what the popover belongs to
	const PopoverTail tail = Popover_Tail(anchor, at, panelW, panelH);
	if(tail.shown)
	{
		static const ARROW arrows[] = { ARROW::UP, ARROW::DOWN, ARROW::LEFT, ARROW::RIGHT };
		const PixelColor face = MenuPaletteCurrent().surfaceRaised;
		GuiArrow& arrow = ui.own<GuiArrow>(arrows[tail.direction], tail.w, tail.h, face, face);
		GuiLayout& tailPlaced = ui.column().size(sw, sh).addGap((float)tail.y).add(ui.row().size(sw, tail.h).addGap((float)tail.x).add(arrow));
		view.add(&tailPlaced);
	}
	return backdrop;
}

// ---- the list picker

namespace {

const int kPopoverW = 280;
const int kPopoverPad = 10;
const int kPopoverTitleH = 24;
const int kPopoverCaptionLineH = 16;
const int kPopoverDividerH = 9;
const int kPopoverRowGap = 2;
const int kPopoverCaptionChars = 38;	// at the caption's size, in the width inside the padding

}

void PickerView::choose(int index)
{
	if(index < 0 || (size_t)index >= items.size())
		return;
	picked = index;
	done = true;
}

void PickerView::focusItem(int index)
{
	if(index < 0 || (size_t)index >= items.size())
		return;
	const bool moved = focus != index;
	focus = index;
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

// The caption under the list: what the row under the cursor is
void PickerView::refreshCaption()
{
	for(size_t i = 0; i < captionLines.size(); i++)
		captionLines[i].clear();
	if(items.empty() || focus < 0 || (size_t)focus >= items.size())
		return;
	const PickerItem& it = items[(size_t)focus];
	std::string text;
	for(size_t i = 0; i < it.lines.size(); i++)
		text += (text.empty() ? "" : " ") + it.lines[i];
	// the rows say what the choice does for each button; a screen that previews the choice shows that itself
	if(!focusCb)
		for(size_t i = 0; i < it.rows.size(); i++)
			text += (text.empty() ? "" : (i == 0 ? "  " : ", ")) + it.rows[i].label + ": " + it.rows[i].value;
	std::vector<std::string> wrapped;
	Popover_Wrap(wrapped, text, kPopoverCaptionChars);
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

PickerView::PickerView(const char * title, const std::vector<PickerItem>& itemList, const PopoverAnchor& anchor) :
	items(itemList), picked(-1), done(false), focus(0), backBtn(nullptr), inUse(0), panelWidth(kPopoverW), panelHeight(0), rowHeight(0)
{
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
	// a way out that needs neither B nor a click on the empty screen around the panel
	GuiCard& close = Popover_CloseButton(ui, [this]{ if(backBtn) static_cast<PopoverBackdrop*>(backBtn)->dismiss(); });
	GuiLayout& captionColumn = ui.column().size(innerW, captionH).pad(0, 0);
	for(int k = 0; k < captionLinesN; k++)
	{
		GuiLabel& line = ui.label().fontSize(UI_FONT_SMALL).muted().bind([this, k]{ return captionLines[(size_t)k]; });
		captionColumn.add(ui.row().size(innerW, kPopoverCaptionLineH).align(LAYOUT_ALIGN::CENTRE).add(line));
	}
	GuiLayout& content = ui.column().size(innerW, panelHeight - 2 * kPopoverPad)
		.add(ui.row().size(innerW, kPopoverTitleH).align(LAYOUT_ALIGN::CENTRE).add(heading).addSpace().add(close))
		.add(Popover_Divider(ui, innerW))
		.addGap(6)
		.add(list)
		.addGap(8)
		.add(captionColumn);

	GuiCard& panel = ui.card().selectable(false).style(MenuPopoverStyle()).corners(GuiCorners(12.0f)).pad(kPopoverPad, kPopoverPad).size(kPopoverW, panelHeight);
	panel.add(content);

	backBtn = &Popover_Mount(ui, *this, cancel, panel, place, kPopoverW, panelHeight, anchor);

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
void Popover_Wrap(std::vector<std::string>& lines, const std::string& text, size_t max)
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
