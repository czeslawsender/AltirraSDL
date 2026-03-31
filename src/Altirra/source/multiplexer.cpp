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

#include "stdafx.h"
#include <ranges>
#include <numeric>
#include <vd2/system/binary.h>
#include <vd2/system/thread.h>
#include <at/atcore/asyncdispatcher.h>
#include <at/atcore/devicecart.h>
#include <at/atcore/logging.h>
#include <at/atcore/propertyset.h>
#include <at/atcore/timerservice.h>
#include <at/atnetworksockets/nativesockets.h>
#include "multiplexer.h"
#include "memorymanager.h"
#include "trace.h"

#define AT_LOG_MUX_DETAILED_ACK(...) (void(0))
//#define AT_LOG_MUX_DETAILED_ACK(...) g_ATLCMux(__VA_ARGS__)

#define AT_LOG_MUX_DETAILED_SEL(...) (void(0))
//#define AT_LOG_MUX_DETAILED_SEL(...) g_ATLCMux(__VA_ARGS__)

#define AT_LOG_MUX_DETAILED_STATE(...) (void(0))
//#define AT_LOG_MUX_DETAILED_STATE(...) g_ATLCMux(__VA_ARGS__)

#define AT_LOG_MUX_DETAILED_XFER(...) (void(0))
//#define AT_LOG_MUX_DETAILED_XFER(...) g_ATLCMux(__VA_ARGS__)

ATLogChannel g_ATLCMux(false, false, "MUX", "CSS Multiplexer status");

void ATCreateDeviceMultiplexer(const ATPropertySet& pset, IATDevice **dev) {
	vdrefptr<ATDeviceMultiplexer> p(new ATDeviceMultiplexer);
	p->SetSettings(pset);

	*dev = p.release();
}

extern const ATDeviceDefinition g_ATDeviceDefMultiplexer = { "multiplexer", "multiplexer", L"Multiplexer", ATCreateDeviceMultiplexer };

ATDeviceMultiplexer::ATDeviceMultiplexer() {
}

ATDeviceMultiplexer::~ATDeviceMultiplexer() {
}

void *ATDeviceMultiplexer::AsInterface(uint32 id) {
	if (id == ATVIA6522Emulator::kTypeID)
		return &mVIA;

	return ATDeviceT::AsInterface(id);
}

void ATDeviceMultiplexer::GetDeviceInfo(ATDeviceInfo& devInfo) {
	devInfo.mpDef = &g_ATDeviceDefMultiplexer;
}

void ATDeviceMultiplexer::GetSettingsBlurb(VDStringW& buf) {
	if (mbHost)
		buf.append_sprintf(L"host on %ls:%u", mbHostAllowExternal ? L"*" : L"localhost", mPort);
	else
		buf.append_sprintf(L"client ID %u connecting to %ls:%u", mDeviceId, mHostAddress.empty() ? L"localhost" : mHostAddress.c_str(), mPort);
}

void ATDeviceMultiplexer::GetSettings(ATPropertySet& pset) {
	pset.Clear();
	pset.SetInt32("device_id", mbHost ? -1 : mDeviceId);
	pset.SetInt32("port", mPort);

	if (!mHostAddress.empty())
		pset.SetString("host_address", mHostAddress.c_str());

	if (mbHostAllowExternal)
		pset.SetInt32("allow_external", true);
}

bool ATDeviceMultiplexer::SetSettings(const ATPropertySet& pset) {
	const int devId = pset.GetInt32("device_id", -1);
	const wchar_t *hostAddress = pset.GetString("host_address", L"");
	const int port = std::clamp<int>(pset.GetInt32("port", 6522), 1024, 49151);
	const bool host = devId < 0;
	const bool allowExternal = pset.GetBool("allow_external", false);

	if (mPort != port || mbHost != host || mHostAddress != hostAddress || mbHostAllowExternal != allowExternal) {
		mPort = port;
		mbHost = host;
		mHostAddress = hostAddress;
		mbHostAllowExternal = allowExternal;

		CloseClientSocket();
		CloseHostSocket();
		CloseAllClientConnections();
	}

	mDeviceId = mbHost ? 0 : devId & 7;

	UpdateSwitchSignals();
	RecreateSockets();

	return true;
}

void ATDeviceMultiplexer::Init() {
	mpScheduler = GetService<IATDeviceSchedulingService>()->GetMachineScheduler();
	mpTimerService = GetService<IATTimerService>();

	mVIA.SetPortOutputFn(
		[](void *data, uint32 outputState) {
			((ATDeviceMultiplexer *)data)->OnVIAOutputUpdated(outputState);
		},
		this
	);

	mVIA.Init(GetService<IATDeviceSchedulingService>()->GetMachineScheduler());

	mpMemMan = GetService<ATMemoryManager>();

	ATMemoryHandlerTable handlers;
	handlers.BindDebugReadHandler<&ATDeviceMultiplexer::OnCCTLDebugRead>();
	handlers.BindReadHandler<&ATDeviceMultiplexer::OnCCTLRead>();
	handlers.BindWriteHandler<&ATDeviceMultiplexer::OnCCTLWrite>();
	handlers.mpThis = this;
	handlers.mbPassAnticReads = true;
	handlers.mbPassReads = true;
	handlers.mbPassWrites = true;
	mpMemLayer = mpMemMan->CreateLayer(kATMemoryPri_Cartridge2, handlers, 0xD5, 0x01);
	mpMemMan->EnableLayer(mpMemLayer, true);

	SetCartEnables(false, false, mpCartPort->IsCCTLEnabled(mCartId));

	RecreateSockets();

	ColdReset();
}

void ATDeviceMultiplexer::Shutdown() {
	SetTraceContext(nullptr);

	if (mpTimerService) {
		mpTimerService->Cancel(&mClientReconnectTimerToken);
		mpTimerService = nullptr;
	}

	CloseClientSocket();
	CloseHostSocket();
	CloseAllClientConnections();

	if (mpCartPort) {
		mpCartPort->RemoveCartridge(mCartId, this);
		mpCartPort = nullptr;
	}

	if (mpMemMan) {
		mpMemMan->DeleteLayerPtr(&mpMemLayer);
		mpMemMan = nullptr;
	}

	mVIA.Shutdown();

	if (mpScheduler) {
		mpScheduler->UnsetEvent(mpCommEvent);
		mpScheduler = nullptr;
	}
}

void ATDeviceMultiplexer::ColdReset() {
	mVIA.Reset();

	ClearInputByte();
	mVIA.SetPortBInput(0xFF, 0xF8);		// bits 0-3 owned by device ID, don't overwrite them
	SetAck(false);
	mVIA.SetCA2Input(true);

	SetHostBusy(true);
	ScheduleState(0);
}

bool ATDeviceMultiplexer::GetErrorStatus(uint32 idx, VDStringW& error) {
	if (idx)
		return false;

	switch(mDeviceStatus) {
		case DeviceStatus::Default:
			break;

		case DeviceStatus::UnableToConnect:
			error = L"Not connected";
			return true;

		case DeviceStatus::Connecting:
			error.sprintf(L"Connecting to %ls:%u", mHostAddress.empty() ? L"localhost" : mHostAddress.c_str(), mPort);
			return true;
	}

	return false;
}

