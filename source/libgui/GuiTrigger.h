#pragma once


//!Menu input trigger management. Determine if action is neccessary based on input data by comparing controller input data to a specific trigger element.
class GuiTrigger
{
	public:
		//!Constructor
		GuiTrigger();
		//!Destructor
		~GuiTrigger();
		//!Sets a simple trigger. Requires: element is selected, and trigger button is pressed
		//!\param ch Controller channel number
		//!\param wiibtns Wii controller trigger button(s) - classic controller buttons are considered separately
		//!\param gcbtns GameCube controller trigger button(s)
		void SetSimpleTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		//!Sets a held trigger. Requires: element is selected, and trigger button is pressed
		//!\param ch Controller channel number
		//!\param wiibtns Wii controller trigger button(s) - classic controller buttons are considered separately
		//!\param gcbtns GameCube controller trigger button(s)
		void SetHeldTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		//!Sets a button-only trigger. Requires: Trigger button is pressed
		//!\param ch Controller channel number
		//!\param wiibtns Wii controller trigger button(s) - classic controller buttons are considered separately
		//!\param gcbtns GameCube controller trigger button(s)
		void SetButtonOnlyTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		//!Sets a button-only trigger. Requires: trigger button is pressed and parent window of element is in focus
		//!\param ch Controller channel number
		//!\param wiibtns Wii controller trigger button(s) - classic controller buttons are considered separately
		//!\param gcbtns GameCube controller trigger button(s)
		void SetButtonOnlyInFocusTrigger(s32 ch, u32 wiibtns, u16 gcbtns);
		//!Get X or Y value from Wii Joystick (classic, nunchuk) input
		//!\param stick Controller stick (left = 0, right = 1)
		//!\param axis Controller stick axis (x-axis = 0, y-axis = 1)
		//!\return Stick value
		s8 WPAD_Stick(u8 stick, int axis);
		//!Get X value from Wii Joystick (classic, nunchuk) input
		//!\param stick Controller stick (left = 0, right = 1)
		//!\return Stick value
		s8 WPAD_StickX(u8 stick);
		//!Get Y value from Wii Joystick (classic, nunchuk) input
		//!\param stick Controller stick (left = 0, right = 1)
		//!\return Stick value
		s8 WPAD_StickY(u8 stick);
		//!Move menu selection left (via pad/joystick). Allows scroll delay and button overriding
		//!\return true if selection should be moved left, false otherwise
		bool Left();
		//!Move menu selection right (via pad/joystick). Allows scroll delay and button overriding
		//!\return true if selection should be moved right, false otherwise
		bool Right();
		//!Move menu selection up (via pad/joystick). Allows scroll delay and button overriding
		//!\return true if selection should be moved up, false otherwise
		bool Up();
		//!Move menu selection down (via pad/joystick). Allows scroll delay and button overriding
		//!\return true if selection should be moved down, false otherwise
		bool Down();

		WPADData wpaddata; //!< Wii controller trigger data
		PADData pad; //!< GameCube controller trigger data
		WPADData * wpad; //!< Wii controller trigger
		s32 chan; //!< Trigger controller channel (0-3, -1 for all)
		u8 type; //!< trigger type (TRIGGER_SIMPLE,	TRIGGER_HELD, TRIGGER_BUTTON_ONLY, TRIGGER_BUTTON_ONLY_IN_FOCUS)
};

extern GuiTrigger userInput[4];
