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

// Declared here rather than in a core header: the helper MOUNT -u and
// IMGMOUNT -u share (dos_programs.cpp), which is not static for this reason.
const char * UnmountHelper(char umount);

/****************************************************************************
 * MountDOSDrive
 *
 * Mounts a folder as a hard drive. Same geometry and label rules as MOUNT.
 ***************************************************************************/
bool MountDOSDrive(char DriveLetter, const char *path, const char *label)
{
	const int index = toupper(DriveLetter) - 'A';
	const char drive = 'A' + index;

	if(index < 0 || index >= DOS_DRIVES || !path || !path[0])
		return false;

	if(Drives[index])
		return false;

	std::string dir = path;
	struct stat test;

	if(stat(dir.c_str(), &test))
	{
		Cross::ResolveHomedir(dir); //Try again after resolving ~
		if(stat(dir.c_str(), &test))
			return false;
	}

	if(!(test.st_mode & S_IFDIR))
		return false;

	if(dir[dir.size() - 1] != CROSS_FILESPLIT)
		dir += CROSS_FILESPLIT;

	// "512,127,16383,4031" as MountDOSBoxDir always passed it: bytes per
	// sector, sectors per cluster, total clusters, free clusters. Hard disk.
	const Bit8u mediaid = 0xF8;
	DOS_Drive * newdrive = new localDrive(dir.c_str(), 512, 127, 16383, 4031, mediaid);

	if(!newdrive)
		return false;

	Drives[index] = newdrive;

	/* Set the correct media byte in the table. The table has 9 bytes per
	 * drive (see dos_programs.cpp); an earlier version of this function
	 * stepped by 2 and so never set the byte for the drive it mounted. */
	mem_writeb(Real2Phys(dos.tables.mediaid) + index * 9, newdrive->GetMediaByte());

	/* Every drive except cdroms gets a label; DRIVELETTER_Drive by default. */
	std::string name;
	if(label && label[0])
		name = label;
	else
	{
		name = drive;
		name += "_DRIVE";
	}
	newdrive->dirCache.SetLabel(name.c_str(), false, true);

	return true;
}

/****************************************************************************
 * UnmountDOSDrive
 ***************************************************************************/
bool UnmountDOSDrive(char DriveLetter)
{
	const int index = toupper(DriveLetter) - 'A';

	if(index < 0 || index >= DOS_DRIVES)
		return false;

	if(!Drives[index])
		return true;

	UnmountHelper((char)('A' + index));

	return Drives[index] == NULL;
}

/****************************************************************************
 * GetDOSDriveInfo
 ***************************************************************************/
bool GetDOSDriveInfo(char DriveLetter, char * info, size_t size)
{
	const int index = toupper(DriveLetter) - 'A';
	static const char prefix[] = "local directory ";

	if(index < 0 || index >= DOS_DRIVES || !Drives[index] || !info || size == 0)
		return false;

	const char * text = Drives[index]->GetInfo();

	if(strncmp(text, prefix, sizeof(prefix) - 1) == 0)
		text += sizeof(prefix) - 1;

	snprintf(info, size, "%s", text);
	return true;
}

/****************************************************************************
 * IsDOSDriveMounted
 ***************************************************************************/
bool IsDOSDriveMounted(char DriveLetter)
{
	const int index = toupper(DriveLetter) - 'A';

	return index >= 0 && index < DOS_DRIVES && Drives[index] != NULL;
}

/****************************************************************************
 * MountDOSBoxDir
 *
 * Mounts a folder as a harddrive before starting the shell. Returns 1 if
 * it was mounted.
 ***************************************************************************/
int MountDOSBoxDir(char DriveLetter, const char *path)
{
	return MountDOSDrive(DriveLetter, path, NULL) ? 1 : 0;
}
