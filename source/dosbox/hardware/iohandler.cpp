/*
 *  Copyright (C) 2002-2019  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <string.h>
#include <stdlib.h>
#include "dosbox.h"
#include "inout.h"
#include "setup.h"
#include "cpu.h"
#include "../cpu/lazyflags.h"
#include "callback.h"

//#define ENABLE_PORTLOG

/* One handler per port and access width (byte, word, dword). Flat arrays of
 * IO_MAX pointers took 1.5 MB, for the few dozen ports anything is ever
 * registered on. The ports are split into pages of 256: a page nothing has been
 * registered on is a shared one that holds the default handler, and only gets a
 * page of its own when a handler is put there. */
#define IO_PAGE_BITS	8
#define IO_PAGE_SIZE	(1 << IO_PAGE_BITS)
#define IO_PAGE_MASK	(IO_PAGE_SIZE - 1)
#define IO_PAGES		((IO_MAX + IO_PAGE_SIZE - 1) >> IO_PAGE_BITS)

static IO_ReadHandler ** io_read_pages[3][IO_PAGES];
static IO_WriteHandler ** io_write_pages[3][IO_PAGES];
static IO_ReadHandler * io_read_default_page[IO_PAGE_SIZE];
static IO_WriteHandler * io_write_default_page[IO_PAGE_SIZE];

#define IO_READ_HANDLER(width,port)		(io_read_pages[width][(port) >> IO_PAGE_BITS][(port) & IO_PAGE_MASK])
#define IO_WRITE_HANDLER(width,port)	(io_write_pages[width][(port) >> IO_PAGE_BITS][(port) & IO_PAGE_MASK])

static Bitu IO_ReadDefault(Bitu port,Bitu iolen);
void IO_WriteDefault(Bitu port,Bitu val,Bitu iolen);

/* Returns false if a page of its own was needed and there was no memory */
static bool IO_SetReadHandler(Bitu width,Bitu port,IO_ReadHandler * handler) {
	IO_ReadHandler ** & page=io_read_pages[width][port >> IO_PAGE_BITS];
	if (page==io_read_default_page) {
		if (handler==IO_ReadDefault) return true;	// that is what the shared page says
		IO_ReadHandler ** own=(IO_ReadHandler **)malloc(IO_PAGE_SIZE*sizeof(IO_ReadHandler *));
		if (!own) return false;
		for (Bitu i=0;i<IO_PAGE_SIZE;i++) own[i]=IO_ReadDefault;
		page=own;
	}
	page[port & IO_PAGE_MASK]=handler;
	return true;
}

static bool IO_SetWriteHandler(Bitu width,Bitu port,IO_WriteHandler * handler) {
	IO_WriteHandler ** & page=io_write_pages[width][port >> IO_PAGE_BITS];
	if (page==io_write_default_page) {
		if (handler==IO_WriteDefault) return true;
		IO_WriteHandler ** own=(IO_WriteHandler **)malloc(IO_PAGE_SIZE*sizeof(IO_WriteHandler *));
		if (!own) return false;
		for (Bitu i=0;i<IO_PAGE_SIZE;i++) own[i]=IO_WriteDefault;
		page=own;
	}
	page[port & IO_PAGE_MASK]=handler;
	return true;
}

static void IO_NoMemory(Bitu port) {
	E_Exit("Out of memory for the I/O handler of port %x",(unsigned int)port);
}

static void IO_InitPages(void) {
	for (Bitu i=0;i<IO_PAGE_SIZE;i++) {
		io_read_default_page[i]=IO_ReadDefault;
		io_write_default_page[i]=IO_WriteDefault;
	}
	for (Bitu w=0;w<3;w++) {
		for (Bitu p=0;p<IO_PAGES;p++) {
			if (io_read_pages[w][p] && io_read_pages[w][p]!=io_read_default_page) free(io_read_pages[w][p]);
			if (io_write_pages[w][p] && io_write_pages[w][p]!=io_write_default_page) free(io_write_pages[w][p]);
			io_read_pages[w][p]=io_read_default_page;
			io_write_pages[w][p]=io_write_default_page;
		}
	}
}

static Bitu IO_ReadBlocked(Bitu /*port*/,Bitu /*iolen*/) {
	return ~0;
}

static void IO_WriteBlocked(Bitu /*port*/,Bitu /*val*/,Bitu /*iolen*/) {
}

