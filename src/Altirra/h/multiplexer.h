//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2025 Avery Lee
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

#ifndef f_AT_MULTIPLEXER_H
#define f_AT_MULTIPLEXER_H

#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <at/atcore/deviceimpl.h>
#include <at/atemulation/via.h>

class ATMemoryManager;
class ATMemoryLayer;
class IATDeviceCartridgePort;
class IATListenSocket;
class IATStreamSocket;
class IATTimerService;
class ATTraceChannelFormatted;
class ATTraceChannelSimple;

class ATDeviceMultiplexer final : public ATDeviceT<IATDeviceCartridge>, public IATSchedulerCallback {
public:
	ATDeviceMultiplexer();
	~ATDeviceMultiplexer();

	void *AsInterface(uint32 id) override;

	void GetDeviceInfo(ATDeviceInfo& devInfo) override;
	void GetSettings(ATPropertySet& pset) override;
	bool SetSettings(const ATPropertySet& pset) override;
	void GetSettingsBlurb(VDStringW& buf) override;
	void Init() override;
	void Shutdown() override;
	void ColdReset() override;
	bool GetErrorStatus(uint32 idx, VDStringW& error) override;
	void SetTraceContext(ATTraceContext *context) override;

public:	// IATDeviceCartridge
	void InitCartridge(IATDeviceCartridgePort *cartPort) override;
	bool IsLeftCartActive() const override;
	void SetCartEnables(bool leftEnable, bool rightEnable, bool cctlEnable) override;
	void UpdateCartSense(bool leftActive) override;

public:	// IATSchedulerCallback
	void OnScheduledEvent(uint32 id) override;

private:
	enum class ClientState : uint8 {
		WaitingForCmd,
		WaitingForValue
	};

	enum class HostCmd : uint8 {
		// Reply to an SIO command. Payload:
		//	Request ID
		//	Status byte 1
		//	Status byte 2
		SIOReply = 0xA0,

		// Reply to an SIO command with returned data frame. Payload:
		//	Request ID
		//	Status byte 1
		//	Status byte 2
		//	Data frame size (0 = 256 bytes)
		//	Data frame (up to 256 bytes)
		SIOReplyWithData = 0xA1,

		// Reply to an SIO command that essentially means no reply. Payload:
		//	Request ID
		SIOReplyTimeout = 0xA2,

		SIOResume = 0xA3,
		SIOPause = 0xA4
	};

	enum class ClientCmd : uint8 {
		// SIO command request. Payload:
		//	Request ID
		//	Device ID (1 byte)
		//	DCB (13 bytes)
		//	Sector data (up to 256 bytes, based on DBYTLO and DSTATS bit 7)
		SIOCommand = 0xA0,

		// Cancel previous SIO command request.
		SIOCommandCancel = 0xA1,
	};

	class HostClientConnection final : public vdrefcount {
	public:
		vdrefptr<IATStreamSocket> mpClientSocket;

		uint32 mReceiveIndex = 0;
		uint32 mReceiveLimit = 1;
		uint32 mSendIndex = 0;
		uint32 mSendLimit = 0;
		bool mbLastHostReadyState = false;

		uint8 mCommandReadyDeviceId = 0;
		uint8 mCurrentRequestId = 0;

		uint8 mReceiveBuffer[1 + 15 + 256] {};
		uint8 mPendingCommand[1 + 15 + 256] {};
		uint8 mSendBuffer[4 + 256] {};
	};

	class ClientConnection final : public vdrefcount {
	public:
		vdrefptr<IATStreamSocket> mpSocket;

		uint32 mReceiveIndex = 0;
		uint32 mReceiveLimit = 1;
		uint32 mSendIndex = 0;
		uint32 mSendLimit = 0;
		uint8 mRequestIdCounter = 0;

		bool mbReplyReady = false;
		bool mbHostReady = false;
		bool mbBusy = false;

		uint8 mReceiveBuffer[4 + 256] {};
		uint8 mSendBuffer[1 + 14 + 256] {};
	};

	sint32 OnCCTLDebugRead(uint32 addr) const;
	sint32 OnCCTLRead(uint32 addr);
	bool OnCCTLWrite(uint32 addr, uint8 value);
	void OnVIAOutputUpdated(uint32 outputState);

	void UpdateSwitchSignals();
	void RecreateSockets();
	void CloseClientSocket();
	void CloseHostSocket();
	void CloseAllClientConnections();

	void OnHostSocketEvent(const ATSocketStatus& status);
	void OnClientSocketEvent(const ATSocketStatus& status);
	bool OnClientSocketEvent2(const ATSocketStatus& status);
	void OnClientReconnect();
	bool OnHostClientConnectionSocketEvent(HostClientConnection& conn, const ATSocketStatus& status);
	bool OnHostClientConnectionSocketEvent2(HostClientConnection& conn, const ATSocketStatus& status);

	void UpdateHostPortStates(uint32 outputState);
	void UpdateClientPortStates(uint32 outputState);

	void SetInputByte(uint8 v);
	void ClearInputByte();

