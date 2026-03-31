//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2012 Avery Lee
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

#ifndef f_AT_SIOMANAGER_H
#define f_AT_SIOMANAGER_H

#include <at/atcore/devicesio.h>
#include <at/atcore/notifylist.h>
#include <at/atcore/scheduler.h>
#include <at/ataudio/pokey.h>

class ATCPUEmulator;
class ATCPUEmulatorMemory;
class ATSimulator;
class IATUIRenderer;
struct ATCPUHookNode;
class ATPokeyEmulator;
class ATPIAEmulator;

struct ATTraceContext;
class ATTraceChannelSimple;
class ATTraceChannelFormatted;

struct ATSIORequest {
	uint8	mDevice;
	uint8	mCommand;
	uint8	mMode;
	uint8	mTimeout;
	uint16	mAddress;
	uint16	mLength;
	uint16	mSector;
	uint8	mAUX[2];
};

class ATSIOManager final : public IATPokeySIODevice, public IATDeviceSIOManager {
	ATSIOManager(const ATSIOManager&) = delete;
	ATSIOManager& operator=(const ATSIOManager&) = delete;
public:
	static constexpr uint32 kMaxTransferSize = 65536;

	ATSIOManager();
	~ATSIOManager();

	bool IsSIOPatchEnabled() const { return mbSIOPatchEnabled; }
	void SetSIOPatchEnabled(bool enable);

	bool GetDiskSIOAccelEnabled() const { return mbDiskSIOAccelEnabled; }
	void SetDiskSIOAccelEnabled(bool enabled);

	bool GetOtherSIOAccelEnabled() const { return mbOtherSIOAccelEnabled; }
	void SetOtherSIOAccelEnabled(bool enabled);

	bool GetDiskBurstTransfersEnabled() const { return mbDiskBurstTransfersEnabled; }
	void SetDiskBurstTransfersEnabled(bool enabled) { mbDiskBurstTransfersEnabled = enabled; }

	bool GetBurstTransfersEnabled() const { return mbBurstTransfersEnabled; }
	void SetBurstTransfersEnabled(bool enabled) { mbBurstTransfersEnabled = enabled; }

	void SetDebuggerDeviceId(uint8 id);

	void Init(ATCPUEmulator *cpu, ATSimulator *sim);
	void Shutdown();

	void ColdReset();
	void WarmReset();

	void SetFrameTime(uint32 cyclesPerFrame);
	void SetReadyState(bool ready);

	void PreLoadState();
	void LoadState(IATObjectState *obj);
	void PostLoadState();

	vdrefptr<IATObjectState> SaveState() const;

	void ReinitHooks();
	void UninitHooks();

	void SetTraceContext(ATTraceContext *context);

	void TryAccelPBIRequest(bool enabled);

	bool TryAccelRequest(const ATSIORequest& req, bool fromPBI);

public:
	virtual void PokeyAttachDevice(ATPokeyEmulator *pokey) override;
	virtual void PokeyBeginWriteSIO(uint8 c, bool command, uint32 cyclesPerBit) override;
	virtual bool PokeyWriteSIO(uint8 c, bool command, uint32 cyclesPerBit, uint64 startTime, bool framingError, bool truncated) override;
	virtual void PokeyBeginCommand() override;
	virtual void PokeyEndCommand() override;
	virtual void PokeySerInReady() override;
	virtual void PokeySetBreak(bool enabled) override;

public:		// IATDeviceSIOManager
	virtual vdrefptr<IATDeviceSIOInterface> AddDevice(IATDeviceSIO *dev) override;

	void BeginCommand();

	virtual sint32 GetHighSpeedIndex() const override { return 10; }
	virtual uint32 GetCyclesPerBitRecv() const override;
	virtual uint32 GetRecvResetCounter() const override;
	virtual uint32 GetCyclesPerBitSend() const override;
	virtual uint32 GetCyclesPerBitBiClock() const override;

	virtual void AddRawDevice(IATDeviceRawSIO *dev) override;
	virtual void RemoveRawDevice(IATDeviceRawSIO *dev) override;
	virtual void SendRawByte(uint8 byte, uint32 cyclesPerBit, bool synchronous, bool forceFramingError, bool simulateInput) override;
	virtual void SetRawInput(bool input) override;

	bool IsSIOCommandAsserted() const override;
	bool IsSIOMotorAsserted() const override;
	bool IsSIOReadyAsserted() const override;
	bool IsSIOForceBreakAsserted() const override;

	virtual void SetSIOInterrupt(IATDeviceRawSIO *dev, bool state) override;
	virtual void SetSIOProceed(IATDeviceRawSIO *dev, bool state) override;
	virtual void SetBiClockNotifyEnabled(IATDeviceRawSIO *dev, bool enabled) override;

