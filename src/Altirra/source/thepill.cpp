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
#include <at/atcore/devicecart.h>
#include "thepill.h"
#include "memorymanager.h"

void ATCreateDeviceThePill(const ATPropertySet& pset, IATDevice **dev) {
	vdrefptr<ATDeviceThePill> p(new ATDeviceThePill);
	p->SetSettings(pset);

	*dev = p.release();
}

extern const ATDeviceDefinition g_ATDeviceDefThePill = { "thepill", nullptr, L"The Pill", ATCreateDeviceThePill };

ATDeviceThePill::ATDeviceThePill() {
	SetSaveStateAgnostic();
}

void ATDeviceThePill::GetDeviceInfo(ATDeviceInfo& info) {
	info.mpDef = &g_ATDeviceDefThePill;
}

void ATDeviceThePill::Init() {
	mpMemMan = GetService<ATMemoryManager>();

	ATMemoryHandlerTable handlers {};
	handlers.mbPassReads = false;
	handlers.mbPassAnticReads = false;
	handlers.mbPassWrites = false;
	handlers.mpDebugReadHandler = [](void *, uint32) -> sint32 { return 0xFF; };
	handlers.mpReadHandler = [](void *, uint32) -> sint32 { return 0xFF; };
	handlers.mpWriteHandler = [](void *, uint32, uint8) { return false; };

	mpMemoryLayer = mpMemMan->CreateLayer(kATMemoryPri_Cartridge1, handlers, 0x80, 0x40);

	mpCartPort->AddCartridge(this, kATCartridgePriority_Default, mCartId);
}

void ATDeviceThePill::Shutdown() {
	if (mpMemMan) {
		mpMemMan->DeleteLayerPtr(&mpMemoryLayer);
		mpMemMan = nullptr;
	}

	if (mpCartPort) {
		if (mCartId) {
			mpCartPort->RemoveCartridge(mCartId, this);
			mCartId = 0;
		}

		mpCartPort = nullptr;
	}
}

uint32 ATDeviceThePill::GetSupportedButtons() const {
	return (UINT32_C(1) << kATDeviceButton_CartridgeSwitch);
}

bool ATDeviceThePill::IsButtonDepressed(ATDeviceButton idx) const {
	if (idx == kATDeviceButton_CartridgeSwitch)
		return mbActive;

	return false;
}

void ATDeviceThePill::ActivateButton(ATDeviceButton idx, bool state) {
	if (idx == kATDeviceButton_CartridgeSwitch) {
		if (mbActive != state) {
			mbActive = state;

			mpMemMan->EnableLayer(mpMemoryLayer, kATMemoryAccessMode_W, state);
		}
	}
}

void ATDeviceThePill::InitCartridge(IATDeviceCartridgePort *cartPort) {
	mpCartPort = cartPort;
}

bool ATDeviceThePill::IsLeftCartActive() const {
	return mbActive;
}

void ATDeviceThePill::SetCartEnables(bool leftEnable, bool rightEnable, bool cctlEnable) {
	uint32 startPage = 0x80;
	uint32 endPage = 0xC0;

	if (!leftEnable)
		startPage = 0xA0;

	if (!rightEnable)
		endPage = 0xA0;

	if (mpMemoryLayer)
		mpMemMan->SetLayerMaskRange(mpMemoryLayer, startPage, endPage - startPage);
}

void ATDeviceThePill::UpdateCartSense(bool leftActive) {
}
