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
#include <at/atcore/devicediskdrive.h>
#include <at/atcore/enumparseimpl.h>
#include <at/atcore/logging.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/wraptime.h>
#include <at/atcpu/memorymap.h>
#include "blackboxfloppy.h"
#include "firmwaremanager.h"
#include "irqcontroller.h"

extern ATLogChannel g_ATLCDiskEmu;

AT_DEFINE_ENUM_TABLE_BEGIN(ATBlackBoxFloppyType)
	{ ATBlackBoxFloppyType::FiveInch180K, "fiveinch180K" },
	{ ATBlackBoxFloppyType::FiveInch360K, "fiveinch360K" },
	{ ATBlackBoxFloppyType::FiveInch12M, "fiveinch12M" },
	{ ATBlackBoxFloppyType::ThreeInch360K, "threeinch360K" },
	{ ATBlackBoxFloppyType::ThreeInch720K, "threeinch720K" },
	{ ATBlackBoxFloppyType::ThreeInch144M, "threeinch144M" },
	{ ATBlackBoxFloppyType::EightInch1M, "eightinch1M" },
AT_DEFINE_ENUM_TABLE_END(ATBlackBoxFloppyType, ATBlackBoxFloppyType::FiveInch180K)

AT_DEFINE_ENUM_TABLE_BEGIN(ATBlackBoxFloppyMappingType)
	{ ATBlackBoxFloppyMappingType::XF551, "xf551" },
	{ ATBlackBoxFloppyMappingType::ATR8000, "atr8000" },
	{ ATBlackBoxFloppyMappingType::Percom, "percom" },
AT_DEFINE_ENUM_TABLE_END(ATBlackBoxFloppyMappingType, ATBlackBoxFloppyMappingType::XF551)

void ATCreateDeviceBlackBoxFloppyEmulator(const ATPropertySet& pset, IATDevice **dev) {
	vdrefptr<ATBlackBoxFloppyEmulator> p(new ATBlackBoxFloppyEmulator);
	p->SetSettings(pset);

	*dev = p.release();
}

extern constexpr ATDeviceDefinition g_ATDeviceDefBlackBoxFloppy = { "blackboxfloppy", "blackboxfloppy", L"Black Box Floppy Board", ATCreateDeviceBlackBoxFloppyEmulator };

ATBlackBoxFloppyEmulator::ATBlackBoxFloppyEmulator() {
}

ATBlackBoxFloppyEmulator::~ATBlackBoxFloppyEmulator() {
}

void *ATBlackBoxFloppyEmulator::AsInterface(uint32 id) {
	if (id == ATBlackBoxFloppyEmulator::kTypeID)
		return this;

	if (id == ATFDCEmulator::kTypeID)
		return &mFDC;

	if (id == ATVIA6522Emulator::kTypeID)
		return &mVIA;

	void *p = ATDiskDriveDebugTargetControl::AsInterface(id);
	if (p)
		return p;

	return ATDeviceT::AsInterface(id);
}

void ATBlackBoxFloppyEmulator::GetDeviceInfo(ATDeviceInfo& info) {
	info.mpDef = &g_ATDeviceDefBlackBoxFloppy;
}

void ATBlackBoxFloppyEmulator::GetSettings(ATPropertySet& settings) {
	VDStringA name;

	settings.Clear();

	for(int i=0; i<4; ++i) {
		if (mDiskInterfaceMappings[i]) {
			name.sprintf("driveslot%u", i);

			settings.SetInt32(name.c_str(), mDiskInterfaceMappings[i]);
		}

		name.sprintf("drivetype%u", i);
		settings.SetEnum(name.c_str(), mDriveTypes[i]);
	}
}

