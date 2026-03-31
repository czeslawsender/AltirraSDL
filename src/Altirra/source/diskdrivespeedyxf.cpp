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
#include <vd2/system/binary.h>
#include <vd2/system/constexpr.h>
#include <vd2/system/hash.h>
#include <vd2/system/int128.h>
#include <vd2/system/math.h>
#include <at/atcore/audiosource.h>
#include <at/atcore/configvar.h>
#include <at/atcore/crc.h>
#include <at/atcore/deviceserial.h>
#include <at/atcore/logging.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/wraptime.h>
#include <at/atemulation/diskutils.h>
#include "audiosampleplayer.h"
#include "debuggerlog.h"
#include "diskdrivespeedyxf.h"
#include "firmwaremanager.h"
#include "memorymanager.h"
#include "trace.h"

extern ATLogChannel g_ATLCDiskEmu;

void ATCreateDeviceDiskDriveSpeedyXF(const ATPropertySet& pset, IATDevice **dev) {
	vdrefptr<ATDeviceDiskDriveSpeedyXF> p(new ATDeviceDiskDriveSpeedyXF());
	p->SetSettings(pset);

	*dev = p.release();
}

extern const ATDeviceDefinition g_ATDeviceDefDiskDriveSpeedyXF = { "diskdrivespeedyxf", "diskdrivespeedyxf", L"Speedy XF disk drive (full emulation)", ATCreateDeviceDiskDriveSpeedyXF };

ATDeviceDiskDriveSpeedyXF::ATDeviceDiskDriveSpeedyXF()
	: mCoProc(true, false)
{
	mBreakpointsImpl.BindBPHandler(mCoProc);
	mBreakpointsImpl.SetStepHandler(this);
	mBreakpointsImpl.SetBPsChangedHandler([this](const uint16 *pc) { mCoProc.OnBreakpointsChanged(pc); });

	const VDFraction& clockRate = VDFraction(1000000, 1);
	mDriveScheduler.SetRate(clockRate);

	memset(&mDummyRead, 0xFF, sizeof mDummyRead);

	mTargetProxy.mpDriveScheduler = &mDriveScheduler;
	mTargetProxy.Init(mCoProc);
	InitTargetControl(mTargetProxy, clockRate.asDouble(), kATDebugDisasmMode_65C02, &mBreakpointsImpl, this);
}

ATDeviceDiskDriveSpeedyXF::~ATDeviceDiskDriveSpeedyXF() {
}

void *ATDeviceDiskDriveSpeedyXF::AsInterface(uint32 iid) {
	switch(iid) {
		case IATDeviceFirmware::kTypeID: return static_cast<IATDeviceFirmware *>(this);
		case IATDeviceDiskDrive::kTypeID: return static_cast<IATDeviceDiskDrive *>(this);
		case IATDeviceSIO::kTypeID: return static_cast<IATDeviceSIO *>(this);
		case IATDeviceAudioOutput::kTypeID: return static_cast<IATDeviceAudioOutput *>(&mAudioPlayer);
		case ATRIOT6532Emulator::kTypeID: return &mRIOT;
		case ATFDCEmulator::kTypeID: return &mFDC;
	}

	void *p = ATDiskDriveDebugTargetControl::AsInterface(iid);
	if (p)
		return p;

	return ATDevice::AsInterface(iid);
}

void ATDeviceDiskDriveSpeedyXF::GetDeviceInfo(ATDeviceInfo& info) {
	info.mpDef = &g_ATDeviceDefDiskDriveSpeedyXF;
}

void ATDeviceDiskDriveSpeedyXF::GetSettingsBlurb(VDStringW& buf) {
	buf.sprintf(L"D%u:", mDriveId + 1);
}

void ATDeviceDiskDriveSpeedyXF::GetSettings(ATPropertySet& settings) {
	settings.SetUint32("id", mDriveId);
}

