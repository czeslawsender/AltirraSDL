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

////////////////////////////////////////////////////////////////////////////////
// Karin Maxi Drive emulation
//
// The Karin Maxi Drive is a PBI/ECI drive interface with 4K of banked firmware,
// a WD1772 FDC, and a couple of control latches. It provides the main computer
// direct access to the FDC, eschewing either a controller or SIO bus
// connection. As there is no hardware buffering support and the main CPU 
// handles sector data transfers, and the firmware must disable playfield DMA
// to ensure that it can service DRQs in time to avoid lost data.
//
// The interface supports double density, 80 track drives, and double sided
// drives, but not high density. It is known to have been used with 5.25"
// and 3.5" double density drives. 6 DIP switches control the firmware behavior
// including drive configuration; the 40/80 track switches are automatically
// set here from the drive type. There is no 288/300 RPM compensation, as the
// FDC is clocked at straight 8MHz instead of 8.333MHz as in the XF551.
//
// The Karin Maxi Drive uses PBI ID 7 ($80), but does not work well with other
// PBI devices due to always having its control registers visible in $D1xx,
// and also occupying the PBI ID 0 bit. A hardware mod to fix this updates
// the decoding GAL to qualify hardware registers accesses with the PBI
// selection bit to fix this.
//
// An updated remake by Stryker, et. al., sometimes known as Karin Maxi Drive
// II, adds a couple of minor features. Two DIP switches are added, one of
// which selects one of two 4K firmwares within an 8K ROM, and the other of
// which disables the firmware without requiring it to be disconnected. These
// modifications are not explicitly emulated here; for the most part the
// firmware selection is only used to select between WD1770 and WD1772 firmware
// versions, which usually have broken out versions anyway.
//

#include <stdafx.h>
#include <vd2/system/bitmath.h>
#include <vd2/system/hash.h>
#include <vd2/system/int128.h>
#include <vd2/system/math.h>
#include <vd2/system/vdstl_algorithm.h>
#include <at/atcore/audiosource.h>
#include <at/atcore/logging.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/deviceparentimpl.h>
#include <at/atcore/deviceserial.h>
#include <at/atcore/wraptime.h>
#include "karinmaxidrive.h"
#include "memorymanager.h"
#include "firmwaremanager.h"
#include "debuggerlog.h"

extern ATLogChannel g_ATLCDiskEmu;

void ATCreateDeviceKarinMaxiDrive(const ATPropertySet& pset, IATDevice **dev) {
	vdrefptr<ATDeviceKarinMaxiDrive> p(new ATDeviceKarinMaxiDrive);
	p->SetSettings(pset);

	*dev = p.release();
}

extern const ATDeviceDefinition g_ATDeviceDefKarinMaxiDrive = { "karinmaxidrive", "karinmaxidrive", L"Karin Maxi Drive", ATCreateDeviceKarinMaxiDrive };

///////////////////////////////////////////////////////////////////////////

uint32 ATDeviceKarinMaxiDrive::Drive::GetTrackCount() const {
	return mType == kDriveType_5_25_40 ? 40 : 80;
}

void ATDeviceKarinMaxiDrive::Drive::OnDiskChanged(bool mediaRemoved) {
	if (mpDiskInterface) {
		IATDiskImage *image = mpDiskInterface->GetDiskImage();
		if (image) {
			const auto& geo = image->GetGeometry();
			mbDoubleSidedMapping = (uint32)(geo.mTrackCount * geo.mSideCount) > GetTrackCount();
		}
	}

	mpParent->OnDiskChanged(mIndex, mediaRemoved);
}

void ATDeviceKarinMaxiDrive::Drive::OnWriteModeChanged() {
	mpParent->OnWriteModeChanged(mIndex);
}

void ATDeviceKarinMaxiDrive::Drive::OnTimingModeChanged() {
	mpParent->OnTimingModeChanged(mIndex);
}

void ATDeviceKarinMaxiDrive::Drive::OnAudioModeChanged() {
	mpParent->OnAudioModeChanged(mIndex);
}

bool ATDeviceKarinMaxiDrive::Drive::IsImageSupported(const IATDiskImage& image) const {
	return true;
}

///////////////////////////////////////////////////////////////////////////

ATDeviceKarinMaxiDrive::ATDeviceKarinMaxiDrive() {
	mFirmwareControl.Init(mROM, sizeof mROM, kATFirmwareType_KarinMaxiDrive);
}

