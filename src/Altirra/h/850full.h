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

#ifndef f_AT_850FULL_H
#define f_AT_850FULL_H

#include <vd2/system/function.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <at/atcore/devicediskdrive.h>
#include <at/atcore/deviceimpl.h>
#include <at/atcore/deviceparentimpl.h>
#include <at/atcore/deviceserial.h>
#include <at/atcore/devicesioimpl.h>
#include <at/atcore/scheduler.h>
#include <at/atcpu/co6502.h>
#include <at/atcpu/breakpoints.h>
#include <at/atcpu/history.h>
#include <at/atcpu/memorymap.h>
#include <at/atdebugger/breakpointsimpl.h>
#include <at/atdebugger/target.h>
#include <at/atemulation/riot.h>
#include "diskdrivefullbase.h"
#include "diskinterface.h"

class ATIRQController;

class ATDevice850Full final : public ATDevice
	, public IATDeviceFirmware
	, public IATDeviceParent
	, public ATDeviceSIO
	, public ATDiskDriveDebugTargetControl
	, public IATDeviceRawSIO
	, public IATDeviceDiagnostics
{
public:
	ATDevice850Full();
	~ATDevice850Full();

	void *AsInterface(uint32 iid) override;

	void GetDeviceInfo(ATDeviceInfo& info) override;
	void GetSettings(ATPropertySet & settings) override;
	bool SetSettings(const ATPropertySet & settings) override;
	void Init() override;
	void Shutdown() override;
	void WarmReset() override;
	void ComputerColdReset() override;
	void PeripheralColdReset() override;

public:		// IATDeviceParent
	IATDeviceBus *GetDeviceBusById(sint32 id) override;
	sint32 GetDeviceBusIdByIndex(uint32 index) override;

public:		// IATDeviceFirmware
	void InitFirmware(ATFirmwareManager *fwman) override;
	bool ReloadFirmware() override;
	const wchar_t *GetWritableFirmwareDesc(uint32 idx) const override;
	bool IsWritableFirmwareDirty(uint32 idx) const override;
	void SaveWritableFirmware(uint32 idx, IVDStream& stream) override;
	ATDeviceFirmwareStatus GetFirmwareStatus() const override;

public:		// ATDeviceSIO
	void InitSIO(IATDeviceSIOManager *mgr) override;

public:	// IATSchedulerCallback
	void OnScheduledEvent(uint32 id) override;

public:	// IATDeviceRawSIO
	void OnCommandStateChanged(bool asserted) override;
	void OnMotorStateChanged(bool asserted) override;
	void OnBeginReceiveByte(uint8 c, bool command, uint32 cyclesPerBit) override;
	void OnReceiveByte(uint8 c, bool command, uint32 cyclesPerBit) override;
	void OnTruncateByte() override;
	void OnSendReady() override;

public:	// IATDeviceDiagnostics
	void DumpStatus(ATConsoleOutput& output) override;

private:
	struct RIOTSignalChange {
		uint32 mRIOTValueBits = 0;
		uint32 mRIOTMaskBits = 0;

		void Set1A(uint8 value, uint8 mask) {
			uint32 value32 = (uint32)value << 0;
			uint32 mask32 = (uint32)mask << 0;

			mRIOTMaskBits |= mask32;
			mRIOTValueBits ^= (mRIOTValueBits ^ value32) & mask32;
		}

		void Set1B(uint8 value, uint8 mask) {
			uint32 value32 = (uint32)value << 8;
			uint32 mask32 = (uint32)mask << 8;

			mRIOTMaskBits |= mask32;
			mRIOTValueBits ^= (mRIOTValueBits ^ value32) & mask32;
		}

		void Set2A(uint8 value, uint8 mask) {
			uint32 value32 = (uint32)value << 16;
			uint32 mask32 = (uint32)mask << 16;

			mRIOTMaskBits |= mask32;
			mRIOTValueBits ^= (mRIOTValueBits ^ value32) & mask32;
		}

		void Set2B(uint8 value, uint8 mask) {
			uint32 value32 = (uint32)value << 24;
			uint32 mask32 = (uint32)mask << 24;

			mRIOTMaskBits |= mask32;
			mRIOTValueBits ^= (mRIOTValueBits ^ value32) & mask32;
		}

		void operator|=(const RIOTSignalChange& other) {
			mRIOTMaskBits |= other.mRIOTMaskBits;
			mRIOTValueBits ^= (mRIOTValueBits ^ other.mRIOTValueBits) & other.mRIOTMaskBits;
		}
	};

	void Sync() override;

	uint8 OnRIOT1DebugRead(uint32 address) const;
	uint8 OnRIOT1Read(uint32 address);
	void OnRIOT1Write(uint32 address, uint8 value);

	uint8 OnRIOT2DebugRead(uint32 address) const;
	uint8 OnRIOT2Read(uint32 address);
	void OnRIOT2Write(uint32 address, uint8 value);

	void OnSerialPortAttach(int index);
	void OnSerialPortDetach(int index);
	void OnSerialPortReadReady(int index);
	void UpdateSerialPortStatus(int index, const ATDeviceSerialStatus& status);
	void UpdateSerialPortControlState(int index);
	ATDeviceSerialTerminalState ComputeSerialPortControlState(int index) const;
	void UpdateSerialPortOutputState(int index, uint8 r2porta);
	void StartShiftOutNewByte(int index, uint32 deviceTime);
	void OnSerialShiftOutComplete(int index);
	
	void UpdateSIODataIn();

	void AddRIOTSignalChange(uint32 deviceTime, const RIOTSignalChange& change, bool truncateOverlappingChanges);

	void ExecuteRIOTQueuedSignalChanges();
	void InvalidateRIOTQueuedSignalChanges();

	enum : uint32 {
		kEventId_RIOTSignalChange = kEventId_FirstCustom,
		kEventId_SerialReceiveByte,		// +4 entries
		kEventId_SerialShiftOutComplete = kEventId_SerialReceiveByte + 4,		// +4 entries
	};

	ATEvent *mpDeviceEventRIOTSignalChange = nullptr;
	ATEvent *mpDeviceEventShiftOutComplete[4] {};
	IATDeviceSIOManager *mpSIOMgr = nullptr;

	ATFirmwareManager *mpFwMgr = nullptr;
	bool mbFirmwareUsable = false;

	ATEvent *mpEventReceiveByte[4] {};

	bool mbLastSIODataIn = false;
	uint32 mLastPOKEYCyclesPerBit = 0;

	bool mbDirectReceiveOutput = true;
	bool mbDirectTransmitOutput = true;

	ATDeviceBusSingleChild mParallelPort;
	ATDeviceBusSingleChild mSerialPort[4];

	struct SerialOutputState {
		uint32 mShiftStartBaseTime = 0;
		uint32 mShiftCyclesPerBit = 0;
		uint32 mShiftRegister = 0;
		uint32 mBitsShifted = 0;
		bool mbLastOutputBit = false;
	} mSerialOutputStates[4];

	// Baud rate setting for each serial port, as 850 setting + 1, or 0 = auto.
	uint8 mSerialPortBaudRateSettings[4] {};

	// Currently tracked baud rate indices. Will be the same as the baud rate
	// settings-1, unless auto is enabled.
	uint8 mSerialPortBaudRateIndices[4] {};

	ATDiskDriveSerialBitTransmitQueue mSerialXmitQueue;

	ATRIOT6532Emulator mRIOT1;
	ATRIOT6532Emulator mRIOT2;
	
	ATCoProcReadMemNode mReadNodeRIOT1 {};
	ATCoProcReadMemNode mReadNodeRIOT2 {};
	ATCoProcWriteMemNode mWriteNodeRIOT1 {};
	ATCoProcWriteMemNode mWriteNodeRIOT2 {};

	ATCoProc6502 mCoProc { false, false };
	
	struct TargetProxy final : public ATDiskDriveDebugTargetProxyBaseT<ATCoProc6502> {
		uint32 GetTime() const override {
			return ATSCHEDULER_GETTIME(mpDriveScheduler);
		}

		ATScheduler *mpDriveScheduler;
	} mTargetProxy;

	struct RIOTSignalChangeEvent {
		uint32 mDeviceTimestamp = 0;
		RIOTSignalChange mChange;
	};

	vdfastvector<RIOTSignalChangeEvent> mRIOTSignalChangeQueue;
	size_t mRIOTSignalChangeQueueNext = 0;

	alignas(4) uint8 mROM[4096] = {};
	alignas(4) uint8 mRAM[256] {};

	ATDebugTargetBreakpointsImpl mBreakpointsImpl;

	uint8 mDummyRead[256] {};
	uint8 mDummyWrite[256] {};

	static const uint32 kCyclesPerBitSettings[16];
	static const uint32 kBaudRateSettings[16];
	static const uint8 kBaudRateSortOrder[14];
};

#endif