bool ATDeviceDiskDriveSpeedyXF::SetSettings(const ATPropertySet& settings) {
	uint32 newDriveId = settings.GetUint32("id", mDriveId) & 3;

	if (mDriveId != newDriveId) {
		mDriveId = newDriveId;
		return false;
	}

	if (mpScheduler)
		UpdateAutoSpeed();
	return true;
}

void ATDeviceDiskDriveSpeedyXF::Init() {
	mSerialXmitQueue.Init(mpScheduler, mpSIOMgr);

	uintptr* readmap = mCoProc.GetReadMap();
	uintptr* writemap = mCoProc.GetWriteMap();

	// set up FDC/6810 handlers
	mReadNodeFDC.mpThis = this;
	mWriteNodeFDC.mpThis = this;

	mReadNodeFDC.mpRead = [](uint32 addr, void* thisptr0) -> uint8 {
		auto* thisptr = (ATDeviceDiskDriveSpeedyXF*)thisptr0;

		uint8 v = thisptr->mFDC.ReadByte((uint8)addr);

		if ((addr & 3) == 0) {
			v &= 0x7F;

			if (!thisptr->mpDiskInterface->IsDiskLoaded() || thisptr->mDiskChangeState != 0)
				v |= 0x80;
		}

		return v;
	};

	mReadNodeFDC.mpDebugRead = [](uint32 addr, void* thisptr0) -> uint8 {
		auto* thisptr = (ATDeviceDiskDriveSpeedyXF*)thisptr0;

		uint8 v = thisptr->mFDC.DebugReadByte((uint8)addr) & 0x7F;

		if ((addr & 3) == 0) {
			v &= 0x7F;

			if (!thisptr->mpDiskInterface->IsDiskLoaded() || thisptr->mDiskChangeState != 0)
				v |= 0x80;
		}

		return v;
	};

	mWriteNodeFDC.mpWrite = [](uint32 addr, uint8 val, void* thisptr0) {
		auto* thisptr = (ATDeviceDiskDriveSpeedyXF*)thisptr0;

		thisptr->mFDC.WriteByte((uint8)addr, val);
	};

	// set up RIOT handlers
	mReadNodeRIOT.mpThis = this;
	mWriteNodeRIOT.mpThis = this;

	mReadNodeRIOT.mpRead = [](uint32 addr, void* thisptr0) -> uint8 {
		auto* thisptr = (ATDeviceDiskDriveSpeedyXF*)thisptr0;

		if (addr & 0x80)
			return thisptr->mRIOT.ReadByte((uint8)addr);
		else
			return thisptr->mRIOTRAM[addr & 0x7F];
	};

	mReadNodeRIOT.mpDebugRead = [](uint32 addr, void* thisptr0) -> uint8 {
		auto* thisptr = (ATDeviceDiskDriveSpeedyXF*)thisptr0;

		if (addr & 0x80)
			return thisptr->mRIOT.DebugReadByte((uint8)addr);
		else
			return thisptr->mRIOTRAM[addr & 0x7F];
	};

	mWriteNodeRIOT.mpWrite = [](uint32 addr, uint8 val, void* thisptr0) {
		auto* thisptr = (ATDeviceDiskDriveSpeedyXF*)thisptr0;

		if (addr & 0x80)
			thisptr->OnRIOTRegisterWrite(addr, val);
		else
			thisptr->mRIOTRAM[addr & 0x7F] = val;
	};

	mWriteNodeControl.BindMethod<&ATDeviceDiskDriveSpeedyXF::OnControlWrite>(this);

	// Initialize memory map. From the official Handbook:
	//
	//	$0000-01FF: Page zero and stack
	//	$0200-027F: RIOT registers
	//	$0280-02FF: RIOT RAM
	//	$0400-0403: FDC
	//	$2000-3FFF: ROM
	//	$4000-4003: Control latch (undocumented)
	//	$8000-BFFF: RAM
	//	$C000-FFFF: ROM
	//
	ATCoProcMemoryMapView mmapView(readmap, writemap, mCoProc.GetTraceMap());

	mmapView.Clear(mDummyRead, mDummyWrite);

	mmapView.SetMemory(0x00, 0x02, mRAM);
	mmapView.SetHandlers(0x02, 0x02, mReadNodeRIOT, mWriteNodeRIOT);
	mmapView.SetHandlers(0x04, 0x04, mReadNodeFDC, mWriteNodeFDC);
	mmapView.SetWriteHandler(0x40, 0x01, mWriteNodeControl);
	mmapView.SetMemory(0x80, 0x40, mRAM + 0x4000);

	// 2332 ROM: A12=1 ($1000-1FFF / $F000-FFFF)
	mmapView.SetReadMem(0x10, 0x10, mROM);

	mRIOT.SetOnIrqChanged([this](bool state) {
		mRIOT.SetInputA(state ? 0x00 : 0x40, 0x40);
		mFDC.OnIndexPulse((mRIOT.ReadOutputA() & 0x40) == 0);
	});

	mRIOT.Init(&mDriveScheduler);
	mRIOT.Reset();

	// Clear port B bit 1 (/READY) and bit 7 (/DATAOUT)
	mRIOT.SetInputB(0x00, 0x82);

	// Set port A bits 0 and 1 to select D1:., and clear DRQ/IRQ from FDC
	static const uint8 k1050IDs[4] = {
		0x03, 0x01, 0x00, 0x02
	};

	mRIOT.SetInputA(k1050IDs[mDriveId & 3], 0xC3);

	// Deassert /IRQ
	mRIOT.SetInputA(0x40, 0x40);

	mFDC.Init(&mDriveScheduler, 300.0f, 8.0f / 8.3333f, ATFDCEmulator::kType_1772);

	mFDC.SetOnIndexChange(
		[this](bool index) {
			mRIOT.SetInputA(0x40, index ? 0x40 : 0);
		}
	);
	mFDC.SetAutoIndexPulse(true, false);

	mFDC.SetDiskInterface(mpDiskInterface);
	mFDC.SetOnDrqChange([this](bool drq) { mRIOT.SetInputA(drq ? 0x80 : 0x00, 0x80); });

	mDriveScheduler.UnsetEvent(mpEventDriveDiskChange);
	mDiskChangeState = 0;
	OnDiskChanged(false);

	OnWriteModeChanged();
	OnTimingModeChanged();
	OnAudioModeChanged();

	UpdateRotationStatus();
	UpdateROMBank();
	UpdateAutoSpeed();

	mFDC.SetSideMapping(ATFDCEmulator::SideMapping::Side2ReversedTracks, 40);
}

