//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2024 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along
//	with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef f_AT_INPUTDEFS_H
#define f_AT_INPUTDEFS_H

#include <vd2/system/vdtypes.h>

// Input codes map to raw inputs from host devices.
enum ATInputCode : uint32 {
	kATInputCode_None			= 0x00,

	kATInputCode_KeyBack		= 0x08,		// VK_BACK
	kATInputCode_KeyTab			= 0x09,		// VK_TAB
	kATInputCode_KeyReturn		= 0x0D,		// VK_RETURN
	kATInputCode_KeyEscape		= 0x1B,		// VK_ESCAPE
	kATInputCode_KeySpace		= 0x20,		// VK_SPACE
	kATInputCode_KeyPrior		= 0x21,		// VK_PRIOR
	kATInputCode_KeyNext		= 0x22,		// VK_NEXT
	kATInputCode_KeyEnd			= 0x23,		// VK_END
	kATInputCode_KeyHome		= 0x24,		// VK_HOME
	kATInputCode_KeyLeft		= 0x25,		// VK_LEFT
	kATInputCode_KeyUp			= 0x26,		// VK_UP
	kATInputCode_KeyRight		= 0x27,		// VK_RIGHT
	kATInputCode_KeyDown		= 0x28,		// VK_DOWN
	kATInputCode_KeyInsert		= 0x2D,		// VK_INSERT
	kATInputCode_KeyDelete		= 0x2E,		// VK_DELETE
	kATInputCode_Key0			= 0x30,		// VK_0
	kATInputCode_Key1			= 0x31,		//
	kATInputCode_Key2			= 0x32,		//
	kATInputCode_Key3			= 0x33,		//
	kATInputCode_Key4			= 0x34,		//
	kATInputCode_Key5			= 0x35,		//
	kATInputCode_Key6			= 0x36,		//
	kATInputCode_Key7			= 0x37,		//
	kATInputCode_Key8			= 0x38,		//
	kATInputCode_Key9			= 0x39,		//
	kATInputCode_KeyA			= 0x41,		// VK_A
	kATInputCode_KeyB			= 0x42,		//
	kATInputCode_KeyC			= 0x43,		//
	kATInputCode_KeyD			= 0x44,		//
	kATInputCode_KeyE			= 0x45,		//
	kATInputCode_KeyF			= 0x46,		//
	kATInputCode_KeyG			= 0x47,		//
	kATInputCode_KeyH			= 0x48,		//
	kATInputCode_KeyI			= 0x49,		//
	kATInputCode_KeyJ			= 0x4A,		//
	kATInputCode_KeyK			= 0x4B,		//
	kATInputCode_KeyL			= 0x4C,		//
	kATInputCode_KeyM			= 0x4D,		//
	kATInputCode_KeyN			= 0x4E,		//
	kATInputCode_KeyO			= 0x4F,		//
	kATInputCode_KeyP			= 0x50,		//
	kATInputCode_Keyq			= 0x51,		//
	kATInputCode_KeyR			= 0x52,		//
	kATInputCode_KeyS			= 0x53,		//
	kATInputCode_KeyT			= 0x54,		//
	kATInputCode_KeyU			= 0x55,		//
	kATInputCode_KeyV			= 0x56,		//
	kATInputCode_KeyW			= 0x57,		//
	kATInputCode_KeyX			= 0x58,		//
	kATInputCode_KeyY			= 0x59,		//
	kATInputCode_KeyZ			= 0x5A,		//
	kATInputCode_KeyNumpad0		= 0x60,		// VK_NUMPAD0
	kATInputCode_KeyNumpad1		= 0x61,		// VK_NUMPAD1
	kATInputCode_KeyNumpad2		= 0x62,		// VK_NUMPAD2
	kATInputCode_KeyNumpad3		= 0x63,		// VK_NUMPAD3
	kATInputCode_KeyNumpad4		= 0x64,		// VK_NUMPAD4
	kATInputCode_KeyNumpad5		= 0x65,		// VK_NUMPAD5
	kATInputCode_KeyNumpad6		= 0x66,		// VK_NUMPAD6
	kATInputCode_KeyNumpad7		= 0x67,		// VK_NUMPAD7
	kATInputCode_KeyNumpad8		= 0x68,		// VK_NUMPAD8
	kATInputCode_KeyNumpad9		= 0x69,		// VK_NUMPAD9
	kATInputCode_KeyMultiply	= 0x6A,		// VK_MULTIPLY
	kATInputCode_KeyAdd			= 0x6B,		// VK_ADD
	kATInputCode_KeySubtract	= 0x6D,		// VK_SUBTRACT
	kATInputCode_KeyDecimal		= 0x6E,		// VK_DECIMAL
	kATInputCode_KeyDivide		= 0x6F,		// VK_DIVIDE
	kATInputCode_KeyF1			= 0x70,		// VK_F1
	kATInputCode_KeyF2			= 0x71,		// VK_F2
	kATInputCode_KeyF3			= 0x72,		// VK_F3
	kATInputCode_KeyF4			= 0x73,		// VK_F4
	kATInputCode_KeyF5			= 0x74,		// VK_F5
	kATInputCode_KeyF6			= 0x75,		// VK_F6
	kATInputCode_KeyF7			= 0x76,		// VK_F7
	kATInputCode_KeyF8			= 0x77,		// VK_F8
	kATInputCode_KeyF9			= 0x78,		// VK_F9
	kATInputCode_KeyF10			= 0x79,		// VK_F10
	kATInputCode_KeyF11			= 0x7A,		// VK_F11
	kATInputCode_KeyF12			= 0x7B,		// VK_F12
	kATInputCode_KeyLShift		= 0xA0,		// VK_LSHIFT
	kATInputCode_KeyRShift		= 0xA1,		// VK_RSHIFT
	kATInputCode_KeyLControl	= 0xA2,		// VK_LCONTROL
	kATInputCode_KeyRControl	= 0xA3,		// VK_RCONTROL
	kATInputCode_KeyOem1		= 0xBA,		// VK_OEM_1   // ';:' for US
	kATInputCode_KeyOemPlus		= 0xBB,		// VK_OEM_PLUS   // '+' any country
	kATInputCode_KeyOemComma	= 0xBC,		// VK_OEM_COMMA   // ',' any country
	kATInputCode_KeyOemMinus	= 0xBD,		// VK_OEM_MINUS   // '-' any country
	kATInputCode_KeyOemPeriod	= 0xBE,		// VK_OEM_PERIOD   // '.' any country
	kATInputCode_KeyOem2		= 0xBF,		// VK_OEM_2   // '/?' for US
	kATInputCode_KeyOem3		= 0xC0,		// VK_OEM_3   // '`~' for US
	kATInputCode_KeyOem4		= 0xDB,		// VK_OEM_4  //  '[{' for US
	kATInputCode_KeyOem5		= 0xDC,		// VK_OEM_5  //  '\|' for US
	kATInputCode_KeyOem6		= 0xDD,		// VK_OEM_6  //  ']}' for US
	kATInputCode_KeyOem7		= 0xDE,		// VK_OEM_7  //  ''"' for US
	kATInputCode_KeyNumpadEnter	= 0x10D,	// VK_RETURN (extended)