void ATDeviceMultiplexer::SetTraceContext(ATTraceContext *context) {
	if (context) {
		ATTraceGroup *group = context->mpCollection->AddGroup(L"Multiplexer");

		mpTraceChannelOutput = group->AddFormattedChannel(context->mBaseTime, context->mBaseTickScale, L"Data Out");
		mpTraceChannelInput = group->AddFormattedChannel(context->mBaseTime, context->mBaseTickScale, L"Data In");
		mpTraceChannelReq = group->AddSimpleChannel(context->mBaseTime, context->mBaseTickScale, L"REQ");
		mpTraceChannelAck = group->AddSimpleChannel(context->mBaseTime, context->mBaseTickScale, L"ACK");

		mCurrentTracingOutputByte = -1;
		mCurrentTracingInputByte = -1;
		mbCurrentTrackingReq = false;
		mbCurrentTrackingAck = false;
	} else {
		if (mpTraceChannelInput) {
			mpTraceChannelInput->TruncateLastEvent(mpScheduler->GetTick64());
			mpTraceChannelInput = nullptr;
		}

		if (mpTraceChannelOutput) {
			mpTraceChannelOutput->TruncateLastEvent(mpScheduler->GetTick64());
			mpTraceChannelOutput = nullptr;
		}

		if (mpTraceChannelReq) {
			mpTraceChannelReq->TruncateLastEvent(mpScheduler->GetTick64());
			mpTraceChannelReq = nullptr;
		}

		if (mpTraceChannelAck) {
			mpTraceChannelAck->TruncateLastEvent(mpScheduler->GetTick64());
			mpTraceChannelAck = nullptr;
		}
	}
}

void ATDeviceMultiplexer::InitCartridge(IATDeviceCartridgePort *cartPort) {
	mpCartPort = cartPort;
	mpCartPort->AddCartridge(this, kATCartridgePriority_PassThrough, mCartId);
}

bool ATDeviceMultiplexer::IsLeftCartActive() const {
	return false;
}

void ATDeviceMultiplexer::SetCartEnables(bool leftEnable, bool rightEnable, bool cctlEnable) {
	if (mpMemLayer)
		mpMemMan->SetLayerMaskRange(mpMemLayer, 0xD5, cctlEnable ? 0x01 : 0x00);
}

void ATDeviceMultiplexer::UpdateCartSense(bool leftActive) {
}

void ATDeviceMultiplexer::OnScheduledEvent(uint32 id) {
	mpCommEvent = nullptr;
	RunCommStateMachine();
}

sint32 ATDeviceMultiplexer::OnCCTLDebugRead(uint32 addr) const {
	if ((uint32)(addr - 0xD570) < 16)
		return mVIA.DebugReadByte((uint8)addr);

	return -1;
}

sint32 ATDeviceMultiplexer::OnCCTLRead(uint32 addr) {
	if ((uint32)(addr - 0xD570) < 16)
		return mVIA.ReadByte((uint8)addr);

	return -1;
}

bool ATDeviceMultiplexer::OnCCTLWrite(uint32 addr, uint8 value) {
	if ((uint32)(addr - 0xD570) < 16) {
		// The multiplexer blocks CCTL accesses to $D57x from reaching sub-carts.
		mVIA.WriteByte((uint8)addr, value);
		return true;
	}

	return false;
}

void ATDeviceMultiplexer::OnVIAOutputUpdated(uint32 outputState) {
	// PA0-PA7 (RW)		Bidirectional bus
	// PB0-PB2 (R)		Client device ID
	// PB3 (W)			Bus driver direction (0 = output)
	// PB4 (W)			Control line output enable
	// PB5 (W)			Active output
	// PB6 (R)			Active sense
	// PB7 (R)			Acknowledge sense (also connected to CA1)
	// CA2 (W)			Achnowledge output

	if (mpTraceChannelOutput) {
		if (!(outputState & 0x0800)) {
			const uint8 v = (uint8)outputState;

			if (mCurrentTracingOutputByte != v) {
				mCurrentTracingOutputByte = v;

				uint64 t = mpScheduler->GetTick64();

				mpTraceChannelInput->TruncateLastEvent(t);
				mpTraceChannelOutput->TruncateLastEvent(t);
				mpTraceChannelOutput->AddOpenTickEventF(t, kATTraceColor_IO_Write, L"%02X", v);
			}
		} else {
			if (mCurrentTracingOutputByte >= 0) {
				mCurrentTracingOutputByte = -1;

				uint64 t = mpScheduler->GetTick64();
				mpTraceChannelOutput->TruncateLastEvent(t);
			}
		}

		const bool req = (outputState & kATVIAOutputBit_CA2) == 0;
		if (mbCurrentTrackingReq != req) {
			mbCurrentTrackingReq = req;
			
			uint64 t = mpScheduler->GetTick64();

			if (req)
				mpTraceChannelReq->AddOpenTickEvent(t, L"", kATTraceColor_IO_Read);
			else
				mpTraceChannelReq->TruncateLastEvent(t);
		}
	}

	if (mbHost) {
		UpdateHostPortStates(outputState);
	} else {
		UpdateClientPortStates(outputState);
	}
}

void ATDeviceMultiplexer::UpdateSwitchSignals() {
	// Update PB0-PB2 from the device ID switches (J0-J2).
	mVIA.SetPortBInput(mDeviceId, 7);
}

void ATDeviceMultiplexer::RecreateSockets() {
	if (!mpMemMan)
		return;

	if (mbHost) {
		CloseClientSocket();

		if (!mpHostSocket) {
			if (mbHostAllowExternal)
				mpHostSocket = ATNetListen(ATSocketAddressType::IPv6, mPort, true);
			else
				mpHostSocket = ATNetListen(ATSocketAddress::CreateLocalhostIPv6(mPort), true);

			if (mpHostSocket) {
				mpHostSocket->SetOnEvent(GetService<IATAsyncDispatcher>(), std::bind_front(&ATDeviceMultiplexer::OnHostSocketEvent, this), true);

				UpdateDeviceStatus();
			}
		}
	} else {
		CloseHostSocket();

		if (!mpClientConnection && !mpConnectingClientSocket) {
			if (!mHostAddress.empty()) {
				VDStringW service;
				service.sprintf(L"%u", mPort);

				mpConnectingClientSocket = ATNetConnect(mHostAddress.c_str(), service.c_str(), true);
			} else
				mpConnectingClientSocket = ATNetConnect(ATSocketAddress::CreateLocalhostIPv6(mPort), true);

			if (mpConnectingClientSocket) {
				mpConnectingClientSocket->SetOnEvent(GetService<IATAsyncDispatcher>(), std::bind_front(&ATDeviceMultiplexer::OnClientSocketEvent, this), true);

				UpdateDeviceStatus();
			}
		}
	}
}

void ATDeviceMultiplexer::CloseClientSocket() {
	if (mpClientConnection) {
		auto p = std::move(mpClientConnection);
		mpClientConnection = nullptr;

		p->mpSocket->CloseSocket(true);
	}

	if (mpConnectingClientSocket) {
		mpConnectingClientSocket->CloseSocket(true);
		mpConnectingClientSocket = nullptr;
	}
}

void ATDeviceMultiplexer::CloseHostSocket() {
	if (mpHostSocket) {
		mpHostSocket->CloseSocket(true);
		mpHostSocket = nullptr;
	}
}

void ATDeviceMultiplexer::CloseAllClientConnections() {
	mpActiveHostClientConnection = nullptr;

	auto connections = std::move(mHostClientConnections);
	mHostClientConnections.clear();

	while(!connections.empty()) {
		auto conn = std::move(connections.back());
		connections.pop_back();

		if (conn->mpClientSocket) {
			conn->mpClientSocket->SetOnEvent(nullptr, nullptr, false);
			conn->mpClientSocket->CloseSocket(true);
			conn->mpClientSocket = nullptr;
		}
	}
}

