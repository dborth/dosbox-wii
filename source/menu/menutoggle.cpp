/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2026
 * menutoggle.cpp
 ***************************************************************************/
#include "menutoggle.h"
#include "menustyle.h"

namespace {

const int SEGMENT_H = 28;
const int INSET = 3;
const int GAP = 2;
const int LABEL_FONT = 15;

const PixelColor OnText() { return MenuRGBA(255, 255, 255, 255); }
const PixelColor OffText() { return MenuPaletteCurrent().text; }

} // namespace

MenuToggle::MenuToggle(UiScope& ui, const std::vector<std::string>& names, int selected, int segmentWidth, std::function<void(int)> onChange)
	: capsule(NULL), changed(onChange), on(selected)
{
	const int count = (int)names.size();
	GuiLayout& row = ui.row().gap(GAP).align(LAYOUT_ALIGN::CENTRE);

	for(int i = 0; i < count; i++)
	{
		GuiLabel& label = ui.label(names[(size_t)i].c_str()).fontSize(LABEL_FONT).textColor(OffText());
		GuiCard& segment = ui.card().size(segmentWidth, SEGMENT_H).style(MenuToggleSegmentStyle(false))
			.corners(GuiCorners(SEGMENT_H / 2.0f)).pad(0, 0)
			.onClick([this, i]{ select(i); if(changed) changed(i); });
		segment.add(label, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));
		row.add(segment);
		segments.push_back(&segment);
		labels.push_back(&label);
	}

	const int width = count * segmentWidth + (count - 1) * GAP + 2 * INSET;
	const int height = SEGMENT_H + 2 * INSET;
	capsule = &ui.card().selectable(false).size(width, height).style(MenuToggleCapsuleStyle())
		.corners(GuiCorners(height / 2.0f)).pad(INSET, INSET);
	capsule->add(row, GuiItem().at(LAYOUT_ALIGN::CENTRE, LAYOUT_ALIGN::CENTRE));

	const int shown = on;
	on = -1;
	select(shown);
}

void MenuToggle::select(int index)
{
	if(index == on)
		return;

	on = index;

	for(size_t i = 0; i < segments.size(); i++)
	{
		const bool lit = (int)i == on;
		segments[i]->style(MenuToggleSegmentStyle(lit));
		labels[i]->textColor(lit ? OnText() : OffText());
	}
}
