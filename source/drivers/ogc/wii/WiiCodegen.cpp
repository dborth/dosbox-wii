/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * WiiCodegen.cpp
 *
 * JIT code cache support for the Wii
 *
 * The Wii has no execute protection, so the code cache is plain heap memory
 * and a write window only has to flush the written range from the D-cache
 * and invalidate it in the I-cache when it closes.
 ***************************************************************************/
#include "../../Codegen.h"

#include <stdlib.h>

namespace {
	int       writeDepth = 0;

	// Watermark for the open write window. nullptr/nullptr means nothing
	// has been marked dirty yet this window.
	uint8_t * dirtyLo = nullptr;
	uint8_t * dirtyHi = nullptr;

	const uintptr_t CACHE_LINE = 32;

	void flushRange(uint8_t * lo, uint8_t * hi)
	{
		uint8_t * line = (uint8_t *)((uintptr_t)lo & ~(CACHE_LINE - 1));

		while (line < hi)
		{
			asm volatile("dcbst %y0\n\t icbi %y0" :: "Z"(*line));
			line += CACHE_LINE;
		}
		asm volatile("sync\n\t isync");
	}
}

uint8_t * Codegen::acquire(size_t preferred, size_t minimum, size_t & got)
{
	uint8_t * mem = (uint8_t *)malloc(preferred);
	got = preferred;

	if (!mem && minimum < preferred)
	{
		mem = (uint8_t *)malloc(minimum);
		got = minimum;
	}
	if (!mem) got = 0;
	return mem;
}

void Codegen::beginWrite()
{
	writeDepth++;
}

void Codegen::markDirty(const void * p, size_t n)
{
	if (n == 0) return;

	uint8_t * lo = (uint8_t *)p;
	uint8_t * hi = lo + n;
	if (!dirtyLo || lo < dirtyLo) dirtyLo = lo;
	if (!dirtyHi || hi > dirtyHi) dirtyHi = hi;
}

void Codegen::endWrite()
{
	if (writeDepth == 0 || --writeDepth > 0) return;

	if (dirtyHi > dirtyLo)
		flushRange(dirtyLo, dirtyHi);

	dirtyLo = nullptr;
	dirtyHi = nullptr;
}
