//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2015 Avery Lee
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

#ifndef f_AT_SIO2SD_H
#define f_AT_SIO2SD_H

#include <vd2/system/vdstl.h>
#include <vd2/system/refcount.h>
#include <at/atcore/deviceimpl.h>
#include <at/atcore/devicesio.h>

class IATBlockDevice;
class IATDeviceSIOManager;

class ATSIO2SDEmulator final : public ATDevice
	, public IATDeviceIndicators
	, public IATDeviceSIO
{
public:
	ATSIO2SDEmulator();
	~ATSIO2SDEmulator();

	void *AsInterface(uint32 iid) override;

	void GetDeviceInfo(ATDeviceInfo& info) override;
	void GetSettings(ATPropertySet& settings) override;
	bool SetSettings(const ATPropertySet& settings) override;
	void Init() override;
	void Shutdown() override;
	void WarmReset() override;
	void ColdReset() override;

public:
	void InitIndicators(IATDeviceIndicatorManager *r) override;

public:
	void InitSIO(IATDeviceSIOManager *mgr) override;
	CmdResponse OnSerialBeginCommand(const ATDeviceSIOCommand& cmd) override;
	void OnSerialAbortCommand() override;
	void OnSerialReceiveComplete(uint32 id, const void *data, uint32 len, bool checksumOK) override;
	void OnSerialFence(uint32 id) override;
	CmdResponse OnSerialAccelCommand(const ATDeviceSIORequest& request) override;

protected:
	CmdResponse DoReadCommand(const void *data, uint32 len);
	CmdResponse DoWriteCommand(uint32 len);
	CmdResponse DoImpliedCommand();
	CmdResponse DoNAKCommand();

	IATDeviceSIOManager *mpSIOMgr = nullptr;
	vdrefptr<IATDeviceSIOInterface> mpSIOInterface;
	IATDeviceIndicatorManager *mpUIRenderer = nullptr;

	uint32 mHighSpeedCPSLo = 0;
	uint32 mHighSpeedCPSHi = 0;
	uint8 mHighSpeedIndex = 0;
	bool mbHighSpeedEnabled = false;
	bool mbHighSpeedPhase = false;
	ATDeviceSIOCommand mCommand {};
};

#endif
