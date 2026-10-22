/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2026
 * menutoggle.h
 *
 * A toggle pill: two or more labels in a capsule, one of them on (Load | Save). Any of them is chosen by
 * pointing and clicking, or by moving to it and pressing A. For a choice between a few things that is
 * always on show, where a popover would hide it.
 ***************************************************************************/
#ifndef _MENUTOGGLE_H_
#define _MENUTOGGLE_H_

#include <functional>
#include <string>
#include <vector>

#include "libgui/Gui.h"
#include "libgui/GuiUi.h"

class MenuToggle
{
	public:
		/**
		 * \param ui The scope everything is made in (the pill lives as long as it does)
		 * \param labels One per segment
		 * \param selected The segment that is on to begin with
		 * \param segmentWidth Each segment's width, in design pixels
		 * \param onChange Called with the segment's number when one is chosen (not when select() sets it)
		 */
		MenuToggle(UiScope& ui, const std::vector<std::string>& labels, int selected, int segmentWidth, std::function<void(int)> onChange);

		/** The pill: add it to a layout */
		GuiCard& element() { return *capsule; }

		int selected() const { return on; }

		/** Turns a segment on, without calling onChange (the screen already knows) */
		void select(int index);

	private:
		GuiCard * capsule;
		std::vector<GuiCard *> segments;
		std::vector<GuiLabel *> labels;
		std::function<void(int)> changed;
		int on;
};

#endif
