/****************************************************************************
 * DOSBox Wii
 *
 * memmanager.h
 *
 * Menu memory
 *
 * Everything the menu allocates (the decode scratch, image and glyph textures,
 * the frame snapshot for the background) comes from one private heap that
 * only exists while the menu is open, instead of from the general heap. The
 * heap is not new memory: it is the dynrec code cache, lent out. The core is
 * suspended in the menu, and the cache can be rebuilt from nothing, so
 * SwitchMemoryModeMenu() throws the translated code away and hands the
 * cache's memory to the menu, and SwitchMemoryModeEmulator() takes the menu's
 * allocations down in one go and gives it back. Nothing is allocated or freed
 * from the general heap on a switch, and a menu that runs out of room fails
 * its allocation instead of taking the emulator's memory.
 *
 * Which memory can be lent is the platform's call (Codegen::isPlainMemory()).
 * Where none can be, the menu gets a block of its own for as long as it is open.
 ***************************************************************************/
#ifndef _MEMMANAGER_H_
#define _MEMMANAGER_H_

#include <stddef.h>

#define IMAGE_DECODE_SCRATCH_SIZE ((640 * 480 * 4) + (480 * sizeof(void *)))

//! The least a lent cache can be and still be worth using as the menu heap.
//! The menu needs about 5 MB at its busiest (the credits window).
#define MENU_HEAP_MIN		(5 * 1024 * 1024)

//! Size of the block the menu gets when nothing can be lent
#define MENU_HEAP_FALLBACK_SIZE	(6 * 1024 * 1024)

//! Opens the menu heap. Call before the first thing the menu allocates,
//! including GFX_Suspend() (its frame snapshot is menu memory).
void SwitchMemoryModeMenu();

//! Frees everything the menu allocated and closes the heap. The menu's
//! images, glyph cache and snapshot are dropped here; anything else that
//! holds a menu allocation must be gone before this is called.
void SwitchMemoryModeEmulator();

//! Allocation from the menu heap. Outside menu mode, and on platforms
//! without the private heap, these are plain heap allocations.
void * memspace_malloc(size_t size);
void * memspace_memalign(size_t alignment, size_t size);
void memspace_free(void * ptr);

#endif