void ATDeviceDiskDriveSpeedyXF::Shutdown() {
	mAudioPlayer.Shutdown();
	mSerialXmitQueue.Shutdown();

	mDriveScheduler.UnsetEvent(mpEventDriveReceiveBit);
	mDriveScheduler.UnsetEvent(mpEventDriveDiskChange);
	ShutdownTargetControl();

	mpFwMgr = nullptr;

	if (mpSIOMgr) {
		mpSIOMgr->RemoveRawDevice(this);
		mpSIOMgr = nullptr;
	}

	mFDC.Shutdown();

	if (mpDiskInterface) {
		mpDiskInterface->SetShowLEDReadout(-1);
		mpDiskInterface->SetShowMotorActive(false);
		mpDiskInterface->SetShowActivity(false, 0);
		mpDiskInterface->RemoveClient(this);
		mpDiskInterface = nullptr;
	}

	mpDiskDriveManager = nullptr;
}

uint32 ATDeviceDiskDriveSpeedyXF::GetComputerPowerOnDelay() const {
	return 0;
}

void ATDeviceDiskDriveSpeedyXF::WarmReset() {
	// If the computer resets, its transmission is interrupted.
	mDriveScheduler.UnsetEvent(mpEventDriveReceiveBit);
}

void ATDeviceDiskDriveSpeedyXF::ComputerColdReset() {
	WarmReset();
}

