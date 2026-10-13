/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controllerprofile.h
 *
 * What the configuration screen lists for a kind of controller: which drawing
 * to use, and the controls to list beside it and under it. A profile is data,
 * so supporting another controller means adding a profile (and a drawing in
 * libgui's GuiControllerLayout form), not touching the screen.
 ***************************************************************************/
#ifndef _CONTROLLERPROFILE_H_
#define _CONTROLLERPROFILE_H_

#include <vector>
#include "libgui/Gui.h"
#include "controlsmodel.h"

/** One row in a list on the configuration screen. */
struct ProfileRow {
	int binding;			/**< Id the model stores its target under (a control id, plus BIND_STICK_CLICK for a stick click). */
	int control;			/**< Control on the drawing to highlight. */
	GLYPH glyph;			/**< Icon beside the name. */
	const char * letter;	/**< Letter on the icon ("L", "ZL", ""). */
	const char * name;		/**< "Left Bumper" */
	bool stickMode;			/**< The target is a stick's mode, not a button action. */
	const char * fixed;		/**< If set, the control cannot be changed and this is shown instead. */
};

/** A titled column of rows under the controller. */
struct ProfileGroup {
	const char * title;
	std::vector<ProfileRow> rows;
};

/** A controller's drawing and the controls to list for it. */
struct ControllerProfile {
	const char * name;
	const GuiControllerLayout * layout;
	float scale;						/**< Size of the drawing on the configuration screen. */
	std::vector<ProfileRow> left;		/**< Listed to the left of the drawing, top to bottom. */
	std::vector<ProfileRow> right;		/**< Listed to the right. */
	std::vector<ProfileGroup> groups;	/**< Listed underneath, left to right. */
};

/** The profile for a kind of hardware. Hardware without a profile of its own gets the Pro-style one. */
const ControllerProfile& Controllers_Profile(PadType type);

#endif