	void SetAck(bool asserted);

	void SetHostBusy(bool busy);
	void FlushHostClientLazyStates(HostClientConnection& conn, bool doPoll);

	void ScheduleState(uint32 state, uint32 delay = 1);

	void RunCommStateMachine();
	void RunHostCommStateMachine();
	void RunClientCommStateMachine();

	static uint8 ComputeReplyChecksum(uint8 v1, uint8 v2);
	static uint8 ComputeDataFrameChecksum(vdspan<const uint8> data);

	void UpdateDeviceStatus();

	enum HostCommState : uint8 {
		kHostCommState_Reset,
		kHostCommState_WaitForSelection,
		kHostCommState_WaitSelectAck,
		kHostCommState_SendDCB,
		kHostCommState_SendDCBByte,
		kHostCommState_SendDCBByteWait,
		kHostCommState_SendDCBByteAckWait,
		kHostCommState_ReceiveDCBReply,
		kHostCommState_ReceiveDCBReplyByte,
		kHostCommState_ReceiveDCBReplyByteWait,
		kHostCommState_ReceiveDCBReplyByteAckWait,

		kHostCommState_SendDataFrameByte,
		kHostCommState_SendDataFrameByteWait,
		kHostCommState_SendDataFrameByteAckWait,

		kHostCommState_ReceiveDataFrameByte,
		kHostCommState_ReceiveDataFrameByteWait,
		kHostCommState_ReceiveDataFrameByteAckWait,

		kHostCommState_ReceiveCommandReply,
		kHostCommState_ReceiveCommandReplyByte,
		kHostCommState_ReceiveCommandReplyByteWait,
		kHostCommState_ReceiveCommandReplyByteAckWait,

		kHostCommState_SendReplyToClient,
	};

	enum ClientCommState : uint8 {
		kClientCommState_Reset,
		kClientCommState_Busy,
		kClientCommState_ClientPoll,
		kClientCommState_ClientPoll2,
		kClientCommState_ReceiveDCB,
		kClientCommState_ReceiveDCBByte,
		kClientCommState_ReceiveDCBByteWait,
		kClientCommState_ReceiveDCBByteAckWait,
		
		kClientCommState_ProcessDCB,

		kClientCommState_SendDCBReplyByte,
		kClientCommState_SendDCBReplyByteWait,
		kClientCommState_SendDCBReplyByteAckWait,

		kClientCommState_ReceiveDataFrame,
		kClientCommState_ReceiveDataFrameByte,
		kClientCommState_ReceiveDataFrameByteWait,
		kClientCommState_ReceiveDataFrameByteAckWait,
		kClientCommState_Execute,
		kClientCommState_ExecuteWait,
		kClientCommState_SendResultFromServer,
		kClientCommState_SendResultByte,
		kClientCommState_SendResultByteWait,
		kClientCommState_SendResultByteAckWait,
		kClientCommState_CommandEnd,
	};

	ATScheduler *mpScheduler = nullptr;
	ATMemoryManager *mpMemMan = nullptr;
	ATMemoryLayer *mpMemLayer = nullptr;

	IATTimerService *mpTimerService = nullptr;
	uint64 mClientReconnectTimerToken = 0;

	IATDeviceCartridgePort *mpCartPort = nullptr;
	uint32 mCartId = 0;

	VDStringW mHostAddress;
	int mPort = 6522;

	uint8 mDeviceId = 0;
	bool mbHost = true;
	bool mbHostBusy = true;
	bool mbHostAllowExternal = false;

	uint32 mCommState = 0;
	uint8 mCommDeviceId = 0;
	uint32 mCommLastOutputState = 0;
	uint32 mCommTransferIndex = 0;
	uint32 mCommTransferLimit = 0;
	ATEvent *mpCommEvent = nullptr;

	ATTraceChannelFormatted *mpTraceChannelInput = nullptr;
	ATTraceChannelFormatted *mpTraceChannelOutput = nullptr;
	ATTraceChannelSimple *mpTraceChannelReq = nullptr;
	ATTraceChannelSimple *mpTraceChannelAck = nullptr;
	sint32 mCurrentTracingOutputByte = -1;
	sint32 mCurrentTracingInputByte = -1;
	bool mbCurrentTrackingReq = false;
	bool mbCurrentTrackingAck = false;

	vdrefptr<IATListenSocket> mpHostSocket;

	vdvector<vdrefptr<HostClientConnection>> mHostClientConnections;
	vdrefptr<HostClientConnection> mpActiveHostClientConnection;

	vdrefptr<IATStreamSocket> mpConnectingClientSocket;
	vdrefptr<ClientConnection> mpClientConnection;

	enum class DeviceStatus : uint8 {
		Default,
		UnableToConnect,
		Connecting
	};

	DeviceStatus mDeviceStatus = DeviceStatus::Default;

	ATVIA6522Emulator mVIA;

	uint8 mCommDCBBuffer[13] {};
	uint8 mCommTransferBuffer[3 + 259 + 3] {};
};

#endif
