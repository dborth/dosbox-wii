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

#include <stddef.h>

#define MAX_APP_DRIVE_LEN		16

//! Folder on a storage device that holds DOSBox's config and the C: drive.
//! FAT is case insensitive, so an existing "DOSBox" folder is found too.
#define DOSBOX_DIR_NAME			"DOSBox"

//! Sets appDrive (eg. "sd:") to the first mounted storage device that has a
//! DOSBOX_DIR_NAME folder, or failing that the first one it can create the
//! folder on. Leaves appDrive empty if no device is usable. Called once the
//! platform is up.
void FindAppDrive();

//! Mounts a folder as DOS drive letter (eg. 'D') with the standard fake
//! hard disk geometry. label is the DOS volume label; NULL gives "D_DRIVE".
//! Returns true if the drive is now mounted. Fails (leaving the letter alone)
//! if the letter is taken or the path is not a folder.
bool MountDOSDrive(char DriveLetter, const char *path, const char *label);

//! Unmounts a DOS drive the way MOUNT -u does (drive table, media byte,
//! current drive moved off it). Returns true if the letter is now free.
//! Only call this at the DOS prompt: a running program may hold files open
//! on the drive.
bool UnmountDOSDrive(char DriveLetter);

//! What a DOS drive letter is mounted from (eg. "sd:/"). Returns false if
//! nothing is mounted on the letter.
bool GetDOSDriveInfo(char DriveLetter, char * info, size_t size);

//! True if the letter has a drive mounted on it.
bool IsDOSDriveMounted(char DriveLetter);

int MountDOSBoxDir(char DriveLetter, const char *path);

extern char appDrive[MAX_APP_DRIVE_LEN];

#endif