void ATDeviceDiskDriveSpeedyXF::PeripheralColdReset() {
	memset(mRAM, 0, sizeof mRAM);

	mRIOT.Reset();
	mFDC.Reset();

	// clear DRQ from FDC -> RIOT port A
	mRIOT.SetInputA(0x00, 0x80);

	mSerialXmitQueue.Reset();
	
	// start the disk drive on a track other than 0/20/39, just to make things interesting
	mCurrentTrack = 10;
	mFDC.SetCurrentTrack(mCurrentTrack, false);

	mFDC.SetMotorRunning(false);
	mFDC.SetDensity(true);

	UpdateWriteProtectStatus();

	mpDiskInterface->SetShowLEDReadout(0);
	mLEDPattern = 0;

	mControlLatch = 0;
	mFDC.SetSide(false);
	
	mCoProc.ColdReset();

	ResetTargetControl();

	// need to update motor and sound status, since the 810 starts with the motor on
	UpdateRotationStatus();

	WarmReset();
}

void ATDeviceDiskDriveSpeedyXF::SetTraceContext(ATTraceContext *context) {
	if (context)
		mFDC.SetTraceContext(context, MasterTimeToDriveTime64(context->mBaseTime + kATDiskDriveTransmitLatency), mDriveScheduler.GetRate().AsInverseDouble());
	else
		mFDC.SetTraceContext(nullptr, 0, 0);
}

void ATDeviceDiskDriveSpeedyXF::InitFirmware(ATFirmwareManager *fwman) {
	mpFwMgr = fwman;

	ReloadFirmware();
}

bool ATDeviceDiskDriveSpeedyXF::ReloadFirmware() {
	const uint64 id = mpFwMgr->GetFirmwareOfType(kATFirmwareType_SpeedyXF, true);
	
	const vduint128 oldHash = VDHash128(mROM, sizeof mROM);

	memset(mROM, 0, sizeof mROM);

	uint32 len = 0;
	mpFwMgr->LoadFirmware(id, mROM, 0, 0x10000, nullptr, &len, nullptr, nullptr, &mbFirmwareUsable);

	// if the ROM is 32K or less, clone it up to 64K
	if (len <= 0x8000)
		memcpy(mROM + 0x8000, mROM, 0x8000);

	const vduint128 newHash = VDHash128(mROM, sizeof mROM);

	return oldHash != newHash;
}

const wchar_t *ATDeviceDiskDriveSpeedyXF::GetWritableFirmwareDesc(uint32 idx) const {
	return nullptr;
}

bool ATDeviceDiskDriveSpeedyXF::IsWritableFirmwareDirty(uint32 idx) const {
	return false;
}

void ATDeviceDiskDriveSpeedyXF::SaveWritableFirmware(uint32 idx, IVDStream& stream) {
}

ATDeviceFirmwareStatus ATDeviceDiskDriveSpeedyXF::GetFirmwareStatus() const {
	return mbFirmwareUsable ? ATDeviceFirmwareStatus::OK : ATDeviceFirmwareStatus::Missing;
}

void ATDeviceDiskDriveSpeedyXF::InitDiskDrive(IATDiskDriveManager *ddm) {
	mpDiskDriveManager = ddm;
	mpDiskInterface = ddm->GetDiskInterface(mDriveId);
	mpDiskInterface->AddClient(this);
	mpDiskInterface->SetShowLEDReadout(0);
}

ATDeviceDiskDriveInterfaceClient ATDeviceDiskDriveSpeedyXF::GetDiskInterfaceClient(uint32 index) {
	return index ? ATDeviceDiskDriveInterfaceClient{} : ATDeviceDiskDriveInterfaceClient{ this, mDriveId };
}

void ATDeviceDiskDriveSpeedyXF::InitSIO(IATDeviceSIOManager *mgr) {
	mpSIOMgr = mgr;
	mpSIOMgr->AddRawDevice(this);
}

