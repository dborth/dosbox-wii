/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * popover.cpp
 ***************************************************************************/
#include "popover.h"
#include "menustyle.h"
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

PopoverBackdrop::PopoverBackdrop(int w, int h) : GuiButton(w, h), panel(nullptr), cancel(nullptr)
{
	setSelectable(false);
	setRumble(false);
}

void PopoverBackdrop::update(InputController * c)
{
	GuiButton::update(c);
	if(getState() != STATE::CLICKED || !c || !panel)
		return;
	const InputPadData& pad = c->getPadData();
	const bool byCancel = cancel && cancel->isClicked(c);
	if(!byCancel && pad.validPointer && panel->isInside(pad.cursor_x, pad.cursor_y))
		resetState();
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
	backdrop.setTrigger(0, ui.trigger());
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
