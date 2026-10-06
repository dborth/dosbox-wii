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


/*
	Output goes to the platform's EmulatorAudioDriver (push model): the mixer
	hands finished buffers to the driver from the emulation thread, there is
	no audio callback thread any more. See MIXER_Pull / MIXER_Pump below.
*/

#include <string.h>
#include <sys/types.h>
#include <math.h>

#if defined (WIN32)
//Midi listing
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#include "mem.h"
#include "pic.h"
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "cross.h"
#include "support.h"
#include "mapper.h"
#include "hardware.h"
#include "programs.h"
#include "midi.h"
#include "drivers/Platform.h"
#include "drivers/AudioDriver.h"
#include "drivers/EmulatorAudioDriver.h"

#define MIXER_SSIZE 4

//#define MIXER_SHIFT 14
//#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)

#define MIXER_VOLSHIFT 13

#define FREQ_SHIFT 14
#define FREQ_NEXT ( 1 << FREQ_SHIFT)
#define FREQ_MASK ( FREQ_NEXT -1 )

#define TICK_SHIFT 14
#define TICK_NEXT ( 1 << TICK_SHIFT)
#define TICK_MASK (TICK_NEXT -1)


static INLINE Bit16s MIXER_CLIP(Bits SAMP) {
	if (SAMP < MAX_AUDIO) {
		if (SAMP > MIN_AUDIO)
			return SAMP;
		else return MIN_AUDIO;
	} else return MAX_AUDIO;
}

static struct {
	Bit32s work[MIXER_BUFSIZE][2];
	//Write/Read pointers for the buffer
	Bitu pos,done;
	Bitu needed, min_needed, max_needed;
	//For every millisecond tick how many samples need to be generated
	Bit32u tick_add;
	Bit32u tick_counter;
	float mastervol[2];
	MixerChannel * channels;
	bool nosound;
	Bit32u freq;
	Bit32u blocksize;
	//Platform sink for finished buffers, NULL in nosound mode
	EmulatorAudioDriver * audio;
	//Receives a buffer when the driver ring is full, so the work buffer can't overrun
	Bit16s * dropbuf;
} mixer;

Bit8u MixTemp[MIXER_BUFSIZE];

MixerChannel * MIXER_AddChannel(MIXER_Handler handler,Bitu freq,const char * name) {
	MixerChannel * chan=new MixerChannel();
	chan->scale = 1.0;
	chan->handler=handler;
	chan->name=name;
	chan->SetFreq(freq);
	chan->next=mixer.channels;
	chan->SetVolume(1,1);
	chan->enabled=false;
	chan->interpolate = false;
	mixer.channels=chan;
	return chan;
}

MixerChannel * MIXER_FindChannel(const char * name) {
	MixerChannel * chan=mixer.channels;
	while (chan) {
		if (!strcasecmp(chan->name,name)) break;
		chan=chan->next;
	}
	return chan;
}

void MIXER_DelChannel(MixerChannel* delchan) {
	MixerChannel * chan=mixer.channels;
	MixerChannel * * where=&mixer.channels;
	while (chan) {
		if (chan==delchan) {
			*where=chan->next;
			delete delchan;
			return;
		}
		where=&chan->next;
		chan=chan->next;
	}
}

void MixerChannel::UpdateVolume(void) {
	volmul[0]=(Bits)((1 << MIXER_VOLSHIFT)*scale*volmain[0]*mixer.mastervol[0]);
	volmul[1]=(Bits)((1 << MIXER_VOLSHIFT)*scale*volmain[1]*mixer.mastervol[1]);
}

void MixerChannel::SetVolume(float _left,float _right) {
	volmain[0]=_left;
	volmain[1]=_right;
	UpdateVolume();
}

void MixerChannel::SetScale( float f ) {
	scale = f;
	UpdateVolume();
}

void MixerChannel::Enable(bool _yesno) {
	if (_yesno==enabled) return;
	enabled=_yesno;
	if (enabled) {
		freq_counter = 0;
		if (done<mixer.done) done=mixer.done;
	}
}

void MixerChannel::SetFreq(Bitu freq) {
	freq_add=(freq<<FREQ_SHIFT)/mixer.freq;

	if (freq != mixer.freq) {
		interpolate = true;
	}
	else {
		interpolate = false;
	}
}

