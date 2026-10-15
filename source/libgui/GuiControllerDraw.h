/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiControllerDraw.h
 *
 * The pure geometry behind GuiControllerView: how a controller and its
 * controls look in each state. No elements, no platform; tested on a host.
 ***************************************************************************/
#pragma once
#include "GuiPaint.h"
#include "GuiTheme.h"
#include "GuiControllerLayout.h"

//!The look a control is drawn in.
//!\ingroup grp_controller
enum class CTL_STATE {
	NORMAL,
	LIT,				//!< Pressed
	HIGHLIGHT,			//!< Being edited / pointed at
	LIT_HIGHLIGHT
};

//!Builds the resting drawing of a controller (silhouette, then every decorative control) in local
//!coordinates, with the layout multiplied by `scale`. The silhouette's outline is made by drawing each
//!piece's outline first and the fills after, so overlapping pieces merge into one shape.
//!\ingroup grp_controller
void GuiBuildControllerBody(GuiMesh& mesh, const GuiControllerLayout& layout, float scale);

//!Builds one control in one state (an analog stick's cap is separate, see GuiBuildControllerStickCap).
//!\ingroup grp_controller
void GuiBuildControllerSpot(GuiMesh& mesh, const GuiControlSpot& spot, CTL_STATE state, float scale);

//!Builds an analog stick's cap, displaced by (x, y) (each -1 to 1) from the middle of its ring.
//!\ingroup grp_controller
void GuiBuildControllerStickCap(GuiMesh& mesh, const GuiControlSpot& spot, float x, float y, bool lit, float scale);

//!Builds the whole controller as thin lines only, no fills and no theme: the silhouette and every control
//!(a stick with its cap at rest) stroked centered on the outline in one color.
//!\param width Line width in drawing units (not multiplied by `scale`)
//!\ingroup grp_controller
void GuiBuildControllerOutline(GuiMesh& mesh, const GuiControllerLayout& layout, float scale, float width, PixelColor color);
