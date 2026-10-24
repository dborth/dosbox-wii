/****************************************************************************
 * libgui
 *
 * Daryl Borth 2009-2026
 * GuiCardBrowser.h
 ***************************************************************************/
#pragma once

#include "libgui/Gui.h"

#include <string>

#define CARD_PAGESIZE 5

//!What one row of a GuiCardBrowser shows. Filled in by the GuiCardSource when the row is on screen.
struct GuiCardInfo
{
	//!How the row looks: a card, a card for something not added yet, or plain text between cards
	enum Kind { CARD, DASHED, HEADER, NOTE };

	Kind kind = CARD;
	std::string tile;	//!< The one character on the row's tile ("K", "+"); empty for no tile
	std::string title;	//!< The row's name
	std::string detail;	//!< A smaller line under the name
	std::string action;	//!< Right-aligned: what A does here, or a status ("Restart", "RUNNING")
	bool accent = false;	//!< The tile (and the action) take the accent colour; otherwise they are neutral
};

//!Where a GuiCardBrowser gets its rows from. Asked only for the rows that are on screen, so the list can be as long as it likes.
class GuiCardSource
{
	public:
		virtual ~GuiCardSource() {}
		virtual int cardCount() = 0;
		virtual void card(int index, GuiCardInfo& out) = 0;
};

class GuiCardTile;

//!A scrolling list of cards: a tile, a title with a line under it, and a word at the right. The
//!pad, pointer and scrollbar work as in GuiOptionBrowser, which this is the taller-row version of.
//!\ingroup grp_core
class GuiCardBrowser : public GuiElement
{
	public:
		//!\param w Width
		//!\param h Height
		//!\param s Where the rows come from - must outlive this GuiCardBrowser
		GuiCardBrowser(int w, int h, GuiCardSource * s);
		~GuiCardBrowser();
		//!\return the row that was clicked this frame, or -1 if none was
		int getClickedOption();
		//!\return the row the highlight is on, or -1 before the first frame
		int getSelectedOption() const;
		void resetState();
		void setFocus(int f);
		void draw() override;
		//!Reads the rows again: after the source's rows changed, or the text of one did
		void triggerUpdate();
		//!Reads the rows now if they have changed (update() does this each frame; this is for a caller with no controller to update with)
		void readRows();
		void resetText();
		void update(InputController * c);

	protected:
		//!\cond INTERNAL
		int optionIndex[CARD_PAGESIZE];
		GuiCardInfo::Kind rowKind[CARD_PAGESIZE];
		GuiButton * optionBtn[CARD_PAGESIZE];
		GuiText * titleTxt[CARD_PAGESIZE];
		GuiText * detailTxt[CARD_PAGESIZE];
		GuiText * actionTxt[CARD_PAGESIZE];
		GuiShape * optionBg[CARD_PAGESIZE];
		GuiCardTile * tile[CARD_PAGESIZE];
		int selectedItem;
		int listOffset;
		GuiCardSource * source;
		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;
		GuiShape * cardShape;
		GuiShape * trackShape;
		GuiShape * thumbShape;
		GuiButton * thumbBtn;
		GuiShape * arrowUpShape;
		GuiShape * arrowDownShape;
		GuiShape * arrowUpGlyph;
		GuiShape * arrowDownGlyph;
		GuiShapeStyle cardStyle, dashedStyle, plainStyle;
		int rowWidth;
		int trackHeight;
		int thumbHeight;
		int thumbTravel;
		int dragPos;
		bool dragging;
		int count;			// rows in the source, as of the last update
		void updateThumb();
		void showRow(int slot, int index);
		GuiTrigger * trigA;
		GuiTrigger * trigHeldA;
		bool listChanged;
		//!\endcond
};
