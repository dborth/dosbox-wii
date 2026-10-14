// The few platform pieces the widgets reach that the preview does not need for real: triggers, sounds, the
// controller and the text translator.
#include "libgui/Gui.h"

GuiTrigger::GuiTrigger() {}
void GuiTrigger::setPrimaryTrigger(int) {}
void GuiTrigger::setSecondaryTrigger(int) {}
void GuiTrigger::setSimpleTrigger(int, uint32_t) {}
void GuiTrigger::setHeldTrigger(int, uint32_t) {}
void GuiTrigger::setButtonOnlyTrigger(int, uint32_t) {}
void GuiTrigger::setButtonOnlyInFocusTrigger(int, uint32_t) {}
bool GuiTrigger::isClicked(const InputController *) const { return false; }
bool GuiTrigger::isHeld(const InputController *) const { return false; }
bool GuiTrigger::isReleased(const InputController *) const { return false; }

GuiSound::GuiSound(const uint8_t *, int32_t, SOUND) {}
GuiSound::~GuiSound() {}
void GuiSound::play() {}

bool InputController::up() const { return false; }
bool InputController::down() const { return false; }
bool InputController::left() const { return false; }
bool InputController::right() const { return false; }
bool InputController::isSecondaryPressed() const { return false; }

GuiTextTranslator::GuiTextTranslator() {}
const char * GuiTextTranslator::getText(const char * msgid) const { return msgid; }

GuiWindow * mainWindow = nullptr;
void UpdateGui() {}

// ---- threads and memory: a single-threaded host needs none of it
#include "drivers/Mutex.h"
#include "memmanager.h"
Mutex::Mutex() {}
Mutex::~Mutex() {}
void Mutex::lock() {}
void Mutex::unlock() {}
void * memspace_malloc(size_t size) { return malloc(size); }
void memspace_free(void * p) { free(p); }
GuiImageData * GuiImageDataCache::tryGet(const uint8_t *, int, int) { return nullptr; }
