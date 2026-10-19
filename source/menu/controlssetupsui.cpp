/****************************************************************************
 * DOSBox Wii Home Menu
 * Daryl Borth 2009-2026
 * controlssetupsui.cpp
 *
 * Loading and saving setups from the menu: the lists behind the Setups
 * button (what to do, which setup, what to tie it to) and the flow that asks
 * for a name and a program, over whichever screen asked.
 ***************************************************************************/
#include "controlssetupsui.h"
#include <stdio.h>
#include "menu.h"
#include "menupage.h"
#include "controlssetups.h"

// ---- the lists

std::vector<PickerItem> Picker_SetupMenuItems(std::vector<SetupAction>& kinds)
{
	ControlsModel& m = Controls();
	std::vector<PickerItem> items;
	kinds.clear();
	const size_t saved = m.setups().size();
	// Load is offered only when there is something to load: a row that just closes the popover reads as broken
	if(saved > 0)
	{
		PickerItem it;
		it.name = "Load a setup...";
		it.current = false;
		it.lines.push_back(saved == 1 ? std::string("1 setup saved.") : GuiFormat("%d setups saved.", (int)saved));
		it.lines.push_back("Replaces every player's buttons");
		it.lines.push_back("and what DOS sees.");
		items.push_back(it);
		kinds.push_back(SetupAction::LOAD);
	}
	if(!m.setupName().empty())
	{
		PickerItem it;
		it.name = GuiFormat("Save as %s", Setup_ClipName(m.setupName(), 16).c_str());
		it.current = false;
		it.lines.push_back("Saves every player's buttons and what");
		it.lines.push_back("DOS sees, replacing what was saved.");
		items.push_back(it);
		kinds.push_back(SetupAction::SAVE_CURRENT);
	}
	{
		PickerItem it;
		it.name = "Save as a new setup...";
		it.current = false;
		if(saved == 0)
		{
			it.lines.push_back("Nothing is saved yet. Saves every player's");
			it.lines.push_back("buttons and what DOS sees under a name,");
			it.lines.push_back("so you can load it here next time.");
		}
		else
		{
			it.lines.push_back("Asks for a name, then the program it");
			it.lines.push_back("loads for. Saves every player's buttons");
			it.lines.push_back("and what DOS sees.");
		}
		items.push_back(it);
		kinds.push_back(SetupAction::SAVE_NEW);
	}
	return items;
}

std::vector<PickerItem> Picker_SetupItems(std::vector<int>& index)
{
	ControlsModel& m = Controls();
	const std::vector<SetupInfo> setups = m.setups();
	std::vector<PickerItem> items;
	index.clear();
	for(size_t i = 0; i < setups.size(); i++)
	{
		PickerItem it;
		it.name = setups[i].name;
		it.current = false;
		it.lines.push_back(setups[i].program.empty() ? std::string("Not tied to a program.") : "Loads for " + setups[i].program + ".");
		PickerRow dos; dos.label = "DOS sees"; dos.value = setups[i].emulated;
		PickerRow preset; preset.label = "Preset"; preset.value = setups[i].preset;
		it.rows.push_back(dos);
		it.rows.push_back(preset);
		items.push_back(it);
		index.push_back((int)i);
	}
	return items;
}

std::vector<PickerItem> Picker_AttachItems(const std::string& running, std::vector<AttachChoice>& kinds)
{
	std::vector<PickerItem> items;
	kinds.clear();
	if(!running.empty())
	{
		PickerItem it;
		it.name = running;
		it.current = true;
		MenuPopover_Wrap(it.lines, "These buttons load by themselves when " + running + " starts.", 44);
		items.push_back(it);
		kinds.push_back(AttachChoice::RUNNING);
	}
	{
		PickerItem it;
		it.name = "No program";
		it.current = running.empty();
		MenuPopover_Wrap(it.lines, "Load it yourself from the Load button.", 44);
		items.push_back(it);
		kinds.push_back(AttachChoice::NONE);
	}
	{
		PickerItem it;
		it.name = "Type a name";
		it.current = false;
		MenuPopover_Wrap(it.lines, "For a program that is not running now.", 44);
		items.push_back(it);
		kinds.push_back(AttachChoice::TYPED);
	}
	return items;
}

// ---- the flows

