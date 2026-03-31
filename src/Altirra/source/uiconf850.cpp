//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2011 Avery Lee
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

#include <stdafx.h>
#include <at/atcore/propertyset.h>
#include <at/atnativeui/dialog.h>
#include <at/atnativeui/uiproxies.h>
#include "resource.h"
#include "simulator.h"
#include "rs232.h"
#include "uiconfgeneric.h"

extern ATSimulator g_sim;

class ATUIDialogDevice850 : public VDDialogFrameW32 {
public:
	ATUIDialogDevice850(ATPropertySet& props);

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);

	ATPropertySet& mPropSet;
	VDUIProxyComboBoxControl mComboSioModes;

	static const wchar_t *const kSioEmuModes[];
};

const wchar_t *const ATUIDialogDevice850::kSioEmuModes[]={
	L"None - Emulated R: handler only",
	L"Minimal - Emulated R: handler + stub loader only",
	L"Full - SIO protocol and 6502 R: handler",
};

ATUIDialogDevice850::ATUIDialogDevice850(ATPropertySet& props)
	: VDDialogFrameW32(IDD_SERIAL_PORTS)
	, mPropSet(props)
{
}

bool ATUIDialogDevice850::OnLoaded() {
	AddProxy(&mComboSioModes, IDC_SIOLEVEL);

	for(size_t i=0; i<vdcountof(kSioEmuModes); ++i)
		mComboSioModes.AddItem(kSioEmuModes[i]);

	return VDDialogFrameW32::OnLoaded();
}

void ATUIDialogDevice850::OnDataExchange(bool write) {
	if (write) {
		mPropSet.Clear();
		
		if (IsButtonChecked(IDC_DISABLE_THROTTLING))
			mPropSet.SetBool("unthrottled", true);

		if (IsButtonChecked(IDC_EXTENDED_BAUD_RATES))
			mPropSet.SetBool("baudex", true);

		int sioLevel = mComboSioModes.GetSelection();
		if (sioLevel >= 0 && sioLevel < kAT850SIOEmulationLevelCount)
			mPropSet.SetUint32("emulevel", (uint32)sioLevel);
	} else {
		uint32 level = mPropSet.GetUint32("emulevel", 0);
		if (level >= kAT850SIOEmulationLevelCount)
			level = 0;

		mComboSioModes.SetSelection(level);

		CheckButton(IDC_DISABLE_THROTTLING, mPropSet.GetBool("unthrottled", false));
		CheckButton(IDC_EXTENDED_BAUD_RATES, mPropSet.GetBool("baudex", false));
	}
}

bool ATUIConfDev850(VDGUIHandle h, ATPropertySet& props) {
	ATUIDialogDevice850 dlg(props);

	return dlg.ShowDialog(h) != 0;
}

////////////////////////////////////////////////////////////////////////////////

bool ATUIConfDev850Full(VDGUIHandle h, ATPropertySet& props) {
	return ATUIShowDialogGenericConfig(h,
		props,
		L"850 Interface Module (full emulation) Options",
		[](IATUIConfigView& view) {
			const auto setBaudRates = [](IATUIConfigIntDropDownView& dropDownView) -> IATUIConfigIntDropDownView& {
				return dropDownView
					.AddChoice(0, L"Auto")
					.AddChoice(2, L"45.5 baud")
					.AddChoice(3, L"50 baud")
					.AddChoice(4, L"56.875 baud")
					.AddChoice(5, L"75 baud")
					.AddChoice(6, L"110 baud")
					.AddChoice(7, L"134.5 baud")
					.AddChoice(8, L"150 baud")
					.AddChoice(1, L"300 baud")
					.AddChoice(10, L"600 baud")
					.AddChoice(11, L"1200 baud")
					.AddChoice(12, L"1800 baud")
					.AddChoice(13, L"2400 baud")
					.AddChoice(14, L"4800 baud")
					.AddChoice(15, L"9600 baud");
			};

			setBaudRates(view.AddIntDropDown())
				.SetTag("serbaud1")
				.SetLabel(L"Port 1 baud rate");

			setBaudRates(view.AddIntDropDown())
				.SetTag("serbaud2")
				.SetLabel(L"Port 2 baud rate");

			setBaudRates(view.AddIntDropDown())
				.SetTag("serbaud3")
				.SetLabel(L"Port 3 baud rate");

			setBaudRates(view.AddIntDropDown())
				.SetTag("serbaud4")
				.SetLabel(L"Port 4 baud rate");
		}
	);
}