ATDeviceKarinMaxiDrive::~ATDeviceKarinMaxiDrive() {
}

void *ATDeviceKarinMaxiDrive::AsInterface(uint32 iid) {
	switch(iid) {
		case IATDeviceFirmware::kTypeID: return static_cast<IATDeviceFirmware *>(&mFirmwareControl);
		case IATDeviceDiskDrive::kTypeID: return static_cast<IATDeviceDiskDrive *>(this);
		case IATDeviceAudioOutput::kTypeID: return static_cast<IATDeviceAudioOutput *>(&mAudioPlayer);
		case ATFDCEmulator::kTypeID: return &mFDC;
	}

	return ATDevice::AsInterface(iid);
}

void ATDeviceKarinMaxiDrive::GetDeviceInfo(ATDeviceInfo& info) {
	info.mpDef = &g_ATDeviceDefKarinMaxiDrive;
}

void ATDeviceKarinMaxiDrive::GetSettingsBlurb(VDStringW& buf) {
	bool first = true;

	for(uint32 i=0; i<kNumDrives; ++i) {
		if (mDrives[i].mType) {
			if (first)
				first = false;
			else
				buf += ',';

			buf.append_sprintf(L"D%u:", i+1);
		}
	}
}

void ATDeviceKarinMaxiDrive::GetSettings(ATPropertySet& settings) {
	VDStringA s;
	for(uint32 i=0; i<kNumDrives; ++i) {
		s.sprintf("drivetype%u", i + 1);
		settings.SetUint32(s.c_str(), (uint32)mDrives[i].mType);
	}

	for(int i=0; i<4; ++i) {
		if (mSwitches & (0x20 >> i)) {
			s.sprintf("sw%d", i + 1);
			settings.SetUint32(s.c_str(), 1);
		}
	}

	settings.SetUint32("hwversion", (uint32)mHwVersion);
}

bool ATDeviceKarinMaxiDrive::SetSettings(const ATPropertySet& settings) {
	VDStringA s;
	bool change = false;

	uint32 hwVersion = settings.GetUint32("hwversion");

	if (hwVersion > (uint32)HwVersion::PBISelectionFix)
		hwVersion = 0;

	if (mHwVersion != (HwVersion)hwVersion) {
		mHwVersion = (HwVersion)hwVersion;

		change = true;
	}

	for(uint32 i=0; i<kNumDrives; ++i) {
		s.sprintf("drivetype%u", i + 1);
		const uint32 driveTypeCode = settings.GetUint32(s.c_str(), i ? kDriveType_None : kDriveType_5_25_40);

		if (driveTypeCode <= kDriveType_3_5) {
			if (mDrives[i].mType != driveTypeCode) {
				mDrives[i].mType = (DriveType)driveTypeCode;
				change = true;
			}
		}
	}

	uint8 switches = 0;

	for(int i=0; i<4; ++i) {
		s.sprintf("sw%d", i+1);

		if (settings.GetUint32(s.c_str(), 0))
			switches |= (0x20 >> i);
	}

	// update SW5/SW6 (track count) by drive type
	if (mDrives[1].mType >= kDriveType_5_25_80)
		switches |= 0x02;

	if (mDrives[0].mType >= kDriveType_5_25_80)
		switches |= 0x01;

	if (mSwitches != switches) {
		mSwitches = switches;

		change = true;
	}

	// SW1 ON switches to D2:/D3:
	if (mSwitches & 1)
		mDriveId = 1;
	else
		mDriveId = 0;
	
	return !change;
}