void ATDeviceDiskDriveSpeedyXF::OnScheduledEvent(uint32 id) {
	if (id == kEventId_DriveReceiveBit) {
		const bool newState = (mReceiveShiftRegister & 1) != 0;

		mReceiveShiftRegister >>= 1;
		mpEventDriveReceiveBit = nullptr;

		if (mReceiveShiftRegister) {
			mReceiveTimingAccum += mReceiveTimingStep;
			mpEventDriveReceiveBit = mDriveScheduler.AddEvent(mReceiveTimingAccum >> 10, this, kEventId_DriveReceiveBit);
			mReceiveTimingAccum &= 0x3FF;
		}

		mRIOT.SetInputB(newState ? 0x00 : 0x40, 0x40);
	} else if (id == kEventId_DriveDiskChange) {
		mpEventDriveDiskChange = nullptr;

		switch(++mDiskChangeState) {
			case 1:		// disk being removed (write protect covered)
			case 2:		// disk removed (write protect clear)
			case 3:		// disk being inserted (write protect covered)
				mDriveScheduler.SetEvent(kDiskChangeStepMS, this, kEventId_DriveDiskChange, mpEventDriveDiskChange);
				break;

			case 4:		// disk inserted (write protect normal)
				mDiskChangeState = 0;
				break;
		}

		UpdateDiskStatus();
	} else
		return ATDiskDriveDebugTargetControl::OnScheduledEvent(id);
}

void ATDeviceDiskDriveSpeedyXF::OnCommandStateChanged(bool asserted) {
	Sync();

	mRIOT.SetInputB(asserted ? 0xFF : 0x00, 0x80);
}

void ATDeviceDiskDriveSpeedyXF::OnMotorStateChanged(bool asserted) {
}

void ATDeviceDiskDriveSpeedyXF::OnReadyStateChanged(bool asserted) {
	mRIOT.SetInputB(asserted ? 0x00 : 0x02, 0x02);
}

void ATDeviceDiskDriveSpeedyXF::OnReceiveByte(uint8 c, bool command, uint32 cyclesPerBit) {
	// ignore bytes with invalid clocking
	if (!cyclesPerBit)
		return;

	Sync();

	mReceiveShiftRegister = c + c + 0x200;

	mReceiveTimingAccum = 0x200;
	mReceiveTimingStep = cyclesPerBit * 572;

	mDriveScheduler.SetEvent(1, this, kEventId_DriveReceiveBit, mpEventDriveReceiveBit);
}

void ATDeviceDiskDriveSpeedyXF::OnSendReady() {
}

void ATDeviceDiskDriveSpeedyXF::OnDiskChanged(bool mediaRemoved) {
	if (mediaRemoved) {
		mDiskChangeState = 0;
		mDriveScheduler.SetEvent(1, this, kEventId_DriveDiskChange, mpEventDriveDiskChange);
	}

	UpdateDiskStatus();
}

void ATDeviceDiskDriveSpeedyXF::OnWriteModeChanged() {
	UpdateWriteProtectStatus();
}

void ATDeviceDiskDriveSpeedyXF::OnTimingModeChanged() {
	mFDC.SetAccurateTimingEnabled(mpDiskInterface->IsAccurateSectorTimingEnabled());
}

void ATDeviceDiskDriveSpeedyXF::OnAudioModeChanged() {
	mbSoundsEnabled = mpDiskInterface->AreDriveSoundsEnabled();

	UpdateRotationStatus();
}

bool ATDeviceDiskDriveSpeedyXF::IsImageSupported(const IATDiskImage& image) const {
	const auto& geo = image.GetGeometry();

	if (geo.mbHighDensity)
		return false;

	if (geo.mSectorSize > 256)
		return false;

	if (geo.mSectorsPerTrack > (geo.mSectorSize > 128 ? 18U : 26U))
		return false;

	if (geo.mTrackCount > 40)
		return false;

	return true;
}