bool ATBlackBoxFloppyEmulator::SetSettings(const ATPropertySet& settings) {
	VDStringA name;
	bool mappingsChanged = false;

	for(int i=0; i<4; ++i) {
		name.sprintf("driveslot%u", i);
		int idx = settings.GetInt32(name.c_str(), 0);

		if (idx < 0 || idx > 15)
			idx = 0;

		if (mDiskInterfaceMappings[i] != idx) {
			mDiskInterfaceMappings[i] = idx;
			mappingsChanged = true;
		}

		name.sprintf("drivetype%u", i);

		auto newType = settings.GetEnum(name.c_str(), ATBlackBoxFloppyType::FiveInch180K);
		if (mDriveTypes[i] != newType) {
			mDriveTypes[i] = newType;
			mappingsChanged = true;
		}

		name.sprintf("drivemapping%u", i);

		auto newMappingType = settings.GetEnum(name.c_str(), ATBlackBoxFloppyMappingType::XF551);
		if (mDriveMappingTypes[i] != newMappingType) {
			mDriveMappingTypes[i] = newMappingType;
			mappingsChanged = true;
		}
	}

	if (mappingsChanged && mbInited) {
		auto prevSelect = mCurrentDriveIndex;

		SelectDrive(-1);

		ShutdownDiskInterfaceMappings();
		InitDiskInterfaceMappings();

		SelectDrive(prevSelect);
	}

	return true;
}

void ATBlackBoxFloppyEmulator::Init() {
	auto *schService = GetService<IATDeviceSchedulingService>();
	ATScheduler *sch = schService->GetMachineScheduler();

	mpSlowScheduler = schService->GetSlowScheduler();

	mAudioPlayer.InitAudioOutput(GetService<IATAudioMixer>());

	mpIrqController = GetService<ATIRQController>();
	mIrqHandle = mpIrqController->AllocateIRQ();

	mFDC.SetOnMotorChange(
		[this](bool enabled) {
			OnFDCMotorChange(enabled);
		}
	);

	mFDC.SetOnStep([this](bool inward) { OnFDCStep(inward); });
	mFDC.SetAutoIndexPulse(true);
	mFDC.Init(sch, 300.0f, 1.0f, ATFDCEmulator::kType_1770);

	mVIA.SetPortOutputFn(
		[](void *data, uint32 state) { ((ATBlackBoxFloppyEmulator *)data)->OnVIAOutputChanged(state); },
		this
	);

	mVIA.SetInterruptFn(
		[this](bool asserted) {
			OnVIAIrqChanged(asserted);
		}
	);

	mVIA.Init(sch);

	memset(&mDummyRead, 0xFF, sizeof mDummyRead);

	mBreakpointsImpl.BindBPHandler(mCoProc);
	mBreakpointsImpl.SetStepHandler(this);
	mBreakpointsImpl.SetBPsChangedHandler([this](const uint16 *pc) { mCoProc.OnBreakpointsChanged(pc); });

	mDriveScheduler.SetRate(sch->GetRate());
	mTargetProxy.mpDriveScheduler = &mDriveScheduler;
	mTargetProxy.Init(mCoProc);
	InitTargetControl(mTargetProxy, sch->GetRate().asDouble(), kATDebugDisasmMode_6502, &mBreakpointsImpl, this);

	// Memory map (6504 - 13-bit address space)
	//
	// 1000-1FFF		ROM
	// 0E00-0FFF		Unmapped
	// 0C00-0DFF		FDC
	// 0A00-0BFF		Status port (R)
	// 0800-09FF		Transfer port (R/W)
	// 0000-07FF		RAM

	using ThisType = ATBlackBoxFloppyEmulator;

	mReadNodeTransferLatch.BindMethods<&ThisType::OnReadTransferLatch, &ThisType::OnDebugReadTransferLatch>(this);
	mWriteNodeTransferLatch.BindMethod<&ThisType::OnWriteTransferLatch>(this);

	mReadNodeStatusLatch.BindMethod<&ThisType::OnReadStatusLatch>(this);

	mReadNodeFDC.BindMethods<&ThisType::OnReadFDC, &ThisType::OnDebugReadFDC>(this);
	mWriteNodeFDC.BindMethod<&ThisType::OnWriteFDC>(this);

	ATCoProcMemoryMapView mmap(mCoProc.GetReadMap(), mCoProc.GetWriteMap());
	mmap.Clear(mDummyRead, mDummyWrite);
	mmap.SetMemory(0x00, 0x08, mRAM);
	mmap.SetHandlers(0x08, 0x02, mReadNodeTransferLatch, mWriteNodeTransferLatch);
	mmap.SetReadHandler(0x0A, 0x02, mReadNodeStatusLatch);
	mmap.SetHandlers(0x0C, 0x02, mReadNodeFDC, mWriteNodeFDC);
	mmap.SetReadMem(0x10, 0x10, mROM);
	mmap.MirrorFwd(0x20, 0xE0, 0x00);

	mbInited = true;
}