void ATDeviceKarinMaxiDrive::Init() {
	mpScheduler = GetService<IATDeviceSchedulingService>()->GetMachineScheduler();

	// Actually a 179X in the KarinMaxiDrive, but the difference between the 179X and 279X is
	// whether there is an internal data separator; this makes no difference to us.
	mFDC.Init(mpScheduler, 300.0f, mpScheduler->GetRate().asDouble() / 1000000.0, ATFDCEmulator::kType_1772);

	mFDC.SetOnStep([this](bool inward) { OnFDCStep(inward); });
	mFDC.SetOnMotorChange([this](bool active) { OnFDCMotorChange(active); });
	mFDC.SetOnFormatDetectFormat([this](uint32 halfTrack, bool side2) { OnFDCFormatDetectFormat(halfTrack, side2); });

	mFDC.SetAutoIndexPulse(true);

	for(auto& drive : mDrives) {
		mpScheduler->UnsetEvent(drive.mpEventDriveDiskChange);
		drive.mDiskChangeState = 0;

		drive.OnDiskChanged(false);

		drive.OnWriteModeChanged();
		drive.OnTimingModeChanged();
		drive.OnAudioModeChanged();
	}

	UpdateRotationStatus();

	mpMemMgr = GetService<ATMemoryManager>();

	mpMemLayerPBIFirmware = mpMemMgr->CreateLayer(kATMemoryPri_PBI, mROM, 0xD8, 0x08, true);
	mpMemMgr->SetLayerName(mpMemLayerPBIFirmware, "Karin Maxi Drive Firmware");

	UpdateROMBank();

	ATMemoryHandlerTable handlerTable;
	handlerTable.mbPassReads = true;
	handlerTable.mbPassAnticReads = true;
	handlerTable.mbPassWrites = true;
	handlerTable.mpThis = this;

	handlerTable.BindDebugReadHandler<&ATDeviceKarinMaxiDrive::OnDebugReadByte>();
	handlerTable.BindReadHandler<&ATDeviceKarinMaxiDrive::OnReadByte>();
	handlerTable.BindWriteHandler<&ATDeviceKarinMaxiDrive::OnWriteByte>();

	mpMemLayerPBIRegisters = mpMemMgr->CreateLayer(kATMemoryPri_PBI, handlerTable, 0xD1, 0x01);
	mpMemMgr->SetLayerName(mpMemLayerPBIRegisters, "Karin Maxi Drive Registers");
	mpMemMgr->EnableLayer(mpMemLayerPBIRegisters, mHwVersion == HwVersion::Original);

	GetService<IATDevicePBIManager>()->AddDevice(this);

}

void ATDeviceKarinMaxiDrive::Shutdown() {
	mAudioPlayer.Shutdown();

	GetService<IATDevicePBIManager>()->RemoveDevice(this);

	if (mpMemMgr) {
		mpMemMgr->DeleteLayerPtr(&mpMemLayerPBIFirmware);
		mpMemMgr->DeleteLayerPtr(&mpMemLayerPBIRegisters);
		mpMemMgr = nullptr;
	}

	mFirmwareControl.Shutdown();

	for(auto& drive : mDrives) {
		if (drive.mpDiskInterface) {
			drive.mpDiskInterface->RemoveClient(&drive);
			drive.mpDiskInterface = nullptr;
		}
	}

	mpDiskDriveManager = nullptr;
}

uint32 ATDeviceKarinMaxiDrive::GetComputerPowerOnDelay() const {
	return 20;
}

void ATDeviceKarinMaxiDrive::WarmReset() {
	mFDC.Reset();

	SelectDrives(0);

	mbMotorRunning = false;
	mFDC.SetMotorRunning(false);
	mFDC.SetDensity(false);
	mFDC.SetWriteProtectOverride(false);
	mFDC.SetAutoIndexPulse(true);

	mbROMBank2 = false;
	UpdateROMBank();
}

void ATDeviceKarinMaxiDrive::ComputerColdReset() {
	WarmReset();
}

void ATDeviceKarinMaxiDrive::PeripheralColdReset() {
	// start the disk drive on a track other than 0/20/39, just to make things interesting
	for(Drive& drive : mDrives)
		drive.mCurrentTrack = 2;

	mFDC.SetCurrentTrack(2, false);

	WarmReset();
}

void ATDeviceKarinMaxiDrive::InitDiskDrive(IATDiskDriveManager *ddm) {
	mpDiskDriveManager = ddm;
	mAvailableDrives = 0;

	for(uint32 i=0; i<kNumDrives; ++i) {
		Drive& drive = mDrives[i];

		drive.mIndex = i;
		drive.mpParent = this;

		if (drive.mType) {
			drive.mpDiskInterface = ddm->GetDiskInterface(i);
			drive.mpDiskInterface->AddClient(&drive);

			mAvailableDrives |= (1 << i);
		}
	}
}

ATDeviceDiskDriveInterfaceClient ATDeviceKarinMaxiDrive::GetDiskInterfaceClient(uint32 index) {
	for(uint32 i=0; i<kNumDrives; ++i) {
		if (mDrives[i].mType && !index--)
			return { &mDrives[i], mDriveId + i };
	}

	return {};
}