void ATDeviceMultiplexer::OnHostSocketEvent(const ATSocketStatus& status) {
	if (status.mbCanAccept && mpHostSocket) {
		vdrefptr<IATStreamSocket> newSocket = mpHostSocket->Accept();

		if (newSocket) {
			auto connection = vdmakerefcounted<HostClientConnection>();
			mHostClientConnections.push_back(connection);

			connection->mpClientSocket = newSocket;

			newSocket->SetOnEvent(
				GetService<IATAsyncDispatcher>(),
				[this, connection = std::move(connection)](const ATSocketStatus& status) {
					OnHostClientConnectionSocketEvent(*connection, status);
				},
				true
			);
		}
	}
}

void ATDeviceMultiplexer::OnClientSocketEvent(const ATSocketStatus& status) {
	if (!OnClientSocketEvent2(status)) {
		if (mpConnectingClientSocket) {
			mpConnectingClientSocket->SetOnEvent(nullptr, nullptr, false);
			mpConnectingClientSocket = nullptr;
		}
		
		if (mpClientConnection) {
			mpClientConnection->mpSocket->SetOnEvent(nullptr, nullptr, false);
			mpClientConnection = nullptr;
		}

		if (mpTimerService && !mbHost) {
			mpTimerService->Request(&mClientReconnectTimerToken, 3.0f, [this] { OnClientReconnect(); });
		}

		UpdateDeviceStatus();
	}
}

bool ATDeviceMultiplexer::OnClientSocketEvent2(const ATSocketStatus& status) {
	if (status.mbClosed || status.mError != ATSocketError{})
		return false;

	if (status.mbRemoteClosed)
		return false;

	if (mpConnectingClientSocket) {
		if (status.mbConnecting)
			return true;

		mpClientConnection = new ClientConnection;
		mpClientConnection->mpSocket = std::move(mpConnectingClientSocket);
		mpConnectingClientSocket = nullptr;

		UpdateDeviceStatus();
	}

	if (mCommState == kClientCommState_Busy)
		ScheduleState(kClientCommState_ClientPoll);

	auto& conn = *mpClientConnection;

	if (status.mbCanRead && !conn.mbReplyReady) {
		for(;;) {
			const uint32 remaining = conn.mReceiveLimit - conn.mReceiveIndex;
			sint32 n = conn.mpSocket->Recv(conn.mReceiveBuffer + conn.mReceiveIndex, remaining);

			if (n <= 0) {
				if (n < 0)
					return false;

				break;
			}

			conn.mReceiveIndex += n;

			if (conn.mReceiveIndex >= conn.mReceiveLimit) {
				const uint8 cmd = conn.mReceiveBuffer[0];
				bool commandDone = false;

				switch(cmd) {
					case (uint8)HostCmd::SIOReply:
						if (conn.mReceiveLimit == 1)
							conn.mReceiveLimit = 4;
						else
							conn.mbReplyReady = true;
						break;

					case (uint8)HostCmd::SIOReplyWithData:
						if (conn.mReceiveLimit == 1)
							conn.mReceiveLimit = 5;
						else if (conn.mReceiveLimit == 5) {
							conn.mReceiveLimit = 5 + conn.mReceiveBuffer[4];
							if (!conn.mReceiveBuffer[4])
								conn.mReceiveLimit += 256;
						} else
							conn.mbReplyReady = true;
						break;

					case (uint8)HostCmd::SIOReplyTimeout:
						if (conn.mReceiveLimit == 1)
							conn.mReceiveLimit = 2;
						else
							conn.mbReplyReady = true;
						break;

					case (uint8)HostCmd::SIOPause:
						g_ATLCMux("Received host command: SIOPause()\n");
						conn.mbHostReady = false;
						commandDone = true;
						break;

					case (uint8)HostCmd::SIOResume:
						g_ATLCMux("Received host command: SIOResume()\n");
						commandDone = true;
						if (!conn.mbHostReady) {
							conn.mbHostReady = true;

							if (mCommState == kClientCommState_Busy)
								ScheduleState(kClientCommState_ClientPoll);
						}
						break;

					default:
						return false;
				}

				if (conn.mbReplyReady) {
					// All SIO replies implicitly change the client's host ready state to true. This allows
					// a client to temporarily monopolize the host, but this is the way MASTER.COM works --
					// it deliberately stretches the selection state after handling a command so the client
					// can immediately issue a new command without waiting for another selection cycle.
					conn.mbHostReady = false;

					switch(conn.mReceiveBuffer[0]) {
						case (uint8)HostCmd::SIOReply:
							g_ATLCMux("Received host command: SIOReply($%02X, $%02X, $%02X)\n"
								, conn.mReceiveBuffer[1]
								, conn.mReceiveBuffer[2]
								, conn.mReceiveBuffer[3]
							);
							conn.mbHostReady = true;
							break;
						case (uint8)HostCmd::SIOReplyWithData:
							g_ATLCMux("Received host command: SIOReplyWithData($%02X, $%02X, $%02X, $%02X)\n"
								, conn.mReceiveBuffer[1]
								, conn.mReceiveBuffer[2]
								, conn.mReceiveBuffer[3]
								, conn.mReceiveBuffer[4]
							);
							conn.mbHostReady = true;
							break;
						case (uint8)HostCmd::SIOReplyTimeout:
							g_ATLCMux("Received host command: SIOReplyTimeout($%02X)\n"
								, conn.mReceiveBuffer[1]
							);
							conn.mbHostReady = true;
							break;
					}

					if (conn.mRequestIdCounter != conn.mReceiveBuffer[1]) {
						conn.mbReplyReady = false;
						g_ATLCMux("Dropping host command due to request counter mismatch\n");
					} else if (mCommState == kClientCommState_ExecuteWait) {
						ScheduleState(kClientCommState_SendResultFromServer);
					} else {
						conn.mbReplyReady = false;
						g_ATLCMux("Dropping host command due to not being in ExecuteWait state\n");
					}

					if (!conn.mbReplyReady)
						commandDone = true;
				}

				if (commandDone) {
					conn.mReceiveIndex = 0;
					conn.mReceiveLimit = 1;
				}
			}
		}
	}

	if (status.mbCanWrite && conn.mSendLimit > 0) {
		for(;;) {
			if (conn.mSendIndex >= conn.mSendLimit) {
				conn.mSendIndex = 0;
				conn.mSendLimit = 0;
				break;
			}

			sint32 n = conn.mpSocket->Send(conn.mSendBuffer + conn.mSendIndex, conn.mSendLimit - conn.mSendIndex);
			if (n <= 0) {
				if (n < 0)
					return false;

				break;
			}

			conn.mSendIndex += n;
		}
	}

	return true;
}

void ATDeviceMultiplexer::OnClientReconnect() {
	mClientReconnectTimerToken = 0;

	RecreateSockets();
}

bool ATDeviceMultiplexer::OnHostClientConnectionSocketEvent(HostClientConnection& conn, const ATSocketStatus& status) {
	if (status.mbClosed || !conn.mpClientSocket || !OnHostClientConnectionSocketEvent2(conn, status)) {
		vdrefptr conn2(&conn);

		if (conn2->mpClientSocket) {
			conn2->mpClientSocket->SetOnEvent(nullptr, nullptr, false);
			conn2->mpClientSocket = nullptr;
		}

		if (mpActiveHostClientConnection == conn2) {
			mpActiveHostClientConnection = nullptr;

			ScheduleState(kHostCommState_Reset);
		}

		auto it = std::find(mHostClientConnections.begin(), mHostClientConnections.end(), conn2);
		if (it != mHostClientConnections.end()) {
			if (&*it != &mHostClientConnections.back())
				*it = std::move(mHostClientConnections.back());

			mHostClientConnections.pop_back();
		}
		return false;
	}

	return true;
}

