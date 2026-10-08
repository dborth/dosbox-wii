/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2026
 * BulkMemory.h
 *
 * Large, long-lived allocations for the DOSBox core
 *
 * For blocks too big or too cold to spend scarce fast memory on (the paging
 * TLB is ~20 MB). The platform decides where they live: MEM2 on Wii, the
 * ordinary heap on Wii U. Blocks are never freed.
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
};