void ATDeviceKarinMaxiDrive::GetPBIDeviceInfo(ATPBIDeviceInfo& devInfo) const {
	devInfo.mDeviceId = 0x80;
	devInfo.mbHasIrq = false;
}

void ATDeviceKarinMaxiDrive::SelectPBIDevice(bool enable) {
	if (mbPBISelected != enable) {
		mbPBISelected = enable;

		if (mpMemLayerPBIFirmware)
			mpMemMgr->EnableLayer(mpMemLayerPBIFirmware, enable);

		if (mpMemLayerPBIRegisters && mHwVersion == HwVersion::PBISelectionFix) {
			mpMemMgr->EnableLayer(mpMemLayerPBIRegisters, enable);
		}
	}
}

bool ATDeviceKarinMaxiDrive::IsPBIOverlayActive() const {
	return mbPBISelected;
}

uint8 ATDeviceKarinMaxiDrive::ReadPBIStatus(uint8 busData, bool debugOnly) {
	return busData;
}

void ATDeviceKarinMaxiDrive::OnScheduledEvent(uint32 id) {
	if (id >= kEventId_DriveDiskChange0 && id < kEventId_DriveDiskChange0 + kNumDrives) {
		const uint32 index = id - kEventId_DriveDiskChange0;
		Drive& drive = mDrives[index];

		drive.mpEventDriveDiskChange = nullptr;

		switch(++drive.mDiskChangeState) {
			case 1:		// disk being removed (write protect covered)
			case 2:		// disk removed (write protect clear)
			case 3:		// disk being inserted (write protect covered)
				mpScheduler->SetEvent(kDiskChangeStepMS, this, kEventId_DriveDiskChange0 + index, drive.mpEventDriveDiskChange);
				break;

			case 4:		// disk inserted (write protect normal)
				drive.mDiskChangeState = 0;
				break;
		}

		UpdateDiskStatus();
	}
}

void ATDeviceKarinMaxiDrive::OnDiskChanged(uint32 index, bool mediaRemoved) {
	Drive& drive = mDrives[index];

	if (mediaRemoved) {
		drive.mDiskChangeState = 0;
		mpScheduler->SetEvent(1, this, kEventId_DriveDiskChange0 + index, drive.mpEventDriveDiskChange);
	}

	UpdateDiskStatus();
}

void ATDeviceKarinMaxiDrive::OnWriteModeChanged(uint32 index) {
	if (mSelectedDrives & (1 << index))
		UpdateWriteProtectStatus();
}

void ATDeviceKarinMaxiDrive::OnTimingModeChanged(uint32 index) {
	if (mSelectedDrives & (1 << index)) {
		bool accurateTiming = false;

		for(uint32 i=0; i<kNumDrives; ++i) {
			if (!(mSelectedDrives & (1 << i)))
				continue;

			if (mDrives[i].mpDiskInterface->IsAccurateSectorTimingEnabled())
				accurateTiming = true;
		}

		mFDC.SetAccurateTimingEnabled(accurateTiming);
	}
}

void ATDeviceKarinMaxiDrive::OnAudioModeChanged(uint32 index) {
	if (mSelectedDrives & (1 << index)) {
		bool driveSounds = false;

		for(uint32 i=0; i<kNumDrives; ++i) {
			if (!(mSelectedDrives & (1 << i)))
				continue;

			if (mDrives[i].mpDiskInterface->AreDriveSoundsEnabled())
				driveSounds = true;
		}

		mbSoundsEnabled = driveSounds;

		UpdateRotationStatus();
	}
}

sint32 ATDeviceKarinMaxiDrive::OnDebugReadByte(uint32 addr) const {
	if ((addr & 0xFC) == 0)
		return mFDC.DebugReadByte((uint8)(addr & 3));
	else if ((addr & 0xFC) == 4) {
		return (mFDC.GetIrqStatus() ? 0x00 : 0x80)
			+ (mFDC.GetDrqStatus() ? 0x00 : 0x40)
			+ mSwitches;
	}

	return -1;
}

sint32 ATDeviceKarinMaxiDrive::OnReadByte(uint32 addr) {
	if ((addr & 0xFC) == 0)
		return mFDC.ReadByte((uint8)(addr & 3));

	return OnDebugReadByte(addr);
}