	kATInputCode_MouseClass		= 0x1000,
	kATInputCode_MouseHoriz		= 0x1000,
	kATInputCode_MouseVert		= 0x1001,
	kATInputCode_MousePadX		= 0x1002,
	kATInputCode_MousePadY		= 0x1003,
	kATInputCode_MouseBeamX		= 0x1004,
	kATInputCode_MouseBeamY		= 0x1005,
	kATInputCode_MouseEmuStickX	= 0x1006,
	kATInputCode_MouseEmuStickY	= 0x1007,
	kATInputCode_MouseLeft		= 0x1100,
	kATInputCode_MouseRight		= 0x1101,
	kATInputCode_MouseUp		= 0x1102,
	kATInputCode_MouseDown		= 0x1103,
	kATInputCode_MouseWheelUp	= 0x1104,
	kATInputCode_MouseWheelDown	= 0x1105,
	kATInputCode_MouseWheel		= 0x1106,
	kATInputCode_MouseHWheelLeft	= 0x1107,
	kATInputCode_MouseHWheelRight	= 0x1108,
	kATInputCode_MouseHWheel		= 0x1109,
	kATInputCode_MouseLMB		= 0x1800,
	kATInputCode_MouseMMB		= 0x1801,
	kATInputCode_MouseRMB		= 0x1802,
	kATInputCode_MouseX1B		= 0x1803,
	kATInputCode_MouseX2B		= 0x1804,

