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
#include "dosbox/include/dosbox.h"
#include "dosbox/dos/drives.h"

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

// Mounts a folder as a harddrive before starting the shell
// Designed for the Wii
int MountDOSBoxDir(char DriveLetter, const char *path) {
	DOS_Drive * newdrive;
	Bit16u sizes[4];
	Bit8u mediaid;
	std::string str_size;
	std::string label;
	str_size="512,127,16383,4031";
	mediaid=0xF8;		/* Hard Disk */
	char number[20];
	const char * scan=str_size.c_str();
	Bitu index=0;Bitu count=0;
	/* Parse the str_size string */
	while (*scan) {
		if (*scan==',') {
			number[index]=0;
			sizes[count++]=atoi(number);
			index=0;
		} else number[index++]=*scan;
		scan++;
	}
	number[index]=0;
	sizes[count++]=atoi(number);

	// get the drive letter
	char drive=toupper(DriveLetter);
	std::string temp_line = path;
	struct stat test;
	bool failed = false;
	if (stat(temp_line.c_str(),&test)) {
		failed = true;
		Cross::ResolveHomedir(temp_line);
		//Try again after resolving ~
		if(!stat(temp_line.c_str(),&test)) failed = false;
	}
	if(failed) {
		printf(MSG_Get("PROGRAM_MOUNT_ERROR_1"),temp_line.c_str());
		return 0;
	}
	/* Not a switch so a normal directory/file */
	if (!(test.st_mode & S_IFDIR)) {
		printf(MSG_Get("PROGRAM_MOUNT_ERROR_2"),temp_line.c_str());
		return 0;
	}
	if (temp_line[temp_line.size()-1]!=CROSS_FILESPLIT) temp_line+=CROSS_FILESPLIT;
	Bit8u bit8size=(Bit8u) sizes[1];
	newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid);
	if (Drives[drive-'A']) {
		printf(MSG_Get("PROGRAM_MOUNT_ALREADY_MOUNTED"),drive,Drives[drive-'A']->GetInfo());
		if (newdrive) delete newdrive;
		return 0;
	}
	if (!newdrive)
		return 0;

	Drives[drive-'A']=newdrive;
	/* Set the correct media byte in the table */
	mem_writeb(Real2Phys(dos.tables.mediaid)+(drive-'A')*2,newdrive->GetMediaByte());
	printf(MSG_Get("PROGRAM_MOUNT_STATUS_2"),drive,newdrive->GetInfo());
	/* For hard drives set the label to DRIVELETTER_Drive.
	 * For floppy drives set the label to DRIVELETTER_Floppy.
	 * This way every drive except cdroms should get a label.*/
	label = drive; label+="_DRIVE";
	newdrive->dirCache.SetLabel(label.c_str(),false,true);
	return 1;
}