bool ATDeviceKarinMaxiDrive::OnWriteByte(uint32 addr, uint8 value) {
	if ((addr & 0xFC) == 0) {
		// $D100-D103 selects the WD1770/1772 FDC.
		mFDC.WriteByte((uint8)(addr & 3), value);
	} else if (addr & 0xF0) {
		if (addr & 8) {
			// $D1x0-D1x7 excluding $D100-D10F activates the upper 74LS259 in the jsobola
			// schematic. D7 is latched into the bit selected by A0-A2.
			//
			// This latch controls two bits. Bit 7 is the PBI device selection bit, which
			// is handled by the PBI device manager and not here. Bit 0 selects the 2K
			// firmware bank.
			if ((addr & 7) == 0) {
				bool bank2 = (value & 0x80) != 0;

				if (mbROMBank2 != bank2) {
					mbROMBank2 = bank2;

					UpdateROMBank();
				}
			}
		} else {
			// $D1x0-D1x7 excluding $D100-D10F activates the lower 74LS259 in the jsobola
			// schematic. Unlike the upper one, the latched bit comes from A5 instead of D7.
			// A0-A2 selects the latched bit.
			switch(addr & 7) {
				case 7:		// unused
				case 6:
					break;

				case 5:		// manual index pulse (1 = asserted)
					mFDC.OnIndexPulse((addr & 0x20) != 0);
					break;

				case 4:		// drive index pulse enable (1 = enabled)
					mFDC.SetAutoIndexPulse((addr & 0x20) != 0);
					break;

				case 3:		// density select (0 = double density)
					mFDC.SetDensity((addr & 0x20) == 0);
					break;

				case 2:		// side select (0 = side 2)
					mFDC.SetSide((addr & 0x20) == 0);
					break;

				case 1:		// drive select 0 (active low)
					SelectDrives((mSelectedDrives & 0xFD) + (addr & 0x20 ? 0 : 2));
					break;

				case 0:		// drive select 1 (active low)
					SelectDrives((mSelectedDrives & 0xFE) + (addr & 0x20 ? 0 : 1));
					break;
			}
		}
	}

	return false;
}

void ATDeviceKarinMaxiDrive::OnFDCStep(bool inward) {
	if (!mSelectedDrives)
		return;

	bool playStepSound = false;
	bool track0 = false;
	uint32 fdcTrack = 0;

	for(uint32 i=0; i<kNumDrives; ++i) {
		if (!(mSelectedDrives & (1 << i)))
			continue;

		Drive& drive = mDrives[i];
		if (inward) {
			// step in (increasing track number)
			const uint32 trackLimit = drive.mType == kDriveType_5_25_40 ? 45*2 : 82*2;
			if (drive.mCurrentTrack < trackLimit) {
				drive.mCurrentTrack += 2;
			}

			playStepSound = true;
		} else {
			// step out (decreasing track number)
			if (drive.mCurrentTrack > 0) {
				drive.mCurrentTrack -= 2;

				playStepSound = true;
			}
		}

		if (drive.mCurrentTrack < 2)
			track0 = true;

		fdcTrack = drive.mCurrentTrack;
	}

	mFDC.SetCurrentTrack(fdcTrack, track0);

	if (playStepSound)
		PlayStepSound();
}

void ATDeviceKarinMaxiDrive::OnFDCMotorChange(bool enabled) {
	if (mbMotorRunning != enabled) {
		mbMotorRunning = enabled;

		mFDC.SetMotorRunning(enabled);
		UpdateRotationStatus();
	}
}

void ATDeviceKarinMaxiDrive::OnFDCFormatDetectFormat(uint32 halfTrack, bool side2) {
	if (!mSelectedDrives)
		return;

	int firstDrive = VDFindLowestSetBitFast(mSelectedDrives);
	Drive& drive = mDrives[firstDrive];

	if (halfTrack == 0 && !side2) {
		// Formatting track 0 side 0 -- reset to side 1 forward mapping
		drive.mbDoubleSidedMapping = false;
		mFDC.SetSideMapping(ATFDCEmulator::SideMapping::Side2Forward, drive.GetTrackCount());
	} else if (halfTrack == 0 && side2) {
		// Formatting track 0 side 1 -- switched to interleaved mapping
		drive.mbDoubleSidedMapping = true;
		mFDC.SetSideMapping(ATFDCEmulator::SideMapping::Side2Interleaved, drive.GetTrackCount());
	}
}