void ATBlackBoxFloppyEmulator::Shutdown() {
	mbInited = false;

	if (mpSlowScheduler) {
		mpSlowScheduler->UnsetEvent(mpSlowEventTickFastSync);
		mpSlowScheduler = nullptr;
	}

	mFDC.SetDiskInterface(nullptr);
	mFDC.Shutdown();
	mVIA.Shutdown();

	ShutdownDiskInterfaceMappings();
	ShutdownTargetControl();

	if (mpIrqController) {
		mpIrqController->FreeIRQ(mIrqHandle);
		mpIrqController = nullptr;
	}

	mAudioPlayer.Shutdown();
}

void ATBlackBoxFloppyEmulator::ColdReset() {
	mManualIndexPhase = 0;

	ResetTargetControl();

	mFDC.Reset();

	WarmReset();
}

void ATBlackBoxFloppyEmulator::WarmReset() {
	mVIA.Reset();
	mCoProc.ColdReset();

	UpdateFDCIndex();
}

uint8 ATBlackBoxFloppyEmulator::DebugReadByteVIA(uint8 addr) const {
	return mVIA.DebugReadByte(addr);
}

uint8 ATBlackBoxFloppyEmulator::ReadByteVIA(uint8 addr) {
	ResetFastSyncWindow();
	Sync();

	return mVIA.ReadByte(addr);
}

void ATBlackBoxFloppyEmulator::WriteByteVIA(uint8 addr, uint8 value) {
	ResetFastSyncWindow();
	Sync();

	mVIA.WriteByte(addr, value);
}

void ATBlackBoxFloppyEmulator::InitFirmware(ATFirmwareManager *fwman) {
	mpFirmwareManager = fwman;

	ReloadFirmware();
}

bool ATBlackBoxFloppyEmulator::ReloadFirmware() {
	bool changed = false;

	if (mpFirmwareManager->LoadFirmware(mpFirmwareManager->GetCompatibleFirmware(kATFirmwareType_BlackBoxFloppy), mROM, 0, sizeof mROM, &changed)) {
		mFirmwareStatus = ATDeviceFirmwareStatus::OK;
	} else {
		mFirmwareStatus = ATDeviceFirmwareStatus::Missing;
	}

	return changed;
}

const wchar_t *ATBlackBoxFloppyEmulator::GetWritableFirmwareDesc(uint32 idx) const {
	return nullptr;
}

bool ATBlackBoxFloppyEmulator::IsWritableFirmwareDirty(uint32 idx) const {
	return false;
}

void ATBlackBoxFloppyEmulator::SaveWritableFirmware(uint32 idx, IVDStream& stream) {
}

ATDeviceFirmwareStatus ATBlackBoxFloppyEmulator::GetFirmwareStatus() const {
	return mFirmwareStatus;
}

void ATBlackBoxFloppyEmulator::InitDiskDrive(IATDiskDriveManager *ddm) {
	InitDiskInterfaceMappings();
}

ATDeviceDiskDriveInterfaceClient ATBlackBoxFloppyEmulator::GetDiskInterfaceClient(uint32 index) {
	return index < 4 ? ATDeviceDiskDriveInterfaceClient { this, index } : ATDeviceDiskDriveInterfaceClient{};
}

void ATBlackBoxFloppyEmulator::OnDiskChanged(bool mediaRemoved) {
	if (mCurrentDriveIndex >= 0) {
		ATDiskInterface *iface = mpDiskInterfaces[mCurrentDriveIndex];
		IATDiskImage *image = iface ? iface->GetDiskImage() : nullptr;

		mFDC.SetDiskImage(image, image != nullptr);

		mbCurrentDriveHasImage = image != nullptr;
		mFDC.SetMotorRunning(mbMotorEnabled && mbCurrentDriveHasImage);
	}
}

void ATBlackBoxFloppyEmulator::OnWriteModeChanged() {
}

void ATBlackBoxFloppyEmulator::OnTimingModeChanged() {
	if (mCurrentDriveIndex >= 0) {
		ATDiskInterface *iface = mpDiskInterfaces[mCurrentDriveIndex];

		if (iface)
			mFDC.SetAccurateTimingEnabled(iface->IsAccurateSectorTimingEnabled());
	}
}

void ATBlackBoxFloppyEmulator::OnAudioModeChanged() {
	if (mCurrentDriveIndex >= 0) {
		ATDiskInterface *iface = mpDiskInterfaces[mCurrentDriveIndex];

		if (iface)
			mbSoundEnabled = iface->AreDriveSoundsEnabled();
	}
}