bool ATDeviceMultiplexer::OnHostClientConnectionSocketEvent2(HostClientConnection& conn, const ATSocketStatus& status) {
	// Receive data
	if (!status.mbCanRead) {
		if (status.mbRemoteClosed)
			return false;
	} else {
		while(conn.mReceiveIndex <= conn.mReceiveLimit) {
			const uint32 remaining = conn.mReceiveLimit - conn.mReceiveIndex;

			if (remaining == 0)
				break;

			sint32 n = conn.mpClientSocket->Recv(conn.mReceiveBuffer + conn.mReceiveIndex, remaining);

			if (n <= 0) {
				if (n < 0)
					return false;

				break;
			}

			conn.mReceiveIndex += n;
			VDASSERT(conn.mReceiveIndex <= conn.mReceiveLimit);

			if (conn.mReceiveIndex >= conn.mReceiveLimit) {
				switch(conn.mReceiveBuffer[0]) {
					case (uint8)ClientCmd::SIOCommand: {
						bool commandReady = false;

						if (conn.mReceiveIndex == 1) {
							// command byte: read 1 byte for reqID + 1 byte for ID + 13 bytes for DCB
							conn.mReceiveLimit += 15;
						} else if (conn.mReceiveIndex == 16) {
							// DCB: check if device ID is valid
							const uint8 deviceId = conn.mReceiveBuffer[2];

							if (!deviceId || (deviceId & (deviceId - 1))) {
								g_ATLCMux("Dropping client -- SIO command received with invalid device ID $%02X\n", deviceId);
								return false;
							}

							// check for write command and add additional sector data if so
							if (conn.mReceiveBuffer[3+3] & 0x80) {
								uint32 len = conn.mReceiveBuffer[3+8];

								if (!len)
									len = 256;

								conn.mReceiveLimit += len;
							} else
								commandReady = true;
						} else
							commandReady = true;

						if (commandReady) {
							// If this is the active connection, we need to preempt the existing command
							if (mpActiveHostClientConnection == &conn) {
								g_ATLCMux("Pre-empting current command due to new command received on connection\n");

								mpActiveHostClientConnection = nullptr;

								ScheduleState(kHostCommState_Reset);
							}

							// store the command
							memcpy(conn.mPendingCommand, conn.mReceiveBuffer, conn.mReceiveLimit);

							conn.mCurrentRequestId = conn.mPendingCommand[1];
							conn.mCommandReadyDeviceId = conn.mPendingCommand[2];

							g_ATLCMux("Received remote command $%02X: ID %02X | Device %02X:%02X | Cmd %02X | Len %04X [%c] | AUX %04X\n"
								, conn.mCurrentRequestId
								, conn.mCommandReadyDeviceId
								, conn.mPendingCommand[3]
								, conn.mPendingCommand[4]
								, conn.mPendingCommand[5]
								, VDReadUnalignedLEU16(&conn.mPendingCommand[11])
								, conn.mPendingCommand[6] & 0x80 ? 'W' : conn.mPendingCommand[6] & 0x40 ? 'R' : ' '
								, VDReadUnalignedLEU16(&conn.mPendingCommand[13])
							);

							// reset receive state
							conn.mReceiveIndex = 0;
							conn.mReceiveLimit = 1;

							// if we are in the WaitForSelection state, force a switch to
							// WaitSelectAck to immediately rescan
							if (mCommState == kHostCommState_WaitForSelection)
								ScheduleState(kHostCommState_WaitSelectAck);
						}
						
						break;
					}

					case (uint8)ClientCmd::SIOCommandCancel:
						g_ATLCMux("Received remote cancel: ID %02X\n", conn.mReceiveBuffer[1]);

						conn.mCommandReadyDeviceId = 0;

						conn.mReceiveIndex = 0;
						conn.mReceiveLimit = 1;

						if (mpActiveHostClientConnection == &conn) {
							mpActiveHostClientConnection = nullptr;

							ScheduleState(kHostCommState_Reset);
						}
						break;

					default:
						g_ATLCMux("Dropping client -- invalid command byte $%02X received\n", conn.mReceiveBuffer[0]);
						return false;
				}
			}
		}
	}

	// Send data
	if (status.mbCanWrite) {
		for(;;) {
			if (conn.mSendIndex >= conn.mSendLimit) {
				conn.mSendIndex = 0;
				conn.mSendLimit = 0;

				FlushHostClientLazyStates(conn, false);

				if (!conn.mSendLimit)
					break;
			}

			sint32 n = conn.mpClientSocket->Send(conn.mSendBuffer + conn.mSendIndex, conn.mSendLimit - conn.mSendIndex);

			if (n <= 0) {
				if (n < 0)
					return false;
			}

			conn.mSendIndex += n;
		}
	}

	return true;
}

void ATDeviceMultiplexer::UpdateHostPortStates(uint32 outputState) {
	const uint32 delta = mCommLastOutputState ^ outputState;
	mCommLastOutputState = outputState;

	// if PB5 goes high, cancel out
	if (delta & outputState & 0x20000) {
		switch(mCommState) {
			case kHostCommState_Reset:
			case kHostCommState_WaitForSelection:
			case kHostCommState_WaitSelectAck:
				break;

			default:
				ScheduleState(kHostCommState_Reset);
				return;
		}
	}

	switch(mCommState) {
		case kHostCommState_WaitForSelection: {
			// check for:
			//	- PA0-PA7 contains exactly one bit set
			//	- PB5 is high to enable output bus driver
			//	- At least one must have just turned on
			const uint32 onMask = delta & outputState;

			if ((onMask & 0x20FF) && (outputState & 0x2000)) {
				const uint8 deviceId = (uint8)outputState;

				if (deviceId && (deviceId & (deviceId - 1)) == 0) {
					AT_LOG_MUX_DETAILED_SEL("Device select %02X\n", deviceId);
					ScheduleState(kHostCommState_WaitSelectAck);
				}
			}
			break;
		}

		case kHostCommState_WaitSelectAck:
			// wait for PB5 to go low
			if (delta & ~outputState & 0x2000)
				ScheduleState(kHostCommState_SendDCB, 100);
			break;

		case kHostCommState_SendDCBByteWait:
		case kHostCommState_SendDataFrameByteWait:
			// check for CA2 low indicating byte has been received
			if (delta & ~outputState & kATVIAOutputBit_CA2) {
				AT_LOG_MUX_DETAILED_ACK("Byte acked\n");

				// lower CA1
				SetAck(false);

				// wait for ack
				ScheduleState(mCommState + 1);
			}
			break;

		case kHostCommState_SendDCBByteAckWait:
		case kHostCommState_SendDataFrameByteAckWait:
			// check for CA2 high indicating byte cycle complete
			if (delta & outputState & kATVIAOutputBit_CA2) {
				AT_LOG_MUX_DETAILED_ACK("Received ACK\n");

				ScheduleState(mCommState - 2);
			}
			break;

		case kHostCommState_ReceiveDCBReplyByteWait:
		case kHostCommState_ReceiveDataFrameByteWait:
		case kHostCommState_ReceiveCommandReplyByteWait:
			// check for CA2 low indicating byte has been sent
			if (delta & ~outputState & kATVIAOutputBit_CA2) {
				// capture byte
				const uint8 v = mVIA.GetOutput() & 0xFF;

				AT_LOG_MUX_DETAILED_XFER("Received data[%2u] = $%02X\n", mCommTransferIndex, v);

				mCommTransferBuffer[mCommTransferIndex++] = v;

				// raise CA1
				SetAck(true);

				// wait for ack
				ScheduleState(mCommState + 1);
				return;
			}
			break;

		case kHostCommState_ReceiveDCBReplyByteAckWait:
		case kHostCommState_ReceiveDataFrameByteAckWait:
		case kHostCommState_ReceiveCommandReplyByteAckWait:
			// check for CA2 high indicating byte cycle complete
			if (delta & outputState & kATVIAOutputBit_CA2) {
				AT_LOG_MUX_DETAILED_ACK("Received ACK\n");

				// lower CA1
				SetAck(false);

				ScheduleState(mCommState - 2);
			}
			break;

		default:
			break;
	}
}

