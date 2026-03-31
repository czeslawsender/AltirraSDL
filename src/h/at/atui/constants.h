//	Altirra - Atari 800/800XL emulator
//	Copyright (C) 2008 Avery Lee
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
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef f_AT_ATUI_CONSTANTS_H
#define f_AT_ATUI_CONSTANTS_H

#include <vd2/system/vdtypes.h>

enum ATUITouchMode : uint32 {				//	flicks		press-and-hold		gestures
	kATUITouchMode_Default,					//	on			on					on
	kATUITouchMode_Immediate,				//	off			off					off
	kATUITouchMode_Direct,					//	off			on					off
	kATUITouchMode_VerticalPan,				//	off			on					vertical pan only
	kATUITouchMode_2DPan,					//	off			on					on
	kATUITouchMode_2DPanSmooth,				//	off			on					on, pan gutters disabled
	kATUITouchMode_MultiTouch,				//	off			on					off, registered as touch window
	kATUITouchMode_MultiTouchImmediate,		//	off			off					off, registered as touch window
	kATUITouchMode_Dynamic,					//	<determined by lookup>
	kATUITouchMode_MultiTouchDynamic		//	<determined by lookup>
};

enum ATUICursorImage : uint32 {
	kATUICursorImage_None,
	kATUICursorImage_Hidden,
	kATUICursorImage_Arrow,
	kATUICursorImage_IBeam,
	kATUICursorImage_Cross,
	kATUICursorImage_Query,
	kATUICursorImage_Move,
	kATUICursorImage_SizeHoriz,
	kATUICursorImage_SizeVert,
	kATUICursorImage_SizeDiagFwd,
	kATUICursorImage_SizeDiagRev,
	kATUICursorImage_Target,
	kATUICursorImage_TargetOff
};

// Currently these need to match the Win32 definitions.
enum ATUIVirtKey {
	kATUIVK_LButton	= 0x01,
	kATUIVK_RButton	= 0x02,
	kATUIVK_Cancel	= 0x03,
	kATUIVK_MButton	= 0x04,
	kATUIVK_XButton1= 0x05,
	kATUIVK_XButton2= 0x06,
	kATUIVK_Back	= 0x08,		// Backspace
	kATUIVK_Tab		= 0x09,		// VK_TAB
	kATUIVK_Return	= 0x0D,
	kATUIVK_Shift	= 0x10,
	kATUIVK_Control	= 0x11,
	kATUIVK_Alt		= 0x12,
	kATUIVK_Pause	= 0x13,
	kATUIVK_CapsLock= 0x14,
	kATUIVK_Escape	= 0x1B,		// VK_ESCAPE
	kATUIVK_Space	= 0x20,		// VK_SPACE
	kATUIVK_Prior	= 0x21,		// VK_PRIOR
	kATUIVK_Next	= 0x22,		// VK_NEXT
	kATUIVK_0		= 0x30,
	kATUIVK_A		= 0x41,
	kATUIVK_End		= 0x23,
	kATUIVK_Home	= 0x24,
	kATUIVK_Left	= 0x25,		// Left arrow
	kATUIVK_Up		= 0x26,		// Up arrow
	kATUIVK_Right	= 0x27,		// Right arrow
	kATUIVK_Down	= 0x28,		// Down arrow
	kATUIVK_Delete	= 0x2E,
	kATUIVK_F1		= 0x70,		// VK_F1
	kATUIVK_F2		= 0x71,		// VK_F2
	kATUIVK_F3		= 0x72,		// VK_F3
	kATUIVK_F4		= 0x73,		// VK_F4
	kATUIVK_F5		= 0x74,		// VK_F5
	kATUIVK_F6		= 0x75,		// VK_F6
	kATUIVK_F7		= 0x76,		// VK_F7
	kATUIVK_F8		= 0x77,		// VK_F8
	kATUIVK_F9		= 0x78,		// VK_F9
	kATUIVK_F10		= 0x79,		// VK_F10
	kATUIVK_F11		= 0x7A,		// VK_F11
	kATUIVK_F12		= 0x7B,		// VK_F12
	kATUIVK_LShift	= 0xA0,		// VK_LSHIFT
	kATUIVK_RShift	= 0xA1,		// VK_RSHIFT
	kATUIVK_LControl= 0xA2,		// VK_LCONTROL
	kATUIVK_RControl= 0xA3,		// VK_RCONTROL
	kATUIVK_LAlt	= 0xA4,		// VK_LMENU
	kATUIVK_RAlt	= 0xA5,		// VK_RMENU
	kATUIVK_NumpadEnter	= 0x10D,	// VK_RETURN (extended)

	kATUIVK_UILeft		= 0x0500,
	kATUIVK_UIRight		= 0x0501,
	kATUIVK_UIUp		= 0x0502,
	kATUIVK_UIDown		= 0x0503,
	kATUIVK_UIAccept	= 0x0504,	// PSx[X], Xbox[A]
	kATUIVK_UIReject	= 0x0505,	// PSx[O], Xbox[B]
	kATUIVK_UIMenu		= 0x0506,	// PSx[T], Xbox[Y]
	kATUIVK_UIOption	= 0x0507,	// PSx[S], Xbox[X]
	kATUIVK_UISwitchLeft	= 0x0508,
	kATUIVK_UISwitchRight	= 0x0509,
	kATUIVK_UILeftShift		= 0x050A,
	kATUIVK_UIRightShift	= 0x050B,
};

#endif
