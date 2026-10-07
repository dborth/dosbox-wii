/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * fileop.h
 *
 * File operations: where DOSBox Wii keeps its files on the console's
 * storage devices.
 *
 * NOTE: this header is included by DOSBox core files, so it must not pull
 * in drivers/ or libgui/ headers (their LOG() macro collides with DOSBox's).
 ***************************************************************************/

#ifndef _FILEOP_H_
#define _FILEOP_H_

#define MAX_APP_DRIVE_LEN		16

//! Folder on a storage device that holds DOSBox's config and the C: drive.
//! FAT is case insensitive, so an existing "DOSBox" folder is found too.
#define DOSBOX_DIR_NAME			"DOSBox"

//! Sets appDrive (eg. "sd:") to the first mounted storage device that has a
//! DOSBOX_DIR_NAME folder, or failing that the first one it can create the
//! folder on. Leaves appDrive empty if no device is usable. Called once the
//! platform is up.
void FindAppDrive();

int MountDOSBoxDir(char DriveLetter, const char *path);

extern char appDrive[MAX_APP_DRIVE_LEN];

#endif