bool ATBlackBoxFloppyEmulator::IsImageSupported(const IATDiskImage& image) const {
	return true;
}

void ATBlackBoxFloppyEmulator::OnScheduledEvent(uint32 id) {
	if (id == kEventId_TickFastSync) {
		mpSlowEventTickFastSync = nullptr;

		if (mFastSyncCounter) {
			if (--mFastSyncCounter)
				mpSlowEventTickFastSync = mpSlowScheduler->AddEvent(1, this, kEventId_TickFastSync);
		}
		return;
	}

	return ATDiskDriveDebugTargetControl::OnScheduledEvent(id);
}

void ATBlackBoxFloppyEmulator::Sync() {
	if (mbInSync)
		return;

	mbInSync = true;

	uint32 newDriveCycleLimit = AccumSubCycles();

	bool ranToCompletion = true;

	VDASSERT(mDriveScheduler.mNextEventCounter >= 0xFF000000);
	if (ATSCHEDULER_GETTIME(&mDriveScheduler) - newDriveCycleLimit >= 0x80000000) {
		mDriveScheduler.SetStopTime(newDriveCycleLimit);
		ranToCompletion = mCoProc.Run(mDriveScheduler);

		VDASSERT(ATWrapTime{ATSCHEDULER_GETTIME(&mDriveScheduler)} <= newDriveCycleLimit);
	}
	
	mbInSync = false;

	if (!ranToCompletion || mFastSyncCounter > 0)
		ScheduleImmediateResume();

	FlushStepNotifications();
}

void ATBlackBoxFloppyEmulator::ResetFastSyncWindow() {
	if (mFastSyncCounter < kFastSyncCount) {
		mFastSyncCounter = kFastSyncCount;

		if (mpSlowScheduler && !mpSlowEventTickFastSync)
			mpSlowScheduler->SetEvent(1, this, kEventId_TickFastSync, mpSlowEventTickFastSync);
	}
}

uint8 ATBlackBoxFloppyEmulator::OnDebugReadTransferLatch(uint32 addr) const {
	// read VIA port A output
	return (uint8)mVIAOutputState;
}

uint8 ATBlackBoxFloppyEmulator::OnReadTransferLatch(uint32 addr) {
	// read VIA port A output
	const uint8 v = (uint8)mVIAOutputState;

	// pull CA1 down for one cycle
	mVIA.SetCA1Input(false);
	mVIA.SetCA1Input(true);

	return v;
}

void ATBlackBoxFloppyEmulator::OnWriteTransferLatch(uint32 addr, uint8 value) {
	// drive value onto VIA port A bus
	mVIA.SetPortAInput(value);

	// pull CA1 down for one cycle so latching can occur -- this lets the computer
	// know a byte is pending, after which the controller waits for CA2 to go
	// low to indicate that the byte has been acknowledged.
	mVIA.SetCA1Input(false);
	mVIA.SetCA1Input(true);
}

uint8 ATBlackBoxFloppyEmulator::OnReadStatusLatch(uint32 addr) const {
	// D7 = DRQ
	// D6 = IRQ
	// D5 = NC
	// D4 = NC
	// D3 = VIA PB5
	// D2 = VIA CB2
	// D1 = VIA PB6
	// D0 = /IRQ

	uint8 value = 0b0'00110000;

	if (mFDC.GetDrqStatus())
		value |= 0x80;

	if (mFDC.GetIrqStatus())
		value |= 0x40;

	if (mVIAOutputState & 0x2000)
		value |= 0x08;

	if (mVIAOutputState & kATVIAOutputBit_CB2)
		value |= 0x04;

	if (mVIAOutputState & 0x4000)
		value |= 0x02;

	if (mVIAOutputState & kATVIAOutputBit_CA2)
		value |= 0x01;

	return value;
}

uint8 ATBlackBoxFloppyEmulator::OnDebugReadFDC(uint32 addr) const {
	return mFDC.DebugReadByte((uint8)addr);
}

uint8 ATBlackBoxFloppyEmulator::OnReadFDC(uint32 addr) {
	return mFDC.ReadByte((uint8)addr);
}

void ATBlackBoxFloppyEmulator::OnWriteFDC(uint32 addr, uint8 value) {
	mFDC.WriteByte((uint8)addr, value);
}