static Bitu IO_ReadDefault(Bitu port,Bitu iolen) {
	switch (iolen) {
	case 1:
		LOG(LOG_IO,LOG_WARN)("Read from port %04X",port);
		IO_SetReadHandler(0,port,IO_ReadBlocked);	// not remembered if there is no memory: harmless
		return 0xff;
	case 2:
		return
			(IO_READ_HANDLER(0,port+0)(port+0,1) << 0) |
			(IO_READ_HANDLER(0,port+1)(port+1,1) << 8);
	case 4:
		return
			(IO_READ_HANDLER(1,port+0)(port+0,2) << 0) |
			(IO_READ_HANDLER(1,port+2)(port+2,2) << 16);
	}
	return 0;
}

void IO_WriteDefault(Bitu port,Bitu val,Bitu iolen) {
	switch (iolen) {
	case 1:
		LOG(LOG_IO,LOG_WARN)("Writing %02X to port %04X",val,port);
		IO_SetWriteHandler(0,port,IO_WriteBlocked);
		break;
	case 2:
		IO_WRITE_HANDLER(0,port+0)(port+0,(val >> 0) & 0xff,1);
		IO_WRITE_HANDLER(0,port+1)(port+1,(val >> 8) & 0xff,1);
		break;
	case 4:
		IO_WRITE_HANDLER(1,port+0)(port+0,(val >> 0 ) & 0xffff,2);
		IO_WRITE_HANDLER(1,port+2)(port+2,(val >> 16) & 0xffff,2);
		break;
	}
}

void IO_RegisterReadHandler(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range) {
	while (range--) {
		if ((mask&IO_MB) && !IO_SetReadHandler(0,port,handler)) IO_NoMemory(port);
		if ((mask&IO_MW) && !IO_SetReadHandler(1,port,handler)) IO_NoMemory(port);
		if ((mask&IO_MD) && !IO_SetReadHandler(2,port,handler)) IO_NoMemory(port);
		port++;
	}
}

void IO_RegisterWriteHandler(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range) {
	while (range--) {
		if ((mask&IO_MB) && !IO_SetWriteHandler(0,port,handler)) IO_NoMemory(port);
		if ((mask&IO_MW) && !IO_SetWriteHandler(1,port,handler)) IO_NoMemory(port);
		if ((mask&IO_MD) && !IO_SetWriteHandler(2,port,handler)) IO_NoMemory(port);
		port++;
	}
}

void IO_FreeReadHandler(Bitu port,Bitu mask,Bitu range) {
	while (range--) {
		if ((mask&IO_MB) && !IO_SetReadHandler(0,port,IO_ReadDefault)) IO_NoMemory(port);
		if ((mask&IO_MW) && !IO_SetReadHandler(1,port,IO_ReadDefault)) IO_NoMemory(port);
		if ((mask&IO_MD) && !IO_SetReadHandler(2,port,IO_ReadDefault)) IO_NoMemory(port);
		port++;
	}
}

void IO_FreeWriteHandler(Bitu port,Bitu mask,Bitu range) {
	while (range--) {
		if ((mask&IO_MB) && !IO_SetWriteHandler(0,port,IO_WriteDefault)) IO_NoMemory(port);
		if ((mask&IO_MW) && !IO_SetWriteHandler(1,port,IO_WriteDefault)) IO_NoMemory(port);
		if ((mask&IO_MD) && !IO_SetWriteHandler(2,port,IO_WriteDefault)) IO_NoMemory(port);
		port++;
	}
}

void IO_ReadHandleObject::Install(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range) {
	if(!installed) {
		installed=true;
		m_port=port;
		m_mask=mask;
		m_range=range;
		IO_RegisterReadHandler(port,handler,mask,range);
	} else E_Exit("IO_readHandler already installed port %x",port);
}

void IO_ReadHandleObject::Uninstall(){
	if(!installed) return;
	IO_FreeReadHandler(m_port,m_mask,m_range);
	installed=false;
}

IO_ReadHandleObject::~IO_ReadHandleObject(){
	Uninstall();
}

void IO_WriteHandleObject::Install(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range) {
	if(!installed) {
		installed=true;
		m_port=port;
		m_mask=mask;
		m_range=range;
		IO_RegisterWriteHandler(port,handler,mask,range);
	} else E_Exit("IO_writeHandler already installed port %x",port);
}

void IO_WriteHandleObject::Uninstall() {
	if(!installed) return;
	IO_FreeWriteHandler(m_port,m_mask,m_range);
	installed=false;
}

