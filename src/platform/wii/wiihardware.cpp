#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <zlib.h>
#include <malloc.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>
#include <fat.h>
#include "wiihardware.h"

char rootdir[10];

enum {
	METHOD_AUTO,
	METHOD_SD,
	METHOD_USB,
	METHOD_DVD,
	METHOD_SMB,
	METHOD_MC_SLOTA,
	METHOD_MC_SLOTB,
	METHOD_SD_SLOTA,
	METHOD_SD_SLOTB
};

bool unmountRequired[9] = { false, false, false, false, false, false, false, false, false };
bool isMounted[9] = { false, false, false, false, false, false, false, false, false };

#ifdef HW_RVL
	const DISC_INTERFACE* sd = &__io_wiisd;
	const DISC_INTERFACE* usb = &__io_usbstorage;
#else
	const DISC_INTERFACE* carda = &__io_gcsda;
	const DISC_INTERFACE* cardb = &__io_gcsdb;
#endif

/****************************************************************************
 * UnmountAllFAT
 * Unmounts all FAT devices
 ***************************************************************************/
void UnmountAllFAT()
{
#ifdef HW_RVL
	fatUnmount("sd:/");
	fatUnmount("usb:/");
#else
	fatUnmount("carda:/");
	fatUnmount("cardb:/");
#endif
}

/****************************************************************************
 * MountFAT
 * Checks if the device needs to be (re)mounted
 * If so, unmounts the device
 * Attempts to mount the device specified
 * Sets libfat to use the device by default
 ***************************************************************************/

bool MountFAT(int method)
{
	bool mounted = true; // assume our disc is already mounted
	char name[10];
	const DISC_INTERFACE* disc = NULL;

	switch(method)
	{
#ifdef HW_RVL
		case METHOD_SD:
			sprintf(name, "sd");
			disc = sd;
			break;
		case METHOD_USB:
			sprintf(name, "usb");
			disc = usb;
			break;
#else
		case METHOD_SD_SLOTA:
			sprintf(name, "carda");
			disc = carda;
			break;

		case METHOD_SD_SLOTB:
			sprintf(name, "cardb");
			disc = cardb;
			break;
#endif
		default:
			return false; // unknown device
	}

	sprintf(rootdir, "%s:/", name);

	if(unmountRequired[method])
	{
		unmountRequired[method] = false;
		fatUnmount(rootdir);
		disc->shutdown();
		isMounted[method] = false;
	}
	if(!isMounted[method])
	{
		if(!disc->startup())
			mounted = false;
		else if(!fatMountSimple(name, disc))
			mounted = false;
	}

	isMounted[method] = mounted;
	return mounted;
}

void MountAllFAT()
{
#ifdef HW_RVL
	MountFAT(METHOD_SD);
	MountFAT(METHOD_USB);
#else
	MountFAT(METHOD_SD_SLOTA);
	MountFAT(METHOD_SD_SLOTB);
#endif
}

void WiiInit() {
	MountAllFAT();
}

bool WiiMessagePause(const char *s) {
	if (s) printf(s);
#ifdef HW_RVL
	while (1) {
		WPAD_ScanPads();
		u16 buttonsDown = WPAD_ButtonsDown(0);
		if( buttonsDown & WPAD_BUTTON_A ) return false;
		if( buttonsDown & WPAD_BUTTON_HOME ) return true;
  	}
#else
	return false;
#endif
}

void WiiFinished() {
	UnmountAllFAT();
#ifdef HW_RVL
	if (WiiMessagePause("Press A to return to homebrew channel.\n")) exit(0);
#endif
}
