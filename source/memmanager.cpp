/****************************************************************************
 * DOSBox Wii
 *
 * memmanager.cpp
 *
 * Menu memory, see memmanager.h
 ***************************************************************************/
#include <malloc.h>
#include <stdlib.h>

#include "memmanager.h"
#include "drivers/Platform.h"
#include "drivers/VideoDriver.h"
#include "drivers/EmulatorVideoDriver.h"
#include "drivers/BulkMemory.h"
#include "drivers/Mutex.h"
#include "libgui/GuiImageData.h"
#include "libgui/GuiTextRenderer.h"

// dosbox/cpu/core_dynrec.cpp. C_DYNREC is always built; these are the only
// part of the core the app touches here, declared rather than included to
// keep DOSBox's headers out of this file.
extern bool CPU_Core_Dynrec_LendCache(void ** base, size_t * size);
extern void CPU_Core_Dynrec_ReclaimCache(void);

static bool menuMode = false;

#ifdef HW_RVL

static mspace menuSpace = nullptr;
static bool cacheLent = false;
static void * ownBlock = nullptr;		// the menu's own block, when nothing could be lent
static void * decodeScratch = nullptr;

void * memspace_malloc(size_t size)
{
	return menuSpace ? mspace_malloc(menuSpace, size) : malloc(size);
}

void * memspace_memalign(size_t alignment, size_t size)
{
	return menuSpace ? mspace_memalign(menuSpace, alignment, size) : memalign(alignment, size);
}

void memspace_free(void * ptr)
{
	if(!ptr)
		return;

	// Not menuSpace-aware on purpose: libogc2's malloc records which heap a
	// block came from, so free() returns it to the right one either way.
	free(ptr);
}

void SwitchMemoryModeMenu()
{
	if(menuMode)
		return;

	void * base = nullptr;
	size_t size = 0;

	cacheLent = CPU_Core_Dynrec_LendCache(&base, &size);

	if(cacheLent && size < MENU_HEAP_MIN)
	{
		CPU_Core_Dynrec_ReclaimCache();
		cacheLent = false;
	}

	if(!cacheLent)
	{
		size = MENU_HEAP_FALLBACK_SIZE;
		ownBlock = BulkMemory::allocate(size);
		base = ownBlock;
	}

	if(base)
	{
		menuSpace = create_mspace_with_base(base, size, 1);
		mspace_set_footprint_limit(menuSpace, size);
	}

	menuMode = true;

	MutexLock scratchGuard(GuiImageData::scratchLock());
	decodeScratch = memspace_malloc(IMAGE_DECODE_SCRATCH_SIZE);
	GuiImageData::setDecodeScratch(decodeScratch, IMAGE_DECODE_SCRATCH_SIZE);
}

void SwitchMemoryModeEmulator()
{
	if(!menuMode)
		return;

	{
		MutexLock scratchGuard(GuiImageData::scratchLock());
		GuiImageData::setDecodeScratch(nullptr, 0);
	}
	decodeScratch = nullptr;

	// Both hold textures in the heap that is about to go
	if(fontSystem)
		fontSystem->clearGlyphCache();
	platform->getVideo()->getEmulatorVideo()->releaseSnapshot();

	if(menuSpace)
		destroy_mspace(menuSpace);
	menuSpace = nullptr;

	if(cacheLent)
		CPU_Core_Dynrec_ReclaimCache();
	cacheLent = false;

	BulkMemory::release(ownBlock);
	ownBlock = nullptr;

	menuMode = false;
}

#else

// No private heap off the Wii (Wii U has memory to spare)
static void * decodeScratch = nullptr;

void * memspace_malloc(size_t size) { return malloc(size); }
void * memspace_memalign(size_t alignment, size_t size) { return memalign(alignment, size); }
void memspace_free(void * ptr) { free(ptr); }

void SwitchMemoryModeMenu()
{
	menuMode = true;
	if(decodeScratch == nullptr) {
		decodeScratch = malloc(IMAGE_DECODE_SCRATCH_SIZE);
		GuiImageData::setDecodeScratch(decodeScratch, IMAGE_DECODE_SCRATCH_SIZE);
	}
}

void SwitchMemoryModeEmulator()
{
	platform->getVideo()->getEmulatorVideo()->releaseSnapshot();
	menuMode = false;
}

#endif
