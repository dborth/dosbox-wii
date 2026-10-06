/****************************************************************************
 * DOSBox Wii
 * Tantric 2009-2010
 ***************************************************************************/

#ifndef WIIHARDWARE_H
#define WIIHARDWARE_H

#define MAX_APP_DRIVE_LEN		16
#define MAX_APP_PATH_LEN		128

// NOTE: this header is included by DOSBox core files, so it must not pull
// in drivers/ or libgui/ headers (their LOG() macro collides with DOSBox's).

void WiiInit();
void WiiMenu();
void CreateAppPath(char origpath[]);
void WiiFinished();

//! Leaves the app if the platform reports a shutdown request. Does not return then.
void WiiCheckExit();

//! True while a HOME button is held. Reads the state InputHal_Update() scanned.
bool MenuRequested();

extern char appDrive[MAX_APP_DRIVE_LEN];
extern char appPath[MAX_APP_PATH_LEN];
extern char dosboxCommand[1024];

#endif
