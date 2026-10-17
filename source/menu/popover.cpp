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
		p.y = a.y + a.h + kGap;
		if(p.y + h > bottom)
			p.y = a.y - h - kGap;
		if(a.edge == PopoverAnchor::LEFT_MARGIN)
			p.x = MENU_PAGE_MARGIN;
		else if(a.edge == PopoverAnchor::RIGHT_MARGIN)
			p.x = screenW - MENU_PAGE_MARGIN - w;
		else
			p.x = (a.x + a.w / 2 > screenW / 2) ? a.x + a.w - w : a.x;
	}
	p.y = std::max(top, std::min(p.y, bottom - h));
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

GuiButton& Popover_Mount(UiScope& ui, GuiLayout& view, GuiTrigger& cancel, GuiCard& panel, const PopoverPlace& at, int panelW, int panelH)
{
	(void)panelW;
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
	return backdrop;
}
