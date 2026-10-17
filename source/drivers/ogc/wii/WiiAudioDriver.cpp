/****************************************************************************
 * Platform Abstraction Layer
 * Daryl Borth 2009-2026
 * WiiAudioDriver.cpp
 ***************************************************************************/
#include <ogc/dsp.h>
#include <ogcsys.h>
#include <unistd.h>

#include "WiiAudioDriver.h"
#include "../OgcEmulatorAudio.h"

static WiiAudioDriver *instance = nullptr;

void WiiAudioDriver::init() {
	instance = this;
	AUDIO_Init(NULL);
	streamVolume = 127;
	emulatorAudio = new OgcEmulatorAudio();
	emulatorAudio->init();
}

void WiiAudioDriver::startEmulatorAudio() {
	emulatorAudio->startAudio();
}

void WiiAudioDriver::stopEmulatorAudio() {
	emulatorAudio->stopAudio();
}

void WiiAudioDriver::startMenuAudio() {

}

void WiiAudioDriver::stopMenuAudio() {

}

void WiiAudioDriver::shutdown() {
	instance = nullptr;
}

WiiAudioDriver::~WiiAudioDriver() {
	delete emulatorAudio;
}
