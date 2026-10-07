/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutMouseDriver.h
 ***************************************************************************/
#pragma once

#include "../MouseDriver.h"

class WutMouseDriver : public MouseDriver
{
	public:
		void init() override;
		void shutdown() override;
		bool poll(MouseEvent & out) override;

	private:
		bool initialized = false;
};
