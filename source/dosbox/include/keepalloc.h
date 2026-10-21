/*
 *  keepalloc.h
 *
 *  Allocate-once blocks for DOSBox restarts.
 *
 *  The core is run again in the same process (RunDOSBox() in a loop), so
 *  anything it allocates and frees on every run only fragments the heap and,
 *  worse, can come back at a different address or size. A KeepBlock is
 *  allocated the first time it is asked for and then kept for the life of the
 *  process: later calls hand back the same memory, zeroed, and it is never
 *  freed.
 *
 *  Variable-sized data (guest RAM, video memory) asks for its largest possible
 *  size as first_cap the first time; later runs only zero what they use.
 *  If a later request does exceed the block, a bigger one is taken and the old
 *  one is abandoned (it cannot be freed: callers may still hold pointers).
 */
#ifndef DOSBOX_KEEPALLOC_H
#define DOSBOX_KEEPALLOC_H

#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include "drivers/BulkMemory.h"

struct KeepBlock {
	void * ptr;
	size_t cap;
};

/* Returns at least `need` bytes, the first `need` of them zeroed, 32 byte
 * aligned; NULL if memory ran out. `bulk` puts the block in BulkMemory (MEM2
 * on Wii) instead of the ordinary heap. `first_cap` is the size taken the
 * first time (0 = just `need`). */
static inline void * Keep_Get(KeepBlock & b, size_t need, size_t first_cap, bool bulk) {
	if (!b.ptr || need > b.cap) {
		size_t cap = first_cap > need ? first_cap : need;
		void * p = bulk ? BulkMemory::allocate(cap) : memalign(32, cap);
		if (!p) return NULL;
		b.ptr = p;
		b.cap = cap;
	}
	memset(b.ptr, 0, need);
	return b.ptr;
}

#endif
