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
#include <vd2/system/hash.h>
#include <vd2/system/int128.h>
#include <vd2/system/math.h>
#include <at/atcore/asyncdispatcher.h>
#include <at/atcore/audiosource.h>
#include <at/atcore/consoleoutput.h>
#include <at/atcore/deviceprinter.h>
#include <at/atcore/deviceserial.h>
#include <at/atcore/deviceindicators.h>
#include <at/atcore/logging.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/wraptime.h>
#include <at/atcpu/memorymap.h>
#include "850full.h"
#include "debuggerlog.h"
#include "firmwaremanager.h"
#include "memorymanager.h"

void ATCreateDevice850Full(const ATPropertySet& pset, IATDevice **dev) {
	vdrefptr<ATDevice850Full> p(new ATDevice850Full);
	p->SetSettings(pset);

	*dev = p.release();
}

extern const ATDeviceDefinition g_ATDeviceDef850Full = { "850full", "850full", L"850 Interface Module (full emulation)", ATCreateDevice850Full };

/////////////////////////////////////////////////////////////////////////////

constexpr uint32 ATDevice850Full::kCyclesPerBitSettings[16] = {
	(uint32)(0.5 + 1108404.5 /  300.0),
	(uint32)(0.5 + 1108404.5 /   45.5),
	(uint32)(0.5 + 1108404.5 /   50.0),
	(uint32)(0.5 + 1108404.5 /   56.875),
	(uint32)(0.5 + 1108404.5 /   75.0),
	(uint32)(0.5 + 1108404.5 /  110.0),
	(uint32)(0.5 + 1108404.5 /  134.5),
	(uint32)(0.5 + 1108404.5 /  150.0),
	(uint32)(0.5 + 1108404.5 /  300.0),
	(uint32)(0.5 + 1108404.5 /  600.0),
	(uint32)(0.5 + 1108404.5 / 1200.0),
	(uint32)(0.5 + 1108404.5 / 1800.0),
	(uint32)(0.5 + 1108404.5 / 2400.0),
	(uint32)(0.5 + 1108404.5 / 4800.0),
	(uint32)(0.5 + 1108404.5 / 9600.0),
	(uint32)(0.5 + 1108404.5 / 9600.0),
};

constexpr uint32 ATDevice850Full::kBaudRateSettings[16] = {
	300,
	46,
	50,
	57,
	75,
	110,
	135,
	150,
	300,
	600,
	1200,
	1800,
	2400,
	4800,
	9600,
	9600,
};

constexpr uint8 ATDevice850Full::kBaudRateSortOrder[14] = {
	1, 2, 3, 4, 5, 6, 7, 0, 9, 10, 11, 12, 13, 14
};

ATDevice850Full::ATDevice850Full() {
	mSerialPort[0].Init(this, 0, IATDeviceSerial::kTypeID, "serial", L"Serial port 1", "serport1");
	mSerialPort[1].Init(this, 1, IATDeviceSerial::kTypeID, "serial", L"Serial port 2", "serport2");
	mSerialPort[2].Init(this, 2, IATDeviceSerial::kTypeID, "serial", L"Serial port 3", "serport3");
	mSerialPort[3].Init(this, 3, IATDeviceSerial::kTypeID, "serial", L"Serial port 4", "serport4");
	mParallelPort.Init(this, 4, IATPrinterOutput::kTypeID, "parallel", L"Printer Output", "parport");

	for(int i = 0; i < 4; ++i) {
		auto& serPort = mSerialPort[i];

		serPort.SetOnAttach(
			[i, this] {
				OnSerialPortAttach(i);
			}
		);

		serPort.SetOnDetach(
			[i, this] {
				OnSerialPortDetach(i);
			}
		);
	}

	mParallelPort.SetOnAttach(
		[this] {
			// lower RIOT #2 PA6 (FAULT)
			mRIOT2.SetInputA(0x00, 0x40);
		}
	);

	mParallelPort.SetOnDetach(
		[this] {
			// raise RIOT #2 PA6 (FAULT)
			mRIOT2.SetInputA(0x40, 0x40);
		}
	);

	// need to raise the max cycles per bit much higher than usual to accommodate
	// 300 baud (5966 cpb).
	mSerialXmitQueue.SetMaxCyclesPerBit(7000);

	mBreakpointsImpl.BindBPHandler(mCoProc);
	mBreakpointsImpl.SetStepHandler(this);
	mBreakpointsImpl.SetBPsChangedHandler([this](const uint16 *pc) { mCoProc.OnBreakpointsChanged(pc); });

	memset(&mDummyRead, 0xFF, sizeof mDummyRead);

	// The schematic shows a 4.43MHz clock crystal, but the Atari 850 Interface Module
	// Field Service Manual p.A4-4 gives the precise rate.
	static constexpr uint32 kClockRateNum = 4433618;
	static constexpr uint32 kClockRateDenom = 4;

	const VDFraction clockRate(kClockRateNum, kClockRateDenom);
	mDriveScheduler.SetRate(clockRate);
	mTargetProxy.mpDriveScheduler = &mDriveScheduler;
	mTargetProxy.Init(mCoProc);
	InitTargetControl(mTargetProxy, clockRate.asDouble(), kATDebugDisasmMode_6502, &mBreakpointsImpl, this);
}