void ATDeviceMultiplexer::UpdateClientPortStates(uint32 outputState) {
	const uint32 delta = mCommLastOutputState ^ outputState;
	mCommLastOutputState = outputState;

	switch(mCommState) {
		case kClientCommState_ClientPoll2:
			// PB5 low -> client answering poll
			if (delta & ~outputState & 0x2000)
				ScheduleState(kClientCommState_ReceiveDCB);
			break;

		case kClientCommState_ReceiveDCBByteWait:
		case kClientCommState_ReceiveDataFrameByteWait:
			// check for CA2 low indicating byte has been sent
			if (delta & ~outputState & kATVIAOutputBit_CA2) {
				// capture byte
				const uint8 v = mVIA.GetOutput() & 0xFF;

				AT_LOG_MUX_DETAILED_XFER("Received data[%2u] = $%02X\n", mCommTransferIndex, v);

				mCommTransferBuffer[mCommTransferIndex++] = v;

				// raise CA1
				SetAck(true);

				// wait for ack
				ScheduleState(mCommState + 1);
			}
			break;

		case kClientCommState_ReceiveDCBByteAckWait:
		case kClientCommState_ReceiveDataFrameByteAckWait:
			// check for CA2 high indicating byte cycle complete
			if (delta & outputState & kATVIAOutputBit_CA2) {
				//g_ATLCMux("Received ACK\n");

				// lower CA1
				SetAck(false);

				ScheduleState(mCommState - 2);
			}
			break;

		case kClientCommState_SendDCBReplyByteWait:
		case kClientCommState_SendResultByteWait:
			// check for CA2 low indicating byte has been sent
			if (delta & ~outputState & kATVIAOutputBit_CA2) {
				AT_LOG_MUX_DETAILED_ACK("Byte acked\n");

				// raise CA1
				SetAck(false);

				// wait for ack
				ScheduleState(mCommState + 1);
			}
			break;

		case kClientCommState_SendDCBReplyByteAckWait:
		case kClientCommState_SendResultByteAckWait:
			// check for CA2 high indicating byte cycle complete
			if (delta & outputState & kATVIAOutputBit_CA2) {
				AT_LOG_MUX_DETAILED_ACK("Received ACK\n");

				ScheduleState(mCommState - 2);
			}
			break;

		default:
			break;
	}
}

void ATDeviceMultiplexer::SetInputByte(uint8 v) {
	mVIA.SetPortAInput(v);

	if (mpTraceChannelInput) {
		mCurrentTracingInputByte = v;

		const uint64 t = mpScheduler->GetTick64();
		mpTraceChannelOutput->TruncateLastEvent(t);
		mpTraceChannelInput->TruncateLastEvent(t);
		mpTraceChannelInput->AddOpenTickEventF(t, kATTraceColor_IO_Read, L"%02X", v);
	}
}

void ATDeviceMultiplexer::ClearInputByte() {
	mVIA.SetPortAInput(0xFF);

	if (mpTraceChannelInput) {
		if (mCurrentTracingInputByte >= 0) {
			mCurrentTracingInputByte = -1;

			const uint64 t = mpScheduler->GetTick64();
			mpTraceChannelInput->TruncateLastEvent(t);
		}
	}
}

void ATDeviceMultiplexer::SetAck(bool asserted) {
	mVIA.SetCA1Input(asserted);
	mVIA.SetPortBInput(asserted ? 0x80 : 0x00, 0x80);

	if (mpTraceChannelAck) {
		if (mbCurrentTrackingAck != asserted) {
			mbCurrentTrackingAck = asserted;

			const uint64 t = mpScheduler->GetTick64();
			if (asserted)
				mpTraceChannelAck->AddOpenTickEvent(t, L"", kATTraceColor_IO_Read);
			else
				mpTraceChannelAck->TruncateLastEvent(t);
		}
	}
}

void ATDeviceMultiplexer::SetHostBusy(bool busy) {
	if (mbHostBusy == busy || !mbHost)
		return;

	mbHostBusy = busy;

	// Iterate over all client connections and update their pause state if
	// they are in a send ready state. Note that the active connection is
	// excluded, as it is implicitly paused.
	for(const auto& conn : mHostClientConnections) {
		FlushHostClientLazyStates(*conn, true);
	}
}

void ATDeviceMultiplexer::FlushHostClientLazyStates(HostClientConnection& conn, bool doPoll) {
	if (&conn == mpActiveHostClientConnection || conn.mSendIndex < conn.mSendLimit)
		return;

	const bool ready = !mbHostBusy;

	if (conn.mbLastHostReadyState != ready) {
		conn.mbLastHostReadyState = ready;

		conn.mSendIndex = 0;
		conn.mSendLimit = 1;
		conn.mSendBuffer[0] = ready ? (uint8)HostCmd::SIOResume : (uint8)HostCmd::SIOPause;

		if (doPoll)
			conn.mpClientSocket->PollSocket();
	}
}

void ATDeviceMultiplexer::ScheduleState(uint32 state, uint32 delay) {
	AT_LOG_MUX_DETAILED_STATE("Switching to state %u\n", state);

	mCommState = state;
	mpScheduler->SetEvent(delay, this, "mux "_vdtypeid, mpCommEvent);
}

void ATDeviceMultiplexer::RunCommStateMachine() {
	if (mbHost)
		RunHostCommStateMachine();
	else
		RunClientCommStateMachine();
}