void ATDeviceDiskDriveSpeedyXF::Sync() {
	uint32 newDriveCycleLimit = AccumSubCycles();

	bool ranToCompletion = true;

	VDASSERT(mDriveScheduler.mNextEventCounter >= 0xFF000000);
	if (ATSCHEDULER_GETTIME(&mDriveScheduler) - newDriveCycleLimit >= 0x80000000) {
		mDriveScheduler.SetStopTime(newDriveCycleLimit);
		ranToCompletion = mCoProc.Run(mDriveScheduler);

		VDASSERT(ATWrapTime{ATSCHEDULER_GETTIME(&mDriveScheduler)} <= newDriveCycleLimit);
	}

	if (!ranToCompletion)
		ScheduleImmediateResume();

	FlushStepNotifications();
}

void ATDeviceDiskDriveSpeedyXF::AddTransmitEdge(bool polarity) {
	mSerialXmitQueue.AddTransmitBit(DriveTimeToMasterTime() + mSerialXmitQueue.kTransmitLatency, polarity);
}

void ATDeviceDiskDriveSpeedyXF::OnRIOTRegisterWrite(uint32 addr, uint8 val) {
	// SpeedyXF RIOT pin assignments, based on schematic by Guus Assmann:
	//
	// PA7 (I): FDC DRQ (1 = DRQ)
	// PA6 (I): /IRQA / index pulse option
	// PA5 (I): FDC DDEN (0 = double density)
	// PA4 (I): DS1?
	// PA3 (O): Drive motor on (0 = on)
	// PA2 (I): NC
	// PA1 (I): Drive ID switch 2 (1 = D1: or D4:)
	// PA0 (I): Drive ID switch 1 (1 = D1: or D2:)
	// PB7 (I): SIO COMMAND (1 = asserted)
	// PB6 (I): SIO DATA OUT (non-inverted)
	// PB5 (O): ROM A15
	// PB4 (I): NC
	// PB3 (O): Drive step (0 = step)
	// PB2 (O): Drive direction (1 = step outward)
	// PB1 (I): SIO READY (0 = computer on)
	// PB0 (O): SIO DATA IN (inverted)

	// check for a write to DRA or DDRA
	if ((addr & 6) == 0) {
		// compare outputs before and after write
		const uint8 outprev = mRIOT.ReadOutputA();
		mRIOT.WriteByte((uint8)addr, val);
		const uint8 outnext = mRIOT.ReadOutputA();

		// check for density change
		const uint8 delta = outprev ^ outnext;

		if (delta & 0x20)
			mFDC.SetDensity(!(outnext & 0x20));

		// check for a spindle motor state change
		if (delta & 8) {
			const bool running = (outnext & 8) == 0;
			mFDC.SetMotorRunning(running);

			UpdateRotationStatus();
		}

		// check for index pulse change (1050)
		if (delta & 0x40) {
			mFDC.OnIndexPulse((mRIOT.ReadOutputA() & 0x40) == 0);
		}
		return;
	}

	// check for a write to DRB or DDRB
	if ((addr & 6) == 2) {
		// compare outputs before and after write
		const uint8 outprev = mRIOT.ReadOutputB();
		mRIOT.WriteByte((uint8)addr, val);
		const uint8 outnext = mRIOT.ReadOutputB();
		const uint8 outdelta = outprev ^ outnext;

		// check for transition on PB0 (SIO input)
		if (outdelta & 1)
			AddTransmitEdge((outnext & 1) != 0);

		// check for stepping transition
		if (outdelta & ~outnext & 0x08) {
			if (outnext & 4) {
				// step out (decreasing track number)
				if (mCurrentTrack > 0) {
					--mCurrentTrack;

					mFDC.SetCurrentTrack(mCurrentTrack * 2, mCurrentTrack != 0);

					PlayStepSound();
				}
			} else {
				// step in (increasing track number)
				if (mCurrentTrack < 90U) {
					++mCurrentTrack;

					mFDC.SetCurrentTrack(mCurrentTrack * 2, mCurrentTrack != 0);
				}

				PlayStepSound();
			}
		}

		if (outdelta & 0x20)
			UpdateROMBank();
	} else {
		mRIOT.WriteByte((uint8)addr, val);
	}
}