ATDevice850Full::~ATDevice850Full() {
}

void *ATDevice850Full::AsInterface(uint32 iid) {
	switch(iid) {
		case IATDeviceScheduling::kTypeID: return static_cast<IATDeviceScheduling *>(this);
		case IATDeviceFirmware::kTypeID: return static_cast<IATDeviceFirmware *>(this);
		case IATDeviceSIO::kTypeID: return static_cast<IATDeviceSIO *>(this);
		case IATDeviceParent::kTypeID: return static_cast<IATDeviceParent *>(this);
		case IATDeviceDiagnostics::kTypeID: return static_cast<IATDeviceDiagnostics *>(this);
	}

	void *p = ATDiskDriveDebugTargetControl::AsInterface(iid);
	if (p)
		return p;

	return ATDevice::AsInterface(iid);
}

void ATDevice850Full::GetDeviceInfo(ATDeviceInfo& info) {
	info.mpDef = &g_ATDeviceDef850Full;
}

void ATDevice850Full::GetSettings(ATPropertySet& settings) {
	settings.Clear();

	settings.SetInt32("serbaud1", mSerialPortBaudRateSettings[0]);
	settings.SetInt32("serbaud2", mSerialPortBaudRateSettings[1]);
	settings.SetInt32("serbaud3", mSerialPortBaudRateSettings[2]);
	settings.SetInt32("serbaud4", mSerialPortBaudRateSettings[3]);
}

bool ATDevice850Full::SetSettings(const ATPropertySet& settings) {
	mSerialPortBaudRateSettings[0] = (uint8)std::clamp<sint32>(settings.GetInt32("serbaud1", 0), 0, 16);
	mSerialPortBaudRateSettings[1] = (uint8)std::clamp<sint32>(settings.GetInt32("serbaud2", 0), 0, 16);
	mSerialPortBaudRateSettings[2] = (uint8)std::clamp<sint32>(settings.GetInt32("serbaud3", 0), 0, 16);
	mSerialPortBaudRateSettings[3] = (uint8)std::clamp<sint32>(settings.GetInt32("serbaud4", 0), 0, 16);

	// lock baud rates for any ports not set to auto
	for(int i=0; i<4; ++i) {
		if (mSerialPortBaudRateSettings[i])
			mSerialPortBaudRateIndices[i] = mSerialPortBaudRateSettings[i] - 1;
	}

	return true;
}

void ATDevice850Full::Init() {
	// We need to do this early to ensure that the clock divisor is set before we perform init processing.
	ResetTargetControl();

	mSerialXmitQueue.Init(mpScheduler, mpSIOMgr);

	mRIOT1.Init(&mDriveScheduler);
	mRIOT2.Init(&mDriveScheduler);

	ATCoProcMemoryMapView mmap(mCoProc.GetReadMap(), mCoProc.GetWriteMap());

	// 850 memory map (6507 - 13-bit address space)
	//
	// A12=1					ROM
	// A12=0, A8=1, A7=0		RIOT #1 RAM
	// A12=0, A8=1, A7=1		RIOT #1 registers (parallel, serial 1)
	// A12=0, A8=0, A7=0		RIOT #2 registers (serial 2-4)
	// A12=0, A8=0, A7=1		RIOT #2 RAM

	mReadNodeRIOT1.BindMethods<&ATDevice850Full::OnRIOT1Read, &ATDevice850Full::OnRIOT1DebugRead>(this);
	mReadNodeRIOT2.BindMethods<&ATDevice850Full::OnRIOT2Read, &ATDevice850Full::OnRIOT2DebugRead>(this);
	mWriteNodeRIOT1.BindMethod<&ATDevice850Full::OnRIOT1Write>(this);
	mWriteNodeRIOT2.BindMethod<&ATDevice850Full::OnRIOT2Write>(this);

	mmap.Clear(mDummyRead, mDummyWrite);
	mmap.SetHandlers(0x00, 0x01, mReadNodeRIOT2, mWriteNodeRIOT2);
	mmap.SetHandlers(0x01, 0x01, mReadNodeRIOT1, mWriteNodeRIOT1);
	mmap.MirrorFwd(0x02, 0x0E, 0x00);
	mmap.SetReadMem(0x10, 0x10, mROM);
	mmap.MirrorFwd(0x20, 0xE0, 0x00);
}

void ATDevice850Full::Shutdown() {
	mParallelPort.Shutdown();
	for(auto& serPort : mSerialPort)
		serPort.Shutdown();

	mDriveScheduler.UnsetEvent(mpDeviceEventRIOTSignalChange);

	for(auto& ev : mpDeviceEventShiftOutComplete)
		mDriveScheduler.UnsetEvent(ev);

	for(auto& serialByteEvent : mpEventReceiveByte) {
		mpScheduler->UnsetEvent(serialByteEvent);
	}

	ShutdownTargetControl();

	mpFwMgr = nullptr;

	mSerialXmitQueue.Shutdown();

	if (mpSIOMgr) {
		mpSIOMgr->RemoveRawDevice(this);
		mpSIOMgr = nullptr;
	}
}

