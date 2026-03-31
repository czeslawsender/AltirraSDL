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
#include "uiconfgeneric.h"

bool ATUIConfDevKarinMaxiDrive(VDGUIHandle hParent, ATPropertySet& props) {
	return ATUIShowDialogGenericConfig(hParent, props, L"Karin Maxi Drive",
		[](IATUIConfigView& view) {
			view.AddIntDropDown()
				.SetLabel(L"Hardware Version")
				.SetTag("hwversion")
				.AddChoice(0, L"Original")
				.AddChoice(1, L"PBI selection fix")
				.SetDefaultValue(0)
				;

			view.AddVerticalSpace();

			for(int i=0; i<2; ++i) {
				view.AddIntDropDown()
					.SetLabel(i ? L"Drive 2" : L"Drive 1")
					.SetTag(i ? "drivetype2" : "drivetype1")
					.AddChoice(0, L"None")
					.AddChoice(1, L"5.25\" Drive (40 tracks)")
					.AddChoice(2, L"5.25\" Drive (80 tracks)")
					.AddChoice(3, L"3.5\" Drive")
					.SetDefaultValue(i ? 0 : 1)
					;
			}

			view.AddVerticalSpace();

			view.AddIntDropDown()
				.SetLabel(L"SW1 Drive IDs")
				.SetTag("sw1")
				.AddChoice(0, L"D1: and D2:")
				.AddChoice(1, L"D2: and D3:")
				;

			view.AddIntDropDown()
				.SetLabel(L"SW2 Config")
				.SetTag("sw2")
				.AddChoice(0, L"Automatic")
				.AddChoice(1, L"Manual (SW3-SW6)")
				;

			view.AddIntDropDown()
				.SetLabel(L"SW3 Drive 2 Step")
				.SetTag("sw3")
				.AddChoice(0, L"3ms")
				.AddChoice(1, L"6ms")
				;

			view.AddIntDropDown()
				.SetLabel(L"SW4 Drive 1 Step")
				.SetTag("sw4")
				.AddChoice(0, L"3ms")
				.AddChoice(1, L"6ms")
				;
		}
	);
}
