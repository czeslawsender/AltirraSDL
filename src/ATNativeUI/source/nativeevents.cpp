//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2025 Avery Lee
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

#include <stdafx.h>
#include <windows.h>
#include <at/atcore/configvar.h>
#include <at/atnativeui/nativeevents.h>
#include <at/atui/constants.h>
#include <at/atui/events.h>

ATConfigVarBool g_ATCVWorkaroundsWindowsTouchKeyboardCodes("workaround.windows.touch_keyboard_codes", true);

ATUIKeyEvent ATUINativeDecodeKeyEvent(VDZWPARAM wParam, VDZLPARAM lParam) {
	ATUIKeyEvent event;
	event.mVirtKey = LOWORD(wParam);
	event.mExtendedVirtKey = event.mVirtKey;
	event.mbIsRepeat = (HIWORD(lParam) & KF_REPEAT) != 0;
	event.mbIsExtendedKey = (HIWORD(lParam) & KF_EXTENDED) != 0;

	// Fix some broken keys injected by the Windows 10/11 touch keyboard (_not_ osk.exe).
	// These keys not only have an invalid scan code, but in some cases are missing the
	// extended key flag as well.
	const uint8 scanCode = (lParam >> 16) & 0xFF;

	if (!scanCode && !event.mbIsExtendedKey && g_ATCVWorkaroundsWindowsTouchKeyboardCodes) {
		switch(event.mVirtKey) {
			case VK_LEFT:
			case VK_RIGHT:
			case VK_UP:
			case VK_DOWN:
			case VK_PRIOR:
			case VK_NEXT:
			case VK_INSERT:
			case VK_DELETE:
			case VK_HOME:
			case VK_END:
				event.mbIsExtendedKey = true;
				break;
		}
	}

	// Decode extended virt key.
	switch(event.mExtendedVirtKey) {
		case VK_RETURN:
			if (event.mbIsExtendedKey)
				event.mExtendedVirtKey = kATUIVK_NumpadEnter;
			break;

		case VK_SHIFT:
			// Windows doesn't set the ext bit for RShift, so we have to use the scan
			// code instead.
			if (MapVirtualKey(LOBYTE(HIWORD(lParam)), 3) == VK_RSHIFT)
				event.mExtendedVirtKey = kATUIVK_RShift;
			else
				event.mExtendedVirtKey = kATUIVK_LShift;
			break;

		case VK_CONTROL:
			event.mExtendedVirtKey = event.mbIsExtendedKey ? kATUIVK_RControl : kATUIVK_LControl;
			break;

		case VK_MENU:
			event.mExtendedVirtKey = event.mbIsExtendedKey ? kATUIVK_RAlt : kATUIVK_LAlt;
			break;
	}

	return event;
}

ATUICharEvent ATUINativeDecodeCharEvent(VDZWPARAM wParam, VDZLPARAM lParam) {
	ATUICharEvent event;
	event.mCh = (uint32)wParam;
	event.mScanCode = (uint32)((lParam >> 16) & 0xFF);
	event.mbIsRepeat = (lParam & 0x40000000) != 0;

	return event;
}
