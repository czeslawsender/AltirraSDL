//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2022 Avery Lee
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
#include <vd2/system/filesys.h>
#include <at/atcore/blockdevice.h>
#include <at/atcore/deviceparent.h>
#include "devicemanager.h"
#include "uiaccessors.h"

class ATDeviceXCmdRescanDynamicDisk final : public IATDeviceXCmd {
public:
	int AddRef() override { return 2; }
	int Release() override { return 1; }

	bool IsSupported(IATDevice *dev, int busIndex) const override;
	ATDeviceXCmdInfo GetInfo() const override;
	void Invoke(ATDeviceManager& devMgr, IATDevice *dev, int busIndex) override;
} g_ATDeviceXCmdRescanDynamicDisk;

bool ATDeviceXCmdRescanDynamicDisk::IsSupported(IATDevice *dev, int busIndex) const {
	return vdpoly_cast<IATBlockDeviceDynamic *>(dev) != nullptr && busIndex < 0;
}

ATDeviceXCmdInfo ATDeviceXCmdRescanDynamicDisk::GetInfo() const {
	ATDeviceXCmdInfo info;

	info.mbRequiresElevation = false;
	info.mDisplayName = L"Rescan Dynamic Disk";
	return info;
}

void ATDeviceXCmdRescanDynamicDisk::Invoke(ATDeviceManager& devmgr, IATDevice *dev, int busIndex) {
	IATBlockDeviceDynamic *bd = vdpoly_cast<IATBlockDeviceDynamic *>(dev);

	if (!bd)
		throw MyError("This device cannot be rescanned.");

	// disconnect the disk -- note that there's a chance the parent bus may vanish,
	// which we must handle (this can happen with deprecated buses)
	IATDeviceParent * parent = dev->GetParent();
	const uint32 parentBusId = dev->GetParentBusId();

	if (parent)
		parent->GetDeviceBusById(parentBusId)->RemoveChildDevice(dev);

	try {
		bd->RescanDynamicDisk();
	} catch(const MyError&) {
		try {
			if (parent) {
				IATDeviceBus *bus = parent->GetDeviceBusById(parentBusId);

				if (bus)
					bus->AddChildDevice(dev);
			}
		} catch(const MyError&) {
			// eat the exception
		}

		throw;
	}

	if (parent) {
		IATDeviceBus *bus = parent->GetDeviceBusById(parentBusId);
		
		if (bus)
			bus->AddChildDevice(dev);
	}
}

void ATDeviceInitXCmdRescanDynamicDisk(ATDeviceManager& dm) {
	dm.RegisterExtendedCommand(g_ATDeviceXCmdRescanDynamicDisk);
}
