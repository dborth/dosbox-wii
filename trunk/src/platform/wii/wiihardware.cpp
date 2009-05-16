#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <zlib.h>
#include <malloc.h>
#include <fat.h>
#include "wiihardware.h"

char appPath[1024];

void WiiInit() {
	fatInitDefault();
	appPath[0] = 0;
}

void CreateAppPath(char origpath[])
{
	char path[1024];
	strncpy(path, origpath, 1024); // make a copy

	char * loc;
	int pos = -1;

	loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	loc = strchr(path,'/'); // looking for / from fat:/ (or sd:/)
	if (loc != NULL)
		pos = loc - path + 1;

	if(pos >= 0 && pos < 1024)
		sprintf(appPath, "sd:/%s", &(path[pos]));
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

}
