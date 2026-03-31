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

#ifndef f_AT_DISKDRIVESPEEDYXF_H
#define f_AT_DISKDRIVESPEEDYXF_H

#include <vd2/system/function.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <at/atcore/devicediskdrive.h>
#include <at/atcore/deviceimpl.h>
#include <at/atcore/deviceserial.h>
#include <at/atcore/devicesioimpl.h>
#include <at/atcpu/co6502.h>
#include <at/atcpu/breakpoints.h>
#include <at/atcpu/history.h>
#include <at/atcpu/memorymap.h>
#include <at/atdebugger/breakpointsimpl.h>
#include <at/atdebugger/target.h>
#include <at/atcore/scheduler.h>
#include <at/atemulation/riot.h>
#include "fdc.h"
#include "diskdrivefullbase.h"
#include "diskinterface.h"

class ATIRQController;

class ATDeviceDiskDriveSpeedyXF final : public ATDevice
	, public IATDeviceFirmware
	, public IATDeviceDiskDrive
	, public ATDeviceSIO
	, public ATDiskDriveDebugTargetControl
	, public IATDeviceRawSIO
	, public IATDiskInterfaceClient
{
public:
	ATDeviceDiskDriveSpeedyXF();
	~ATDeviceDiskDriveSpeedyXF();

	void *AsInterface(uint32 iid) override;

	void GetDeviceInfo(ATDeviceInfo& info) override;
	void GetSettingsBlurb(VDStringW& buf) override;
	void GetSettings(ATPropertySet& settings) override;
	bool SetSettings(const ATPropertySet& settings) override;
	void Init() override;
	void Shutdown() override;
	uint32 GetComputerPowerOnDelay() const override;
	void WarmReset() override;
	void ComputerColdReset() override;
	void PeripheralColdReset() override;
	void SetTraceContext(ATTraceContext *context) override;

public:		// IATDeviceFirmware
	void InitFirmware(ATFirmwareManager *fwman) override;
	bool ReloadFirmware() override;
	const wchar_t *GetWritableFirmwareDesc(uint32 idx) const override;
	bool IsWritableFirmwareDirty(uint32 idx) const override;
	void SaveWritableFirmware(uint32 idx, IVDStream& stream) override;
	ATDeviceFirmwareStatus GetFirmwareStatus() const override;

public:		// IATDeviceDiskDrive
	void InitDiskDrive(IATDiskDriveManager *ddm) override;
	ATDeviceDiskDriveInterfaceClient GetDiskInterfaceClient(uint32 index) override;

public:		// ATDeviceSIO
	void InitSIO(IATDeviceSIOManager *mgr) override;

public:	// IATSchedulerCallback
	void OnScheduledEvent(uint32 id) override;

public:	// IATDeviceRawSIO
	void OnCommandStateChanged(bool asserted) override;
	void OnMotorStateChanged(bool asserted) override;
	void OnReadyStateChanged(bool asserted) override;
	void OnReceiveByte(uint8 c, bool command, uint32 cyclesPerBit) override;
	void OnSendReady() override;

public:	// IATDiskInterfaceClient
	void OnDiskChanged(bool mediaRemoved) override;
	void OnWriteModeChanged() override;
	void OnTimingModeChanged() override;
	void OnAudioModeChanged() override;
	bool IsImageSupported(const IATDiskImage& image) const override;

protected:
	void Sync() override;

	void AddTransmitEdge(bool polarity);

	void OnRIOTRegisterWrite(uint32 addr, uint8 val);
	void OnControlWrite(uint32 addr, uint8 val);

	void PlayStepSound();
	void UpdateRotationStatus();
	void UpdateDiskStatus();
	void UpdateWriteProtectStatus();
	void UpdateROMBank();
	void OnWriteEnabled();
	void UpdateAutoSpeed();

	enum {
		kEventId_DriveReceiveBit = kEventId_FirstCustom,
		kEventId_DriveDiskChange,
	};

	ATEvent *mpEventDriveReceiveBit = nullptr;
	ATEvent *mpEventDriveDiskChange = nullptr;
	IATDeviceSIOManager *mpSIOMgr = nullptr;
	IATDiskDriveManager *mpDiskDriveManager = nullptr;
	ATDiskInterface *mpDiskInterface = nullptr;

	ATFirmwareManager *mpFwMgr = nullptr;

	static constexpr uint32 kDiskChangeStepMS = 50;

	uint32 mReceiveShiftRegister = 0;
	uint32 mReceiveTimingAccum = 0;
	uint32 mReceiveTimingStep = 0;

	uint32 mCurrentTrack = 0;
	uint8 mControlLatch = 0;
	sint32 mLEDPattern = 0;

	uint8 mDriveId = 0;

	bool mbFirmwareUsable = false;
	bool mbSoundsEnabled = false;

	ATDiskDriveAudioPlayer mAudioPlayer;

	uint32 mLastStepSoundTime = 0;
	uint32 mLastStepPhase = 0;
	uint8 mDiskChangeState = 0;

	ATCoProcReadMemNode mReadNodeFDC {};
	ATCoProcReadMemNode mReadNodeRIOT{};
	ATCoProcWriteMemNode mWriteNodeFDC {};
	ATCoProcWriteMemNode mWriteNodeRIOT{};
	ATCoProcWriteMemNode mWriteNodeControl{};

	ATCoProc6502 mCoProc;

	struct TargetProxy final : public ATDiskDriveDebugTargetProxyBaseT<ATCoProc6502> {
		uint32 GetTime() const override {
			return ATSCHEDULER_GETTIME(mpDriveScheduler);
		}

		ATScheduler *mpDriveScheduler;
	} mTargetProxy;

	ATFDCEmulator mFDC;
	ATRIOT6532Emulator mRIOT;

	vdfastvector<ATCPUHistoryEntry> mHistory;

	ATDiskDriveSerialBitTransmitQueue mSerialXmitQueue;

	VDALIGN(4) uint8 mRIOTRAM[0x80] = {};
	VDALIGN(4) uint8 mRAM[0x8000] = {};
	VDALIGN(4) uint8 mROM[0x10000] = {};
	VDALIGN(4) uint8 mDummyWrite[0x100] = {};
	VDALIGN(4) uint8 mDummyRead[0x100] = {};
		
	ATDebugTargetBreakpointsImpl mBreakpointsImpl;
};

#endif
