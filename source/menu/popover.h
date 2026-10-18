/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * popover.h
 *
 * The mechanics of a question asked in place: a panel that opens beside the
 * control that asked it, over the screen it came from. Where it goes
 * (Popover_Place), the little tail that points at its anchor (Popover_Tail),
 * and the backdrop that closes it (PopoverBackdrop, Popover_Mount). Nothing
 * here knows how the menu looks: the space it must leave at the screen's
 * edges and the tail's colour are given. The menu's look is menupopover.h.
 *
 * Needs only libgui (and GuiArrow, for the tail).
 ***************************************************************************/
#ifndef _POPOVER_H_
#define _POPOVER_H_

#include "libgui/Gui.h"

/** The control a popover opens beside, in canvas pixels. */
struct PopoverAnchor {
	/** Which way the panel lines up: with the anchor's nearer edge, or against a margin (to leave the other side of the screen clear). */
	enum Edge { AUTO, LEFT_MARGIN, RIGHT_MARGIN };
	int x, y, w, h;
	bool valid;
	Edge edge;
	PopoverAnchor() : x(0), y(0), w(0), h(0), valid(false), edge(AUTO) {}
	PopoverAnchor(int ax, int ay, int aw, int ah, Edge e = AUTO) : x(ax), y(ay), w(aw), h(ah), valid(true), edge(e) {}
};

/** Where the popover's top-left corner goes. */
struct PopoverPlace { int x, y; };

/** What a popover keeps clear at the screen's edges: the bars at the top and bottom, and the margin at both sides. */
struct PopoverInsets { int top, bottom, side; };

/** The most height a popover can have: the band between the bars. */
int Popover_MaxHeight(int screenHeight, const PopoverInsets& insets);

/**
 * Places a popover of `w` x `h` beside its anchor: below it when it fits in the band between the bars, else above,
 * lined up with the anchor's right edge when the anchor is on the right half of the screen and its left edge
 * otherwise, and kept inside the margins. With no anchor it is centred.
 */
PopoverPlace Popover_Place(const PopoverAnchor& anchor, int w, int h, int screenWidth, int screenHeight, const PopoverInsets& insets);

/**
 * Where the little tail on a popover goes, and which way it points (toward the anchor); none when the panel covers the anchor
 * or there is no anchor. `x`,`y` are the tail's top-left corner; `w`,`h` its size.
 */
struct PopoverTail { bool shown; int direction; int x, y, w, h; };
enum { POPOVER_TAIL_UP, POPOVER_TAIL_DOWN, POPOVER_TAIL_LEFT, POPOVER_TAIL_RIGHT };
PopoverTail Popover_Tail(const PopoverAnchor& anchor, const PopoverPlace& at, int panelW, int panelH);

/**
 * Puts a built panel on a popover view: the backdrop behind it (B or a click outside closes: its button is returned),
 * and the panel at `at` on the screen. The tail (when the anchor gets one) is drawn in `tailColor`, the panel's face.
 */
GuiButton& Popover_Mount(UiScope& ui, GuiLayout& view, GuiTrigger& cancel, GuiCard& panel, const PopoverPlace& at, int panelW, int panelH, const PopoverAnchor& anchor, PixelColor tailColor);

/**
 * The invisible button across the whole screen behind a popover: B anywhere, or a click outside the panel, presses
 * it (that is how a popover is dismissed). A click on the panel itself is not a dismissal and is swallowed.
 * It cannot take the D-pad's selection.
 */
class PopoverBackdrop : public GuiButton
{
	public:
		PopoverBackdrop(int w, int h);
		/** Closes the popover as B does (the close button calls this): the backdrop reads as clicked, and stays so. */
		void dismiss();
		/** The panel: a click inside it does not count. */
		void setPanel(GuiElement * panelElement) { panel = panelElement; }
		/** The trigger that means "close" (B), which counts wherever the pointer is. */
		void setCancel(GuiTrigger * t) { cancel = t; }
		/** The trigger whose press, with the pointer off the panel, closes the popover. */
		void setPrimary(GuiTrigger * t) { primary = t; }
		void update(InputController * c) override;
	private:
		GuiElement * panel;
		GuiTrigger * cancel;
		GuiTrigger * primary;
		bool closing;
};

#endif
