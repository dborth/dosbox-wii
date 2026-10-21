// The few platform pieces the widgets reach that the preview does not need for real: triggers, sounds, the
// controller and the text translator.
#include "libgui/Gui.h"

GuiTrigger::GuiTrigger() : type(TRIGGER_TYPE::SIMPLE), action(TRIGGER_ACTION::NONE), chan(-1), conditionMask(0) {}
void GuiTrigger::setPrimaryTrigger(int) { action = TRIGGER_ACTION::PRIMARY; }
void GuiTrigger::setSecondaryTrigger(int) { action = TRIGGER_ACTION::SECONDARY; }
void GuiTrigger::setSimpleTrigger(int, uint32_t) {}
void GuiTrigger::setHeldTrigger(int, uint32_t) {}
void GuiTrigger::setButtonOnlyTrigger(int, uint32_t) {}
void GuiTrigger::setButtonOnlyInFocusTrigger(int, uint32_t) {}
// which controller channels have a trigger pressed this frame (a test sets it; nothing set means no presses)
// (a secondary trigger, B, has a mask of its own)
static int hostPressMask = 0, hostSecondaryMask = 0;
void Host_SetPressMask(int mask) { hostPressMask = mask; }
void Host_SetSecondaryMask(int mask) { hostSecondaryMask = mask; }
bool GuiTrigger::isClicked(const InputController * c) const
{
	const int mask = action == TRIGGER_ACTION::SECONDARY ? hostSecondaryMask : hostPressMask;
	return c && c->getChannel() >= 0 && ((mask >> c->getChannel()) & 1) != 0;
}
bool GuiTrigger::isHeld(const InputController *) const { return false; }
bool GuiTrigger::isReleased(const InputController *) const { return false; }

InputController::InputController(int ch) : channel(ch), sideways(false), scrollTimer(0.0f), lastDeltaTime(1.0f / 60.0f), stickDir(StickDir::None), prevStickDir(StickDir::None), stickEdge(false), internalScrollTimer(0.0f), holdDuration(0.0f) {}
void InputController::update(const InputPadData& data, float) { currentData = data; }
bool InputController::up() const { return false; }
bool InputController::down() const { return false; }
bool InputController::left() const { return false; }
bool InputController::right() const { return false; }
bool InputController::isSecondaryPressed() const { return false; }

GuiTextTranslator::GuiTextTranslator() {}
const char * GuiTextTranslator::getText(const char * msgid) const { return msgid; }

GuiWindow * mainWindow = nullptr;
void UpdateGui() {}

// the on-screen keyboard: the preview types what a test sets (nothing set means the user cancelled)
static const char * hostKeyboardText = nullptr;
void Host_SetKeyboardText(const char * text) { hostKeyboardText = text; }
bool OnScreenKeyboard(char * var, uint32_t maxlen)
{
	if(!hostKeyboardText)
		return false;
	snprintf(var, maxlen, "%s", hostKeyboardText);
	return true;
}

// ---- threads and memory: a single-threaded host needs none of it
#include "drivers/Mutex.h"
#include "memmanager.h"
Mutex::Mutex() {}
Mutex::~Mutex() {}
void Mutex::lock() {}
void Mutex::unlock() {}
void * memspace_malloc(size_t size) { return malloc(size); }
void memspace_free(void * p) { free(p); }
