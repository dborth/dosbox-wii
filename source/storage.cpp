/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * storage.cpp
 *
 * See storage.h.
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <atomic>
#include <sys/stat.h>
#include "storage.h"
#include "dosboxsupport.h"
#include "memmanager.h"
#include "drivers/Platform.h"
#include "drivers/FileSystemDriver.h"
#include "drivers/Logger.h"
#include "drivers/Thread.h"
#include "drivers/Time.h"
#include "dosbox/include/dosbox.h"
#include "dosbox/dos/drives.h"

/****************************************************************************
 * App drive and DOS drives
 ***************************************************************************/

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

	if(!S_ISDIR(test.st_mode))
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

/****************************************************************************
 * The Drives screen's model
 ***************************************************************************/

#ifdef __WIIU__
#define DRIVEMAP_THREAD_STACKSIZE	(128 * 1024)
#else
#define DRIVEMAP_THREAD_STACKSIZE	(8 * 1024)
#endif
#define POLL_STEP_US				50000	//!< the thread checks for a stop request this often...
#define POLL_STEPS					20		//!< ...and polls the devices every this many steps (1 s)
#define UPDATE_INTERVAL_MS			500		//!< the screen re-reads the devices this often
#define MAX_OWNED					8

struct DriveRow
{
	int id;						//!< Device id from FileSystemDriver.h
	char name[20];
	char label[16];				//!< Volume label; empty if none
	char path[32];				//!< Where the driver has it mounted, eg. "sd:/"; empty if not mounted
};

//! The rows. Allocated from the menu heap while the screen is open.
struct DriveMapState
{
	DriveRow rows[MAX_STORAGE_DEVICES];
	int rowCount;
	StorageDevice scratch[MAX_STORAGE_DEVICES];
};

//! A DOS drive this screen mounted
struct OwnedDrive
{
	char letter;				//!< 0 = unused
	char path[32];
};

static DriveMapState * state = NULL;
static OwnedDrive owned[MAX_OWNED];
static Thread pollThread;
static std::atomic<bool> pollRunning(false);	//!< PollMain() has not yet returned
static Ticks lastUpdate = 0;

/****************************************************************************
 * Helpers
 ***************************************************************************/
static FileSystemDriver * FS()
{
	return platform->getFileSystem();
}

//! Paths are the same if they only differ by a trailing slash: the DOS drive
//! always has one, the driver's path may not.
static bool SamePath(const char * a, const char * b)
{
	size_t la = strlen(a);
	size_t lb = strlen(b);

	if(la > 0 && a[la - 1] == '/') la--;
	if(lb > 0 && b[lb - 1] == '/') lb--;

	return la == lb && strncmp(a, b, la) == 0;
}

static OwnedDrive * FindOwned(const char * path)
{
	for(int i = 0; i < MAX_OWNED; i++)
		if(owned[i].letter && SamePath(owned[i].path, path))
			return &owned[i];
	return NULL;
}

static bool IsRowMounted(const DriveRow & r)
{
	return r.path[0] != 0;
}

/****************************************************************************
 * Reload
 *
 * Reads the devices into state->rows. Returns true if anything differs from
 * what was there. Memory only, no I/O.
 ***************************************************************************/
static bool Reload()
{
	FileSystemDriver * fs = FS();
	StorageDevice * list = state->scratch;
	const int count = fs->enumerateStorageDevices(list);
	DriveRow newRows[MAX_STORAGE_DEVICES];
	int newCount = 0;

	for(int i = 0; i < count && newCount < MAX_STORAGE_DEVICES; i++)
	{
		if(list[i].alwaysListed)	// DVD and network shares are not handled here
			continue;

		DriveRow & r = newRows[newCount++];
		const char * mount = fs->getMountPath(list[i].id);

		memset(&r, 0, sizeof(r));
		r.id = list[i].id;
		snprintf(r.name, sizeof(r.name), "%s", list[i].name);
		snprintf(r.label, sizeof(r.label), "%s", list[i].volumeLabel);
		snprintf(r.path, sizeof(r.path), "%s", mount ? mount : "");
	}

	const bool changed = (newCount != state->rowCount) ||
		memcmp(newRows, state->rows, sizeof(DriveRow) * newCount) != 0;

	if(changed)
	{
		memcpy(state->rows, newRows, sizeof(DriveRow) * newCount);
		state->rowCount = newCount;
	}

	return changed;
}

/****************************************************************************
 * Tidy
 *
 * Forgets drives the user has since unmounted from DOS, and removes the DOS
 * drive of a device that has gone. A DOS drive is only a path, so one left
 * behind is harmless; but taking it away has to wait for the DOS prompt, as
 * a running program may have files open on it. Returns true if anything
 * changed.
 ***************************************************************************/
static bool Tidy()
{
	bool changed = false;

	for(int i = 0; i < MAX_OWNED; i++)
	{
		OwnedDrive & o = owned[i];
		char info[64];

		if(!o.letter)
			continue;

		// unmounted from DOS (MOUNT -u), or the letter has been used for
		// something else since
		if(!GetDOSDriveInfo(o.letter, info, sizeof(info)) || !SamePath(info, o.path))
		{
			o.letter = 0;
			changed = true;
			continue;
		}

		bool deviceThere = false;

		for(int r = 0; r < state->rowCount; r++)
			if(IsRowMounted(state->rows[r]) && SamePath(state->rows[r].path, o.path))
				deviceThere = true;

		if(!deviceThere && IsShellIdle() && UnmountDOSDrive(o.letter))
		{
			o.letter = 0;
			changed = true;
		}
	}

	return changed;
}