void ATDevice850Full::WarmReset() {
	// If the computer resets, its transmission is interrupted.
	OnTruncateByte();
}

void ATDevice850Full::ComputerColdReset() {
	// The 850 ties SIO READY to reset in hardware, so the 850 is hard reset
	// whenever the computer is power cycled.
	PeripheralColdReset();
}

void ATDevice850Full::PeripheralColdReset() {
	mSerialXmitQueue.Reset();
	InvalidateRIOTQueuedSignalChanges();

	// truncate all output shifters
	for(auto& ev : mpDeviceEventShiftOutComplete)
		mDriveScheduler.UnsetEvent(ev);

	for(auto& ostate : mSerialOutputStates)
		ostate.mShiftCyclesPerBit = 0;
	
	mbLastSIODataIn = true;
	mbDirectReceiveOutput = true;
	mbDirectTransmitOutput = true;

	mCoProc.ColdReset();
	mRIOT1.Reset();
	mRIOT2.Reset();
	UpdateSIODataIn();

	memset(&mRAM, 0xFF, sizeof mRAM);

	ResetTargetControl();

	WarmReset();
}

IATDeviceBus *ATDevice850Full::GetDeviceBusById(sint32 id) {
	switch(id) {
		case 0: return &mSerialPort[0];
		case 1: return &mSerialPort[1];
		case 2: return &mSerialPort[2];
		case 3: return &mSerialPort[3];
		case 4: return &mParallelPort;
		default:
			return nullptr;
	}
}

sint32 ATDevice850Full::GetDeviceBusIdByIndex(uint32 index) {
	return index < 5 ? (sint32)index : -1;
}

void ATDevice850Full::InitFirmware(ATFirmwareManager *fwman) {
	mpFwMgr = fwman;

	ReloadFirmware();
}

bool ATDevice850Full::ReloadFirmware() {
	const vduint128 oldIHash = VDHash128(mROM, sizeof mROM);

	uint8 irom[4096] = {};

	uint32 len = 0;
	bool iromUsable = false;
	mpFwMgr->LoadFirmware(mpFwMgr->GetFirmwareOfType(kATFirmwareType_850, true), irom, 0, sizeof irom, nullptr, &len, nullptr, nullptr, &iromUsable);

	memcpy(mROM, irom, sizeof mROM);

	const vduint128 newIHash = VDHash128(mROM, sizeof mROM);

	mbFirmwareUsable = iromUsable;

	return oldIHash != newIHash;
}

const wchar_t *ATDevice850Full::GetWritableFirmwareDesc(uint32 idx) const {
	return nullptr;
}

bool ATDevice850Full::IsWritableFirmwareDirty(uint32 idx) const {
	return false;
}

void ATDevice850Full::SaveWritableFirmware(uint32 idx, IVDStream& stream) {
}

ATDeviceFirmwareStatus ATDevice850Full::GetFirmwareStatus() const {
	return mbFirmwareUsable ? ATDeviceFirmwareStatus::OK : ATDeviceFirmwareStatus::Missing;
}

void ATDevice850Full::InitSIO(IATDeviceSIOManager *mgr) {
	mpSIOMgr = mgr;
	mpSIOMgr->AddRawDevice(this);
}

void ATDevice850Full::OnScheduledEvent(uint32 id) {
	if (id == kEventId_RIOTSignalChange) {
		mpDeviceEventRIOTSignalChange = nullptr;

		ExecuteRIOTQueuedSignalChanges();
	} else if (id >= kEventId_SerialReceiveByte && id < kEventId_SerialReceiveByte + 4) {
		const int serialPortIndex = id - kEventId_SerialReceiveByte;

		mpEventReceiveByte[serialPortIndex] = nullptr;

		OnSerialPortReadReady(serialPortIndex);
	} else if (id >= kEventId_SerialShiftOutComplete && id < kEventId_SerialShiftOutComplete + 4) {
		const int serialPortIndex = id - kEventId_SerialShiftOutComplete;

		mpDeviceEventShiftOutComplete[serialPortIndex] = nullptr;

		OnSerialShiftOutComplete(serialPortIndex);
	} else
		return ATDiskDriveDebugTargetControl::OnScheduledEvent(id);
}

void ATDevice850Full::OnCommandStateChanged(bool asserted) {
	// SIO COMMAND -> RIOT #1 PA7
	RIOTSignalChange change;
	change.Set1A(asserted ? 0x80 : 0x00, 0x80);

	AddRIOTSignalChange(MasterTimeToDriveTime(), change, true);
}

void ATDevice850Full::OnMotorStateChanged(bool asserted) {
}

