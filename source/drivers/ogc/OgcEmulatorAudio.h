/****************************************************************************
 * DOSBox Wii
 * Daryl Borth 2008-2026
 * OgcEmulatorAudio.h
 *
 * 32 kHz direct-queued audio driver with dynamic rate control.
 ***************************************************************************/
#pragma once

#include <gctypes.h>
#include "../EmulatorAudioDriver.h"

// Hardware DMA callback trampoline, registered with AUDIO_RegisterDMACallback()
// which requires a bare C function pointer.
void AudioDMACallback();

class OgcEmulatorAudio : public EmulatorAudioDriver
{
	public:
		//! Output format. The core's mixer must be configured to produce
		//! exactly this (signed 16-bit, stereo, interleaved L/R, host
		//! endian) - the hardware plays whatever it is given at this rate.
		static constexpr int SAMPLE_RATE = 32000;

		//! Stereo frames per queued buffer. 544 frames = exactly 17.0 ms at
		//! 32 kHz, close to the 16.67 ms of the 48 kHz driver so the queue
		//! thresholds in the .cpp keep their tuned timing. Must keep
		//! BYTES_PER_BUFFER a multiple of 32 (AUDIO_InitDMA length unit).
		static constexpr int FRAMES_PER_BUFFER = 544;
		static constexpr int BYTES_PER_BUFFER = FRAMES_PER_BUFFER * 4;

		OgcEmulatorAudio();
		~OgcEmulatorAudio() override;

		void init() override {}
		void resetAudio() override;

		//! Takes over the AI: sets the DSP sample rate to 32 kHz (ASND_Init
		//! forces 48 kHz), clears the queue and registers the DMA callback.
		//! DMA itself starts from commitWrite() once pre-roll is queued.
		void startAudio();

		//! Unregisters the DMA callback and halts DMA. Leaves the ring
		//! contents as they are; startAudio() clears them on re-entry.
		void stopAudio();

		int getUnplayed() override;

		double getDynamicRate() override;
		bool canWrite() override;
		uint16_t* getWriteBuffer() override;
		void commitWrite() override;

		int getSampleRate() override { return SAMPLE_RATE; }
		int getFramesPerBuffer() override { return FRAMES_PER_BUFFER; }

		// Called only via the AudioDMACallback trampoline above.
		void dmaCallback();

	private:
		// BUFFERCOUNT must be a power of two so the ring index can advance
		// with a cheap bitwise mask (see nextIndex) instead of an integer modulo.
		static constexpr int BUFFERCOUNT = 16;
		static constexpr int MAX_QUEUED_BUFFERS = 12; // Leave a 4-buffer safety zone to prevent input lag

		// Number of stereo frames over which we ramp to/from zero when the
		// ring runs genuinely dry. 64 frames = 2 ms at 32 kHz: long enough
		// to remove the audible click of a hard jump to silence, short
		// enough to add no perceptible latency.
		static constexpr int FADE_FRAMES = 64;

		// Discrete state of the dynamic-rate controller (hysteresis pitch
		// bending). Only touched outside interrupt context (getDynamicRate),
		// so it needs no synchronization.
		enum RateState {
			RATE_STATE_NEUTRAL,
			RATE_STATE_DRAINING,  // running slow to shrink an over-full queue
			RATE_STATE_FILLING,   // running fast to grow an under-full queue
		};

		static int nextIndex(int current) { return (current + 1) & (BUFFERCOUNT - 1); }
		int getUnplayedInternal() const { return (nextab - playab + BUFFERCOUNT) & (BUFFERCOUNT - 1); }

		void buildFadeOutBuffer();
		void applyFadeIn(u8* buf);

		u8 soundbuffer[BUFFERCOUNT][BYTES_PER_BUFFER] __attribute__((aligned(32)));
		u8 fadeBuffer[BYTES_PER_BUFFER] __attribute__((aligned(32)));

		// Volatile indices crossing the emulator-thread/ISR boundary (MUST bypass registers)
		volatile int playab;
		volatile int nextab;

		// Only touched outside interrupt context (no volatile needed)
		bool dma_started;
		RateState rateState;

		// Declick state -- tracks the tail of the last real audio actually
		// queued, so a starvation event can ramp down from where the
		// waveform really was instead of snapping to zero, and ramp back in
		// the same way on recovery.
		s16 lastL;
		s16 lastR;
		bool wasStarved;
};