void MixerChannel::Mix(Bitu _needed) {
	needed=_needed;
	while (enabled && needed>done) {
		Bitu left = (needed - done);
		left *= freq_add;
		left  = (left >> FREQ_SHIFT) + ((left & FREQ_MASK)!=0);
		handler(left);
	}
}

void MixerChannel::AddSilence(void) {
	if (done<needed) {
		done=needed;
		//Make sure the next samples are zero when they get switched to prev
		nextSample[0] = 0;
		nextSample[1] = 0;
		//This should trigger an instant request for new samples
		freq_counter = FREQ_NEXT;
	}
}

template<class Type,bool stereo,bool signeddata,bool nativeorder>
inline void MixerChannel::AddSamples(Bitu len, const Type* data) {
	//Position where to write the data
	Bitu mixpos = mixer.pos + done;
	//Position in the incoming data
	Bitu pos = 0;
	//Mix and data for the full length
	while (1) {
		//Does new data need to get read?
		while (freq_counter >= FREQ_NEXT) {
			//Would this overflow the source data, then it's time to leave
			if (pos >= len)
				return;
			freq_counter -= FREQ_NEXT;
			prevSample[0] = nextSample[0];
			if (stereo) {
				prevSample[1] = nextSample[1];
			}
			if ( sizeof( Type) == 1) {
				if (!signeddata) {
					if (stereo) {
						nextSample[0]=(((Bit8s)(data[pos*2+0] ^ 0x80)) << 8);
						nextSample[1]=(((Bit8s)(data[pos*2+1] ^ 0x80)) << 8);
					} else {
						nextSample[0]=(((Bit8s)(data[pos] ^ 0x80)) << 8);
					}
				} else {
					if (stereo) {
						nextSample[0]=(data[pos*2+0] << 8);
						nextSample[1]=(data[pos*2+1] << 8);
					} else {
						nextSample[0]=(data[pos] << 8);
					}
				}
			//16bit and 32bit both contain 16bit data internally
			} else  {
				if (signeddata) {
					if (stereo) {
						if (nativeorder) {
							nextSample[0]=data[pos*2+0];
							nextSample[1]=data[pos*2+1];
						} else {
							if ( sizeof( Type) == 2) {
								nextSample[0]=(Bit16s)host_readw((HostPt)&data[pos*2+0]);
								nextSample[1]=(Bit16s)host_readw((HostPt)&data[pos*2+1]);
							} else {
								nextSample[0]=(Bit32s)host_readd((HostPt)&data[pos*2+0]);
								nextSample[1]=(Bit32s)host_readd((HostPt)&data[pos*2+1]);
							}
						}
					} else {
						if (nativeorder) {
							nextSample[0] = data[pos];
						} else {
							if ( sizeof( Type) == 2) {
								nextSample[0]=(Bit16s)host_readw((HostPt)&data[pos]);
							} else {
								nextSample[0]=(Bit32s)host_readd((HostPt)&data[pos]);
							}
						}
					}
				} else {
					if (stereo) {
						if (nativeorder) {
							nextSample[0]=(Bits)data[pos*2+0]-32768;
							nextSample[1]=(Bits)data[pos*2+1]-32768;
						} else {
							if ( sizeof( Type) == 2) {
								nextSample[0]=(Bits)host_readw((HostPt)&data[pos*2+0])-32768;
								nextSample[1]=(Bits)host_readw((HostPt)&data[pos*2+1])-32768;
							} else {
								nextSample[0]=(Bits)host_readd((HostPt)&data[pos*2+0])-32768;
								nextSample[1]=(Bits)host_readd((HostPt)&data[pos*2+1])-32768;
							}
						}
					} else {
						if (nativeorder) {
							nextSample[0]=(Bits)data[pos]-32768;
						} else {
							if ( sizeof( Type) == 2) {
								nextSample[0]=(Bits)host_readw((HostPt)&data[pos])-32768;
							} else {
								nextSample[0]=(Bits)host_readd((HostPt)&data[pos])-32768;
							}
						}
					}
				}
			}
			//This sample has been handled now, increase position
			pos++;
		}
		//Where to write
		mixpos &= MIXER_BUFMASK;
		Bit32s* write = mixer.work[mixpos];
		if (!interpolate) {
			write[0] += prevSample[0] * volmul[0];
			write[1] += (stereo ? prevSample[1] : prevSample[0]) * volmul[1];
		}
		else {
			Bits diff_mul = freq_counter & FREQ_MASK;
			Bits sample = prevSample[0] + (((nextSample[0] - prevSample[0]) * diff_mul) >> FREQ_SHIFT);
			write[0] += sample*volmul[0];
			if (stereo) {
				sample = prevSample[1] + (((nextSample[1] - prevSample[1]) * diff_mul) >> FREQ_SHIFT);
			}
			write[1] += sample*volmul[1];
		}
		//Prepare for next sample
		freq_counter += freq_add;
		mixpos++;
		done++;
	}
}

