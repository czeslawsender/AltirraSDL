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

bool ATUIConfDevMultiplexer(VDGUIHandle hParent, ATPropertySet& props) {
	return ATUIShowDialogGenericConfig(hParent, props, L"Multiplexer",
		[](IATUIConfigView& view) {
			auto& combo = view.AddIntDropDown();
			combo.AddChoice(-1, L"Host");
			combo.AddChoice(0, L"Client (ID 1)");
			combo.AddChoice(1, L"Client (ID 2)");
			combo.AddChoice(2, L"Client (ID 3)");
			combo.AddChoice(3, L"Client (ID 4)");
			combo.AddChoice(4, L"Client (ID 5)");
			combo.AddChoice(5, L"Client (ID 6)");
			combo.AddChoice(6, L"Client (ID 7)");
			combo.AddChoice(7, L"Client (ID 8)");
			combo.SetValue(-1);

			combo.SetLabel(L"Device ID").SetTag("device_id");

			view.AddVerticalSpace();

			auto& hostView = view.AddStringEdit();
			hostView.SetLabel(L"Host Address");
			hostView.SetTag("host_address");

			hostView.SetEnableExpr(
				[&]() -> bool {
					return combo.GetValue() >= 0;
				}
			);

			auto& portView = view.AddIntEdit();
			portView.SetDefault(6522);
			portView.SetLabel(L"TCP Port");
			portView.SetTag("port");

			auto& listenExt = view.AddCheckbox();
			listenExt.SetText(L"Allow connections from other computers");
			listenExt.SetTag("allow_external");

			listenExt.SetEnableExpr(
				[&]() -> bool {
					return combo.GetValue() < 0;
				}
			);
		}
	);
}