IO_WriteHandleObject::~IO_WriteHandleObject(){
	Uninstall();
	//LOG_MSG("FreeWritehandler called with port %X",m_port);
}

struct IOF_Entry {
	Bitu cs;
	Bitu eip;
};

#define IOF_QUEUESIZE 16
static struct {
	Bitu used;
	IOF_Entry entries[IOF_QUEUESIZE];
} iof_queue;

static Bits IOFaultCore(void) {
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=1;
	Bits ret=CPU_Core_Full_Run();
	CPU_CycleLeft+=CPU_Cycles;
	if (ret<0) E_Exit("Got a dosbox close machine in IO-fault core?");
	if (ret)
		return ret;
	if (!iof_queue.used) E_Exit("IO-faul Core without IO-faul");
	IOF_Entry * entry=&iof_queue.entries[iof_queue.used-1];
	if (entry->cs == SegValue(cs) && entry->eip==reg_eip)
		return -1;
	return 0;
}


/* Some code to make io operations take some virtual time. Helps certain
 * games with their timing of certain operations
 */


#define IODELAY_READ_MICROS 1.0
#define IODELAY_WRITE_MICROS 0.75

inline void IO_USEC_read_delay_old() {
	if(CPU_CycleMax > static_cast<Bit32s>((IODELAY_READ_MICROS*1000.0))) {
		// this could be calculated whenever CPU_CycleMax changes
		Bits delaycyc = static_cast<Bits>((CPU_CycleMax/1000)*IODELAY_READ_MICROS);
		if(CPU_Cycles > delaycyc) CPU_Cycles -= delaycyc;
		else CPU_Cycles = 0;
	}
}

inline void IO_USEC_write_delay_old() {
	if(CPU_CycleMax > static_cast<Bit32s>((IODELAY_WRITE_MICROS*1000.0))) {
		// this could be calculated whenever CPU_CycleMax changes
		Bits delaycyc = static_cast<Bits>((CPU_CycleMax/1000)*IODELAY_WRITE_MICROS);
		if(CPU_Cycles > delaycyc) CPU_Cycles -= delaycyc;
		else CPU_Cycles = 0;
	}
}


#define IODELAY_READ_MICROSk (Bit32u)(1024/1.0)
#define IODELAY_WRITE_MICROSk (Bit32u)(1024/0.75)

inline void IO_USEC_read_delay() {
	Bits delaycyc = CPU_CycleMax/IODELAY_READ_MICROSk;
	if(GCC_UNLIKELY(delaycyc > CPU_Cycles)) delaycyc = CPU_Cycles;
	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;
}

inline void IO_USEC_write_delay() {
	Bits delaycyc = CPU_CycleMax/IODELAY_WRITE_MICROSk;
	if(GCC_UNLIKELY(delaycyc > CPU_Cycles)) delaycyc = CPU_Cycles;
	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;
}

#ifdef ENABLE_PORTLOG
static Bit8u crtc_index = 0;
const char* const len_type[] = {" 8","16","32"};
void log_io(Bitu width, bool write, Bitu port, Bitu val) {
	switch(width) {
	case 0:
		val&=0xff;
		break;
	case 1:
		val&=0xffff;
		break;
	}
	if (write) {
		// skip the video cursor position spam
		if (port==0x3d4) {
			if (width==0) crtc_index = (Bit8u)val;
			else if(width==1) crtc_index = (Bit8u)(val>>8);
		}
		if (crtc_index==0xe || crtc_index==0xf) {
			if((width==0 && (port==0x3d4 || port==0x3d5))||(width==1 && port==0x3d4))
				return;
		}

		switch(port) {
		//case 0x020: // interrupt command
		//case 0x040: // timer 0
		//case 0x042: // timer 2
		//case 0x043: // timer control
		//case 0x061: // speaker control
		case 0x3c8: // VGA palette
		case 0x3c9: // VGA palette
		// case 0x3d4: // VGA crtc
		// case 0x3d5: // VGA crtc
		// case 0x3c4: // VGA seq
		// case 0x3c5: // VGA seq
			break;
		default:
			LOG_MSG("iow%s % 4x % 4x, cs:ip %04x:%04x", len_type[width],
				port, val, SegValue(cs),reg_eip);
			break;
		}
	} else {
		switch(port) {
		//case 0x021: // interrupt status
		//case 0x040: // timer 0
		//case 0x042: // timer 2
		//case 0x061: // speaker control
		case 0x201: // joystick status
		case 0x3c9: // VGA palette
		// case 0x3d4: // VGA crtc index
		// case 0x3d5: // VGA crtc
		case 0x3da: // display status - a real spammer
			// don't log for the above cases
			break;
		default:
			LOG_MSG("ior%s % 4x % 4x,\t\tcs:ip %04x:%04x", len_type[width],
				port, val, SegValue(cs),reg_eip);
			break;
		}
	}
}
#else
#define log_io(W, X, Y, Z)
#endif


