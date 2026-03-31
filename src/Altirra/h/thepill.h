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

#ifndef f_AT_THEPILL_H
#define f_AT_THEPILL_H

#include <at/atcore/deviceimpl.h>

class ATMemoryLayer;

class ATDeviceThePill final : public ATDeviceT<IATDeviceButtons, IATDeviceCartridge> {
public:
	ATDeviceThePill();

	void GetDeviceInfo(ATDeviceInfo& info) override;
	void Init() override;
	void Shutdown() override;

public:	// IATDeviceButtons
	uint32 GetSupportedButtons() const override;
	bool IsButtonDepressed(ATDeviceButton idx) const override;
	void ActivateButton(ATDeviceButton idx, bool state) override;

public:	// IATDeviceCartridge
	void InitCartridge(IATDeviceCartridgePort *cartPort) override;
	bool IsLeftCartActive() const override;
	void SetCartEnables(bool leftEnable, bool rightEnable, bool cctlEnable) override;
	void UpdateCartSense(bool leftActive) override;

private:
	ATMemoryManager *mpMemMan = nullptr;
	ATMemoryLayer *mpMemoryLayer = nullptr;
	IATDeviceCartridgePort *mpCartPort = nullptr;
	bool mbActive = false;
	uint32 mCartId = 0;
};

#endif