	kATInputCode_JoyClass		= 0x2000,
	kATInputCode_JoyHoriz1		= 0x2000,
	kATInputCode_JoyVert1		= 0x2001,
	kATInputCode_JoyVert2		= 0x2002,
	kATInputCode_JoyHoriz3		= 0x2003,
	kATInputCode_JoyVert3		= 0x2004,
	kATInputCode_JoyVert4		= 0x2005,
	kATInputCode_JoyPOVHoriz	= 0x2006,
	kATInputCode_JoyPOVVert		= 0x2007,
	kATInputCode_JoyStick1Left	= 0x2100,
	kATInputCode_JoyStick1Right	= 0x2101,
	kATInputCode_JoyStick1Up	= 0x2102,
	kATInputCode_JoyStick1Down	= 0x2103,
	kATInputCode_JoyStick2Up	= 0x2104,
	kATInputCode_JoyStick2Down	= 0x2105,
	kATInputCode_JoyStick3Left	= 0x2106,
	kATInputCode_JoyStick3Right	= 0x2107,
	kATInputCode_JoyStick3Up	= 0x2108,
	kATInputCode_JoyStick3Down	= 0x2109,
	kATInputCode_JoyStick4Up	= 0x210A,
	kATInputCode_JoyStick4Down	= 0x210B,
	kATInputCode_JoyPOVLeft		= 0x210C,
	kATInputCode_JoyPOVRight	= 0x210D,
	kATInputCode_JoyPOVUp		= 0x210E,
	kATInputCode_JoyPOVDown		= 0x210F,
	kATInputCode_JoyButton0		= 0x2800,

	kATInputCode_ClassMask		= 0xF000,
	kATInputCode_IdMask			= 0xFFFF,

	kATInputCode_FlagCheck0		= 0x00010000,
	kATInputCode_FlagCheck1		= 0x00020000,
	kATInputCode_FlagCheckMask	= 0x00030000,
	kATInputCode_FlagValue0		= 0x00040000,
	kATInputCode_FlagValue1		= 0x00080000,
	kATInputCode_FlagValueMask	= 0x000C0000,
	kATInputCode_FlagMask		= 0x000F0000,

	kATInputCode_SpecificUnit	= 0x80000000,
	kATInputCode_UnitScale		= 0x01000000,
	kATInputCode_UnitShift		= 24
};

enum ATInputControllerType : uint32 {
	kATInputControllerType_None,
	kATInputControllerType_Joystick,
	kATInputControllerType_Paddle,
	kATInputControllerType_STMouse,
	kATInputControllerType_Console,
	kATInputControllerType_5200Controller,
	kATInputControllerType_InputState,
	kATInputControllerType_LightGun,
	kATInputControllerType_Tablet,
	kATInputControllerType_KoalaPad,
	kATInputControllerType_AmigaMouse,
	kATInputControllerType_Keypad,
	kATInputControllerType_Trackball_CX80,
	kATInputControllerType_5200Trackball,
	kATInputControllerType_Driving,
	kATInputControllerType_Keyboard,
	kATInputControllerType_LightPen,
	kATInputControllerType_PowerPad,
	kATInputControllerType_LightPenStack
};

bool ATInputIs5200ControllerType(ATInputControllerType type);