	virtual void SetExternalClock(IATDeviceRawSIO *dev, uint32 initialOffset, uint32 period) override;

public:
	enum StepType : uint8 {
		kStepType_None,
		kStepType_Delay,
		kStepType_Send,
		kStepType_SendAutoProtocol,
		kStepType_Receive,
		kStepType_ReceiveAutoProtocol,
		kStepType_SetTransferRate,
		kStepType_SetSynchronousTransmit,
		kStepType_Fence,
		kStepType_EndCommand,
		kStepType_AccelSendACK,
		kStepType_AccelSendNAK,
		kStepType_AccelSendComplete,
		kStepType_AccelSendError,
	};
	
	class SIOInterface;

	void RemoveInterface(SIOInterface& iface);
	void BeginReceive(SIOInterface& iface);
	void EndReceive(SIOInterface& iface);
	void BeginSend(SIOInterface& iface);
	void EndSend(SIOInterface& iface);

private:
	class RawDeviceListLock;
	friend class RawDeviceListLock;
	friend class ATSaveStateSioCommandStep;

	uint8 OnHookSIOV(uint16);

	void CancelAllCommands();
	void SetPendingDeviceId(uint8 deviceId);
	static bool IsDiskDevice(uint8 deviceId);
	void OnMotorStateChanged(bool asserted);
	void OnSerialOutputClockChanged();
	void TraceReceive(uint8 c, uint32 cyclesPerBit, bool postReceive);
	void UpdatePollState(uint8 cmd, uint8 aux1, uint8 aux2);
	void ProcessCommandFrame();

	ATCPUEmulator *mpCPU = nullptr;
	ATCPUEmulatorMemory *mpMemory = nullptr;
	ATSimulator *mpSim = nullptr;
	IATUIRenderer *mpUIRenderer = nullptr;
	ATScheduler *mpScheduler = nullptr;
	ATPokeyEmulator *mpPokey = nullptr;
	ATPIAEmulator *mpPIA = nullptr;
	int mPIAOutput = 0;

	ATCPUHookNode *mpSIOVHook = nullptr;

	bool	mbCommandState = false;
	bool	mbMotorState = false;
	bool	mbReadyState = false;
	uint64	mCommandFrameEndTime = 0;
	uint64	mCommandDeassertTime = 0;
	uint32	mCommandCyclesPerBit = 0;
	uint8	mCommandBufferIndex = 0;
	uint8	mCommandBuffer[5] {};

	bool	mbSIOPatchEnabled = false;
	bool	mbOtherSIOAccelEnabled = false;
	bool	mbDiskSIOAccelEnabled = false;
	bool	mbBurstTransfersEnabled = false;
	bool	mbDiskBurstTransfersEnabled = false;
	uint8	mDebuggerDeviceId = 0;
	uint8	mPollCount = 0;
	uint32	mAccelBufferAddress = 0;
	const ATDeviceSIORequest *mpAccelRequest = nullptr;
	uint8	*mpAccelStatus = nullptr;
	SIOInterface *mpPendingInterface = nullptr;
	uint8	mPendingDeviceId = 0;
	bool	mbPendingDeviceDisk = false;

	uint32	mAccessedDisks = 0;

	uint32	mCyclesPerFrame = 1;

	bool	mbLoadingState = false;

	ATNotifyList<SIOInterface *> mSIOInterfaces;

	ATNotifyList<SIOInterface *> mActiveCommandInterfaces;
	ATNotifyList<SIOInterface *> mSendingInterfaces;
	ATNotifyList<SIOInterface *> mReceivingInterfaces;

	vdfastvector<IATDeviceRawSIO *> mSIORawDevices;
	vdfastvector<IATDeviceRawSIO *> mSIORawDevicesNew;
	sint32 mSIORawDevicesBusy = 0;

	vdfastvector<IATDeviceRawSIO *> mSIOInterruptActive;
	vdfastvector<IATDeviceRawSIO *> mSIOProceedActive;
	vdfastvector<IATDeviceRawSIO *> mNotifyBiClockDevices;

	struct ExternalClock {
		IATDeviceRawSIO *mpDevice;
		uint32 mTimeBase;
		uint32 mPeriod;
	};

	vdfastvector<ExternalClock> mExternalClocks;

	struct Step {
		StepType mType;
		union {
			uint32 mTransferLength;
			uint32 mFenceId;
			uint32 mDelayTicks;
			uint32 mTransferCyclesPerBit;
			bool mbEnable;
		};

		union {
			uint32 mTransferCyclesPerByte;
			uint32 mTransferId;
		};
	};

	ATTraceContext *mpTraceContext = nullptr;
	ATTraceChannelSimple *mpTraceChannelBusCommand = nullptr;
	ATTraceChannelFormatted *mpTraceChannelBusSend = nullptr;
	ATTraceChannelFormatted *mpTraceChannelBusReceive = nullptr;
	uint64 mTraceCommandStartTime = 0;

	vdfastvector<uint8> mCachedTransferBuffer;
	vdfastdeque<Step> mCachedStepQueue;
};

#endif	// f_AT_SIOMANAGER_H