void ATDeviceMultiplexer::RunHostCommStateMachine() {
	for(;;) {
		if (mpCommEvent)
			break;

		switch(mCommState) {
			case kHostCommState_Reset:
				if (mpActiveHostClientConnection) {
					g_ATLCMux("Resetting active command\n");

					vdrefptr conn(std::move(mpActiveHostClientConnection));
					mpActiveHostClientConnection = nullptr;

					VDASSERT(conn->mSendLimit == 0);

					conn->mSendBuffer[0] = (uint8)HostCmd::SIOReplyTimeout;
					conn->mSendBuffer[1] = conn->mCurrentRequestId;
					conn->mSendIndex = 0;
					conn->mSendLimit = 2;

					conn->mCommandReadyDeviceId = 0;
					conn->mbLastHostReadyState = true;

					if (conn->mReceiveIndex >= conn->mReceiveLimit) {
						conn->mReceiveIndex = 0;
						conn->mReceiveLimit = 1;
					}

					conn->mpClientSocket->PollSocket();
				}

				// ensure PB6 is lowered
				mVIA.SetPortBInput(0x00, 0x40);

				// lower CA1, set portA to input
				ClearInputByte();
				SetAck(false);

				ScheduleState(kHostCommState_WaitForSelection, 10000);

				break;

			case kHostCommState_WaitForSelection:
				// wait for PA0-PA7 match
				return;

			case kHostCommState_WaitSelectAck: {
				// recheck if the device ID is still selected (PA = device ID, PB5=1)
				const uint32 outputState = mVIA.GetOutput();
				const uint8 deviceId = (uint8)outputState;

				if (!(outputState & 0x2000) || !deviceId || (deviceId & (deviceId - 1))) {
					mCommState = kHostCommState_WaitForSelection;

					// notify all other connections that we are not busy
					SetHostBusy(false);
					break;
				}

				// check if there is a connection with a ready command and empty reply
				// buffer for the currently selected ID; don't handle a command if the
				// reply buffer is still draining
				bool anyCommandPending = false;
				for(const auto& conn : mHostClientConnections) {
					if (conn->mCommandReadyDeviceId) {
						anyCommandPending = true;

						if (conn->mCommandReadyDeviceId == deviceId && conn->mSendLimit == 0) {
							mpActiveHostClientConnection = conn;
							break;
						}
					}
				}

				if (!mpActiveHostClientConnection) {
					mCommState = kHostCommState_WaitForSelection;

					// notify all other connections that we are not busy if no command
					// is pending on any connection
					if (!anyCommandPending)
						SetHostBusy(false);

					break;
				}

				g_ATLCMux("ID $%02X matched, waiting for initiation\n", deviceId);

				// active connection now assumes implicit not ready
				mpActiveHostClientConnection->mbLastHostReadyState = false;

				// notify all other connections that we are now busy
				SetHostBusy(true);

				// raise PB6
				mVIA.SetPortBInput(0x40, 0x40);

				// wait for PB5 to go low
				return;
			}

			case kHostCommState_SendDCB:
				// lower PB6
				mVIA.SetPortBInput(0x00, 0x40);

				// copy DCB from active connection
				memcpy(mCommDCBBuffer, &mpActiveHostClientConnection->mPendingCommand[3], sizeof mCommDCBBuffer);
				memcpy(mCommTransferBuffer, mCommDCBBuffer, sizeof mCommDCBBuffer);

				// compute DCB checksum
				mCommTransferBuffer[13] = ComputeDataFrameChecksum(mCommDCBBuffer);

				g_ATLCMux("Initiating command | ID %02X | Device %02X:%02X | Command %02X | Dir %02X | Buffer $%04X($%04X) | Aux %04X\n"
					, mCommDeviceId
					, mCommTransferBuffer[0]
					, mCommTransferBuffer[1]
					, mCommTransferBuffer[2]
					, mCommTransferBuffer[3]
					, VDReadUnalignedLEU16(&mCommTransferBuffer[4])
					, VDReadUnalignedLEU16(&mCommTransferBuffer[8])
					, VDReadUnalignedLEU16(&mCommTransferBuffer[10])
				);

				mCommTransferIndex = 0;
				mCommTransferLimit = 14;

				ScheduleState(kHostCommState_SendDCBByte, 100);
				break;

			case kHostCommState_SendDCBByte:
				if (mCommTransferIndex >= 14) {
					ClearInputByte();

					mCommState = kHostCommState_ReceiveDCBReply;
					break;
				}
				
				AT_LOG_MUX_DETAILED_XFER("Sending DCB[%02u] = $%02X\n", mCommTransferIndex, mCommTransferBuffer[mCommTransferIndex]);

				SetInputByte(mCommTransferBuffer[mCommTransferIndex++]);
				SetAck(true);

				ScheduleState(kHostCommState_SendDCBByteWait, 1000);
				return;

			case kHostCommState_SendDCBByteWait:
			case kHostCommState_SendDCBByteAckWait:
				g_ATLCMux("Timeout while sending DCB\n");
				mCommState = kHostCommState_Reset;
				break;

			case kHostCommState_ReceiveDCBReply:
				mCommTransferIndex = 0;
				mCommTransferLimit = 2;

				ClearInputByte();

				mCommState = kHostCommState_ReceiveDCBReplyByte;
				break;

			case kHostCommState_ReceiveDCBReplyByte:
				if (mCommTransferIndex >= 3) {
					const uint8 computedChecksum = ComputeReplyChecksum(mCommTransferBuffer[0], mCommTransferBuffer[1]);
					const bool checksumOK = (computedChecksum == mCommTransferBuffer[2]);

					g_ATLCMux("DCB reply: %02X %02X %02X (checksum %s)\n"
						, mCommTransferBuffer[0]
						, mCommTransferBuffer[1]
						, mCommTransferBuffer[2]
						, checksumOK ? "ok" : "BAD");

					if (!checksumOK) {
						mCommState = kHostCommState_Reset;
						break;
					}

					if (mCommTransferBuffer[1] & 0x40) {
						mCommState = kHostCommState_SendReplyToClient;
					} else if (mCommDCBBuffer[3] & 0x80) {
						mCommTransferIndex = 0;
						mCommTransferLimit = mCommDCBBuffer[8] ? mCommDCBBuffer[8] + 1 : 256 + 1;

						memcpy(mCommTransferBuffer, &mpActiveHostClientConnection->mPendingCommand[16], mCommTransferLimit - 1);
						mCommTransferBuffer[mCommTransferLimit - 1] = ComputeDataFrameChecksum(vdspan(mCommTransferBuffer, mCommTransferLimit - 1));

						mCommState = kHostCommState_SendDataFrameByte;
					} else if (mCommDCBBuffer[3] & 0x40) {
						mCommTransferIndex = 0;
						mCommTransferLimit = mCommDCBBuffer[8] ? mCommDCBBuffer[8] + 1 : 256 + 1;
						mCommState = kHostCommState_ReceiveDataFrameByte;
					} else
						mCommState = kHostCommState_SendReplyToClient;
					break;
				}

				ScheduleState(kHostCommState_ReceiveDCBReplyByteWait, 1000);
				break;

			case kHostCommState_ReceiveDCBReplyByteWait:
			case kHostCommState_ReceiveDCBReplyByteAckWait:
				g_ATLCMux("Timeout while waiting for DCB reply\n");
				mCommState = kHostCommState_Reset;
				break;

			case kHostCommState_SendDataFrameByte:
				if (mCommTransferIndex >= mCommTransferLimit) {
					const uint8 computedChecksum = ComputeDataFrameChecksum(vdspan(mCommTransferBuffer, mCommTransferLimit - 1));
					const bool checksumOK = (computedChecksum == mCommTransferBuffer[mCommTransferLimit - 1]);

					if (checksumOK) {
						mCommState = kHostCommState_ReceiveCommandReply;
					} else {
						mCommState = kHostCommState_Reset;
					}
					break;
				}

				// write byte
				SetInputByte(mCommTransferBuffer[mCommTransferIndex++]);

				// raise CA1
				SetAck(true);

				ScheduleState(kHostCommState_SendDataFrameByteWait, mCommTransferIndex ? 1000 : 10000000);
				break;

			case kHostCommState_SendDataFrameByteWait:
			case kHostCommState_SendDataFrameByteAckWait:
				g_ATLCMux("Timeout while sending data frame\n");
				mCommState = kHostCommState_Reset;
				break;

			case kHostCommState_ReceiveDataFrameByte:
				if (mCommTransferIndex >= mCommTransferLimit) {
					const uint8 computedChecksum = ComputeDataFrameChecksum(vdspan(mCommTransferBuffer, mCommTransferLimit - 1));
					const bool checksumOK = (computedChecksum == mCommTransferBuffer[mCommTransferLimit - 1]);

					g_ATLCMux("Received data frame: checksum %s\n", checksumOK ? "ok" : "BAD");

					if (checksumOK) {
						mpActiveHostClientConnection->mSendBuffer[4] = (uint8)(mCommTransferLimit - 1);
						memcpy(mpActiveHostClientConnection->mSendBuffer + 5, mCommTransferBuffer, mCommTransferLimit - 1);
						mCommState = kHostCommState_ReceiveCommandReply;
					} else {
						mCommState = kHostCommState_Reset;
					}
					break;
				}

				ScheduleState(kHostCommState_ReceiveDataFrameByteWait, mCommTransferIndex ? 1000 : 10000000);
				break;

			case kHostCommState_ReceiveDataFrameByteWait:
			case kHostCommState_ReceiveDataFrameByteAckWait:
				g_ATLCMux("Timeout while receiving data frame\n");
				mCommState = kHostCommState_Reset;
				break;

			case kHostCommState_ReceiveCommandReply:
				mCommTransferIndex = 0;
				mCommTransferLimit = 3;
				
				mCommState = kHostCommState_ReceiveCommandReplyByte;
				break;

			case kHostCommState_ReceiveCommandReplyByte:
				if (mCommTransferIndex >= mCommTransferLimit) {
					const uint8 computedChecksum = ComputeReplyChecksum(mCommTransferBuffer[0], mCommTransferBuffer[1]);
					const bool checksumOK = (computedChecksum == mCommTransferBuffer[2]);

					g_ATLCMux("Operation result: %02X %02X %02X (checksum %s)\n"
						, mCommTransferBuffer[0]
						, mCommTransferBuffer[1]
						, mCommTransferBuffer[2]
						, checksumOK ? "ok" : "BAD"
					);


					if (checksumOK)
						mCommState = kHostCommState_SendReplyToClient;
					else
						mCommState = kHostCommState_Reset;
					break;
				}

				ScheduleState(kHostCommState_ReceiveCommandReplyByteWait, 100000);
				break;

			case kHostCommState_ReceiveCommandReplyByteWait:
			case kHostCommState_ReceiveCommandReplyByteAckWait:
				g_ATLCMux("Timeout while receiving operation result\n");
				mCommState = kHostCommState_Reset;
				break;

			case kHostCommState_SendReplyToClient:
				VDASSERT(mpActiveHostClientConnection->mSendLimit == 0);
				
				g_ATLCMux("Completing command\n");

				mCommState = kHostCommState_Reset;

				mpActiveHostClientConnection->mCommandReadyDeviceId = 0;
				mpActiveHostClientConnection->mbLastHostReadyState = false;
				mpActiveHostClientConnection->mReceiveIndex = 0;
				mpActiveHostClientConnection->mReceiveLimit = 1;

				mpActiveHostClientConnection->mSendBuffer[1] = mpActiveHostClientConnection->mCurrentRequestId;
				mpActiveHostClientConnection->mSendBuffer[2] = mCommTransferBuffer[0];
				mpActiveHostClientConnection->mSendBuffer[3] = mCommTransferBuffer[1];
				
				if (mCommDCBBuffer[3] & 0x40) {
					mpActiveHostClientConnection->mSendBuffer[0] = (uint8)HostCmd::SIOReplyWithData;
					mpActiveHostClientConnection->mSendLimit = 5;

					if (mpActiveHostClientConnection->mSendBuffer[4])
						mpActiveHostClientConnection->mSendLimit += mpActiveHostClientConnection->mSendBuffer[4];
					else
						mpActiveHostClientConnection->mSendLimit += 256;
				} else {
					VDASSERT(mpActiveHostClientConnection->mSendLimit == 0);
					mpActiveHostClientConnection->mSendBuffer[0] = (uint8)HostCmd::SIOReply;
					mpActiveHostClientConnection->mSendLimit = 4;
				}

				{
					auto conn = std::move(mpActiveHostClientConnection);
					mpActiveHostClientConnection = nullptr;

					conn->mpClientSocket->PollSocket();
				}
				break;

			default:
				VDFAIL("Invalid host receive state.");
				mCommState = kHostCommState_Reset;
				break;
		}
	}
}