void ATDevice850Full::OnBeginReceiveByte(uint8 c, bool command, uint32 cyclesPerBit) {
	// stash last cycles/bit for auto-baud rate
	mLastPOKEYCyclesPerBit = cyclesPerBit;

	// compute shift timing
	const double deviceCyclesPerBitF = cyclesPerBit * mpScheduler->GetRate().AsInverseDouble() * mDriveScheduler.GetRate().asDouble();

	// inverted SIO DATA OUT -> RIOT#2 PA0
	// queue immediate edge for start bit; truncate any later changes to RIOT#2 PA0
	// in case we have slight overlap due to roundoff
	const uint32 deviceTimeStart = MasterTimeToDriveTime();
	RIOTSignalChange change;
	change.Set2A(1, 1);

	AddRIOTSignalChange(deviceTimeStart, change, true);

	// compute shift pattern -- inverted data bits LSB first, followed by inverted
	// stop bit
	uint32 shiftPattern = ~c & 0xFF;

	// queue 9 more transitions for the leading edges of each data bit, LSB
	// first, and then the stop bit
	for(int i=0; i < 9; ++i) {
		uint32 deviceEdgeTime = deviceTimeStart + (uint32)VDRoundToInt(deviceCyclesPerBitF * (i + 1));

		change.Set2A(shiftPattern & 1, 1);
		shiftPattern >>= 1;
		AddRIOTSignalChange(deviceEdgeTime, change, true);
	}
}

void ATDevice850Full::OnReceiveByte(uint8 c, bool command, uint32 cyclesPerBit) {
}

void ATDevice850Full::OnTruncateByte() {
	RIOTSignalChange change;
	change.Set2A(0, 1);

	AddRIOTSignalChange(MasterTimeToDriveTime(), change, true);
}

void ATDevice850Full::OnSendReady() {
}

void ATDevice850Full::DumpStatus(ATConsoleOutput& output) {
	const ATDeviceSerialTerminalState termStates[3] {
		ComputeSerialPortControlState(0),
		ComputeSerialPortControlState(1),
		ComputeSerialPortControlState(2)
	};

	output <<=	 "     Port 1    Port 2    Port 3    Port 4";
	output		("DTR   %-3s       %-3s       %-3s"
		, termStates[0].mbDataTerminalReady ? "on" : "off"
		, termStates[1].mbDataTerminalReady ? "on" : "off"
		, termStates[2].mbDataTerminalReady ? "on" : "off"
	);

	output		("RTS   %-3s"
		, termStates[0].mbRequestToSend ? "on" : "off"
	);

	const uint8 r2porta = mRIOT2.ReadOutputA();
	output		("XMT   %-3s       %-3s       %-3s       %-3s"
		, r2porta & 0x02 ? "on" : "off"
		, r2porta & 0x04 ? "on" : "off"
		, r2porta & 0x08 ? "on" : "off"
		, r2porta & 0x10 ? "on" : "off"
	);
}