void MixerChannel::AddStretched(Bitu len,Bit16s * data) {
	if (done >= needed) {
		LOG_MSG("Can't add, buffer full");
		return;
	}
	//Target samples this inputs gets stretched into
	Bitu outlen = needed - done;
	Bitu index = 0;
	Bitu index_add = (len << FREQ_SHIFT)/outlen;
	Bitu mixpos = mixer.pos + done;
	done = needed;
	Bitu pos = 0;

	while (outlen--) {
		Bitu new_pos = index >> FREQ_SHIFT;
		if (pos != new_pos) {
			pos = new_pos;
			//Forward the previous sample
			prevSample[0] = data[0];
			data++;
		}
		Bits diff = data[0] - prevSample[0];
		Bits diff_mul = index & FREQ_MASK;
		index += index_add;
		mixpos &= MIXER_BUFMASK;
		Bits sample = prevSample[0] + ((diff * diff_mul) >> FREQ_SHIFT);
		mixer.work[mixpos][0] += sample * volmul[0];
		mixer.work[mixpos][1] += sample * volmul[1];
		mixpos++;
	}
}

void MixerChannel::AddSamples_m8(Bitu len, const Bit8u * data) {
	AddSamples<Bit8u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s8(Bitu len,const Bit8u * data) {
	AddSamples<Bit8u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m8s(Bitu len,const Bit8s * data) {
	AddSamples<Bit8s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s8s(Bitu len,const Bit8s * data) {
	AddSamples<Bit8s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s16(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16u(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s16u(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m32(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s32(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16_nonnative(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s16_nonnative(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,true,true,false>(len,data);
}
void MixerChannel::AddSamples_m16u_nonnative(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,false,false,false>(len,data);
}
void MixerChannel::AddSamples_s16u_nonnative(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,true,false,false>(len,data);
}
void MixerChannel::AddSamples_m32_nonnative(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s32_nonnative(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,true,true,false>(len,data);
}

void MixerChannel::FillUp(void) {
	if (!enabled || done<mixer.done)
		return;
	float index=PIC_TickIndex();
	Mix((Bitu)(index*mixer.needed));
}

extern bool ticksLocked;
static inline bool Mixer_irq_important(void) {
	/* In some states correct timing of the irqs is more important then
	 * non stuttering audo */
	return (ticksLocked || (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)));
}

static Bit32u calc_tickadd(Bit32u freq) {
#if TICK_SHIFT >16
	Bit64u freq64 = static_cast<Bit64u>(freq);
	freq64 = (freq64<<TICK_SHIFT)/1000;
	Bit32u r = static_cast<Bit32u>(freq64);
	return r;
#else
	return (freq<<TICK_SHIFT)/1000;
#endif
}

/* Mix a certain amount of new samples */
static void MIXER_MixData(Bitu needed) {
	MixerChannel * chan=mixer.channels;
	while (chan) {
		chan->Mix(needed);
		chan=chan->next;
	}
	if (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)) {
		Bit16s convert[1024][2];
		Bitu added=needed-mixer.done;
		if (added>1024)
			added=1024;
		Bitu readpos=(mixer.pos+mixer.done)&MIXER_BUFMASK;
		for (Bitu i=0;i<added;i++) {
			Bits sample=mixer.work[readpos][0] >> MIXER_VOLSHIFT;
			convert[i][0]=MIXER_CLIP(sample);
			sample=mixer.work[readpos][1] >> MIXER_VOLSHIFT;
			convert[i][1]=MIXER_CLIP(sample);
			readpos=(readpos+1)&MIXER_BUFMASK;
		}
		CAPTURE_AddWave( mixer.freq, added, (Bit16s*)convert );
	}
	//Reset the the tick_add for constant speed
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.freq);
	mixer.done = needed;
}

static void MIXER_Pump(void);

static void MIXER_Mix(void) {
	MIXER_MixData(mixer.needed);
	mixer.tick_counter += mixer.tick_add;
	mixer.needed+=(mixer.tick_counter >> TICK_SHIFT);
	mixer.tick_counter &= TICK_MASK;
	MIXER_Pump();
}

static void MIXER_Mix_NoSound(void) {
	MIXER_MixData(mixer.needed);
	/* Clear piece we've just generated */
	for (Bitu i=0;i<mixer.needed;i++) {
		mixer.work[mixer.pos][0]=0;
		mixer.work[mixer.pos][1]=0;
		mixer.pos=(mixer.pos+1)&MIXER_BUFMASK;
	}
	/* Reduce count in channels */
	for (MixerChannel * chan=mixer.channels;chan;chan=chan->next) {
		if (chan->done>mixer.needed) chan->done-=mixer.needed;
		else chan->done=0;
	}
	/* Set values for next tick */
	mixer.tick_counter += mixer.tick_add;
	mixer.needed = (mixer.tick_counter >> TICK_SHIFT);
	mixer.tick_counter &= TICK_MASK;
	mixer.done=0;
}

/* Take one driver buffer (need frames) out of the work buffer.
 * This is the old SDL audio callback body. SDL called it at the hardware's
 * pace; now MIXER_Pump decides when to call it.
 * rate is the driver's dynamic rate multiplier: the buffer is built from
 * need*rate source frames resampled to need output frames, so a rate above 1
 * drains the work buffer faster and makes the driver ring grow more slowly.
 * Returns false if nothing was written to output. */
static bool MIXER_Pull(Bit16s * output, Bitu need, double rate) {
	Bitu src=(Bitu)(need*rate);	//Source frames consumed for 'need' output frames
	Bitu reduce;
	Bitu pos;
	//Local resampling counter to manipulate the data when sending it off to the driver
	Bitu index, index_add;
	Bits sample;
	/* Enough room in the buffer ? */
	if (mixer.done < src) {
//		LOG_MSG("Full underrun need %d, have %d, min %d", src, mixer.done, mixer.min_needed);
		if((src - mixer.done) > (src >>7) ) //Max 1 procent stretch.
			return false;
		reduce = mixer.done;
		index_add = (reduce << TICK_SHIFT) / need;
		mixer.tick_add = calc_tickadd(mixer.freq+mixer.min_needed);
	} else if (mixer.done < mixer.max_needed) {
		Bitu left = mixer.done - src;
		if (left < mixer.min_needed) {
			if( !Mixer_irq_important() ) {
				Bitu needed = mixer.needed - src;
				Bitu diff = (mixer.min_needed>needed?mixer.min_needed:needed) - left;
				mixer.tick_add = calc_tickadd(mixer.freq+(diff*3));
				left = 0; //No stretching as we compensate with the tick_add value
			} else {
				left = (mixer.min_needed - left);
				left = 1 + (2*left) / mixer.min_needed; //left=1,2,3
			}
//			LOG_MSG("needed underrun need %d, have %d, min %d, left %d", need, mixer.done, mixer.min_needed, left);
			reduce = src - left;
			index_add = (reduce << TICK_SHIFT) / need;
		} else {
			reduce = src;
			index_add = (reduce << TICK_SHIFT) / need;
//			LOG_MSG("regular run need %d, have %d, min %d, left %d", need, mixer.done, mixer.min_needed, left);

			/* Mixer tick value being updated:
			 * 3 cases:
			 * 1) A lot too high. >division by 5. but maxed by 2* min to prevent too fast drops.
			 * 2) A little too high > division by 8
			 * 3) A little to nothing above the min_needed buffer > go to default value
			 */
			Bitu diff = left - mixer.min_needed;
			if(diff > (mixer.min_needed<<1)) diff = mixer.min_needed<<1;
			if(diff > (mixer.min_needed>>1))
				mixer.tick_add = calc_tickadd(mixer.freq-(diff/5));
			else if (diff > (mixer.min_needed>>2))
				mixer.tick_add = calc_tickadd(mixer.freq-(diff>>3));
			else
				mixer.tick_add = calc_tickadd(mixer.freq);
		}
	} else {
		/* There is way too much data in the buffer */
//		LOG_MSG("overflow run need %d, have %d, min %d", need, mixer.done, mixer.min_needed);
		if (mixer.done > MIXER_BUFSIZE)
			index_add = MIXER_BUFSIZE - 2*mixer.min_needed;
		else
			index_add = mixer.done - 2*mixer.min_needed;
		index_add = (index_add << TICK_SHIFT) / need;
		reduce = mixer.done - 2* mixer.min_needed;
		mixer.tick_add = calc_tickadd(mixer.freq-(mixer.min_needed/5));
	}
	/* Reduce done count in all channels */
	for (MixerChannel * chan=mixer.channels;chan;chan=chan->next) {
		if (chan->done>reduce) chan->done-=reduce;
		else chan->done=0;
	}

	// Reset mixer.tick_add when irqs are important
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.freq);

	mixer.done -= reduce;
	mixer.needed -= reduce;
	pos = mixer.pos;
	mixer.pos = (mixer.pos + reduce) & MIXER_BUFMASK;
	index = 0;
	if(need != reduce) {
		while (need--) {
			Bitu i = (pos + (index >> TICK_SHIFT )) & MIXER_BUFMASK;
			index += index_add;
			sample=mixer.work[i][0]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
			sample=mixer.work[i][1]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
		}
		/* Clean the used buffer */
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			mixer.work[pos][0]=0;
			mixer.work[pos][1]=0;
			pos++;
		}
	} else {
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			sample=mixer.work[pos][0]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
			sample=mixer.work[pos][1]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
			mixer.work[pos][0]=0;
			mixer.work[pos][1]=0;
			pos++;
		}
	}
	return true;
}

/* Hand finished buffers to the platform audio driver.
 * Runs on the emulation thread at the end of every mixer tick; it replaces
 * the SDL audio thread, so all mixer state is now single-threaded and the
 * SDL_LockAudio/SDL_UnlockAudio pairs are gone. The driver's DMA interrupt
 * only touches the driver's own ring indices. */
static void MIXER_Pump(void) {
	EmulatorAudioDriver * audio=mixer.audio;
	if (!audio) return;
	const Bitu need=mixer.blocksize;	//Frames per driver buffer
	for (;;) {
		double rate=audio->getDynamicRate();
		Bitu src=(Bitu)(need*rate);
		/* Keep the prebuffer behind the read position, as the SDL callback aimed to */
		if (mixer.done < src + mixer.min_needed) break;
		if (audio->canWrite()) {
			if (!MIXER_Pull((Bit16s *)audio->getWriteBuffer(),need,rate)) break;
			audio->commitWrite();
		} else if (mixer.done > mixer.max_needed) {
			/* Driver ring is full (emulation running ahead of real time):
			 * drop a buffer so the work buffer can't overrun */
			if (!MIXER_Pull(mixer.dropbuf,need,rate)) break;
		} else break;
	}
}

static void MIXER_Stop(Section* sec) {
	if (mixer.audio) {
		mixer.audio=0;
		platform->getAudio()->stopEmulatorAudio();
	}
	delete [] mixer.dropbuf;
	mixer.dropbuf=0;
}

class MIXER : public Program {
public:
	void MakeVolume(char * scan,float & vol0,float & vol1) {
		Bitu w=0;
		bool db=(toupper(*scan)=='D');
		if (db) scan++;
		while (*scan) {
			if (*scan==':') {
				++scan;w=1;
			}
			char * before=scan;
			float val=(float)strtod(scan,&scan);
			if (before==scan) {
				++scan;continue;
			}
			if (!db) val/=100;
			else val=powf(10.0f,(float)val/20.0f);
			if (val<0) val=1.0f;
			if (!w) {
				vol0=val;
			} else {
				vol1=val;
			}
		}
		if (!w) vol1=vol0;
	}

	void Run(void) {
		if(cmd->FindExist("/LISTMIDI")) {
			ListMidi();
			return;
		}
		if (cmd->FindString("MASTER",temp_line,false)) {
			MakeVolume((char *)temp_line.c_str(),mixer.mastervol[0],mixer.mastervol[1]);
		}
		MixerChannel * chan = mixer.channels;
		while (chan) {
			if (cmd->FindString(chan->name,temp_line,false)) {
				MakeVolume((char *)temp_line.c_str(),chan->volmain[0],chan->volmain[1]);
			}
			chan->UpdateVolume();
			chan = chan->next;
		}
		if (cmd->FindExist("/NOSHOW")) return;
		WriteOut("Channel  Main    Main(dB)\n");
		ShowVolume("MASTER",mixer.mastervol[0],mixer.mastervol[1]);
		for (chan = mixer.channels;chan;chan = chan->next)
			ShowVolume(chan->name,chan->volmain[0],chan->volmain[1]);
	}
private:
	void ShowVolume(const char * name,float vol0,float vol1) {
		WriteOut("%-8s %3.0f:%-3.0f  %+3.2f:%-+3.2f \n",name,
			vol0*100,vol1*100,
			20*log(vol0)/log(10.0f),20*log(vol1)/log(10.0f)
		);
	}

	void ListMidi(){
		if(midi.handler) midi.handler->ListAll(this);
	};

};

static void MIXER_ProgramStart(Program * * make) {
	*make=new MIXER;
}

MixerChannel* MixerObject::Install(MIXER_Handler handler,Bitu freq,const char * name){
	if(!installed) {
		if(strlen(name) > 31) E_Exit("Too long mixer channel name");
		safe_strncpy(m_name,name,32);
		installed = true;
		return MIXER_AddChannel(handler,freq,name);
	} else {
		E_Exit("already added mixer channel.");
		return 0; //Compiler happy
	}
}

MixerObject::~MixerObject(){
	if(!installed) return;
	MIXER_DelChannel(MIXER_FindChannel(m_name));
}


void MIXER_Init(Section* sec) {
	sec->AddDestroyFunction(&MIXER_Stop);

	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Read out config section */
	mixer.freq=section->Get_int("rate");
	mixer.nosound=section->Get_bool("nosound");
	mixer.blocksize=section->Get_int("blocksize");

	/* Initialize the internal stuff */
	mixer.channels=0;
	mixer.pos=0;
	mixer.done=0;
	memset(mixer.work,0,sizeof(mixer.work));
	mixer.mastervol[0]=1.0f;
	mixer.mastervol[1]=1.0f;

	/* The mixer feeds the platform's EmulatorAudioDriver. The hardware plays
	 * whatever it is given at its fixed rate, so the driver's rate and buffer
	 * size override the config values (same role as "got different values"). */
	mixer.audio=0;
	mixer.dropbuf=0;
	EmulatorAudioDriver * audio=0;
	if (!mixer.nosound && platform && platform->getAudio())
		audio=platform->getAudio()->getEmulatorAudio();

	mixer.tick_counter=0;
	if (mixer.nosound) {
		LOG_MSG("MIXER: No Sound Mode Selected.");
		mixer.tick_add=calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else if (!audio) {
		mixer.nosound = true;
		LOG_MSG("MIXER: No emulator audio driver, running in nosound mode.");
		mixer.tick_add=calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else {
		const Bit32u freq=(Bit32u)audio->getSampleRate();
		const Bit32u blocksize=(Bit32u)audio->getFramesPerBuffer();
		if((mixer.freq != freq) || (mixer.blocksize != blocksize))
			LOG_MSG("MIXER: Audio driver is fixed at freq %d, blocksize %d",freq,blocksize);
		mixer.freq=freq;
		mixer.blocksize=blocksize;
		mixer.tick_add=calc_tickadd(mixer.freq);
		mixer.dropbuf=new Bit16s[2*mixer.blocksize];
		mixer.audio=audio;
		TIMER_AddTickHandler(MIXER_Mix);
		/* Take the audio interface over from the menu (replaces SDL_PauseAudio(0)) */
		platform->getAudio()->stopMenuAudio();
		platform->getAudio()->startEmulatorAudio();
	}
	mixer.min_needed=section->Get_int("prebuffer");
	if (mixer.min_needed>100) mixer.min_needed=100;
	mixer.min_needed=(mixer.freq*mixer.min_needed)/1000;
	mixer.max_needed=mixer.blocksize * 2 + 2*mixer.min_needed;
	mixer.needed=mixer.min_needed+1;
	PROGRAMS_MakeFile("MIXER.COM",MIXER_ProgramStart);
}
