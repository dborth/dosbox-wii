/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiSound.cpp
 *
 * Generic - Everything platform-specific lives behind audioSystem
 ***************************************************************************/

#include "Gui.h"

int GuiSound::defaultMusicVolume = 100;
int GuiSound::defaultSfxVolume = 100;
GuiSound* GuiSound::defaultOver = nullptr;
GuiSound* GuiSound::defaultClick = nullptr;
GuiSound* GuiSound::playingOGG = nullptr;
GuiSound* GuiSound::activeMusic = nullptr;

GuiSound::GuiSound() { }

GuiSound::GuiSound(const uint8_t * s, int32_t l, SOUND t)
{
	sound = s;
	length = l;
	type = t;
	voice = -1;
	volume = 100;
	loop = false;
}

GuiSound::~GuiSound()
{

}

void GuiSound::play()
{

}

void GuiSound::stop()
{

}

void GuiSound::pause()
{

}

void GuiSound::resume()
{

}

bool GuiSound::isPlaying()
{
	return false;
}

void GuiSound::setVolume(int vol)
{

}

void GuiSound::setLoop(bool l)
{
	loop = l;
}

void GuiSound::setDefaultSounds(GuiSound * over, GuiSound * click)
{
	defaultOver = over;
	defaultClick = click;
}

void GuiSound::setDefaultVolume(VOLUME_TYPE t, int v)
{
	if (t == VOLUME_TYPE::SFX) {
		defaultSfxVolume = v;
	}
}
