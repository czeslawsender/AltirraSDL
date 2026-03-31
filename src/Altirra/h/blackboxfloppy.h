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

#ifndef f_AT_BLACKBOXFLOPPY_H
#define f_AT_BLACKBOXFLOPPY_H

#include <vd2/system/unknown.h>
#include <at/atcore/devicediskdrive.h>
#include <at/atcore/deviceimpl.h>
#include <at/atcore/enumparse.h>
#include <at/atcpu/co6502.h>
#include <at/atcpu/memorymap.h>
#include <at/atemulation/via.h>
#include "blackboxfloppy.h"
#include "diskdrivefullbase.h"
#include "diskinterface.h"
#include "fdc.h"

class ATIRQController;

enum class ATBlackBoxFloppyType : uint8 {
	FiveInch180K,
	FiveInch360K,
	FiveInch12M,
	ThreeInch360K,
	ThreeInch720K,
	ThreeInch144M,
	EightInch1M
};

AT_DECLARE_ENUM_TABLE(ATBlackBoxFloppyType);

enum class ATBlackBoxFloppyMappingType : uint8 {
	XF551,
	ATR8000,
	Percom
};

AT_DECLARE_ENUM_TABLE(ATBlackBoxFloppyMappingType);

class ATBlackBoxFloppyEmulator final
	: public ATDeviceT<IATDeviceFirmware, IATDeviceDiskDrive>
	, public ATDiskDriveDebugTargetControl
	, public IATDiskInterfaceClient
{
public:
	static const auto kTypeID = "ATBlackBoxFloppyEmulator"_vdtypeid;

	ATBlackBoxFloppyEmulator();
	~ATBlackBoxFloppyEmulator();

	void *AsInterface(uint32 id) override;

	void GetDeviceInfo(ATDeviceInfo& info) override;
	void GetSettings(ATPropertySet& settings) override;
	bool SetSettings(const ATPropertySet& settings) override;
	void Init() override;
	void Shutdown() override;
	void ColdReset() override;
	void WarmReset() override;

public:
	uint8 DebugReadByteVIA(uint8 addr) const;
	uint8 ReadByteVIA(uint8 addr);
	void WriteByteVIA(uint8 addr, uint8 value);

public:	// IATDeviceFirmware
	void InitFirmware(ATFirmwareManager *fwman) override;
	bool ReloadFirmware() override;

	const wchar_t *GetWritableFirmwareDesc(uint32 idx) const override;
	bool IsWritableFirmwareDirty(uint32 idx) const override;
	void SaveWritableFirmware(uint32 idx, IVDStream& stream) override;

	ATDeviceFirmwareStatus GetFirmwareStatus() const override;

public:	// IATDeviceDiskDrive
	void InitDiskDrive(IATDiskDriveManager *ddm) override;
	ATDeviceDiskDriveInterfaceClient GetDiskInterfaceClient(uint32 index) override;

public:	// IATDiskDriveClient
	void OnDiskChanged(bool mediaRemoved) override;
	void OnWriteModeChanged() override;
	void OnTimingModeChanged() override;
	void OnAudioModeChanged() override;
	bool IsImageSupported(const IATDiskImage& image) const override;

public:
	void OnScheduledEvent(uint32 id) override;

private:
	static constexpr uint32 kFastSyncCount = 3;

	static constexpr uint32 kEventId_TickFastSync = kEventId_FirstCustom;

	void Sync() override;
	void ResetFastSyncWindow();

	uint8 OnDebugReadTransferLatch(uint32 addr) const;
	uint8 OnReadTransferLatch(uint32 addr);
	void OnWriteTransferLatch(uint32 addr, uint8 value);
	uint8 OnReadStatusLatch(uint32 addr) const;
	uint8 OnDebugReadFDC(uint32 addr) const;
	uint8 OnReadFDC(uint32 addr);
	void OnWriteFDC(uint32 addr, uint8 value);

	void OnVIAOutputChanged(uint32 state);
	void OnVIAIrqChanged(bool asserted);

	void OnFDCStep(bool inward);
	void OnFDCMotorChange(bool enabled);

	void SelectDrive(int index);
	bool IsAutoIndexEnabled() const;
	void UpdateFDCTrack();
	void UpdateFDCSpeeds();
	void UpdateFDCIndex();

	void InitDiskInterfaceMappings();
	void ShutdownDiskInterfaceMappings();

	bool mbInited = false;
	bool mbInSync = false;
	bool mbMotorEnabled = false;
	bool mbSoundEnabled = false;
	uint32 mVIAOutputState = 0;
	uint8 mManualIndexPhase = 0;

	ATIRQController *mpIrqController = nullptr;
	uint32 mIrqHandle = 0;

	ATFirmwareManager *mpFirmwareManager = nullptr;
	ATDeviceFirmwareStatus mFirmwareStatus = ATDeviceFirmwareStatus::Missing;

	ATScheduler *mpSlowScheduler = nullptr;
	ATEvent *mpSlowEventTickFastSync = nullptr;
	uint32 mFastSyncCounter = 0;

	int mCurrentDriveIndex = -1;
	bool mbCurrentDriveHasImage = false;
	ATDiskInterface *mpDiskInterfaces[4] {};
	int mCurrentDriveTracks[4] {};

	int mDiskInterfaceMappings[4] {};
	ATBlackBoxFloppyType mDriveTypes[4] {};
	ATBlackBoxFloppyMappingType mDriveMappingTypes[4] {};

	uint32 mLastStepSoundTime = 0;

	struct TargetProxy final : public ATDiskDriveDebugTargetProxyBaseT<ATCoProc6502> {
		uint32 GetTime() const override {
			return ATSCHEDULER_GETTIME(mpDriveScheduler);
		}

		ATScheduler *mpDriveScheduler;
	} mTargetProxy;

	ATCoProcReadMemNode mReadNodeTransferLatch {};
	ATCoProcReadMemNode mReadNodeStatusLatch {};
	ATCoProcWriteMemNode mWriteNodeTransferLatch {};
	ATCoProcReadMemNode mReadNodeFDC {};
	ATCoProcWriteMemNode mWriteNodeFDC {};

	ATVIA6522Emulator mVIA;
	ATFDCEmulator mFDC;

	ATCoProc6502 mCoProc { false, false };

	ATDiskDriveAudioPlayer mAudioPlayer;

	alignas(4) uint8 mROM[4096] {};
	alignas(4) uint8 mRAM[2048] {};

	ATDebugTargetBreakpointsImpl mBreakpointsImpl;

	alignas(4) uint8 mDummyRead[256] {};
	alignas(4) uint8 mDummyWrite[256] {};
};

#endif

