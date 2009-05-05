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

void WiiInit() {
	fatInitDefault();
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
#ifdef HW_RVL
	if (WiiMessagePause("Press A to return to homebrew channel.\n")) exit(0);
#endif
}
