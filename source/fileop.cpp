/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * fileop.cpp
 *
 * File operations
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "fileop.h"
#include "drivers/Platform.h"
#include "drivers/FileSystemDriver.h"
#include "drivers/Logger.h"

char appDrive[MAX_APP_DRIVE_LEN];

/****************************************************************************
 * FindAppDrive
 *
 * Storage is found by looking, not by where the app was launched from.
 ***************************************************************************/
void FindAppDrive()
{
	static const int devices[] = { DEVICE_SD, DEVICE_USB, DEVICE_USB2, DEVICE_USB3 };
	const int count = sizeof(devices) / sizeof(devices[0]);
	FileSystemDriver * fs = platform->getFileSystem();

	appDrive[0] = 0;

	// Pass 0 wants a folder that already exists; pass 1 creates one
	for(int pass = 0; pass < 2; pass++)
	{
		for(int i = 0; i < count; i++)
		{
			const char * mount = fs->getMountPath(devices[i]); // eg. "sd:/"
			if(!mount || mount[0] == 0)
				continue;

			char dir[MAX_APP_DRIVE_LEN + 16];
			snprintf(dir, sizeof(dir), "%s%s", mount, DOSBOX_DIR_NAME);

			struct stat st;
			bool usable = (stat(dir, &st) == 0 && S_ISDIR(st.st_mode));

			if(!usable && pass == 1)
			{
				mkdir(dir, 0777);
				usable = (stat(dir, &st) == 0 && S_ISDIR(st.st_mode));
			}

			if(!usable)
				continue;

			// appDrive is the device without the trailing slash: "sd:"
			snprintf(appDrive, MAX_APP_DRIVE_LEN, "%s", mount);
			size_t len = strlen(appDrive);
			if(len > 0 && appDrive[len - 1] == '/')
				appDrive[len - 1] = 0;
			return;
		}
	}

	LOG_ERROR("No storage device with a %s folder, and none could create one", DOSBOX_DIR_NAME);
}
