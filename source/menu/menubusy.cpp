/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * menubusy.cpp
 *
 * See menubusy.h.
 ***************************************************************************/
#include "menubusy.h"
#include "menu.h"
#include "menuart.h"
#include "menupage.h"
#include "menubusypacer.h"
#include "menupopover.h"
#include "menustyle.h"
#include "drivers/Thread.h"
#include "drivers/Time.h"

namespace {

const int kBusyW = 280;
const int kBusyPad = 10;
const int kBusyTitleH = 24;
const int kBusyLineH = 18;
const int kBusyLines = 2;
const int kBusyChars = 38;				// at the detail's size, in the width inside the padding
const uint32_t kStackSize = 64 * 1024;

// What the worker is given, and how it says it is done
struct BusyJob
{
	const std::function<void()> * work;
	Mutex lock;
	bool done;
};

void * busyMain(void * arg)
{
	BusyJob * job = (BusyJob *)arg;
	(*job->work)();
	MutexLock guard(job->lock);
	job->done = true;
	return nullptr;
}

}

MenuBusyView::MenuBusyView(const char * title, const std::string& detail) :
	GuiPopover(GuiPopover::Anchor(), MenuPopover_Insets()), shown(false), spinner(nullptr)
{
	std::vector<std::string> lines;
	MenuPopover_Wrap(lines, detail, kBusyChars);
	if(lines.size() > (size_t)kBusyLines)
	{
		lines.resize(kBusyLines);
		std::string& last = lines.back();
		if(last.size() > 2)
			last.resize(last.size() - 2);
		last += "..";
	}

	const int innerW = kBusyW - 2 * kBusyPad;
	const int panelH = 2 * kBusyPad + kBusyTitleH + 7 + 8 + MENU_SPINNER_SIZE + 6 + kBusyLines * kBusyLineH;

	GuiLabel& heading = ui.label(title).fontSize(UI_FONT_SMALL).muted();
	spinner = &ui.make<MenuSpinner>();

	GuiLayout& content = ui.column().size(innerW, panelH - 2 * kBusyPad)
		.add(ui.row().size(innerW, kBusyTitleH).align(LAYOUT_ALIGN::CENTRE).add(heading))
		.add(MenuPopover_Divider(ui, innerW))
		.addGap(8)
		.add(ui.row().size(innerW, MENU_SPINNER_SIZE).align(LAYOUT_ALIGN::CENTRE).addSpace().add(*spinner).addSpace())
		.addGap(6);
	for(int i = 0; i < kBusyLines; i++)
	{
		if((size_t)i < lines.size())
			content.add(ui.row().size(innerW, kBusyLineH).align(LAYOUT_ALIGN::CENTRE).addSpace()
				.add(ui.label(lines[(size_t)i].c_str()).fontSize(UI_FONT_SMALL).muted()).addSpace());
		else
			content.addGap(kBusyLineH);
	}

	GuiCard& panel = ui.card().selectable(false).style(MenuPopoverStyle()).corners(GuiCorners(12.0f)).pad(kBusyPad, kBusyPad).size(kBusyW, panelH);
	panel.add(content);
	setPanel(panel, MenuPaletteCurrent().surfaceRaised);
}

void MenuBusyView::setPhase(int phase)
{
	spinner->setPhase(phase);
}

// The screen under is washed the way a disabled screen is (GuiWindow::drawWash), then the panel goes over it
void MenuBusyView::draw()
{
	if(!shown)
		return;
	VideoDriver * video = platform->getVideo();
	video->getImageRenderer()->drawRectangle(0, 0, video->getScreenWidth(), video->getScreenHeight(), (PixelColor){0x00, 0x00, 0x00, 0x80});
	GuiPopover::draw();
}

void MenuBusy_Run(const char * title, const std::string& detail, const std::function<void()>& work)
{
	BusyJob job;
	job.work = &work;
	job.done = false;

	MenuBusyView view(title, detail);
	MenuScreenWindow screen(view);
	// The screen under still holds the focus, which only moves by itself when that screen is removed: hand it over
	// once the view has been laid out, as MenuPopover_Pick does. From here the input is the view's, shown or not.
	UpdateGui();
	mainWindow->changeFocus(&screen.window());

	Thread worker;
	if(!worker.start(busyMain, &job, kStackSize, ThreadPriority::Low))
	{
		// no thread to be had: the work is done here, and the menu waits without the panel
		work();
		return;
	}

	const Ticks begin = SystemTime::now();
	MenuBusyPacer pacer;

	while(true)
	{
		UpdateGui();

		// B and a click outside the panel mean "close", and nothing here can be closed
		if(view.isDismissed())
			view.reopen();

		bool done;
		{
			MutexLock guard(job.lock);
			done = job.done;
		}

		pacer.update(SystemTime::diffMillisecs(begin, SystemTime::now()), done);
		view.setShown(pacer.shown());
		view.setPhase(pacer.phase());

		if(pacer.over())
			break;
	}

	worker.join();
}