void ATBlackBoxFloppyEmulator::OnVIAOutputChanged(uint32 state) {
	const uint32 delta = mVIAOutputState ^ state;

	mVIAOutputState = state;

	if (delta & kATVIAOutputBit_CA2) {
		if (state & kATVIAOutputBit_CA2) {
			// assert IRQ
		} else {
			// negate IRQ
		}
	}

	// CB2 -> index pulse select (0 = drive, 1 = manual)
	bool updateIndex = false;
	if (delta & kATVIAOutputBit_CB2)
		updateIndex = true;

	// PB4 -> side select
	if (delta & 0x1000)
		mFDC.SetSide((state & 0x1000) == 0);

	// PB0 -> drive select 0 (inverted)
	// PB1 -> drive select 1 (inverted)
	// PB2 -> drive select 2 (inverted)
	// PB3 -> drive select 3
	if (delta & 0x0F00) {
		static constexpr sint8 kSelTable[16] {
			-1,
			0,
			1, -1,
			2, -1, -1, -1,
			3, -1, -1, -1, -1, -1, -1, -1
		};

		int newIndex = kSelTable[((state >> 8) & 15) ^ 8];

		SelectDrive(newIndex);
	}

	// PB5 -> selects FDC density (1 = FM, 0 = MFM)
	if (delta & 0x2000) {
		mFDC.SetDensity((state & 0x2000) == 0);
	}

	// PB6 -> selects FDC clock (0 = 16MHz, 1 = 8.333MHz)
	if (delta & 0x4000) {
		UpdateFDCSpeeds();
	}

	// PB7 -> clocks manual index pulse
	if (delta & 0x8000) {
		// falling edge clocks first flip flop
		if (!(state & 0x8000)) {
			++mManualIndexPhase;

			// falling edge from first flip flop clocks second flip flop
			// that actually drives manual index pulse
			if (!(mManualIndexPhase & 1) && !IsAutoIndexEnabled())
				updateIndex = true;
		}
	}

	if (updateIndex)
		UpdateFDCIndex();
}

void ATBlackBoxFloppyEmulator::OnVIAIrqChanged(bool asserted) {
	if (asserted)
		mpIrqController->Assert(mIrqHandle, false);
	else
		mpIrqController->Negate(mIrqHandle, false);

}

void ATBlackBoxFloppyEmulator::OnFDCStep(bool inward) {
	if (mCurrentDriveIndex < 0) {
		g_ATLCDiskEmu("Ignoring step signal as no drive selected\n");
		return;
	}

	int& track = mCurrentDriveTracks[mCurrentDriveIndex];
	if (inward) {
		if (track >= 94)
			return;

		++track;
	} else {
		if (track == 0)
			return;

		--track;
	}

	if (mbSoundEnabled) {
		const uint32 t = ATSCHEDULER_GETTIME(&mDriveScheduler);
		const uint32 dt = t - mLastStepSoundTime;
		float volumeScale = 1.0f;
	
		// Play at full volume at 12ms/step.
		// Play at half volume at 6ms/step.
		// Play at quarter volume at 3ms/step.

		if (dt < 20000)
			volumeScale = (int)dt / 20000.0f;

		mAudioPlayer.PlayStepSound(kATAudioSampleId_DiskStep2, volumeScale);

		mLastStepSoundTime = t;
	}

	g_ATLCDiskEmu("Disk %d stepped to track %d\n", mCurrentDriveIndex, track);

	UpdateFDCTrack();
}

void ATBlackBoxFloppyEmulator::OnFDCMotorChange(bool enabled) {
	if (mbMotorEnabled != enabled) {
		mbMotorEnabled = enabled;

		mFDC.SetMotorRunning(enabled);
		mAudioPlayer.SetRotationSoundEnabled(mbSoundEnabled && mbMotorEnabled);
	}
}

