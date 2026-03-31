//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2026 Avery Lee
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

#ifndef f_AT_KARINMAXIDRIVE_H
#define f_AT_KARINMAXIDRIVE_H

#include <vd2/system/function.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <at/atcore/devicediskdrive.h>
#include <at/atcore/deviceimpl.h>
#include <at/atcore/devicepbi.h>
#include <at/atcore/scheduler.h>
#include "fdc.h"
#include "diskdrivefullbase.h"
#include "diskinterface.h"

class ATMemoryManager;
class ATMemoryLayer;

class ATDeviceKarinMaxiDrive final : public ATDevice
	, public IATDeviceDiskDrive
	, public IATPBIDevice
	, public IATSchedulerCallback
{
public:
	ATDeviceKarinMaxiDrive();
	~ATDeviceKarinMaxiDrive();

	void *AsInterface(uint32 iid) override;

	void GetDeviceInfo(ATDeviceInfo& info) override;
	void GetSettingsBlurb(VDStringW& buf) override;
	void GetSettings(ATPropertySet & settings) override;
	bool SetSettings(const ATPropertySet & settings) override;
	void Init() override;
	void Shutdown() override;
	uint32 GetComputerPowerOnDelay() const override;
	void WarmReset() override;
	void ComputerColdReset() override;
	void PeripheralColdReset() override;

public:		// IATDeviceDiskDrive
	void InitDiskDrive(IATDiskDriveManager *ddm) override;
	ATDeviceDiskDriveInterfaceClient GetDiskInterfaceClient(uint32 index) override;

public:
	void GetPBIDeviceInfo(ATPBIDeviceInfo& devInfo) const override;
	void SelectPBIDevice(bool enable) override;
	bool IsPBIOverlayActive() const override;
	uint8 ReadPBIStatus(uint8 busData, bool debugOnly) override;

public:	// IATSchedulerCallback
	void OnScheduledEvent(uint32 id) override;

public:
	void OnDiskChanged(uint32 index, bool mediaRemoved);
	void OnWriteModeChanged(uint32 index);
	void OnTimingModeChanged(uint32 index);
	void OnAudioModeChanged(uint32 index);

protected:
	sint32 OnDebugReadByte(uint32 addr) const;
	sint32 OnReadByte(uint32 addr);
	bool OnWriteByte(uint32 addr, uint8 value);

	void OnFDCStep(bool inward);
	void OnFDCMotorChange(bool enabled);
	void OnFDCFormatDetectFormat(uint32 halfTrack, bool side2);

	void PlayStepSound();
	void UpdateRotationStatus();
	void UpdateDiskStatus();
	void UpdateWriteProtectStatus();
	void UpdateFDCSpeed();
	void UpdateROMBank();
	void SelectDrives(uint8 mask);

	static constexpr uint32 kNumDrives = 2;

	enum : uint32 {
		kEventId_DriveDiskChange0 = 1,		// 4 events, one per drive
	};

	ATScheduler *mpScheduler = nullptr;
	ATEvent *mpEventDriveReceiveBit = nullptr;
	IATDeviceSIOManager *mpSIOMgr = nullptr;
	IATDiskDriveManager *mpDiskDriveManager = nullptr;

	ATFirmwareManager *mpFwMgr = nullptr;
	bool mbFirmwareUsable = false;

	ATMemoryManager *mpMemMgr = nullptr;
	ATMemoryLayer *mpMemLayerPBIFirmware = nullptr;
	ATMemoryLayer *mpMemLayerPBIRegisters = nullptr;

	static constexpr uint32 kDiskChangeStepMS = 50;

	uint8 mDriveId = 0;
	uint8 mSwitches = 0;

	// The ATR8000 can select multiple drives at once, and in fact the firmware does so
	// for the power-on restore. Therefore, we must also track the multi-select case.
	uint8 mSelectedDrives = 0;
	uint8 mAvailableDrives = 0;

	bool mbSoundsEnabled = false;
	bool mbMotorRunning = false;

	bool mbPBISelected = false;
	bool mbROMBank2 = false;
	
	enum class HwVersion : uint8 {
		Original,
		PBISelectionFix
	} mHwVersion = HwVersion::Original;

	ATDiskDriveAudioPlayer mAudioPlayer;
	uint32 mLastStepSoundTime = 0;
	uint32 mLastStepPhase = 0;

	enum DriveType : uint32 {
		kDriveType_None,
		kDriveType_5_25_40,
		kDriveType_5_25_80,
		kDriveType_3_5,
	};

	struct Drive : public IATDiskInterfaceClient {
		ATDeviceKarinMaxiDrive *mpParent = nullptr;
		uint32 mIndex = 0;

		ATDiskInterface *mpDiskInterface = nullptr;
		ATEvent *mpEventDriveDiskChange = nullptr;
		uint32 mCurrentTrack = 0;
		uint8 mDiskChangeState = 0;

		DriveType mType = kDriveType_5_25_40;
		bool mbDoubleSidedMapping = false;

		uint32 GetTrackCount() const;

		void OnDiskChanged(bool mediaRemoved) override;
		void OnWriteModeChanged() override;
		void OnTimingModeChanged() override;
		void OnAudioModeChanged() override;
		bool IsImageSupported(const IATDiskImage& image) const override;
	} mDrives[kNumDrives];

	ATFDCEmulator mFDC;

	VDALIGN(4) uint8 mROM[0x1000] = {};

	ATDiskDriveFirmwareControl mFirmwareControl;
	ATDeviceBusSingleChild mParallelBus;
};

#endif
