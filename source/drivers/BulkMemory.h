/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * BulkMemory.h
 *
 * Large, long-lived allocations for the DOSBox core
 *
 * For large, long-lived blocks (guest RAM, paging TLB, dynrec code cache and
 * block table: tens of MB). The platform decides where they live: MEM2 on
 * Wii, the ordinary heap on Wii U.
 *
 * On Wii,anything we explicitly want in MEM2 is requested here
 *
 * Implementations: WiiBulkMemory.cpp (ogc/wii), WutBulkMemory.cpp (wut)
 ***************************************************************************/
#pragma once

#include <cstddef>

class BulkMemory
{
	public:
		//! \param bytes size wanted
		//! \return a block aligned to at least a cache line, contents
		//! unspecified, or nullptr if it cannot be satisfied
		static void * allocate(size_t bytes);

		//! frees a block returned by allocate(); nullptr is ignored
		static void release(void * block);
};