void ATBlackBoxFloppyEmulator::SelectDrive(int index) {
	if (mCurrentDriveIndex == index)
		return;

	mCurrentDriveIndex = index;

	ATDiskInterface *iface = index >= 0 ? mpDiskInterfaces[index] : nullptr;
	mFDC.SetDiskInterface(iface);

	if (iface) {
		IATDiskImage *image = iface->GetDiskImage();

		mbCurrentDriveHasImage = image != nullptr;

		mFDC.SetDiskImage(image, image != nullptr);
		mFDC.SetMotorRunning(mbMotorEnabled && mbCurrentDriveHasImage);

		uint32 trackCount = 40;

		switch(mDriveTypes[index]) {
			case ATBlackBoxFloppyType::ThreeInch360K:
			case ATBlackBoxFloppyType::ThreeInch720K:
			case ATBlackBoxFloppyType::ThreeInch144M:
				trackCount = 80;
				break;

			case ATBlackBoxFloppyType::FiveInch180K:
			case ATBlackBoxFloppyType::FiveInch360K:
				trackCount = 40;
				break;

			case ATBlackBoxFloppyType::FiveInch12M:
				trackCount = 80;
				break;

			case ATBlackBoxFloppyType::EightInch1M:
				trackCount = 77;
				break;
		}

		ATFDCEmulator::SideMapping sideMapping {};

		switch(mDriveMappingTypes[index]) {
			case ATBlackBoxFloppyMappingType::XF551:
			default:
				sideMapping = ATFDCEmulator::SideMapping::Side2Reversed;
				break;

			case ATBlackBoxFloppyMappingType::ATR8000:
				sideMapping = ATFDCEmulator::SideMapping::Side2Forward;
				break;

			case ATBlackBoxFloppyMappingType::Percom:
				sideMapping = ATFDCEmulator::SideMapping::Side2ReversedOffByOne;
				break;
		}

		mFDC.SetSideMapping(sideMapping, trackCount);

		mbSoundEnabled = iface->AreDriveSoundsEnabled();
		mAudioPlayer.SetRotationSoundEnabled(mbSoundEnabled && mbMotorEnabled);
	} else {
		mFDC.SetDiskImage(nullptr, false);
		mFDC.SetMotorRunning(false);

		mbSoundEnabled = false;
		mbCurrentDriveHasImage = false;
		mAudioPlayer.SetRotationSoundEnabled(false);
	}

	UpdateFDCTrack();
	UpdateFDCSpeeds();
	OnAudioModeChanged();
	OnTimingModeChanged();
}

bool ATBlackBoxFloppyEmulator::IsAutoIndexEnabled() const {
	return (mVIAOutputState & kATVIAOutputBit_CB2) == 0;
}

void ATBlackBoxFloppyEmulator::UpdateFDCTrack() {
	const int track = mCurrentDriveIndex >= 0 ? mCurrentDriveTracks[mCurrentDriveIndex] : 1;

	mFDC.SetCurrentTrack(track * 2, track == 0);
}

void ATBlackBoxFloppyEmulator::UpdateFDCSpeeds() {
	const bool fastClock = !(mVIAOutputState & 0x4000);
	float rpm = 300.0f;

	if (mCurrentDriveIndex >= 0) {
		switch(mDriveTypes[mCurrentDriveIndex]) {
			case ATBlackBoxFloppyType::FiveInch12M:
			case ATBlackBoxFloppyType::EightInch1M:
				rpm = 360.0f;
				break;

			default:
				break;
		}
	}

	mFDC.SetSpeeds(rpm, fastClock ? 1.0f : 8.0f / 8.3333333f, fastClock);
}

void ATBlackBoxFloppyEmulator::UpdateFDCIndex() {
	// The manual index flip flop and the index selector can toggle simultaneously,
	// so the logic below is designed to avoid glitching the FDC.

	const bool autoIndex = IsAutoIndexEnabled();

	if (autoIndex)
		mFDC.SetAutoIndexPulse(true, true);

	mFDC.OnIndexPulse(!autoIndex && (mManualIndexPhase & 2) != 0);

	if (!autoIndex)
		mFDC.SetAutoIndexPulse(true, false);
}

void ATBlackBoxFloppyEmulator::InitDiskInterfaceMappings() {
	auto *ddm = GetService<IATDiskDriveManager>();

	for(int i=0; i<4; ++i) {
		if (mDiskInterfaceMappings[i] > 0) {
			mpDiskInterfaces[i] = ddm->GetDiskInterface(mDiskInterfaceMappings[i] - 1);
			mpDiskInterfaces[i]->AddClient(this);
		}
	}
}

void ATBlackBoxFloppyEmulator::ShutdownDiskInterfaceMappings() {
	for(ATDiskInterface*& iface : mpDiskInterfaces) {
		if (iface) {
			iface->RemoveClient(this);
			iface = nullptr;
		}
	}
}
