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
	SETTINGS_PAGE_AUDIO,
	SETTINGS_PAGE_COUNT
};

//! Page heading, eg. "Performance".
const char * Settings_PageTitle(SettingsPage page);

//! Number of rows on a page. Row indices are 0 .. count-1 with no gaps.
int Settings_RowCount(SettingsPage page);

//! The row's name.
const char * Settings_RowLabel(SettingsPage page, int row);

/**
 * The row's current value, as shown to the user. A row that cannot be
 * changed right now says why in the text (eg. a setting that is only
 * available at the DOS prompt), and a row that does not apply shows "-".
 * Always reads the live state, so call it again after any change.
 */
void Settings_RowValue(SettingsPage page, int row, char * buf, size_t size);

/**
 * One line of help for the row, taken from DOSBox's own help text for the
 * property (newlines collapsed) unless the row overrides it. A row that is
 * locked right now starts with the reason.
 */
void Settings_RowHelp(SettingsPage page, int row, char * buf, size_t size);

/**
 * Moves the row to its next (direction > 0) or previous (direction < 0)
 * value and applies it to the running core. Wraps at the ends.
 *
 * @return true if the value changed. False if the row is locked right now,
 *         does not apply, or has no other value it may take.
 */
bool Settings_RowStep(SettingsPage page, int row, int direction);

#endif