void ATDeviceDiskDriveSpeedyXF::OnControlWrite(uint32 addr, uint8 val) {
	g_ATLCDiskEmu("WriteControl($%04X, $%02X);\n", addr, val);

	switch(addr & 7) {
		case 0: {
			sint32 v = (sint32)val << 8;
			sint32 delta = (mLEDPattern ^ v) & 0xFF00;

			if (delta) {
				mLEDPattern ^= delta;

				mpDiskInterface->SetShowLEDReadout(mLEDPattern);
			}
			break;
		}

		case 1: {
			sint32 v = (sint32)val;
			sint32 delta = (mLEDPattern ^ v) & 0xFF;

			if (delta) {
				mLEDPattern ^= delta;

				mpDiskInterface->SetShowLEDReadout(mLEDPattern);
			}
			break;
		}

		case 4: {
			uint8 delta = mControlLatch ^ val;

			if (!delta)
				return;

			mControlLatch = val;

			if (delta & 0x10)
				mFDC.SetSide((val & 0x10) != 0);
			break;
		}

		default:
			break;
	}
}

void ATDeviceDiskDriveSpeedyXF::PlayStepSound() {
	if (!mbSoundsEnabled)
		return;

	const uint32 t = ATSCHEDULER_GETTIME(&mDriveScheduler);
	
	if (t - mLastStepSoundTime > 50000)
		mLastStepPhase = 0;

	mAudioPlayer.PlayStepSound(kATAudioSampleId_DiskStep2, 0.3f + 0.7f * cosf((float)mLastStepPhase++ * nsVDMath::kfPi));

	mLastStepSoundTime = t;
}

void ATDeviceDiskDriveSpeedyXF::UpdateRotationStatus() {
	bool motorEnabled;

	motorEnabled = (mRIOT.ReadOutputA() & 8) == 0;

	mpDiskInterface->SetShowMotorActive(motorEnabled);

	mAudioPlayer.SetRotationSoundEnabled(motorEnabled && mbSoundsEnabled);
}

void ATDeviceDiskDriveSpeedyXF::UpdateROMBank() {
	const uint8 *const romBase = mROM + (mRIOT.ReadOutputB() & 0x20 ? 0x8000 : 0);
	uintptr* readmap = mCoProc.GetReadMap();
	uintptr* writemap = mCoProc.GetWriteMap();

	ATCoProcMemoryMapView mmapView(readmap, writemap, mCoProc.GetTraceMap());
	mmapView.SetReadMem(0x20, 0x20, romBase + 0x0000);
	mmapView.SetReadMem(0xC0, 0x40, romBase + 0x4000);
}

void ATDeviceDiskDriveSpeedyXF::UpdateDiskStatus() {
	IATDiskImage *image = mpDiskInterface->GetDiskImage();

	mFDC.SetDiskImage(image, image != nullptr && mDiskChangeState == 0);

	if (!image)
		mFDC.SetDiskChangeStartupHackEnabled(false);

	UpdateWriteProtectStatus();
}

void ATDeviceDiskDriveSpeedyXF::UpdateWriteProtectStatus() {
	const bool wpsense = mpDiskInterface->GetDiskImage() && !mpDiskInterface->IsDiskWritable();
	const bool wpoverride = mDiskChangeState ? (mDiskChangeState & 1) != 0 : wpsense;

	mFDC.SetWriteProtectOverride(wpoverride);
}

void ATDeviceDiskDriveSpeedyXF::OnWriteEnabled() {
}


void ATDeviceDiskDriveSpeedyXF::UpdateAutoSpeed() {
	const float clockMultiplier = 8.0f / 8.333333f;
	float speed = 300.0f;

	// default clock speed
	mFDC.SetSpeeds(speed, clockMultiplier, false);
}
