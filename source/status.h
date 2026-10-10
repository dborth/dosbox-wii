/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * status.h
 *
 * The Status page's model: what DOSBox is set to and what it is actually
 * doing, at a glance. Read-only.
 *
 * Every line shows the value as configured and, in brackets, the value in
 * effect when that is something else (a core of "auto" that resolved to the
 * dynamic recompiler, a Sound Blaster 16 that fell back to a Pro 2, the
 * cycle count the home screen's +/- buttons moved to). A trailing " *" marks
 * a setting that is not the default.
 *
 * Like settings.h this is plain C++ with no DOSBox and no libgui types: the
 * menu only ever sees formatted text.
 ***************************************************************************/

#ifndef _STATUS_H_
#define _STATUS_H_

#include <stddef.h>

//! Reads the state again and rebuilds every line. Call when the page opens;
//! the accessors below describe the state as of the last call. The number of
//! rows can change between calls (controllers, drives).
void Status_Refresh();

//! Number of rows. Group headings are rows too (with no value and no help).
int Status_RowCount();

//! The row's name. Valid until the next Status_Refresh().
const char * Status_RowLabel(int row);

//! The row's value as shown to the user, at most size-1 characters.
void Status_RowValue(int row, char * buf, size_t size);

//! One line of help for the row, at most size-1 characters.
void Status_RowHelp(int row, char * buf, size_t size);

#endif
