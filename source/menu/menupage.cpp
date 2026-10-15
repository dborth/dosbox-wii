/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menupage.cpp
 ***************************************************************************/
#include "menupage.h"
#include "menustyle.h"

GuiTextButton& MenuPage_Button(UiScope& ui, const char * text, int width)
{
	return ui.button(text).size(width, 38).fontSize(18).style(MenuBarButtonStyle());
}

GuiCard& MenuPage_Section(UiScope& ui, int w)
{
	GuiCard& c = ui.card().selectable(false).style(MenuSectionStyle()).corners(GuiCorners(10.0f)).pad(8, 6).size(w, 0);
	return c;
}

static MenuPageFrame buildFrame(UiScope& ui, GuiOverlay& page, const char * title, std::function<std::string()> hint, bool withBody)
{
	const int sw = platform->getVideo()->getScreenWidth();
	const int sh = platform->getVideo()->getScreenHeight();

	MenuPageFrame f;
	f.bodyHeight = sh - MENU_BAR_TOP_H - MENU_BAR_BOTTOM_H;
	f.body = nullptr;
	f.content = nullptr;

	f.title = &ui.label(title).fontSize(MENU_PAGE_TITLE_FONT).textColor(MenuChromeText());
	f.headerLeft = &ui.row().gap(10).align(LAYOUT_ALIGN::CENTRE);
	f.headerRight = &ui.row().gap(10).align(LAYOUT_ALIGN::CENTRE);
	GuiLayout& header = ui.row().size(sw, MENU_BAR_TOP_H).pad(MENU_PAGE_MARGIN, 0).gap(18).align(LAYOUT_ALIGN::CENTRE)
		.add(*f.title).add(*f.headerLeft).addSpace().add(*f.headerRight);

	if(withBody)
	{
		// The body fills the screen between the bars (the padding keeps it clear of them). It goes in the page first,
		// so the first thing the D-pad can select is in it and not on the header.
		f.body = &ui.column().size(sw, sh).pad(MENU_PAGE_MARGIN, MENU_BAR_TOP_H + 4, MENU_PAGE_MARGIN, MENU_BAR_BOTTOM_H + 4)
			.align(LAYOUT_ALIGN::CENTRE).justifyContent(LAYOUT_JUSTIFY::CENTRE);

		// everything on the page sits on one card; sections inside it are flat tints, not more cards
		f.content = &ui.column().gap(6).align(LAYOUT_ALIGN::CENTRE);
		GuiCard& panel = ui.card().selectable(false).style(MenuPageCardStyle()).pad(MENU_PAGE_CARD_PAD, MENU_PAGE_CARD_PAD).size(MENU_PAGE_BODY_W, 0);
		panel.add(*f.content);
		f.body->add(panel);
	}
	else
		page.setFixedSize(sw, sh);

	f.hint = &ui.label().fontSize(MENU_PAGE_HINT_FONT).textColor(MenuChromeTextMuted());
	f.hint->bind(hint);
	f.hint->setMaxWidth(MENU_PAGE_BODY_W - 236);
	f.hint->setScroll(SCROLL::HORIZONTAL);
	f.footerRight = &ui.row().gap(10).align(LAYOUT_ALIGN::CENTRE);
	GuiLayout& footer = ui.row().size(sw, MENU_BAR_BOTTOM_H).pad(MENU_PAGE_MARGIN, 0).align(LAYOUT_ALIGN::CENTRE)
		.add(*f.hint).addSpace().add(*f.footerRight);

	if(f.body)
		page.add(*f.body, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
	page.add(header, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::START));
	page.add(footer, GuiItem().at(LAYOUT_ALIGN::START, LAYOUT_ALIGN::END));
	page.setAlignment(ALIGN_H::LEFT, ALIGN_V::TOP);
	page.setPosition(0, 0);
	return f;
}

MenuPageFrame MenuPage_Build(UiScope& ui, GuiOverlay& page, const char * title, std::function<std::string()> hint)
{
	return buildFrame(ui, page, title, hint, true);
}

MenuPageFrame MenuPage_BuildBars(UiScope& ui, GuiOverlay& page, const char * title, std::function<std::string()> hint)
{
	return buildFrame(ui, page, title, hint, false);
}
