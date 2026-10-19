/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiPopover.h
 *
 * A question asked in place: a panel that opens beside the control that asked
 * it, over the screen it came from. GuiPopover is the full-screen overlay that
 * holds it. It puts the panel where GuiPopoverPlace() says, draws the little
 * tail that points at the anchor, and closes on B or on a click outside the
 * panel. What is on the panel, and how it looks, is up to the caller: build a
 * GuiCard, then mount() it.
 *
 *   class Ask : public GuiPopover {
 *       public:
 *           Ask(const GuiPopoverAnchor& anchor) {
 *               GuiCard& panel = ui.card().selectable(false).size(280, 160).add(...);
 *               mount(panel, 280, 160, anchor, insets);
 *               ui.attachTo(*this);
 *           }
 *   };
 *
 * The caller runs it like any other screen and watches backButton() for the
 * dismissal.
 ***************************************************************************/
#pragma once

#include "GuiPopoverPlacement.h"

//!The invisible button across the whole screen behind a popover: B anywhere, or a click outside the panel, presses
//!it (that is how a popover is dismissed). A click on the panel itself is not a dismissal and is swallowed.
//!It cannot take the D-pad's selection.
//!\ingroup grp_layout
class GuiPopoverBackdrop : public GuiButton
{
	public:
		GuiPopoverBackdrop(int w, int h);
		//!Closes the popover as B does (a close button calls this): the backdrop reads as clicked, and stays so
		void dismiss();
		//!The panel: a click inside it does not count
		void setPanel(GuiElement * panelElement) { panel = panelElement; }
		//!The trigger that means "close" (B), which counts wherever the pointer is
		void setCancel(GuiTrigger * t) { cancel = t; }
		//!The trigger whose press, with the pointer off the panel, closes the popover
		void setPrimary(GuiTrigger * t) { primary = t; }
		void update(InputController * c) override;
	private:
		GuiElement * panel;
		GuiTrigger * cancel;
		GuiTrigger * primary;
		bool closing;
};

//!A panel beside the control that asked, over the screen it came from. Derive from it (or hold one), build the panel
//!in `ui`, and mount() it; the popover is as large as the screen and takes its place in the page it covers.
//!\ingroup grp_layout
class GuiPopover : public GuiOverlay
{
	public:
		GuiPopover();
		//!Puts the built `panel` (a card of `panelW` x `panelH`) beside `anchor`, with the backdrop behind it and the tail
		//!pointing at the anchor, drawn in `tailColor` (the panel's face). `insets` is what to keep clear at the screen's edges.
		//!Call once, before attaching `ui`.
		void mount(GuiCard& panel, int panelW, int panelH, const GuiPopoverAnchor& anchor, const GuiPopoverInsets& insets, PixelColor tailColor);
		//!The same, with the tail in the theme's surfaceRaised
		void mount(GuiCard& panel, int panelW, int panelH, const GuiPopoverAnchor& anchor, const GuiPopoverInsets& insets);
		//!Closes the popover as B does: backButton() reads as clicked
		void dismiss();
		//!Pressed when the popover is dismissed (B anywhere, or a click outside the panel). Valid after mount().
		GuiButton& backButton() { return *backdrop; }
		//!Where the panel went (its top-left corner, in canvas pixels). Valid after mount().
		const GuiPopoverPosition& position() const { return placed; }
	protected:
		UiScope ui;	//!< Owns the panel and everything else the popover is made of
	private:
		GuiTrigger * cancelTrigger;
		GuiPopoverBackdrop * backdrop;
		GuiPopoverPosition placed;
};
