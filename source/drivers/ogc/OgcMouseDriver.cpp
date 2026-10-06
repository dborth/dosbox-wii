/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcMouseDriver.cpp
 ***************************************************************************/
#include <ogc/machine/processor.h>

#ifdef HW_RVL
#include <ogc/usbmouse.h>
#endif

#include "OgcMouseDriver.h"

void OgcMouseDriver::init()
{
	#ifdef HW_RVL
	if (!initialized)
		initialized = (MOUSE_Init() >= 0);
	#endif
}

void OgcMouseDriver::shutdown()
{
	#ifdef HW_RVL
	if (initialized)
		MOUSE_Deinit();
	#endif
	initialized = false;
}

bool OgcMouseDriver::poll(MouseEvent & out)
{
	#ifdef HW_RVL
	mouse_event me;

	if (initialized && MOUSE_GetEvent(&me)) {
		out.dx = me.rx;
		out.dy = me.ry;
		out.buttons = 0;
		if (me.button & 0x1) out.buttons |= MOUSE_BTN_LEFT;
		if (me.button & 0x2) out.buttons |= MOUSE_BTN_RIGHT;
		if (me.button & 0x4) out.buttons |= MOUSE_BTN_MIDDLE;
		return true;
	}
	#else
	(void)out;
	#endif
	return false;
}
