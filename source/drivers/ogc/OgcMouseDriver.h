/****************************************************************************
 * Platform Abstraction Layer (OGC driver)
 * Daryl Borth 2026
 * OgcMouseDriver.h
 *
 * USB mouse via libogc's usbmouse
 ***************************************************************************/
#pragma once

#include "../MouseDriver.h"

class OgcMouseDriver : public MouseDriver
{
	public:
		void init() override;
		void shutdown() override;
		bool poll(MouseEvent & out) override;

	private:
		bool initialized = false;
};
