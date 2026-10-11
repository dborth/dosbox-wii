/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutSystemKeyboard.cpp
 ***************************************************************************/
#include <coreinit/memdefaultheap.h>
#include <nn/swkbd.h>
#include <vpad/input.h>

#include "WutSystemKeyboard.h"
#include "WutInputDriver.h"
#include "../Logger.h"

// A fade in or out is a few dozen frames; this only stops a keyboard that
// never reaches Hidden from looping forever
static const int kMaxPumpFrames = 240;

WutSystemKeyboard::WutSystemKeyboard(WutInputDriver * input)
	: input(input)
{
}

WutSystemKeyboard::~WutSystemKeyboard()
{
	destroy();
}

bool WutSystemKeyboard::ensureCreated()
{
	if(created)
		return true;

	FSInit();
	fsClient = static_cast<FSClient *>(MEMAllocFromDefaultHeap(sizeof(FSClient)));
	if(!fsClient)
		return false;

	if(FSAddClient(fsClient, FS_ERROR_FLAG_NONE) != FS_STATUS_OK)
	{
		MEMFreeToDefaultHeap(fsClient);
		fsClient = nullptr;
		return false;
	}

	nn::swkbd::CreateArg createArg;
	createArg.regionType = nn::swkbd::RegionType::USA;
	createArg.workMemory = MEMAllocFromDefaultHeap(nn::swkbd::GetWorkMemorySize(0));
	createArg.fsClient = fsClient;

	if(!createArg.workMemory || !nn::swkbd::Create(createArg))
	{
		LOG_ERROR("nn::swkbd::Create failed");

		if(createArg.workMemory)
			MEMFreeToDefaultHeap(createArg.workMemory);

		FSDelClient(fsClient, FS_ERROR_FLAG_NONE);
		MEMFreeToDefaultHeap(fsClient);
		fsClient = nullptr;
		return false;
	}

	workMemory = createArg.workMemory;
	nn::swkbd::MuteAllSound(false);
	created = true;
	return true;
}

bool WutSystemKeyboard::appear()
{
	nn::swkbd::AppearArg arg;
	arg.keyboardArg.configArg.languageType = nn::swkbd::LanguageType::English;
	arg.keyboardArg.configArg.controllerType = nn::swkbd::ControllerType::DrcGamepad;
	arg.keyboardArg.configArg.keyboardMode = nn::swkbd::KeyboardMode::Full;
	// The confirm button is DOS's Enter key
	arg.keyboardArg.configArg.okString = u"Enter";
	// Picking a suggested word replaces what was typed, which DOS cannot follow
	arg.keyboardArg.configArg.showWordSuggestions = false;
	arg.inputFormArg.hintText = u"";

	return nn::swkbd::AppearInputForm(arg);
}

bool WutSystemKeyboard::open()
{
	if(opened)
		return true;

	if(!ensureCreated())
		return false;

	// One that is still fading out from last time
	pumpUntilHidden();

	if(!appear())
	{
		LOG_ERROR("nn::swkbd::AppearInputForm failed");
		return false;
	}

	opened = true;
	return true;
}

void WutSystemKeyboard::close()
{
	if(!opened)
		return;

	opened = false;
	nn::swkbd::DisappearInputForm();
	pumpUntilHidden();
}

void WutSystemKeyboard::calc()
{
	VPADStatus vpad;
	const bool haveVpad = input && input->takeVpadSample(&vpad);

	nn::swkbd::ControllerInfo controllerInfo;
	controllerInfo.vpad = haveVpad ? &vpad : nullptr;
	nn::swkbd::Calc(controllerInfo);

	if(nn::swkbd::IsNeedCalcSubThreadFont())
		nn::swkbd::CalcSubThreadFont();

	if(nn::swkbd::IsNeedCalcSubThreadPredict())
		nn::swkbd::CalcSubThreadPredict();
}

void WutSystemKeyboard::pumpUntilHidden()
{
	for(int i = 0; i < kMaxPumpFrames && nn::swkbd::GetStateInputForm() != nn::swkbd::State::Hidden; i++)
		calc();
}

void WutSystemKeyboard::drawGamePad()
{
	if(!opened)
		return;

	calc();
	nn::swkbd::DrawDRC();
}

const char16_t * WutSystemKeyboard::getText() const
{
	if(!opened)
		return u"";

	const char16_t * text = nn::swkbd::GetInputFormString();
	return text ? text : u"";
}

bool WutSystemKeyboard::takeOk()
{
	if(!opened || !nn::swkbd::IsDecideOkButton(nullptr))
		return false;

	// The form stays decided until it is shown again, so show it again, empty
	nn::swkbd::DisappearInputForm();
	pumpUntilHidden();
	appear();
	return true;
}

bool WutSystemKeyboard::takeCancel()
{
	return opened && nn::swkbd::IsDecideCancelButton(nullptr);
}

void WutSystemKeyboard::destroy()
{
	if(!created)
		return;

	close();
	nn::swkbd::Destroy();
	MEMFreeToDefaultHeap(workMemory);
	workMemory = nullptr;

	FSDelClient(fsClient, FS_ERROR_FLAG_NONE);
	MEMFreeToDefaultHeap(fsClient);
	fsClient = nullptr;
	created = false;
}
