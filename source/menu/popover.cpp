/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * popover.cpp
 *
 * See popover.h.
 ***************************************************************************/
#include "popover.h"
#include <algorithm>

static const int kGap = 6;	// between the anchor and the panel

int Popover_MaxHeight(int screenHeight, const PopoverInsets& insets)
{
	return screenHeight - insets.top - insets.bottom - 2 * kGap;
}

PopoverPlace Popover_Place(const PopoverAnchor& a, int w, int h, int screenW, int screenH, const PopoverInsets& insets)
{
	const int top = insets.top + kGap;
	const int bottom = screenH - insets.bottom - kGap;
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
			p.x = insets.side;
		else if(a.edge == PopoverAnchor::RIGHT_MARGIN)
			p.x = screenW - insets.side - w;
		else
			p.x = (a.x + a.w / 2 > screenW / 2) ? a.x + a.w - w : a.x;
	}
	p.x = std::max(insets.side, std::min(p.x, screenW - insets.side - w));
	return p;
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

GuiButton& Popover_Mount(UiScope& ui, GuiLayout& view, GuiTrigger& cancel, GuiCard& panel, const PopoverPlace& at, int panelW, int panelH, const PopoverAnchor& anchor, PixelColor tailColor)
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
		static const SHAPE kinds[] = { SHAPE::TRIANGLE_UP, SHAPE::TRIANGLE_DOWN, SHAPE::TRIANGLE_LEFT, SHAPE::TRIANGLE_RIGHT };
		GuiShape& arrow = ui.own<GuiShape>(tail.w, tail.h);
		arrow.setArrow(kinds[tail.direction], tailColor, tailColor);
		GuiLayout& tailPlaced = ui.column().size(sw, sh).addGap((float)tail.y).add(ui.row().size(sw, tail.h).addGap((float)tail.x).add(arrow));
		view.add(&tailPlaced);
	}
	return backdrop;
}