// Input triggers identify the target on emulated controller.
enum ATInputTrigger : uint32 {
	kATInputTrigger_Button0		= 0x0000,
	kATInputTrigger_Up			= 0x0100,
	kATInputTrigger_Down		= 0x0101,
	kATInputTrigger_Left		= 0x0102,
	kATInputTrigger_Right		= 0x0103,
	kATInputTrigger_ScrollUp	= 0x0104,
	kATInputTrigger_ScrollDown	= 0x0105,
	kATInputTrigger_Start		= 0x0200,
	kATInputTrigger_Select		= 0x0201,
	kATInputTrigger_Option		= 0x0202,
	kATInputTrigger_Turbo		= 0x0203,
	kATInputTrigger_ColdReset	= 0x0204,
	kATInputTrigger_WarmReset	= 0x0205,
	kATInputTrigger_Rewind		= 0x0206,
	kATInputTrigger_RewindMenu	= 0x0207,
	kATInputTrigger_KeySpace	= 0x0300,
	kATInputTrigger_5200_0		= 0x0400,
	kATInputTrigger_5200_1		= 0x0401,
	kATInputTrigger_5200_2		= 0x0402,
	kATInputTrigger_5200_3		= 0x0403,
	kATInputTrigger_5200_4		= 0x0404,
	kATInputTrigger_5200_5		= 0x0405,
	kATInputTrigger_5200_6		= 0x0406,
	kATInputTrigger_5200_7		= 0x0407,
	kATInputTrigger_5200_8		= 0x0408,
	kATInputTrigger_5200_9		= 0x0409,
	kATInputTrigger_5200_Star	= 0x040A,
	kATInputTrigger_5200_Pound	= 0x040B,
	kATInputTrigger_5200_Start	= 0x040C,
	kATInputTrigger_5200_Pause	= 0x040D,
	kATInputTrigger_5200_Reset	= 0x040E,
	kATInputTrigger_UILeft		= 0x0500,
	kATInputTrigger_UIRight		= 0x0501,
	kATInputTrigger_UIUp		= 0x0502,
	kATInputTrigger_UIDown		= 0x0503,
	kATInputTrigger_UIAccept	= 0x0504,	// PSx[X], Xbox[A]
	kATInputTrigger_UIReject	= 0x0505,	// PSx[O], Xbox[B]
	kATInputTrigger_UIMenu		= 0x0506,	// PSx[T], Xbox[Y]
	kATInputTrigger_UIOption	= 0x0507,	// PSx[S], Xbox[X]
	kATInputTrigger_UISwitchLeft	= 0x0508,
	kATInputTrigger_UISwitchRight	= 0x0509,
	kATInputTrigger_UILeftShift		= 0x050A,
	kATInputTrigger_UIRightShift	= 0x050B,
	kATInputTrigger_Axis0		= 0x0800,
	kATInputTrigger_Flag0		= 0x0900,
	kATInputTrigger_ClassMask	= 0xFF00,
	kATInputTrigger_Mask		= 0xFFFF,

	// D2D: Button state as usual.
	// D2A: Absolute positioning.
	// A2D: Threshold.
	// A2A: Absolute positioning.
	kATInputTriggerMode_Default		= 0x00000000,

	kATInputTriggerMode_AutoFire	= 0x00010000,

	kATInputTriggerMode_Toggle		= 0x00020000,
	kATInputTriggerMode_ToggleAF	= 0x00030000,

	// D2D: N/A
	// D2A: Accumulate deltas.
	// A2D: N/A
	// A2A: Accumulate deltas.
	kATInputTriggerMode_Relative	= 0x00040000,

	// D2D: N/A
	// D2A: Position -> Value.
	// A2D: N/A
	// A2A: Position -> Value.
	kATInputTriggerMode_Absolute	= 0x00050000,

	// D2D: Starts on, invert state.
	// D2A: N/A
	// A2D: N/A
	// A2A: Axis reversed.
	kATInputTriggerMode_Inverted	= 0x00060000,

	kATInputTriggerMode_Mask		= 0x000F0000,
	kATInputTriggerSpeed_Mask		= 0x00F00000,
	kATInputTriggerSpeed_Shift		= 20,
	kATInputTriggerAccel_Mask		= 0x0F000000,
	kATInputTriggerAccel_Shift		= 24
};

enum class ATInputPointerCoordinateSpace : uint8 {
	None,
	Normalized,
	Beam
};

#endif
