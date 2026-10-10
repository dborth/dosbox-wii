/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2009-2026
 * drivemap.cpp
 *
 * See drivemap.h.
 *
 * The rows come from FileSystemDriver::enumerateStorageDevices(), so they
 * are whatever the platform has: all four FAT slots on Wii (present or not),
 * only what is present on Wii U. A device is mountable when the driver has it
 * mounted (getMountPath() is not empty).
 *
 * What is a DOS drive is remembered in owned[]: the letter and the path it
 * was mounted from. It is kept past the screen closing so that a drive can be
 * unmounted, or cleaned up when its device is removed, the next time the
 * screen is opened.
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "drivemap.h"
#include "fileop.h"
#include "dosboxsupport.h"
#include "memmanager.h"
#include "drivers/Platform.h"
#include "drivers/FileSystemDriver.h"
#include "drivers/Thread.h"
#include "drivers/Time.h"

#define DRIVEMAP_THREAD_STACKSIZE	(8 * 1024)
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
	pollThread.start(PollMain, nullptr, DRIVEMAP_THREAD_STACKSIZE, ThreadPriority::Low);

	lastUpdate = SystemTime::now();
}

void DriveMap_Close()
{
	if(!state)
		return;

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