void IO_WriteB(Bitu port,Bitu val) {
	log_io(0, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,1)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit8u old_al = reg_al;
		Bit16u old_dx = reg_dx;
		reg_al = val;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x08;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_al = old_al;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else {
		IO_USEC_write_delay();
		IO_WRITE_HANDLER(0,port)(port,val,1);
	}
}

void IO_WriteW(Bitu port,Bitu val) {
	log_io(1, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,2)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit16u old_ax = reg_ax;
		Bit16u old_dx = reg_dx;
		reg_ax = val;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x0a;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_ax = old_ax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else {
		IO_USEC_write_delay();
		IO_WRITE_HANDLER(1,port)(port,val,2);
	}
}

void IO_WriteD(Bitu port,Bitu val) {
	log_io(2, true, port, val);
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,4)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit32u old_eax = reg_eax;
		Bit16u old_dx = reg_dx;
		reg_eax = val;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x0c;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		reg_eax = old_eax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else IO_WRITE_HANDLER(2,port)(port,val,4);
}

Bitu IO_ReadB(Bitu port) {
	Bitu retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,1)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit8u old_al = reg_al;
		Bit16u old_dx = reg_dx;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x00;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_al;
		reg_al = old_al;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
		return retval;
	}
	else {
		IO_USEC_read_delay();
		retval = IO_READ_HANDLER(0,port)(port,1);
	}
	log_io(0, false, port, retval);
	return retval;
}

Bitu IO_ReadW(Bitu port) {
	Bitu retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,2)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit16u old_ax = reg_ax;
		Bit16u old_dx = reg_dx;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x02;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_ax;
		reg_ax = old_ax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	}
	else {
		IO_USEC_read_delay();
		retval = IO_READ_HANDLER(1,port)(port,2);
	}
	log_io(1, false, port, retval);
	return retval;
}

Bitu IO_ReadD(Bitu port) {
	Bitu retval;
	if (GCC_UNLIKELY(GETFLAG(VM) && (CPU_IO_Exception(port,4)))) {
		LazyFlags old_lflags;
		memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
		CPU_Decoder * old_cpudecoder;
		old_cpudecoder=cpudecoder;
		cpudecoder=&IOFaultCore;
		IOF_Entry * entry=&iof_queue.entries[iof_queue.used++];
		entry->cs=SegValue(cs);
		entry->eip=reg_eip;
		CPU_Push16(SegValue(cs));
		CPU_Push16(reg_ip);
		Bit32u old_eax = reg_eax;
		Bit16u old_dx = reg_dx;
		reg_dx = port;
		RealPt icb = CALLBACK_RealPointer(call_priv_io);
		SegSet16(cs,RealSeg(icb));
		reg_eip = RealOff(icb)+0x04;
		CPU_Exception(cpu.exception.which,cpu.exception.error);

		DOSBOX_RunMachine();
		iof_queue.used--;

		retval = reg_eax;
		reg_eax = old_eax;
		reg_dx = old_dx;
		memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
		cpudecoder=old_cpudecoder;
	} else {
		retval = IO_READ_HANDLER(2,port)(port,4);
	}
	log_io(2, false, port, retval);
	return retval;
}

class IO :public Module_base {
public:
	IO(Section* configuration):Module_base(configuration){
	iof_queue.used=0;
	IO_InitPages();
	IO_FreeReadHandler(0,IO_MA,IO_MAX);
	IO_FreeWriteHandler(0,IO_MA,IO_MAX);
	}
	~IO()
	{
		IO_InitPages();	// puts every port back on the shared pages, freeing the rest
	}
};

static IO* test;

void IO_Destroy(Section*) {
	delete test;
}

void IO_Init(Section * sect) {
	test = new IO(sect);
	sect->AddDestroyFunction(&IO_Destroy);
}
