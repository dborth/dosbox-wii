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
#include "libgui/GuiGlyph.h"
#include "libgui/GuiControllerLayout.h"
#include "controlsmodel.h"

/** One row in a list on the configuration screen. */
struct ProfileRow {
	int binding;			/**< Id the model stores its target under (a control id, plus BIND_STICK_CLICK for a stick click). */
	int control;			/**< Control on the drawing to highlight. */
	GLYPH glyph;			/**< Icon beside the name. */
	const char * letter;	/**< Letter on the icon ("L", "ZL", ""). */
	const char * name;		/**< "Left Bumper" */
	int stick;				/**< 1 or 2 for a stick's movement (left, right): shown, not changed; it is the DOS joystick's axes. */
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

/** What differs between two players holding the same kind of hardware. */
struct ProfileOptions {
	bool sideways;		/**< A lone Wiimote held sideways: its own drawing, with the D-pad directions as the player holds it. Ignored by every other kind. */
	bool stickClicks;	/**< The driver reports L3 / R3 for this pad (the Wii U drivers do for the Pro and the GamePad; the Wii one never does). Without it the click rows are left out rather than listed dead. */

	ProfileOptions() : sideways(false), stickClicks(false) {}
};

/**
 * The profile for a kind of hardware. Every profile comes from the same builder (controllerprofile.cpp): a kind is a
 * layout, a size, the controls to list where, and any names that differ from the Wii U Pro's. A slot with nothing
 * connected gets the Pro-style profile.
 */
ControllerProfile Controllers_Profile(PadType type, const ProfileOptions& options = ProfileOptions());

/**
 * The same for a player slot, taking the orientation and the click support from what the model reports. `shown` is
 * which of the player's devices to draw (an index into PlayerInfo::devices); the primary one when it is out of range
 * or the player lists none.
 */
ControllerProfile Controllers_Profile(const PlayerInfo& player, int shown = 0);

#endif