namespace {

// Load: choose a saved setup. What the hint line should say after.
std::string LoadFlow(const GuiPopoverAnchor& anchor)
{
	std::vector<int> index;
	const int chosen = MenuPopover_Pick("Load a setup", Picker_SetupItems(index), anchor);
	if(chosen < 0)
		return std::string();
	const std::vector<SetupInfo> setups = Controls().setups();
	if(!Controls().loadSetup(index[(size_t)chosen]))
		return "That setup could not be loaded";
	return "Loaded " + setups[(size_t)index[(size_t)chosen]].name;
}

// Save: a name, what program it is for, and a check before replacing one of the same name.
std::string SaveFlow(const ScreenHooks& screen, const GuiPopoverAnchor& anchor)
{
	ControlsModel& m = Controls();
	const std::string running = Setup_CleanProgram(m.runningProgram());

	char typed[64];
	snprintf(typed, sizeof(typed), "%s", Setup_SuggestedName(running).c_str());
	screen.hide();	// the on-screen keyboard is a screen of its own
	const bool named = OnScreenKeyboard(typed, sizeof(typed));
	screen.show();
	if(!named)
		return std::string();
	const std::string name = Setup_CleanName(typed);
	if(name.empty())
		return "That name can't be used";

	std::vector<AttachChoice> kinds;
	const int attach = MenuPopover_Pick("Load these buttons for", Picker_AttachItems(running, kinds), anchor);
	if(attach < 0)
		return std::string();
	std::string program;
	if(kinds[(size_t)attach] == AttachChoice::RUNNING)
		program = running;
	else if(kinds[(size_t)attach] == AttachChoice::TYPED)
	{
		char prog[64] = { 0 };
		screen.hide();
		const bool typedOk = OnScreenKeyboard(prog, sizeof(prog));
		screen.show();
		if(!typedOk)
			return std::string();
		program = Setup_CleanProgram(prog);
	}

	const std::vector<SetupInfo> existing = m.setups();
	for(size_t i = 0; i < existing.size(); i++)
	{
		if(Setup_SameName(existing[i].name, name))
		{
			std::vector<std::string> answers;
			answers.push_back("Replace it");
			answers.push_back("Cancel");
			if(MenuPopover_Ask(GuiFormat("%s already exists", name.c_str()).c_str(), answers, "Replacing it loses what was saved under that name.", anchor) != 0)
				return std::string();
			break;
		}
	}
	return m.saveSetup(name, program) ? "Saved " + name : std::string("Could not save the setup");
}

} // namespace

// Saves over the setup that is loaded: no questions, it is the same name and the same program.
std::string Setups_SaveCurrent()
{
	ControlsModel& m = Controls();
	const std::string name = m.setupName();
	std::string program;
	const std::vector<SetupInfo> existing = m.setups();
	for(size_t i = 0; i < existing.size(); i++)
		if(Setup_SameName(existing[i].name, name))
			program = existing[i].program;
	return m.saveSetup(name, program) ? "Saved " + name : std::string("Could not save the setup");
}

// The Setup button: load a setup, or save this one. What the hint line should say after.
std::string Setups_Run(const ScreenHooks& screen, const GuiPopoverAnchor& anchor)
{
	std::vector<SetupAction> kinds;
	const int chosen = MenuPopover_Pick(Controls().setups().empty() ? "Save a setup" : "Load or save a setup", Picker_SetupMenuItems(kinds), anchor);
	if(chosen < 0)
		return std::string();
	switch(kinds[(size_t)chosen])
	{
		case SetupAction::LOAD:
			return Controls().setups().empty() ? std::string("No saved setups yet.  Save one first") : LoadFlow(anchor);
		case SetupAction::SAVE_CURRENT:
			return Setups_SaveCurrent();
		default:
			return SaveFlow(screen, anchor);
	}
}

std::string Controls_SetupMenu(GuiWindow * home, const GuiPopoverAnchor& anchor)
{
	// the home screen stays drawn (dimmed) under the popover; only the on-screen keyboard takes it away
	home->setState(STATE::DISABLED);
	const ScreenHooks hooks = { [home]{ mainWindow->remove(home); }, [home]{ mainWindow->append(home); } };
	const std::string said = Setups_Run(hooks, anchor);
	home->setState(STATE::DEFAULT);
	mainWindow->changeFocus(home);
	home->resetState();
	return said;
}
