/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * drivemap.h
 *
 * The Drives screen's model: the storage devices the platform has, and
 * which of them the user has mounted as DOS drives. Nothing is mounted
 * automatically; a device the driver has mounted is offered, and A on its
 * row mounts it on the first free letter (or unmounts it again).
 *
 * While the screen is open a short-lived thread polls for hot-plug, and
 * rows are re-read as devices come and go. A DOS drive whose device has
 * gone is removed (at the DOS prompt, where nothing can have a file open on
 * it).
 *
 * This header is plain C++, like settings.h: no DOSBox and no libgui types.
 * Everything except the polling thread runs on the main thread.
 ***************************************************************************/

#ifndef _DRIVEMAP_H_
#define _DRIVEMAP_H_

#include <stddef.h>

//! The Drives screen is opening: reads the devices and starts watching for
//! hot-plug. Pair with DriveMap_Close().
void DriveMap_Open();

//! The Drives screen is closing: stops the polling thread and waits for it.
void DriveMap_Close();

//! Call every frame while the screen is open. Cheap. Returns true when
//! what the rows show has changed (a device was plugged in or removed, or
//! one was mounted or unmounted other than by DriveMap_RowAction); the
//! screen then re-reads the rows, rebuilding its list if
//! DriveMap_RowCount() differs.
bool DriveMap_Update();

//! One row per storage device, then a last read-only row listing the DOS
//! drives. Never 0 while the screen is open.
int DriveMap_RowCount();

//! The row's name and its current state, as shown to the user.
void DriveMap_RowText(int row, char * label, size_t labelSize, char * value, size_t valueSize);

//! A line of help for the row: what A will do.
void DriveMap_RowHelp(int row, char * buf, size_t size);

/**
 * Mounts the row's device as a DOS drive, or unmounts it if it is one.
 *
 * @return NULL if it was done, otherwise a line saying why not.
 */
const char * DriveMap_RowAction(int row);

#endif