void ATDeviceKarinMaxiDrive::PlayStepSound() {
	if (!mbSoundsEnabled)
		return;

	const uint32 t = ATSCHEDULER_GETTIME(mpScheduler);
	
	if (t - mLastStepSoundTime > 50000)
		mLastStepPhase = 0;

	mAudioPlayer.PlayStepSound(kATAudioSampleId_DiskStep2H, 0.3f + 0.7f * cosf((float)mLastStepPhase++ * nsVDMath::kfPi));

	mLastStepSoundTime = t;
}

void ATDeviceKarinMaxiDrive::UpdateRotationStatus() {
	if (mSelectedDrives == 0) {
		mAudioPlayer.SetRotationSoundEnabled(false);
		return;
	}

	const bool driveMotorRunning = mbMotorRunning;

	for(uint32 i=0; i<kNumDrives; ++i) {
		if (mSelectedDrives & (1 << i))
			mDrives[i].mpDiskInterface->SetShowMotorActive(driveMotorRunning);
	}

	mAudioPlayer.SetRotationSoundEnabled(driveMotorRunning && mbSoundsEnabled);
}

void ATDeviceKarinMaxiDrive::UpdateDiskStatus() {
	// The KarinMaxiDrive has RDY tied to +5V, so the FDC thinks the disk is always ready.
	// Only give the FDC a disk if exactly one drive is selected, however.

	if (!mSelectedDrives || (mSelectedDrives & (mSelectedDrives - 1))) {
		mFDC.SetDiskImage(nullptr, true);
	} else {
		Drive& drive = mDrives[VDFindLowestSetBitFast(mSelectedDrives)];
		IATDiskImage *image = drive.mpDiskInterface ? drive.mpDiskInterface->GetDiskImage() : nullptr;

		mFDC.SetDiskImage(image, true);
	}

	UpdateWriteProtectStatus();
}

void ATDeviceKarinMaxiDrive::UpdateWriteProtectStatus() {
	bool wpOverride = false;

	// check if any selected drives need WP override due to disk change emulation
	if (mSelectedDrives) {
		for(uint32 i=0; i<kNumDrives; ++i) {
			if (!(mSelectedDrives & (1 << i)))
				continue;

			Drive& drive = mDrives[i];

			if ((drive.mDiskChangeState & 1) != 0) {
				wpOverride = true;
				break;
			}
		}
	}

	mFDC.SetWriteProtectOverride(wpOverride);
}

void ATDeviceKarinMaxiDrive::UpdateFDCSpeed() {
	if (mSelectedDrives) {
		const float rpm = 300.0f;
		mFDC.SetSpeeds(rpm, 1.0f, false);
	}
}

void ATDeviceKarinMaxiDrive::UpdateROMBank() {
	if (mpMemLayerPBIFirmware)
		mpMemMgr->SetLayerMemory(mpMemLayerPBIFirmware, mROM + (mbROMBank2 ? 0x800 : 0));
}

void ATDeviceKarinMaxiDrive::SelectDrives(uint8 mask) {
	mask &= mAvailableDrives;

	if (mSelectedDrives == mask)
		return;

	const uint8 deactivatedDrives = mSelectedDrives & ~mask;

	if (deactivatedDrives) {
		for(uint32 i=0; i<kNumDrives; ++i) {
			if (!(deactivatedDrives & (1 << i)))
				continue;

			Drive& oldDrive = mDrives[i];

			oldDrive.mpDiskInterface->SetShowMotorActive(false);
		}
	}

	mSelectedDrives = mask;

	if (mask) {
		int firstDrive = VDFindLowestSetBitFast(mask);

		Drive& drive = mDrives[firstDrive];
		mFDC.SetDiskInterface(drive.mpDiskInterface);
		mFDC.SetCurrentTrack(drive.mCurrentTrack, drive.mCurrentTrack == 0);
		mFDC.SetSideMapping(drive.mbDoubleSidedMapping ? ATFDCEmulator::SideMapping::Side2Interleaved : ATFDCEmulator::SideMapping::Side2Forward, drive.GetTrackCount());

		UpdateFDCSpeed();
		OnWriteModeChanged(firstDrive);
		OnTimingModeChanged(firstDrive);
		OnAudioModeChanged(firstDrive);
	} else {
		mFDC.SetDiskInterface(nullptr);
		mFDC.SetCurrentTrack(20, false);
	}

	UpdateDiskStatus();
	UpdateRotationStatus();
}
