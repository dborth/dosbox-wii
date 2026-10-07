/****************************************************************************
 * DOSBox Wii
 * Tantric 2009-2010
 ***************************************************************************/

#ifndef WIIHARDWARE_H
#define WIIHARDWARE_H

#define MAX_APP_DRIVE_LEN		16
#define MAX_APP_PATH_LEN		128

//! Folder on a storage device that holds DOSBox's config and the C: drive.
//! FAT is case insensitive, so an existing "DOSBox" folder is found too.
#define DOSBOX_DIR_NAME			"dosbox"

// NOTE: this header is included by DOSBox core files, so it must not pull
// in drivers/ or libgui/ headers (their LOG() macro collides with DOSBox's).

void WiiInit();
void WiiMenu();
void CreateAppPath(char origpath[]);

//! Sets appDrive (eg. "sd:") to the first mounted storage device that has a
//! DOSBOX_DIR_NAME folder, or failing that the first one it can create the
//! folder on. Leaves appDrive empty if no device is usable. Called by
//! WiiInit(), once the platform is up.
void FindAppDrive();
void WiiFinished();

//! Leaves the app if the platform reports a shutdown request. Does not return then.
void WiiCheckExit();

//! True while a HOME button is held. Reads the state InputHal_Update() scanned.
bool MenuRequested();

extern char appDrive[MAX_APP_DRIVE_LEN];
extern char appPath[MAX_APP_PATH_LEN];
extern char dosboxCommand[1024];

#endif
