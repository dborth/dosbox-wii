/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menupage.h
 *
 * The frame every full-screen menu page shares. The header and footer bars
 * belong to the main window and stay up while pages come and go (see
 * HomeMenu); a page is a view the size of the screen that puts its title on
 * the header, a hint line and its buttons on the footer, and floats its
 * content between them:
 *
 *   header   title                                    [page actions]
 *   body     content, centered, inside the overscan margins
 *   footer   hint line (what the highlighted control does)  [buttons]
 *
 * Nothing but cards, panels and the bars themselves should sit on the page:
 * what is behind the body is the paused game, which can be any color.
 ***************************************************************************/
#ifndef _MENUPAGE_H_
#define _MENUPAGE_H_

#include <string>
#include <functional>
#include "libgui/Gui.h"

/** Space kept clear at each side of the screen, for TVs that crop the picture. */
#define MENU_PAGE_MARGIN	44
/** Width of the body between the margins. */
#define MENU_PAGE_BODY_W	(640 - 2 * MENU_PAGE_MARGIN)
/** Font size of the page title and of the hint line. */
#define MENU_PAGE_TITLE_FONT	28
#define MENU_PAGE_HINT_FONT		16

/** The parts of a page that the caller fills. All are owned by the UiScope. */
struct MenuPageFrame {
	GuiLayout * headerLeft;		/**< Right after the title */
	GuiLayout * headerRight;	/**< Right end of the header: page-wide controls */
	GuiLayout * body;			/**< The area between the bars; children are centered */
	GuiLayout * content;		/**< Inside the one card that sits on the body: a column of sections */
	GuiLayout * footerRight;	/**< Right end of the footer: Back and friends */
	GuiLabel * title;			/**< The page title, white on the header */
	GuiLabel * hint;			/**< Left end of the footer; bound to `hint` */
	int bodyHeight;				/**< Height of the body in design pixels */
};

/**
 * Fills `page` (made a screen-sized overlay) with the header, body and footer.
 * @param ui The page's scope
 * @param page The view being built; its size and position are set here
 * @param title The page title
 * @param hint Returns the hint line, read every frame
 */
MenuPageFrame MenuPage_Build(UiScope& ui, GuiOverlay& page, const char * title, std::function<std::string()> hint);

/** Width of the content inside the page card. */
#define MENU_PAGE_CONTENT_W	(MENU_PAGE_BODY_W - 2 * MENU_PAGE_CARD_PAD)
#define MENU_PAGE_CARD_PAD	8

/** A flat tinted section for the page card (a player, a group of buttons): `w` wide, as tall as its content. */
GuiCard& MenuPage_Section(UiScope& ui, int w);

/** A button for the bars and the pages: Back, Reset. */
GuiTextButton& MenuPage_Button(UiScope& ui, const char * text, int width = 104);

#endif
