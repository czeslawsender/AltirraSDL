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
#include "blackboxfloppy.h"
#include "uiconfgeneric.h"

bool ATUIConfDevBlackBoxFloppy(VDGUIHandle hParent, ATPropertySet& props) {
	return ATUIShowDialogGenericConfig(hParent, props, L"Black Box Floppy Board",
		[](IATUIConfigView& view) {
			VDStringA tag;
			VDStringW label;
			for(int slot = 0; slot < 4; ++slot) {
				tag.sprintf("driveslot%u", slot);
				label.sprintf(L"PBI Floppy %u", slot);

				auto& driveOption = view.AddIntDropDown();
				
				driveOption.SetLabel(label.c_str()).SetTag(tag.c_str());

				driveOption.AddChoice(0, L"Not connected");

				for(int i=1; i<15; ++i) {
					label.sprintf(L"D%d:", i);

					driveOption.AddChoice(i, label.c_str());
				}

				tag.sprintf("drivetype%u", slot);

				auto& driveTypeOption = view.AddDropDown<ATBlackBoxFloppyType>();
				
				driveTypeOption.SetTag(tag.c_str());

				driveTypeOption.SetDefaultValue(ATBlackBoxFloppyType::FiveInch180K);
				driveTypeOption.AddChoice(ATBlackBoxFloppyType::FiveInch180K, L"180K 5.25\" 40 track, single-sided");
				driveTypeOption.AddChoice(ATBlackBoxFloppyType::FiveInch360K, L"360K 5.25\" 40 track, double-sided");
				driveTypeOption.AddChoice(ATBlackBoxFloppyType::FiveInch12M, L"1.2M 5.25\" 80 track, double-sided HD");
				driveTypeOption.AddChoice(ATBlackBoxFloppyType::ThreeInch360K, L"360K 3.5\" 80 track, single-sided");
				driveTypeOption.AddChoice(ATBlackBoxFloppyType::ThreeInch720K, L"720K 3.5\" 80 track, double-sided");
				driveTypeOption.AddChoice(ATBlackBoxFloppyType::ThreeInch144M, L"1.4M 3.5\" 80 track, double-sided HD");
				driveTypeOption.AddChoice(ATBlackBoxFloppyType::EightInch1M, L"1M 8\" 77 track, double-sided HD");

				auto& driveMappingTypeOption = view.AddDropDown<ATBlackBoxFloppyMappingType>();
				
				tag.sprintf("drivemapping%u", slot);
				driveMappingTypeOption.SetTag(tag.c_str());

				driveMappingTypeOption.SetDefaultValue(ATBlackBoxFloppyMappingType::XF551);
				driveMappingTypeOption.AddChoice(ATBlackBoxFloppyMappingType::XF551, L"Map double-sided as XF551");
				driveMappingTypeOption.AddChoice(ATBlackBoxFloppyMappingType::ATR8000, L"Map double-sided as ATR8000");
				driveMappingTypeOption.AddChoice(ATBlackBoxFloppyMappingType::Percom, L"Map double-sided as PERCOM");

				view.AddVerticalSpace();
			}
		}
	);
}
