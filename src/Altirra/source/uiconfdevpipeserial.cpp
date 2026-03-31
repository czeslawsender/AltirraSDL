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

bool ATUIConfDevPipeSerial(VDGUIHandle hParent, ATPropertySet& props) {
	return ATUIShowDialogGenericConfig(hParent, props, L"Named pipe serial device",
		[](IATUIConfigView& view) {
			view.AddStringEdit().SetDefault(L"AltirraSerial", true).SetLabel(L"&Pipe Name").SetTag("pipe_name");

			view.AddIntEdit().SetRange(1, 1000000).SetDefault(9600, true).SetLabel(L"&Baud Rate").SetTag("baud_rate");
		}
	);
}