/****************************************************************************
 * PollMain
 *
 * Lets the driver notice devices coming and going, and mount what it finds.
 ***************************************************************************/
static void * PollMain(void *)
{
	FileSystemDriver * fs = FS();

	while(!pollThread.stopRequested())
	{
		int removed[MAX_STORAGE_DEVICES];
		int removedCount = 0;
		bool listChanged = false;

		fs->pollStorageDevices(removed, removedCount, listChanged);

		for(int i = 0; i < POLL_STEPS && !pollThread.stopRequested(); i++)
			usleep(POLL_STEP_US);
	}

	pollRunning = false;	// the last thing the thread does
	return nullptr;
}

/****************************************************************************
 * DriveMap_Open / DriveMap_Close
 ***************************************************************************/
void DriveMap_Open()
{
	if(state)
		return;

	state = (DriveMapState *) memspace_malloc(sizeof(DriveMapState));

	if(!state)
		return;

	memset(state, 0, sizeof(DriveMapState));
	Reload();
	Tidy();

	// no thread: devices are then only read as they stand
	pollRunning = true;

	if(!pollThread.start(PollMain, nullptr, DRIVEMAP_THREAD_STACKSIZE, ThreadPriority::Low))
		pollRunning = false;

	lastUpdate = SystemTime::now();
}

void DriveMap_BeginClose()
{
	if(state)
		pollThread.requestStop();
}

bool DriveMap_CloseReady()
{
	return !state || !pollRunning;
}

void DriveMap_Close()
{
	if(!state)
		return;

	// Instant if the caller waited for DriveMap_CloseReady(); otherwise this
	// waits out whatever mount or probe the thread is in the middle of
	pollThread.requestStop();
	pollThread.join();

	memspace_free(state);
	state = NULL;
}

bool DriveMap_Update()
{
	if(!state)
		return false;

	const Ticks now = SystemTime::now();

	if(SystemTime::diffMillisecs(lastUpdate, now) < UPDATE_INTERVAL_MS)
		return false;

	lastUpdate = now;

	const bool reloaded = Reload();
	const bool tidied = Tidy();

	return reloaded || tidied;
}

/****************************************************************************
 * Rows
 ***************************************************************************/
int DriveMap_RowCount()
{
	return state ? state->rowCount + 1 : 0;
}

static void DosDriveLetters(char * out, size_t size)
{
	size_t n = 0;

	out[0] = 0;
	for(char c = 'A'; c <= 'Y'; c++)
	{
		if(IsDOSDriveMounted(c) && n + 4 < size)
			n += snprintf(out + n, size - n, "%s%c:", n ? " " : "", c);
	}

	if(n == 0)
		snprintf(out, size, "None");
}

void DriveMap_RowText(int row, char * label, size_t labelSize, char * value, size_t valueSize)
{
	if(!state || row < 0 || row >= state->rowCount)
	{
		snprintf(label, labelSize, "DOS drives");
		DosDriveLetters(value, valueSize);
		return;
	}

	const DriveRow & r = state->rows[row];
	const OwnedDrive * o = IsRowMounted(r) ? FindOwned(r.path) : NULL;

	snprintf(label, labelSize, "%s", r.name);

	if(o)
		snprintf(value, valueSize, "%c:  %s", o->letter, r.label);
	else if(IsRowMounted(r))
		snprintf(value, valueSize, "Not mounted");
	else
		snprintf(value, valueSize, "Not available");
}

void DriveMap_RowHelp(int row, char * buf, size_t size)
{
	if(!state || row < 0 || row >= state->rowCount)
	{
		// every DOS drive and where it is mounted from; the help line scrolls
		size_t n = 0;
		char info[64];

		buf[0] = 0;
		for(char c = 'A'; c <= 'Y'; c++)
		{
			if(GetDOSDriveInfo(c, info, sizeof(info)) && n + 8 < size)
				n += snprintf(buf + n, size - n, "%s%c: %s", n ? "   " : "", c, info);
		}

		if(n == 0)
			snprintf(buf, size, "No DOS drives are mounted.");
		return;
	}

	const DriveRow & r = state->rows[row];

	if(IsRowMounted(r) && FindOwned(r.path))
		snprintf(buf, size, "A: unmount it from DOS. Only at the DOS prompt.");
	else if(IsRowMounted(r))
		snprintf(buf, size, "A: mount it as a DOS drive.");
	else
		snprintf(buf, size, "Not found. A drive that is plugged in shows up here by itself.");
}

const char * DriveMap_RowAction(int row)
{
	if(!state || row < 0 || row >= state->rowCount)
		return NULL; // the read-only row

	const DriveRow & r = state->rows[row];

	if(!IsRowMounted(r))
		return "That drive is not available.";

	OwnedDrive * o = FindOwned(r.path);

	if(o)
	{
		if(!IsShellIdle())
			return "Unmount at the DOS prompt, not while a program is running.";

		if(!UnmountDOSDrive(o->letter))
			return "Could not unmount the drive.";

		o->letter = 0;
		return NULL;
	}

	OwnedDrive * slot = NULL;

	for(int i = 0; i < MAX_OWNED && !slot; i++)
		if(!owned[i].letter)
			slot = &owned[i];

	if(!slot)
		return "Too many drives are mounted.";

	char letter = 0;

	for(char c = 'D'; c <= 'Y' && !letter; c++)
		if(!IsDOSDriveMounted(c))
			letter = c;

	if(!letter)
		return "No drive letter is free.";

	if(!MountDOSDrive(letter, r.path, r.label[0] ? r.label : NULL))
		return "Could not mount the drive.";

	slot->letter = letter;
	snprintf(slot->path, sizeof(slot->path), "%s", r.path);
	return NULL;
}