void ATDeviceMultiplexer::RunClientCommStateMachine() {
	for(;;) {
		if (mpCommEvent)
			break;

		switch(mCommState) {
			case kClientCommState_Reset:
				// ensure PB6 is lowered
				mVIA.SetPortBInput(0x00, 0x40);

				// lower CA1, set portA to input
				ClearInputByte();
				SetAck(false);

				if (mpClientConnection && mpClientConnection->mbBusy) {
					mpClientConnection->mbBusy = false;
					mpClientConnection->mbReplyReady = false;

					mpClientConnection->mpSocket->PollSocket();
				}

				ScheduleState(kClientCommState_ClientPoll, 10000);
				break;

			case kClientCommState_Busy:
				// nothing to do here, we are waiting until the client connection
				// is re-established and is idle
				return;

			case kClientCommState_ClientPoll:
				// if we don't have a client connection or the client connection
				// is busy, skip the poll
				if (!mpClientConnection || mpClientConnection->mbBusy || !mpClientConnection->mbHostReady) {
					mCommState = kClientCommState_Busy;
					return;
				}

				mCommDeviceId <<= 1;

				if (!mCommDeviceId)
					mCommDeviceId = 1;

				// output poll device ID on port A
				SetInputByte(mCommDeviceId);

				// set ~600 cycle timeout
				ScheduleState(kClientCommState_ClientPoll2, 600);
				break;

			case kClientCommState_ClientPoll2:
				ScheduleState(kClientCommState_ClientPoll, 100);

				ClearInputByte();
				break;

			case kClientCommState_ReceiveDCB:
				// raise PB6 and lower CA1
				mVIA.SetPortBInput(0x40, 0x40);
				SetAck(false);

				// set up to receive 13 DCB bytes + checksum byte
				mCommTransferIndex = 0;
				mCommState = kClientCommState_ReceiveDCBByte;
				break;

			case kClientCommState_ReceiveDCBByte:
				if (mCommTransferIndex >= 14) {
					mCommState = kClientCommState_ProcessDCB;
					break;
				}

				ScheduleState(kClientCommState_ReceiveDCBByteWait, 1000);
				break;

			case kClientCommState_ReceiveDCBByteWait:
				mCommState = kClientCommState_Reset;
				break;

			case kClientCommState_ReceiveDCBByteAckWait:
				mCommState = kClientCommState_Reset;
				break;

			case kClientCommState_ProcessDCB: {
				// copy DCB from transfer buffer to DCB buffer
				memcpy(mCommDCBBuffer, mCommTransferBuffer, sizeof mCommDCBBuffer);

				// validate checksum
				const uint8 computedChecksum = ComputeDataFrameChecksum(mCommDCBBuffer);
				const bool checksumValid = (computedChecksum == mCommTransferBuffer[13]);
				
				g_ATLCMux("Received command | Device %02X:%02X | Command %02X | Dir %02X | Buffer $%04X($%04X) | Aux %04X | Checksum %s\n"
					, mCommDCBBuffer[0]
					, mCommDCBBuffer[1]
					, mCommDCBBuffer[2]
					, mCommDCBBuffer[3]
					, VDReadUnalignedLEU16(&mCommDCBBuffer[4])
					, VDReadUnalignedLEU16(&mCommDCBBuffer[8])
					, VDReadUnalignedLEU16(&mCommDCBBuffer[10])
					, checksumValid ? "ok" : "BAD"
				);

				if (!checksumValid) {
					mCommState = kClientCommState_Reset;
					break;
				}

				// set up reply
				mCommTransferBuffer[0] = 0x01;
				mCommTransferBuffer[1] = 0x00;
				mCommTransferBuffer[2] = ComputeReplyChecksum(mCommTransferBuffer[0], mCommTransferBuffer[1]);
				mCommTransferIndex = 0;
				mCommTransferLimit = 3;
				mCommState = kClientCommState_SendDCBReplyByte;
				break;
			}

			case kClientCommState_SendDCBReplyByte: {
				if (mCommTransferIndex >= mCommTransferLimit) {
					// Bit 6 on the second status byte indicates an error we should route
					// directly to the client.
					if (mCommTransferBuffer[1] & 0x40)
						mCommState = kClientCommState_SendResultFromServer;
					else
						mCommState = kClientCommState_ReceiveDataFrame;
					break;
				}

				const uint8 v = mCommTransferBuffer[mCommTransferIndex];
				AT_LOG_MUX_DETAILED_XFER("Sending DCB reply[%3u] = $%02X\n", mCommTransferIndex, v);
				SetInputByte(v);
				++mCommTransferIndex;

				SetAck(true);

				ScheduleState(kClientCommState_SendDCBReplyByteWait, 1000);

				break;
			}

			case kClientCommState_SendDCBReplyByteWait:
			case kClientCommState_SendDCBReplyByteAckWait:
				mCommState = kClientCommState_Reset;
				break;

			case kClientCommState_ReceiveDataFrame:
				ClearInputByte();

				// check if DSTATS bit 7 is set, meaning that we need a data
				// frame
				if (mCommDCBBuffer[3] & 0x80) {
					// data frame needed, set up to receive
					mCommTransferIndex = 0;
					mCommTransferLimit = mCommDCBBuffer[8] ? mCommDCBBuffer[8] + 1 : 256 + 1;
					mCommState = kClientCommState_ReceiveDataFrameByte;

					// lower CA1 to start
					SetAck(false);

					AT_LOG_MUX_DETAILED_XFER("Receiving data frame of %u bytes\n", mCommTransferLimit - 1);
					break;
				} else {
					// no data frame go to execute stage
					mCommState = kClientCommState_Execute;
				}
				break;

			case kClientCommState_ReceiveDataFrameByte:
				if (mCommTransferIndex >= mCommTransferLimit) {
					const uint8 receivedChecksum = mCommTransferBuffer[mCommTransferLimit - 1];
					const uint8 computedChecksum = ComputeDataFrameChecksum(vdspan(mCommTransferBuffer, mCommTransferLimit - 1));
					const bool checksumOK = receivedChecksum == computedChecksum;

					AT_LOG_MUX_DETAILED_XFER("Received data frame of %u bytes (checksum %s)\n", mCommTransferLimit - 1, checksumOK ? "ok" : "BAD");

					if (!checksumOK) {
						mCommState = kClientCommState_Reset;
						break;
					}

					mCommState = kClientCommState_Execute;
					break;
				}

				ScheduleState(kClientCommState_ReceiveDataFrameByteWait, 10000);
				break;

			case kClientCommState_ReceiveDataFrameByteWait:
				mCommState = kClientCommState_Reset;
				break;

			case kClientCommState_ReceiveDataFrameByteAckWait:
				mCommState = kClientCommState_Reset;
				break;

			case kClientCommState_Execute: {
				// begin constructing command
				g_ATLCMux("Sending command to host\n");
				VDASSERT(mpClientConnection->mSendIndex == 0);

				uint32 sendLen = 3 + 13;
				mpClientConnection->mSendBuffer[0] = (uint8)ClientCmd::SIOCommand;
				mpClientConnection->mSendBuffer[1] = ++mpClientConnection->mRequestIdCounter;
				mpClientConnection->mSendBuffer[2] = mCommDeviceId;

				// copy over the DCB -- we only need the main 13 bytes, the checksum
				// is known good when a command is sent as we short circuit locally
				// if it is bad
				memcpy(&mpClientConnection->mSendBuffer[3], mCommDCBBuffer, 13);

				// copy over the sector buffer as well if we are doing a write command
				if (mCommDCBBuffer[3] & 0x80) {
					uint32 len = mCommDCBBuffer[8];

					if (!len)
						len = 256;

					memcpy(&mpClientConnection->mSendBuffer[sendLen], mCommTransferBuffer, len);
					sendLen += len;
				}

				// send off command
				mpClientConnection->mSendLimit = sendLen;
				mpClientConnection->mpSocket->PollSocket();

				// wait for server response
				mCommState = kClientCommState_ExecuteWait;
				return;
			}

			case kClientCommState_ExecuteWait:
				// we are waiting for a server response
				return;

			case kClientCommState_SendResultFromServer: {
				if (!mpClientConnection->mbReplyReady) {
					mCommState = kClientCommState_Reset;
					break;
				}

				mpClientConnection->mbReplyReady = false;
				mpClientConnection->mReceiveIndex = 0;
				mpClientConnection->mReceiveLimit = 1;

				const uint8 serverReplyCmd = mpClientConnection->mReceiveBuffer[0];
				if (serverReplyCmd == (uint8)HostCmd::SIOReplyTimeout) {
					mCommState = kClientCommState_Reset;
					break;
				}

				// send first two bytes as reply, with checksum
				uint32 dataLen = 0;
				uint32 statusOffset = 0;

				if (serverReplyCmd == (uint8)HostCmd::SIOReplyWithData) {
					dataLen = mpClientConnection->mReceiveBuffer[4];
					if (!dataLen)
						dataLen = 256;

					statusOffset = dataLen + 1;
				}

				mCommTransferBuffer[statusOffset + 0] = mpClientConnection->mReceiveBuffer[2];
				mCommTransferBuffer[statusOffset + 1] = mpClientConnection->mReceiveBuffer[3];
				mCommTransferBuffer[statusOffset + 2] = ComputeReplyChecksum(mCommTransferBuffer[statusOffset + 0], mCommTransferBuffer[statusOffset + 1]);
				mCommTransferIndex = 0;
				mCommTransferLimit = statusOffset + 3;
				
				// if there is remaining data, send it as the sector data before the status
				if (dataLen) {
					memcpy(&mCommTransferBuffer[0], &mpClientConnection->mReceiveBuffer[5], dataLen);
					mCommTransferBuffer[dataLen] = ComputeDataFrameChecksum(vdspan(mCommTransferBuffer, dataLen));
				}

				mpClientConnection->mpSocket->PollSocket();

				SetAck(false);

				ScheduleState(kClientCommState_SendResultByte, 1000);
				break;
			}

			case kClientCommState_SendResultByte: {
				if (mCommTransferIndex >= mCommTransferLimit) {
					mCommState = kClientCommState_CommandEnd;
					break;
				}

				const uint8 v = mCommTransferBuffer[mCommTransferIndex];
				AT_LOG_MUX_DETAILED_XFER("Sending result[%3u] = $%02X\n", mCommTransferIndex, v);
				SetInputByte(v);
				++mCommTransferIndex;

				SetAck(true);

				ScheduleState(kClientCommState_SendResultByteWait, 1000);
				break;
			}

			case kClientCommState_SendResultByteWait:
			case kClientCommState_SendResultByteAckWait:
				mCommState = kClientCommState_Reset;
				break;

			case kClientCommState_CommandEnd:
				// we need a delay as the client gets upset if PB6 goes low too soon
				ScheduleState(kClientCommState_Reset, 1000);
				break;

			default:
				VDFAIL("Invalid client receive state.");
				mCommState = kClientCommState_ClientPoll;
				break;
		}
	}
}

void ATDeviceMultiplexer::UpdateDeviceStatus() {
	DeviceStatus status = DeviceStatus::Default;

	if (!mbHost && !mpClientConnection) {
		if (mpConnectingClientSocket)
			status = DeviceStatus::Connecting;
		else
			status = DeviceStatus::UnableToConnect;
	}

	if (mDeviceStatus != status) {
		mDeviceStatus = status;

		NotifyStatusChanged();
	}
}

uint8 ATDeviceMultiplexer::ComputeReplyChecksum(uint8 v1, uint8 v2) {
	return (uint8)(v1 + v2 + 0x80);
}

uint8 ATDeviceMultiplexer::ComputeDataFrameChecksum(vdspan<const uint8> data) {
	return (uint8)std::accumulate(data.begin(), data.end(), 0x55);
}

#undef AT_LOG_MUX_DETAILED_ACK
#undef AT_LOG_MUX_DETAILED_XFER
