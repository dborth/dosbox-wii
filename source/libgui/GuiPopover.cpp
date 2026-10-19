/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiPopover.cpp
 *
 * See GuiPopover.h.
 ***************************************************************************/
#include "Gui.h"

GuiPopoverBackdrop::GuiPopoverBackdrop(int w, int h) : GuiButton(w, h), panel(nullptr), cancel(nullptr), primary(nullptr), closing(false)
{
	setSelectable(false);
	setRumble(false);
}

void GuiPopoverBackdrop::dismiss()
{
	closing = true;
	setState(STATE::CLICKED);
}

void GuiPopoverBackdrop::update(InputController * c)
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

GuiPopover::GuiPopover() : cancelTrigger(nullptr), backdrop(nullptr)
{
	placed.x = 0;
	placed.y = 0;
	cancelTrigger = &ui.own<GuiTrigger>();
	cancelTrigger->setSecondaryTrigger();

	setFixedSize(platform->getVideo()->getScreenWidth(), platform->getVideo()->getScreenHeight());
	setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	setPosition(0, 0);
}

void GuiPopover::dismiss()
{
	if(backdrop)
		backdrop->dismiss();
}

void GuiPopover::mount(GuiCard& panel, int panelW, int panelH, const GuiPopoverAnchor& anchor, const GuiPopoverInsets& insets)
{
	mount(panel, panelW, panelH, anchor, insets, GuiTheme::current().surfaceRaised);
}

void GuiPopover::mount(GuiCard& panel, int panelW, int panelH, const GuiPopoverAnchor& anchor, const GuiPopoverInsets& insets, PixelColor tailColor)
{
	const int sw = platform->getVideo()->getScreenWidth();
	const int sh = platform->getVideo()->getScreenHeight();
	placed = GuiPopoverPlace(anchor, panelW, panelH, sw, sh, insets);

	backdrop = &ui.make<GuiPopoverBackdrop>(sw, sh);
	GuiTrigger * click = ui.trigger();
	backdrop->setTrigger(0, click);
	backdrop->setPrimary(click);
	backdrop->setTrigger(1, cancelTrigger);
	backdrop->setCancel(cancelTrigger);
	backdrop->setPanel(&panel);
	add(backdrop, GuiItem().background());
	GuiLayout& at = ui.column().size(sw, sh).addGap((float)placed.y).add(ui.row().size(sw, panelH).addGap((float)placed.x).add(panel));
	add(&at);

	// the tail, drawn over the panel's edge in the panel's color, pointing at what the popover belongs to
	const GuiPopoverTail tail = GuiPopoverTailAt(anchor, placed, panelW, panelH);
	if(tail.shown)
	{
		static const SHAPE kinds[] = { SHAPE::TRIANGLE_UP, SHAPE::TRIANGLE_DOWN, SHAPE::TRIANGLE_LEFT, SHAPE::TRIANGLE_RIGHT };
		GuiShape& arrow = ui.own<GuiShape>(tail.w, tail.h);
		arrow.setArrow(kinds[(int)tail.direction], tailColor, tailColor);
		GuiLayout& tailPlaced = ui.column().size(sw, sh).addGap((float)tail.y).add(ui.row().size(sw, tail.h).addGap((float)tail.x).add(arrow));
		add(&tailPlaced);
	}
}
