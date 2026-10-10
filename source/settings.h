/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * settings.h
 *
 * The settings registry: a curated table of the DOSBox settings the menu
 * shows, how each one is displayed, and how a change is applied to the
 * running core.
 *
 * This header is deliberately plain C++: no DOSBox and no libgui types, so
 * menu.cpp can use it without pulling either into the other. Everything
 * returned is formatted text; the menu never sees a Property or a Section.
 ***************************************************************************/

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stddef.h>

//! The settings pages. More are added here (and to the page table in
//! settings.cpp) as they are built.
enum SettingsPage
{
	SETTINGS_PAGE_PERFORMANCE = 0,
	SETTINGS_PAGE_VIDEO,
	SETTINGS_PAGE_AUDIO,
	SETTINGS_PAGE_COUNT
};

//! Page numbers below SETTINGS_PAGE_COUNT are the curated pages above. The
//! all-settings editor follows them: one page per config section that has
//! something to show, numbered SETTINGS_PAGE_COUNT onwards (see
//! Settings_AllPage). Everything below takes either kind of page number.

//! How many sections the all-settings editor has pages for.
int Settings_AllSectionCount();

//! The page number of the all-settings page for the n'th section, in
//! 0 .. Settings_AllSectionCount()-1.
static inline int Settings_AllPage(int index) { return SETTINGS_PAGE_COUNT + index; }

//! Page heading, eg. "Performance". For an all-settings page, the section's
//! name as shown to the user.
const char * Settings_PageTitle(int page);

//! Number of rows on a page. Row indices are 0 .. count-1 with no gaps. A row
//! the platform cannot honour (scanlines on a mode that has none) is not
//! counted, so the same page can have fewer rows on another console.
int Settings_RowCount(int page);

//! The row's name. On an all-settings page it is the property's name in
//! dosbox.conf.
const char * Settings_RowLabel(int page, int row);

/**
 * The row's current value, as shown to the user. A row that cannot be
 * changed right now says why in the text (eg. a setting that is only
 * available at the DOS prompt), and a row that does not apply shows "-".
 * Always reads the live state, so call it again after any change.
 */
void Settings_RowValue(int page, int row, char * buf, size_t size);

/**
 * One line of help for the row, taken from DOSBox's own help text for the
 * property (newlines collapsed) unless the row overrides it. A row that is
 * locked right now starts with the reason.
 */
void Settings_RowHelp(int page, int row, char * buf, size_t size);

/**
 * Moves the row to its next (direction > 0) or previous (direction < 0)
 * value and applies it to the running core. Wraps at the ends.
 *
 * @return true if the value changed. False if the row is locked right now,
 *         does not apply, or has no other value it may take.
 */
bool Settings_RowStep(int page, int row, int direction);

/**
 * Saves the running config to dosbox.conf (see ConfigSave in configsave.h
 * for how the file is replaced). First the values the home screen's +/-
 * buttons left running (cycles, frameskip) are put into the config, so the
 * file holds what the pages show. Takes no re-initialisation: nothing runs
 * differently afterwards.
 *
 * @param message  always set: one line for the user, where it went or why not
 * @return true if it was saved
 */
bool Settings_Save(char * message, size_t size);

#endif
