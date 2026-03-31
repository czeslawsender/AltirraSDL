//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2017 Avery Lee
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
//
//	As a special exception, this library can also be redistributed and/or
//	modified under an alternate license. See COPYING.RMT in the same source
//	archive for details.

#ifndef f_AT_ATCORE_DEVICEPARENT_H
#define f_AT_ATCORE_DEVICEPARENT_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>

class IATDevice;

// Interface for a device bus on a parent device. A device bus hosts zero or
// more child devices with a defined persistent identification tag, a display
// name, and one or more child device types it can accept.
//
// Device buses also have a bus ID associated with them, used for enumeration
// from the parent device. Bus IDs must be non-negative but do not need to be
// contiguous. Bus IDs should be stable if possible and MUST be stable as long
// as child devices are connected.
//
// The device tree must be a proper tree; child devices may not be shared
// between buses and device loops must not be formed.
//
class IATDeviceBus {
public:
	// Return the display name associated with the bus.
	virtual const wchar_t *GetBusName() const = 0;

	// Return the persistent tag used to identify the bus in serialization.
	// Bus tag names should conform to basic C identifier rules, i.e.:
	// [a-zA-Z_][a-zA-Z0-9_]*. Bus tags only need to be unique within the same
	// device instance.
	virtual const char *GetBusTag() const = 0;

	// Return the supported child device types for the bus.
	virtual const char *GetSupportedType(uint32 index) = 0;

	// Enumerate the child devices currently attached to the bus.
	virtual void GetChildDevices(vdfastvector<IATDevice *>& devs) = 0;

	// Retrieve a prefix to apply to the display label for a child device
	// in the UI.
	virtual void GetChildDevicePrefix(uint32 index, VDStringW& s) = 0;

	// Attach a new child device to the bus. This may fail if the bus does
	// not support the child device. If successful, the bus is responsible for
	// calling SetParent() on the newly attached child device, and should also
	// hold a reference on the child device.
	virtual void AddChildDevice(IATDevice *dev) = 0;

	// Remove a child device from the bus. The child device should be connected
	// to the bus, but this is not guaranteed. If it is removed from the bus,
	// the bus is responsible for clearing the child device's reference to the
	// bus via SetParent().
	virtual void RemoveChildDevice(IATDevice *dev) = 0;
};

// Interface for a device that can act as a parent host for child devices.
// A device parent exposes zero or more device buses, which can in turn
// host zero or more devices.
class IATDeviceParent : public IVDUnknown {
public:
	enum { kTypeID = 'adpt' };

	// Retrieve a device bus by device bus ID. Returns null if the ID does not
	// correspond to a valid bus.
	virtual IATDeviceBus *GetDeviceBusById(sint32 id) = 0;

	// Enumerate valid bus IDs on the device. Returns -1 if there are no more
	// valid bus IDs. It is possible and valid for a device parent to expose
	// no bus IDs; this occasionally occurs when the presence of a bus depends
	// on a device setting.
	virtual sint32 GetDeviceBusIdByIndex(uint32 index) = 0;
};

#endif