void ATDevice850Full::Sync() {
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

// RIOT connections:
//
// RIOT #1 (parallel, serial 1):
//	PA0 (I): Serial 1 pin 4 (RX)
//	PA1 (I): Serial 1 pin 6 (inverted DSR)
//	PA2 (I): Serial 2 pin 6 (inverted DSR)
//	PA3 (I): Serial 3 pin 6 (inverted DSR)
//	PA4 (I): Serial 1 pin 2 (inverted CD)
//	PA5 (I): Serial 1 pin 8 (inverted CTS)
//	PA6 (I): Grounded
//	PA7 (I): SIO COMMAND (active high)
//	PB0 (O): Parallel pin 2 (D0)
//	PB1 (O): Parallel pin 3 (D1)
//	PB2 (O): Parallel pin 4 (D2)
//	PB3 (O): Parallel pin 5 (D3)
//	PB4 (O): Parallel pin 6 (D4)
//	PB5 (O): Parallel pin 7 (D5)
//	PB6 (O): Parallel pin 8 (D6)
//	PB7 (O): Parallel pin 15 (D7)
//
// RIOT #2 (serial 2-4)
//	PA0 (I): inverted SIO DATA OUT
//	PA1 (O): Serial 1 pin 3 (TX)
//	PA2 (O): Serial 2 pin 3 (TX)
//	PA3 (O): Serial 3 pin 3 (TX)
//	PA4 (O): Serial 4 pin 3 (TX)
//	PA5 (O): SIO DATA IN
//	PA6 (I): Parallel pin 12 (inverted /FAULT)
//	PA7 (I): Serial 3 pin 4 (RX)
//	PB0 (I): Serial 4 pin 4 (RX)
//	PB1 (O): Serial 1 pin 1 (inverted DTR)
//	PB2 (O): Serial 2 pin 1 (inverted DTR)
//	PB3 (O): Serial 3 pin 1 (inverted DTR)
//	PB4 (O): Serial 1 pin 7 (RTS)
//	PB5 (O): Parallel pin 1 (inverted /STROBE)
//	PB6 (I): Parallel pin 13 (inverted BUSY)
//	PB7 (I): Serial 2 pin 4 (RX)

uint8 ATDevice850Full::OnRIOT1DebugRead(uint32 address) const {
	if (!(address & 0x80))
		return mRAM[0x80 + (address & 0x7F)];

	return mRIOT1.DebugReadByte((uint8)address);
}

uint8 ATDevice850Full::OnRIOT1Read(uint32 address) {
	if (!(address & 0x80))
		return mRAM[0x80 + (address & 0x7F)];

	return mRIOT1.ReadByte((uint8)address);
}

void ATDevice850Full::OnRIOT1Write(uint32 address, uint8 value) {
	if (!(address & 0x80)) {
		mRAM[0x80 + (address & 0x7F)] = value;
		return;
	}
	
	mRIOT1.WriteByte((uint8)address, value);
}

uint8 ATDevice850Full::OnRIOT2DebugRead(uint32 address) const {
	if (address & 0x80)
		return mRAM[address & 0x7F];

	return mRIOT2.DebugReadByte((uint8)address);
}

uint8 ATDevice850Full::OnRIOT2Read(uint32 address) {
	if (address & 0x80)
		return mRAM[address & 0x7F];

	return mRIOT2.ReadByte((uint8)address);
}

void ATDevice850Full::OnRIOT2Write(uint32 address, uint8 value) {
	if (address & 0x80) {
		mRAM[address & 0x7F] = value;
		return;
	}

	const uint8 preva = mRIOT2.ReadOutputA();
	const uint8 prevb = mRIOT2.ReadOutputB();

	mRIOT2.WriteByte((uint8)address, value);

	const uint8 nexta = mRIOT2.ReadOutputA();
	const uint8 nextb = mRIOT2.ReadOutputB();
	const uint8 deltaa = nexta ^ preva;
	const uint8 deltab = nextb ^ prevb;
	
	if (deltaa) {
		if (deltaa & 0x02)
			UpdateSerialPortOutputState(0, nexta);

		if (deltaa & 0x04)
			UpdateSerialPortOutputState(1, nexta);

		if (deltaa & 0x08)
			UpdateSerialPortOutputState(2, nexta);

		if (deltaa & 0x10)
			UpdateSerialPortOutputState(3, nexta);
	}

	if (deltab) {
		// check for control signal changes
		if (deltab & 0x12)
			UpdateSerialPortControlState(0);

		if (deltab & 0x04)
			UpdateSerialPortControlState(1);

		if (deltab & 0x08)
			UpdateSerialPortControlState(2);

		// check for PB5 positive edge to pulse /STROBE
		if (nextb & ~prevb & 0x20) {
			if (auto *printer = mParallelPort.GetChild<IATPrinterOutput>()) {
				const uint8 c = (uint8)~mRIOT1.ReadOutputB();

				printer->WriteRaw(&c, 1);
			}
		}
	}

	UpdateSIODataIn();
}

void ATDevice850Full::OnSerialPortAttach(int index) {
	IATDeviceSerial *dev = mSerialPort[index].GetChild<IATDeviceSerial>();

	if (dev) {
		dev->SetOnReadReady(
			[index, this] {
				OnSerialPortReadReady(index);
			}
		);

		// port 4 doesn't have any incoming signals, so we don't need to
		// monitor for it
		if (index < 3) {
			dev->SetOnStatusChange(
				[index, this](const ATDeviceSerialStatus& status) {
					UpdateSerialPortStatus(index, status);
				}
			);

			UpdateSerialPortStatus(index, dev->GetStatus());
		}

		UpdateSerialPortControlState(index);
		OnSerialPortReadReady(index);
	}
}

void ATDevice850Full::OnSerialPortDetach(int index) {
	IATDeviceSerial *dev = mSerialPort[index].GetChild<IATDeviceSerial>();

	if (dev) {
		dev->SetOnReadReady(nullptr);
		dev->SetOnStatusChange(nullptr);
	}

	// reset control signals
	RIOTSignalChange change;

	switch(index) {
		case 0:	// port 1: RX, CTS, CD, DSR
			change.Set1A(0xFF, 0x33);
			break;

		case 1:	// port 2: RX, DSR
			change.Set1A(0xFF, 0x04);
			change.Set2B(0xFF, 0x80);
			break;

		case 2:	// port 3: RX, DSR
			change.Set1A(0xFF, 0x08);
			change.Set2A(0xFF, 0x80);
			break;

		case 3:	// port 4: RX
			change.Set2B(0xFF, 0x01);
			break;
	}

	AddRIOTSignalChange(MasterTimeToDriveTime(), change, true);

	mpScheduler->UnsetEvent(mpEventReceiveByte[index]);
}

void ATDevice850Full::OnSerialPortReadReady(int index) {
	// bail if a byte is already shifting
	if (mpEventReceiveByte[index])
		return;

	// check if we still have a device
	IATDeviceSerial *dev = mSerialPort[index].GetChild<IATDeviceSerial>();
	if (!dev)
		return;

	// try to read a byte
	uint8 c = 0;
	uint32 baudRate = 0;
	if (!dev->Read(baudRate, c))
		return;

	// if no baud rate is specified, we can't handle this byte
	if (!baudRate)
		return;

	// compute timing in device clock cycles
	const double invBaudRate = 1.0 / (double)baudRate;
	const double deviceCyclesPerBitF = mDriveScheduler.GetRate().asDouble() * invBaudRate;

	// Insert event for leading edge of start bit, truncating any previous
	// transmission (conceptually shouldn't happen, but may due to roundoff).
	//
	// port 1: RIOT #1 PA0
	// port 2: RIOT #2 PB7
	// port 3: RIOT #2 PA7
	// port 4: RIOT #2 PB0
	//
	RIOTSignalChange change;
	switch(index) {
		case 0:	change.Set1A(0x00, 0x01); break;
		case 1:	change.Set2B(0x00, 0x80); break;
		case 2:	change.Set2A(0x00, 0x80); break;
		case 3:	change.Set2B(0x00, 0x01); break;
	}

	const uint64 deviceBaseTime = MasterTimeToDriveTime();
	AddRIOTSignalChange(deviceBaseTime, change, true);

	// Add 9 more changes for the leading edges of the 8 data bits, LSB first,
	// followed by the stop bit.

	uint32 shiftPattern = (c & 0xFF) | 0x100;

	for(int i = 0; i < 9; ++i) {
		const uint32 deviceEdgeTime = deviceBaseTime + VDRoundToInt(deviceCyclesPerBitF * (i + 1));

		change.mRIOTValueBits = shiftPattern & 1 ? change.mRIOTMaskBits : 0;
		shiftPattern >>= 1;

		AddRIOTSignalChange(deviceEdgeTime, change, false);
	}

	// queue event to recheck the serial port after byte is complete in master time
	const uint32 byteTime = VDRoundToInt32(mpScheduler->GetRate().asDouble() * invBaudRate * 10.0);

	mpScheduler->SetEvent(std::max<uint32>(1, byteTime), this, kEventId_SerialReceiveByte + index, mpEventReceiveByte[index]);
}

void ATDevice850Full::UpdateSerialPortStatus(int index, const ATDeviceSerialStatus& status) {
	RIOTSignalChange change;

	switch(index) {
		case 0:
			// ~DSR -> RIOT#1 PA1
			// ~CD -> RIOT#1 PA4
			// ~CTS -> RIOT#1 PA5
			change.Set1A(
				(status.mbDataSetReady ? 0x00 : 0x02) +
				(status.mbCarrierDetect ? 0x00 : 0x10) +
				(status.mbClearToSend ? 0x00 : 0x20),
				0x32
			);

			break;

		case 1:
			// ~DSR -> RIOT#1 PA2
			change.Set1A(status.mbDataSetReady ? 0x00 : 0xFF, 0x04);
			break;

		case 2:
			// ~DSR -> RIOT#1 PA3
			change.Set1A(status.mbDataSetReady ? 0x00 : 0xFF, 0x08);
			break;
	}

	AddRIOTSignalChange(MasterTimeToDriveTime(), change, false);
}

void ATDevice850Full::UpdateSerialPortControlState(int index) {
	// All output control signals are on RIOT#2 port B:
	//	PB1 (O): Serial 1 pin 1 (inverted DTR)
	//	PB2 (O): Serial 2 pin 1 (inverted DTR)
	//	PB3 (O): Serial 3 pin 1 (inverted DTR)
	//	PB4 (O): Serial 1 pin 7 (RTS)
	IATDeviceSerial *dev = mSerialPort[index].GetChild<IATDeviceSerial>();
	if (!dev)
		return;

	dev->SetTerminalState(ComputeSerialPortControlState(index));
}

ATDeviceSerialTerminalState ATDevice850Full::ComputeSerialPortControlState(int index) const {
	ATDeviceSerialTerminalState termState;

	if (index < 4) {
		const uint8 pb = mRIOT2.ReadOutputB();

		switch(index) {
			case 0:
				termState.mbDataTerminalReady = !(pb & 0x02);
				termState.mbRequestToSend = !(pb & 0x10);
				break;

			case 1:
				termState.mbDataTerminalReady = !(pb & 0x04);
				break;

			case 2:
				termState.mbDataTerminalReady = !(pb & 0x08);
				break;
		}
	}

	return termState;
}

void ATDevice850Full::UpdateSerialPortOutputState(int index, uint8 r2porta) {
	const uint32 deviceTime = mDriveScheduler.GetTick();
	SerialOutputState& ostate = mSerialOutputStates[index];

	// determine current output state
	//	PA1 (O): Serial 1 pin 3 (TX)
	//	PA2 (O): Serial 2 pin 3 (TX)
	//	PA3 (O): Serial 3 pin 3 (TX)
	//	PA4 (O): Serial 4 pin 3 (TX)
	const bool outputState = (r2porta & (2 << index)) != 0;

	// check if we are currently shifting
	if (!ostate.mShiftCyclesPerBit) {
		// we are not -- check if we have a start bit, if we don't then we're
		// not interested
		if (outputState)
			return;

		// init for new byte
		StartShiftOutNewByte(index, deviceTime);
		return;
	}

	// determine number of bits that have been sampled by this point, and limit
	// to 9 bits (8 data + 1 stop)
	const uint32 bitCounter = std::min<uint32>(10, (deviceTime - ostate.mShiftStartBaseTime) / ostate.mShiftCyclesPerBit);

	// determine number of new bits to shift in
	const uint32 newBits = bitCounter - ostate.mBitsShifted;

	// early out if no new bits shifted
	if (!newBits)
		return;

	// check if we're shifting in a bogus start bit
	if (ostate.mBitsShifted == 0 && ostate.mbLastOutputBit) {
		// yup, bogus start bit, cancel the currently shifting byte
		mDriveScheduler.UnsetEvent(mpDeviceEventShiftOutComplete[index]);

		// check if we're also starting a new byte (!)
		if (!outputState)
			StartShiftOutNewByte(index, deviceTime);

		return;
	}

	// update bits shifted counter
	ostate.mBitsShifted = bitCounter;

	// shift in the bits -- note that we must use the _last_ output state, not
	// the new output state
	ostate.mShiftRegister >>= newBits;

	if (ostate.mbLastOutputBit)
		ostate.mShiftRegister += 0x400 - (0x400 >> newBits);

	const bool prevOutput = ostate.mbLastOutputBit;
	ostate.mbLastOutputBit = outputState;

	// check if we have completed a full byte (1 start + 8 data + 1 stop)
	if (bitCounter >= 10) {
		// clear the wait for byte end event
		mDriveScheduler.UnsetEvent(mpDeviceEventShiftOutComplete[index]);

		// check if we got a framing error
		if (ostate.mShiftRegister & 0x200) {
			// send the byte out to the device, if there is one attached
			IATDeviceSerial *dev = mSerialPort[index].GetChild<IATDeviceSerial>();
			if (dev) {
				const uint8 c = (uint8)(ostate.mShiftRegister >> 1);

				dev->Write(kBaudRateSettings[mSerialPortBaudRateIndices[index]], c);
			}
		}

		// end shift
		ostate.mShiftCyclesPerBit = 0;

		// check if we need to start a new byte
		if (prevOutput && !outputState)
			StartShiftOutNewByte(index, deviceTime);
	}
}

void ATDevice850Full::StartShiftOutNewByte(int index, uint32 deviceTime) {
	SerialOutputState& ostate = mSerialOutputStates[index];

	// check if we are set to auto baud rate
	if (mSerialPortBaudRateSettings[index] == 0) {
		// query POKEY's receive rate, and choose the closest 850 baud rate
		// if it's <19200 baud
		if (mLastPOKEYCyclesPerBit && mLastPOKEYCyclesPerBit > 130) {
			const uint32 pokeyBaudRate = (uint32)(0.5 + mpScheduler->GetRate().asDouble() / (double)mLastPOKEYCyclesPerBit);

			uint8 baudIndex = (uint8)(std::lower_bound(
				std::begin(kBaudRateSortOrder),
				std::end(kBaudRateSortOrder),
				pokeyBaudRate,
				[](uint8 i, uint32 baudRate) {
					return kBaudRateSettings[i] < baudRate;
				}
			) - std::begin(kBaudRateSortOrder));

			if (baudIndex >= 14) {
				baudIndex = 13;
			} else if (baudIndex) {
				// check if the previous baud rate is a better match
				uint32 baudRate1 = kBaudRateSettings[kBaudRateSortOrder[baudIndex - 1]];
				uint32 baudRate2 = kBaudRateSettings[kBaudRateSortOrder[baudIndex]];

				if (baudRate2 - pokeyBaudRate > pokeyBaudRate - baudRate1)
					--baudIndex;
			}

			mSerialPortBaudRateIndices[index] = kBaudRateSortOrder[baudIndex];
		}
	}

	// set cycles per bit according to current baud rate
	ostate.mShiftCyclesPerBit = kCyclesPerBitSettings[mSerialPortBaudRateIndices[index]];

	// offset the base time backwards by half a bit cell so the start bit is
	// sampled next half a bit from now
	ostate.mShiftStartBaseTime = deviceTime - ostate.mShiftCyclesPerBit / 2;

	ostate.mShiftRegister = 0;
	ostate.mBitsShifted = 0;
	ostate.mbLastOutputBit = false;

	// set wait for byte end event for when the stop bit should be sampled,
	// so we don't want endlessly
	mDriveScheduler.SetEvent(
		(ostate.mShiftStartBaseTime + ostate.mShiftCyclesPerBit * 10) - deviceTime,
		this,
		kEventId_SerialShiftOutComplete + index,
		mpDeviceEventShiftOutComplete[index]
	);
}

void ATDevice850Full::OnSerialShiftOutComplete(int index) {
	// all we need to do here is force an update
	UpdateSerialPortOutputState(index, mRIOT2.ReadOutputA());
}

void ATDevice850Full::UpdateSIODataIn() {
	const bool newOutputBit = (mRIOT2.ReadOutputA() & 0x20) != 0;

	if (mbLastSIODataIn != newOutputBit) {
		mbLastSIODataIn = newOutputBit;

		const uint32 t = DriveTimeToMasterTime();

		mSerialXmitQueue.AddTransmitBit(t + mSerialXmitQueue.kTransmitLatency, newOutputBit);
	}
}

void ATDevice850Full::AddRIOTSignalChange(
	uint32 deviceTime,
	const RIOTSignalChange& change,
	bool truncateOverlappingChanges
) {
	// check if we should compact the queue
	size_t n = mRIOTSignalChangeQueue.size();
	if (mRIOTSignalChangeQueueNext >= n) {
		mRIOTSignalChangeQueueNext = 0;
		mRIOTSignalChangeQueue.clear();
	} else if (mRIOTSignalChangeQueueNext >= 64 && mRIOTSignalChangeQueueNext * 8 >= mRIOTSignalChangeQueue.size()) {
		mRIOTSignalChangeQueue.erase(mRIOTSignalChangeQueue.begin(), mRIOTSignalChangeQueue.begin() + mRIOTSignalChangeQueueNext);
		mRIOTSignalChangeQueueNext = 0;
	}

	// find insertion point
	auto it = std::lower_bound(
		mRIOTSignalChangeQueue.begin() + mRIOTSignalChangeQueueNext,
		mRIOTSignalChangeQueue.end(),
		deviceTime,
		[](const RIOTSignalChangeEvent& change, uint32 t) {
			return ATWrapTime{change.mDeviceTimestamp} < t;
		}
	);

	// if we're truncating overlapping changes, scan all events from this point
	// and clear the overlapping mask bits
	if (truncateOverlappingChanges) {
		for(auto it2 = it, it2End = mRIOTSignalChangeQueue.end(); it2 != it2End; ++it2) {
			if (it2->mChange.mRIOTMaskBits & change.mRIOTMaskBits)
				it2->mChange.mRIOTMaskBits &= ~change.mRIOTMaskBits;
		}
	}

	// check if there is already a change pending on the same device cycle
	if (it != mRIOTSignalChangeQueue.end() && it->mDeviceTimestamp == deviceTime) {
		// merge the change
		it->mChange |= change;
		return;
	}
	
	// insert new change entry
	RIOTSignalChangeEvent newEvent;
	newEvent.mDeviceTimestamp = deviceTime;
	newEvent.mChange = change;
	
	const bool insertAtFront = (it == mRIOTSignalChangeQueue.begin() + mRIOTSignalChangeQueueNext);
	mRIOTSignalChangeQueue.insert(it, newEvent);

	// check if we're already at the specified time (possible due to roundoff or
	// order of operations)
	const uint32 deviceTimeNow = mDriveScheduler.GetTick();

	if (deviceTime == deviceTimeNow) {
		ExecuteRIOTQueuedSignalChanges();
		return;
	}

	VDASSERT(deviceTime > deviceTimeNow);

	// queue scheduler event if we inserted at front

	if (insertAtFront) {
		mDriveScheduler.SetEvent(deviceTime - deviceTimeNow, this, kEventId_RIOTSignalChange, mpDeviceEventRIOTSignalChange);
	} else {
		VDASSERT(mpDeviceEventRIOTSignalChange);
	}
}

void ATDevice850Full::ExecuteRIOTQueuedSignalChanges() {
	const uint32 deviceTimeNow = mDriveScheduler.GetTick();

	const size_t n = mRIOTSignalChangeQueue.size();
	for(;;) {
		if (mRIOTSignalChangeQueueNext >= n) {
			mDriveScheduler.UnsetEvent(mpDeviceEventRIOTSignalChange);
			return;
		}

		const RIOTSignalChangeEvent& nextChange = mRIOTSignalChangeQueue[mRIOTSignalChangeQueueNext];

		if (ATWrapTime{nextChange.mDeviceTimestamp} > deviceTimeNow) {
			mDriveScheduler.SetEvent(nextChange.mDeviceTimestamp - deviceTimeNow, this, kEventId_RIOTSignalChange, mpDeviceEventRIOTSignalChange);
			break;
		}

		VDASSERT(nextChange.mDeviceTimestamp == deviceTimeNow);

		mRIOT1.SetInputA((uint8)(nextChange.mChange.mRIOTValueBits >>  0), (uint8)(nextChange.mChange.mRIOTMaskBits >>  0));
		mRIOT1.SetInputB((uint8)(nextChange.mChange.mRIOTValueBits >>  8), (uint8)(nextChange.mChange.mRIOTMaskBits >>  8));
		mRIOT2.SetInputA((uint8)(nextChange.mChange.mRIOTValueBits >> 16), (uint8)(nextChange.mChange.mRIOTMaskBits >> 16));
		mRIOT2.SetInputB((uint8)(nextChange.mChange.mRIOTValueBits >> 24), (uint8)(nextChange.mChange.mRIOTMaskBits >> 24));

		++mRIOTSignalChangeQueueNext;
	}
}

void ATDevice850Full::InvalidateRIOTQueuedSignalChanges() {
	mRIOTSignalChangeQueue.clear();
	mRIOTSignalChangeQueueNext = 0;

	mDriveScheduler.UnsetEvent(mpDeviceEventRIOTSignalChange);
}
