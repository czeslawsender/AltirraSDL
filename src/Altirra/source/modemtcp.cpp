//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2011 Avery Lee
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

#include <stdafx.h>
#include <bitset>
#include <vd2/system/thread.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/VDString.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "modemtcp.h"
#include "rs232.h"

#ifdef _MSC_VER
	#pragma comment(lib, "ws2_32.lib")
#endif

namespace ATNetTelnetOptions {
	enum ATTelnetOption : uint8 {
		TransmitBinary = 0,
		Echo = 1,
		SuppressGoAhead = 3,
		Status = 5,
		TimingMark = 6,
		TerminalType = 24,
		Naws = 31,
		TerminalSpeed = 32,
		ToggleFlowControl = 33,
		LineMode = 34,
		XDisplayLocation = 35,
		Environ = 36,
		Authentication = 37,
		Encrypt = 38,
		NewEnviron = 39
	};
}

namespace {
	const char *GetTelnetOptionName(uint8 c) {
		using namespace ATNetTelnetOptions;

		switch(c) {
		case TransmitBinary:	return "TRANSMIT-BINARY";
		case Echo:				return "ECHO";
		case SuppressGoAhead:	return "SUPPRESS-GO-AHEAD";
		case Status:			return "STATUS";
		case TimingMark:		return "TIMING-MARK";
		case TerminalType:		return "TERMINAL-TYPE";
		case Naws:				return "NAWS";
		case TerminalSpeed:		return "TERMINAL-SPEED";
		case ToggleFlowControl:	return "TOGGLE-FLOW-CONTROL";
		case LineMode:			return "LINEMODE";
		case XDisplayLocation:	return "X-DISPLAY-LOCATION";
		case Environ:			return "ENVIRON";
		case Authentication:	return "AUTHENTICATION";
		case Encrypt:			return "ENCRYPT";
		case NewEnviron:		return "NEW-ENVIRON";
		default:				return "?";
		}
	}

	const char *GetWinsockError(uint32 err) {
		switch(err) {
#define X(v) case v: return #v;
			X(WSAEINTR)
			X(WSAEBADF)
			X(WSAEACCES)
			X(WSAEFAULT)
			X(WSAEINVAL)
			X(WSAEMFILE)
			X(WSAEWOULDBLOCK)
			X(WSAEINPROGRESS)
			X(WSAEALREADY)
			X(WSAENOTSOCK)
			X(WSAEDESTADDRREQ)
			X(WSAEMSGSIZE)
			X(WSAEPROTOTYPE)
			X(WSAENOPROTOOPT)
			X(WSAEPROTONOSUPPORT)
			X(WSAESOCKTNOSUPPORT)
			X(WSAEOPNOTSUPP)
			X(WSAEPFNOSUPPORT)
			X(WSAEAFNOSUPPORT)
			X(WSAEADDRINUSE)
			X(WSAEADDRNOTAVAIL)
			X(WSAENETDOWN)
			X(WSAENETUNREACH)
			X(WSAENETRESET)
			X(WSAECONNABORTED)
			X(WSAECONNRESET)
			X(WSAENOBUFS)
			X(WSAEISCONN)
			X(WSAENOTCONN)
			X(WSAESHUTDOWN)
			X(WSAETOOMANYREFS)
			X(WSAETIMEDOUT)
			X(WSAECONNREFUSED)
			X(WSAELOOP)
			X(WSAENAMETOOLONG)
			X(WSAEHOSTDOWN)
			X(WSAEHOSTUNREACH)
			X(WSAENOTEMPTY)
			X(WSAEPROCLIM)
			X(WSAEUSERS)
			X(WSAEDQUOT)
			X(WSAESTALE)
			X(WSAEREMOTE)
			X(WSASYSNOTREADY)
			X(WSAVERNOTSUPPORTED)
			X(WSANOTINITIALISED)
			X(WSAEDISCON)
			X(WSAENOMORE)
			X(WSAECANCELLED)
			X(WSAEINVALIDPROCTABLE)
			X(WSAEINVALIDPROVIDER)
			X(WSAEPROVIDERFAILEDINIT)
			X(WSASYSCALLFAILURE)
			X(WSASERVICE_NOT_FOUND)
			X(WSATYPE_NOT_FOUND)
			X(WSA_E_NO_MORE)
			X(WSA_E_CANCELLED)
			X(WSAEREFUSED)
			X(WSAHOST_NOT_FOUND)
			X(WSATRY_AGAIN)
			X(WSANO_RECOVERY)
			X(WSANO_DATA)
#undef X

			default:
				return "?";
		}
	}
}

class ATModemDriverTCP final : public IATModemDriver, public VDThread {
public:
	ATModemDriverTCP();
	~ATModemDriverTCP();

	bool Init(const char *address, const char *service, uint32 port, bool loggingEnabled, IATModemDriverCallback *callback) override;
	void Shutdown() override;

	bool GetLastIncomingAddress(VDStringA& address, uint32& port) override;

	void SetLoggingEnabled(bool enabled) override;
	void SetConfig(const ATRS232Config& config) override;

	uint32 Write(const void *data, uint32 len) override;
	uint32 Write(const void *data, uint32 len, bool escapeChars);

	uint32 Read(void *buf, uint32 len) override;
	bool ReadLogMessages(VDStringA& messages) override;

protected:
	void ThreadRun() override;

	void WorkerShutdown();
	void OnCommandLocked();
	void OnRead(uint32 bytes);
	void OnWrite();
	void OnError(int code);
	void QueueRead();
	void QueueWrite();
	void FlushSpecialReplies();

	void SendDo(uint8 c);
	void SendDont(uint8 c);
	void SendWill(uint8 c);
	void SendWont(uint8 c);
	void SendCommand(uint8 cmd, uint8 opt);

	void Log(const char *msg);
	void LogF(const char *format, ...);

	IATModemDriverCallback *mpCB;
	VDStringA mAddress;
	VDStringA mService;
	uint32 mPort;

	VDStringA mIncomingAddress;
	uint32 mIncomingPort;

	VDSignal	mThreadInited;
	SOCKET mSocket;
	SOCKET mSocket2;
	WSAEVENT mCommandEvent;
	WSAEVENT mNetworkEvent;
	WSAEVENT mNetwork2Event;
	bool	mbReadEOF;
	bool	mbConnected;
	bool	mbListenIPv6;
	VDStringA	mTelnetTermType;
	WSAOVERLAPPED mOverlappedRead;
	WSAOVERLAPPED mOverlappedWrite;

	VDAtomicInt	mbTelnetEmulation;

	// begin mutex protected members
	VDCriticalSection	mMutex;
	uint32	mWriteQueuedBytes;
	bool	mbExit;

	VDStringA	mLogMessages;
	bool	mbLoggingEnabled;

	uint32	mReadIndex;
	uint32	mReadLevel;

	uint8	mReadBuffer[4096];
	uint8	mWriteBuffer[4096];
	// end mutex protected members

	vdfastvector<uint8> mSpecialReplies;
	uint32	mSpecialReplyIndex;

	VDStringA	mWorkerLog;
	bool	mbWorkerLoggingEnabled;

	enum TelnetState {
		kTS_Disabled,
		kTS_WaitingForIAC,
		kTS_WaitingForCommandByte,
		kTS_WaitingForDoOptionByte,
		kTS_WaitingForDontOptionByte,
		kTS_WaitingForWillOptionByte,
		kTS_WaitingForWontOptionByte
	};

	enum TelnetSubState {
		kTSS_None,
		kTSS_SubOptionCode,
		kTSS_SubData_Discard,
		kTSS_SubData_TerminalType
	};

	TelnetState mTelnetState;
	TelnetSubState mTelnetSubState;
	bool		mbTelnetListeningMode;
	bool		mbTelnetWaitingForEchoResponse;
	bool		mbTelnetWaitingForSGAResponse;

	bool		mbTelnetLFConversion;
	bool		mbTelnetSawIncomingCR;
	bool		mbTelnetSawOutgoingCR;
	bool		mbTelnetSawIncomingATASCII;
	bool		mbTelnetSentTerminalType;
	bool		mbTelnetBinaryModeIncoming;
	bool		mbTelnetBinaryModeOutgoing;
	uint32		mTelnetBinaryModeIncomingPending;

	// Which options we have sent a DO or DONT for, and shouldn't send another
	// one unless we are deliberately asking for a mode change.
	std::bitset<256> mTelnetSentDoDont;
};

IATModemDriver *ATCreateModemDriverTCP() {
	return new ATModemDriverTCP;
}

ATModemDriverTCP::ATModemDriverTCP()
	: VDThread("Altirra TCP modem worker")
	, mSocket(INVALID_SOCKET)
	, mSocket2(INVALID_SOCKET)
	, mCommandEvent(WSA_INVALID_EVENT)
	, mNetworkEvent(WSA_INVALID_EVENT)
	, mNetwork2Event(WSA_INVALID_EVENT)
	, mbListenIPv6(true)
	, mbLoggingEnabled(false)
	, mbTelnetEmulation(false)
	, mbTelnetLFConversion(false)
	, mbTelnetSawIncomingCR(false)
	, mbTelnetSawOutgoingCR(false)
	, mbTelnetSawIncomingATASCII(false)
	, mbTelnetBinaryModeIncoming(false)
	, mbTelnetBinaryModeOutgoing(false)
	, mTelnetBinaryModeIncomingPending(0)
{
}

ATModemDriverTCP::~ATModemDriverTCP() {
	Shutdown();
}

bool ATModemDriverTCP::Init(const char *address, const char *service, uint32 port, bool loggingEnabled, IATModemDriverCallback *callback) {
	if (address)
		mAddress = address;
	else
		mAddress.clear();

	if (service)
		mService = service;
	else
		mService.clear();

	mPort = port;

	mIncomingAddress.clear();
	mIncomingPort = 0;

	mpCB = callback;
	mWriteQueuedBytes = 0;
	mReadIndex = 0;
	mReadLevel = 0;

	mbLoggingEnabled = loggingEnabled;
	mbWorkerLoggingEnabled = loggingEnabled;
	mbTelnetListeningMode = mAddress.empty();
	mbTelnetSawIncomingCR = false;
	mbTelnetSawOutgoingCR = false;
	mbTelnetSentTerminalType = false;

	mThreadInited.tryWait(0);

	mbExit = false;
	if (!ThreadStart())
		return false;

	// wait for initialization
	HANDLE h[2] = {mThreadInited.getHandle(), getThreadHandle()};
	WaitForMultipleObjects(2, h, FALSE, INFINITE);

	return true;
}

void ATModemDriverTCP::Shutdown() {
	mMutex.Lock();
	mbExit = true;
	mMutex.Unlock();
	WSASetEvent(mCommandEvent);
	ThreadWait();
}

bool ATModemDriverTCP::GetLastIncomingAddress(VDStringA& address, uint32& port) {
	mMutex.Lock();
	address = mIncomingAddress;
	port = mIncomingPort;
	mMutex.Unlock();

	return !address.empty();
}

void ATModemDriverTCP::SetLoggingEnabled(bool enabled) {
	mMutex.Lock();
	mbLoggingEnabled = enabled;
	mMutex.Unlock();
	WSASetEvent(mCommandEvent);
}

void ATModemDriverTCP::SetConfig(const ATRS232Config& config) {
	mbTelnetEmulation = config.mbTelnetEmulation;
	mbTelnetLFConversion = mbTelnetEmulation && config.mbTelnetLFConversion;
	mbListenIPv6 = config.mbListenForIPv6;
	mTelnetTermType = config.mTelnetTermType;

	for(VDStringA::iterator it = mTelnetTermType.begin(), itEnd = mTelnetTermType.end();
		it != itEnd;
		++it)
	{
		*it = toupper((unsigned char)*it);
	}
}

uint32 ATModemDriverTCP::Read(void *buf, uint32 len) {
	if (!len)
		return 0;

	mMutex.Lock();
	uint32 tc = mReadLevel - mReadIndex;

	if (tc > len)
		tc = len;

	memcpy(buf, mReadBuffer + mReadIndex, tc);
	mReadIndex += tc;

	if (tc && mReadIndex >= mReadLevel)
		WSASetEvent(mCommandEvent);
	mMutex.Unlock();

	return tc;
}

bool ATModemDriverTCP::ReadLogMessages(VDStringA& messages) {
	mMutex.Lock();
	messages = mLogMessages;
	mLogMessages.clear();
	mMutex.Unlock();

	return !messages.empty();
}

uint32 ATModemDriverTCP::Write(const void *data, uint32 len) {
	return Write(data, len, true);
}

uint32 ATModemDriverTCP::Write(const void *data, uint32 len, bool escapeChars) {
	if (!len)
		return 0;

	mMutex.Lock();
	bool wasZero = (mWriteQueuedBytes == 0);

	uint32 tc;
	if (escapeChars) {
		const uint8 *data8 = (const uint8 *)data;

		while(len && mWriteQueuedBytes < sizeof mWriteBuffer) {
			uint8 c = *data8++;
			--len;

			if (mbTelnetEmulation) {
				if (mbTelnetLFConversion && !mbTelnetSawIncomingATASCII) {
					if (c == 0x0D)
						mbTelnetSawOutgoingCR = true;
					else if (mbTelnetSawOutgoingCR) {
						mbTelnetSawOutgoingCR = false;

						// drop LF after CR (we would have already transmitted it)
						if (c == 0x0A)
							continue;
					}
				} else if (!mbTelnetBinaryModeOutgoing) {
					if (c == 0x0D) {
						if (mWriteQueuedBytes >= (sizeof mWriteBuffer) - 1)
							break;

						// escape CR as CR NUL
						mWriteBuffer[mWriteQueuedBytes++] = c;
						c = 0;
					}
				}
				
				if (c == 0xFF) {
					if (mWriteQueuedBytes >= (sizeof mWriteBuffer) - 1)
						break;

					mWriteBuffer[mWriteQueuedBytes++] = 0xFF;
				}
			}

			mWriteBuffer[mWriteQueuedBytes++] = c;

			if (mbTelnetEmulation && mbTelnetLFConversion) {
				if (c == 0x0D && !mbTelnetSawIncomingATASCII) {
					if (mWriteQueuedBytes < sizeof mWriteBuffer)
						mWriteBuffer[mWriteQueuedBytes++] = 0x0A;
				}
			}
		}

		tc = (uint32)(data8 - (const uint8 *)data);
	} else {
		tc = sizeof mWriteBuffer - mWriteQueuedBytes;

		if (tc > len)
			tc = len;

		memcpy(mWriteBuffer + mWriteQueuedBytes, data, tc);
		mWriteQueuedBytes += tc;
	}

	if (wasZero)
		WSASetEvent(mCommandEvent);
	mMutex.Unlock();

	return tc;
}

void ATModemDriverTCP::ThreadRun() {
	mbConnected = false;
	mbReadEOF = false;
	mTelnetState = kTS_WaitingForIAC;
	mTelnetSubState = kTSS_None;
	mbTelnetBinaryModeIncoming = false;
	mTelnetBinaryModeIncomingPending = 0;
	mbTelnetBinaryModeOutgoing = false;

	mSpecialReplies.clear();
	mSpecialReplyIndex = 0;

	mCommandEvent = WSACreateEvent();
	mNetworkEvent = WSACreateEvent();
	mNetwork2Event = WSACreateEvent();

	if (mCommandEvent == WSA_INVALID_EVENT ||
		mNetworkEvent == WSA_INVALID_EVENT ||
		mNetwork2Event == WSA_INVALID_EVENT)
	{
		VDDEBUG("ModemTCP: Unable to create events.\n");
		if (mpCB)
			mpCB->OnEvent(this, kATModemPhase_Init, kATModemEvent_AllocFail);

		WorkerShutdown();
		return;
	}

	mThreadInited.signal();

	LONG networkEventMask = 0;

	if (mAddress.empty()) {
		// create IPv4 listening socket
		mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (mSocket == INVALID_SOCKET) {
			VDDEBUG("ModemTCP: Unable to create socket.\n");
			if (mpCB)
				mpCB->OnEvent(this, kATModemPhase_Init, kATModemEvent_AllocFail);

			WorkerShutdown();
			return;
		}

		sockaddr_in sa = {0};
		sa.sin_port = htons(mPort);
		sa.sin_addr.S_un.S_addr = INADDR_ANY;
		sa.sin_family = AF_INET;
		if (bind(mSocket, (sockaddr *)&sa, sizeof sa)) {
			VDDEBUG("ModemTCP: Unable to bind socket.\n");
			if (mpCB)
				mpCB->OnEvent(this, kATModemPhase_Listen, kATModemEvent_GenericError);

			WorkerShutdown();
			return;
		}

		BOOL reuse = TRUE;
		setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof reuse);

		if (listen(mSocket, 1)) {
			DWORD err = WSAGetLastError();

			VDDEBUG("ModemTCP: Unable to enable listening on socket.\n");
			if (mpCB) {
				ATModemEvent event = kATModemEvent_GenericError;

				if (err == WSAEADDRINUSE)
					event = kATModemEvent_LineInUse;
				else if (err == WSAENETDOWN)
					event = kATModemEvent_NoDialTone;

				mpCB->OnEvent(this, kATModemPhase_Listen, event);
			}

			WorkerShutdown();
			return;
		}

		if (SOCKET_ERROR == WSAEventSelect(mSocket, mNetworkEvent, FD_ACCEPT | FD_READ | FD_WRITE | FD_CLOSE)) {
			VDDEBUG("ModemTCP: Unable to enable asynchronous accept.\n");
			if (mpCB)
				mpCB->OnEvent(this, kATModemPhase_Accept, kATModemEvent_GenericError);

			WorkerShutdown();
			return;
		}

		// create IPv6 listening socket (OK for this to fail)
		if (mbListenIPv6) {
			mSocket2 = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

			if (mSocket2 != INVALID_SOCKET) {
				sockaddr_in6 sa6 = {0};
				sa6.sin6_port = htons(mPort);
				sa6.sin6_family = AF_INET6;
				if (!bind(mSocket2, (sockaddr *)&sa6, sizeof sa6)) {
					// hey... we successfully bound to IPv6!
					BOOL reuse = TRUE;
					setsockopt(mSocket2, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof reuse);

					if (!listen(mSocket2, 1)) {
						if (SOCKET_ERROR == WSAEventSelect(mSocket2, mNetwork2Event, FD_ACCEPT | FD_READ | FD_WRITE | FD_CLOSE)) {
							closesocket(mSocket2);
							mSocket2 = INVALID_SOCKET;
						}
					} else {
						closesocket(mSocket2);
						mSocket2 = INVALID_SOCKET;
					}
				} else {
					closesocket(mSocket2);
					mSocket2 = INVALID_SOCKET;
				}
			}
		}

		for(;;) {
			union {
				char buf[256];
				sockaddr addr;
			} sa2 = {0};
			int salen = sizeof(sa2);
			SOCKET sock2 = accept(mSocket, &sa2.addr, &salen);

			if (sock2 == INVALID_SOCKET && mSocket2 != INVALID_SOCKET)
				sock2 = accept(mSocket2, &sa2.addr, &salen);

			if (sock2 != INVALID_SOCKET) {
				closesocket(mSocket);

				if (mSocket2 != INVALID_SOCKET) {
					closesocket(mSocket2);
					mSocket2 = INVALID_SOCKET;
				}

				WSACloseEvent(mNetworkEvent);

				mNetworkEvent = WSACreateEvent();
				mSocket = sock2;

				if (mNetworkEvent == WSA_INVALID_EVENT) {
					VDDEBUG("ModemTCP: unable to create accepted socket listening event.\n");

					if (mpCB)
						mpCB->OnEvent(this, kATModemPhase_Accept, kATModemEvent_GenericError);

					WorkerShutdown();
					return;
				}

				// Disable Nagle's algorithm after completing the connection and before setting
				// async events.
				BOOL nodelay = TRUE;
				if (setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof nodelay)) {
					[[maybe_unused]] int setSockOptErr = WSAGetLastError();

					VDDEBUG("ModemTCP: Unable to disable nagling (%d / %s).\n", setSockOptErr, GetWinsockError(setSockOptErr));
				}

				WSAEventSelect(mSocket, mNetworkEvent, FD_READ | FD_WRITE | FD_CLOSE);
				networkEventMask = FD_CONNECT;

				// we're connected... grab the incoming address before we send the connected
				// event
				vdfastvector<char> namebuf(NI_MAXHOST, 0);
				vdfastvector<char> servbuf(NI_MAXSERV, 0);
				int revresult = getnameinfo(&sa2.addr, salen, namebuf.data(), NI_MAXHOST, servbuf.data(), NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);

				mMutex.Lock();
				if (!revresult) { 
					mIncomingAddress = namebuf.data();
					mIncomingPort = atoi(servbuf.data());
				} else {
					mIncomingAddress.clear();
					mIncomingPort = 0;
				}
				mMutex.Unlock();

				VDDEBUG("ModemTCP: Inbound connection accepted.\n");
				break;
			}

			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				VDDEBUG("ModemTCP: accept() call failed.\n");

				if (mpCB)
					mpCB->OnEvent(this, kATModemPhase_Accept, kATModemEvent_GenericError);

				WorkerShutdown();
				return;
			}

			HANDLE h[3] = { mCommandEvent, mNetworkEvent, mNetwork2Event };
			for(;;) {
				DWORD r = WSAWaitForMultipleEvents(mSocket2 != INVALID_SOCKET ? 3 : 2, h, FALSE, INFINITE, FALSE);

				if (r == WAIT_OBJECT_0) {
					mMutex.Lock();
					OnCommandLocked();

					WSAResetEvent(mCommandEvent);
					bool exit = mbExit;
					mMutex.Unlock();

					if (exit) {
						WorkerShutdown();
						return;
					}
				} else if (r == WAIT_OBJECT_0 + 1) {
					WSANETWORKEVENTS events;
					WSAEnumNetworkEvents(mSocket, mNetworkEvent, &events);
					break;
				} else if (r == WAIT_OBJECT_0 + 2) {
					WSANETWORKEVENTS events;
					WSAEnumNetworkEvents(mSocket2, mNetwork2Event, &events);
					break;
				} else {
					VDDEBUG("ModemTCP: WFME() failed.\n");

					if (mpCB)
						mpCB->OnEvent(this, kATModemPhase_Accept, kATModemEvent_GenericError);

					WorkerShutdown();
					return;
				}
			}
		}
	} else {
		VDDEBUG("ModemTCP: Looking up %s:%s\n", mAddress.c_str(), mService.c_str());

		addrinfo hint = {0};
		hint.ai_family = AF_UNSPEC;
		hint.ai_socktype = SOCK_STREAM;

		addrinfo *results = NULL;
		if (getaddrinfo(mAddress.c_str(), mService.c_str(), &hint, &results)) {
			VDDEBUG("ModemTCP: Name lookup failed.\n");

			if (mpCB)
				mpCB->OnEvent(this, kATModemPhase_NameLookup, kATModemEvent_NameLookupFailed);

			WorkerShutdown();
			return;
		}

		VDDEBUG("ModemTCP: Contacting %s:%s\n", mAddress.c_str(), mService.c_str());

		int cr = -1;
		
		for(addrinfo *p = results; p; p = p->ai_next) {
			mMutex.Lock();
			bool exit = mbExit;
			mMutex.Unlock();

			if (exit) {
				freeaddrinfo(results);
				WorkerShutdown();
				return;
			}

			if (p->ai_socktype != SOCK_STREAM)
				continue;

			if (p->ai_family != PF_INET && p->ai_family != PF_INET6)
				continue;

			mSocket = WSASocket(p->ai_family, p->ai_socktype, p->ai_protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
			if (mSocket != INVALID_SOCKET) {
				// Disable Nagle's algorithm before initiating the connection. It fails intermittently if
				// done while the connect is pending.
				BOOL nodelay = TRUE;
				if (setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof nodelay)) {
					[[maybe_unused]] int setSockOptErr = WSAGetLastError();

					VDDEBUG("ModemTCP: Unable to disable nagling (%d / %s).\n", setSockOptErr, GetWinsockError(setSockOptErr));
				}

				if (SOCKET_ERROR != WSAEventSelect(mSocket, mNetworkEvent, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE))
					cr = connect(mSocket, p->ai_addr, (int)p->ai_addrlen);
			}

			if (!cr || WSAGetLastError() == WSAEWOULDBLOCK)
				break;

			closesocket(mSocket);
			mSocket = INVALID_SOCKET;
		}

		freeaddrinfo(results);

		if (mSocket == INVALID_SOCKET) {
			VDDEBUG("ModemTCP: Unable to connect.\n");
			if (mpCB)
				mpCB->OnEvent(this, kATModemPhase_Connecting, kATModemEvent_ConnectFailed);
			
			WorkerShutdown();
			return;
		}

		VDDEBUG("ModemTCP: Contacted %s\n", mAddress.c_str());
	}

	// make out of band data inline for reliable Telnet -- this avoids the need to try
	// to compensate for differences in TCB Urgent data handling, as well as annoyances
	// in OOB and Async interfaces at Winsock level
	BOOL oobinline = TRUE;
	setsockopt(mSocket, SOL_SOCKET, SO_OOBINLINE, (const char *)&oobinline, sizeof oobinline);

	mbTelnetWaitingForEchoResponse = false;
	mbTelnetWaitingForSGAResponse = false;

	mTelnetSentDoDont.reset();

	mbTelnetSawIncomingCR = false;
	mbTelnetSawIncomingATASCII = false;

	WSAEVENT events[2] = {
		mCommandEvent,
		mNetworkEvent,
	};

	QueueRead();
	QueueWrite();

	for(;;) {
		if (!mbConnected && mbReadEOF) {
			mMutex.Lock();
			bool readDone = (mReadIndex >= mReadLevel);
			mMutex.Unlock();

			if (readDone) {
				if (mpCB)
					mpCB->OnEvent(this, kATModemPhase_Connected, kATModemEvent_ConnectionDropped);

				break;
			}
		}

		if (networkEventMask & FD_CONNECT) {
			if (!mbConnected) {
				mbConnected = true;

				if (mbTelnetListeningMode && mbTelnetEmulation) {
					// Ask the client to begin line mode negotiation.
					mSpecialReplies.push_back(0xFF);	// IAC
					mSpecialReplies.push_back(0xFB);	// WILL
					mSpecialReplies.push_back(ATNetTelnetOptions::Echo);
					mSpecialReplies.push_back(0xFF);	// IAC
					mSpecialReplies.push_back(0xFD);	// DO
					mSpecialReplies.push_back(ATNetTelnetOptions::SuppressGoAhead);
					mSpecialReplies.push_back(0xFF);	// IAC
					mSpecialReplies.push_back(0xFD);	// DO
					mSpecialReplies.push_back(ATNetTelnetOptions::LineMode);
					mbTelnetWaitingForEchoResponse = true;
					mbTelnetWaitingForSGAResponse = true;

					FlushSpecialReplies();
				}

				if (mpCB)
					mpCB->OnEvent(this, kATModemPhase_Connected, kATModemEvent_Connected);
			}

			networkEventMask &= ~FD_CONNECT;
		}

		if (networkEventMask & FD_CLOSE) {
			mbConnected = false;
			mbReadEOF = true;

			if (mpCB)
				mpCB->OnEvent(this, kATModemPhase_Connected, kATModemEvent_ConnectionClosing);

			// loop back around, begin waiting for drain
			networkEventMask &= ~FD_CLOSE;
			continue;
		}

		if (networkEventMask & FD_READ) {
			QueueRead();
		}

		if (networkEventMask & FD_WRITE) {
			QueueWrite();
		}

		networkEventMask = 0;

		DWORD waitResult = WSAWaitForMultipleEvents(2, events, FALSE, INFINITE, TRUE);

		if (waitResult == WSA_WAIT_EVENT_0) {
			mMutex.Lock();
			OnCommandLocked();
			WSAResetEvent(mCommandEvent);
			bool exit = mbExit;

			if (exit) {
				mMutex.Unlock();
				WorkerShutdown();
				return;
			}

			bool shouldWrite = mWriteQueuedBytes;
			bool shouldRead = mReadIndex >= mReadLevel;
			mMutex.Unlock();

			if (shouldWrite)
				QueueWrite();

			if (shouldRead)
				QueueRead();
		} else if (waitResult == WSA_WAIT_EVENT_0 + 1) {
			WSANETWORKEVENTS events {};

			if (!WSAEnumNetworkEvents(mSocket, mNetworkEvent, &events)) {
				networkEventMask = events.lNetworkEvents;

				for(int i=0; i<FD_MAX_EVENTS; ++i) {
					if (networkEventMask & (1 << i)) {
						if (events.iErrorCode[i]) {
							OnError(events.iErrorCode[i]);

							if (i == FD_CONNECT_BIT) {
								networkEventMask = 0;
								if (mpCB)
									mpCB->OnEvent(this, kATModemPhase_Connecting, kATModemEvent_ConnectFailed);

								WorkerShutdown();
								return;
							}
						}
					}
				}
			} else {
				WSAResetEvent(mNetworkEvent);
			}
		} else if (waitResult == WAIT_FAILED)
			break;
	}

	WorkerShutdown();
}

void ATModemDriverTCP::WorkerShutdown() {
	if (mSocket2 != INVALID_SOCKET) {
		shutdown(mSocket2, SD_SEND);
		closesocket(mSocket2);
		mSocket2 = INVALID_SOCKET;
	}

	if (mSocket != INVALID_SOCKET) {
		shutdown(mSocket, SD_SEND);
		closesocket(mSocket);
		mSocket = INVALID_SOCKET;
	}

	if (mCommandEvent != WSA_INVALID_EVENT) {
		WSACloseEvent(mCommandEvent);
		mCommandEvent = WSA_INVALID_EVENT;
	}

	if (mNetworkEvent != WSA_INVALID_EVENT) {
		WSACloseEvent(mNetworkEvent);
		mNetworkEvent = WSA_INVALID_EVENT;
	}

	if (mNetwork2Event != WSA_INVALID_EVENT) {
		WSACloseEvent(mNetwork2Event);
		mNetwork2Event = WSA_INVALID_EVENT;
	}
}

void ATModemDriverTCP::OnCommandLocked() {
	mbWorkerLoggingEnabled = mbLoggingEnabled;
}

void ATModemDriverTCP::OnRead(uint32 bytes) {
	if (!bytes) {
		mbReadEOF = true;
		return;
	}

	// Parse the read buffer and strip out any special commands. We immediately
	// queue replies for these.
	uint8 *dst = mReadBuffer;
	TelnetState state = mTelnetState;
	TelnetSubState substate = mTelnetSubState;

	if (!mbTelnetEmulation) {
		state = kTS_WaitingForIAC;
	} else {
		for(uint32 i=0; i<bytes; ++i) {
			uint8 c = mReadBuffer[i];

			switch(state) {
				case kTS_WaitingForIAC:
					if (c == 0xFF) {
						state = kTS_WaitingForCommandByte;
						continue;
					}
					break;

				case kTS_WaitingForCommandByte:
					switch(c) {
						case 0xF0:	// SE
							substate = kTSS_None;
							state = kTS_WaitingForIAC;
							continue;
						case 0xFA:	// SB
							substate = kTSS_SubOptionCode;
							state = kTS_WaitingForIAC;
							continue;
						case 0xFB:	// WILL
							state = kTS_WaitingForWillOptionByte;
							continue;
						case 0xFC:	// WONT
							state = kTS_WaitingForWontOptionByte;
							continue;

						case 0xFD:	// DO
							state = kTS_WaitingForDoOptionByte;
							continue;

						case 0xFE:	// DONT
							state = kTS_WaitingForDontOptionByte;
							continue;

						case 0xFF:	// escape
							state = kTS_WaitingForIAC;
							break;

						default:
							state = kTS_WaitingForIAC;
							continue;
					}
					break;

				case kTS_WaitingForDoOptionByte:
					LogF("Received DO %u (%s)\n", c, GetTelnetOptionName(c));
					switch(c) {
						case ATNetTelnetOptions::TransmitBinary:
							if (mbTelnetLFConversion) {
								SendWont(ATNetTelnetOptions::TransmitBinary);
							} else {
								SendWill(ATNetTelnetOptions::TransmitBinary);

								if (!mbTelnetBinaryModeOutgoing) {
									mbTelnetBinaryModeOutgoing = true;
									mbTelnetSawOutgoingCR = false;

									if (!mbTelnetBinaryModeIncoming) {
										// request that the other side transmit binary
										SendDo(ATNetTelnetOptions::TransmitBinary);

										++mTelnetBinaryModeIncomingPending;
									}
								}
							}
							break;

						case ATNetTelnetOptions::Echo:
							if (mbTelnetWaitingForEchoResponse) {
								mbTelnetWaitingForEchoResponse = false;
								break;
							}

							if (mbTelnetListeningMode) {
								// This is a lie (we don't know what the Atari will do). But in general,
								// BBS programs will echo characters except for specific cases like password
								// entry.
								SendWill(ATNetTelnetOptions::Echo);
							} else {
								// We don't support local echoing.
								SendWont(ATNetTelnetOptions::Echo);
							}
							break;

						case ATNetTelnetOptions::SuppressGoAhead:
							// We do this.
							SendWill(ATNetTelnetOptions::SuppressGoAhead);
							break;

						case ATNetTelnetOptions::TerminalType:	// TERMINAL-TYPE (RFC 1091)
							if (mbTelnetListeningMode || mTelnetTermType.empty()) {
								SendWont(ATNetTelnetOptions::TerminalType);
							} else {
								SendWill(ATNetTelnetOptions::TerminalType);
							}
							break;

						default:
							// Whatever it is, we won't do it.
							SendWont(c);
							break;
					}

					state = kTS_WaitingForIAC;
					continue;

				case kTS_WaitingForDontOptionByte:
					LogF("Received DONT %u (%s)\n", c, GetTelnetOptionName(c));

					switch(c) {
						case ATNetTelnetOptions::TransmitBinary:
							SendWont(ATNetTelnetOptions::TransmitBinary);
							mbTelnetBinaryModeOutgoing = false;

							if (mbTelnetBinaryModeIncoming) {
								SendDont(ATNetTelnetOptions::TransmitBinary);

								++mTelnetBinaryModeIncomingPending;
							}
							break;

						default:
							// Whatever it is, we're already not doing it.
							SendWont(c);
							break;
					}
					state = kTS_WaitingForIAC;
					continue;

				case kTS_WaitingForWillOptionByte:
					LogF("Received WILL %u (%s)\n", c, GetTelnetOptionName(c));

					switch(c) {
						case ATNetTelnetOptions::TransmitBinary:
							if (mTelnetBinaryModeIncomingPending) {
								--mTelnetBinaryModeIncomingPending;

								mbTelnetBinaryModeIncoming = true;
								mbTelnetSawIncomingCR = false;
							}
							break;

						case ATNetTelnetOptions::Echo:
							if (mbTelnetListeningMode)
								SendDont(ATNetTelnetOptions::Echo);
							break;

						case ATNetTelnetOptions::SuppressGoAhead:
							if (mbTelnetWaitingForSGAResponse) {
								mbTelnetWaitingForSGAResponse = false;
								break;
							}

							SendDo(ATNetTelnetOptions::SuppressGoAhead);
							break;

						case ATNetTelnetOptions::LineMode:
							if (mbTelnetListeningMode) {
								// Heeeey... turns out this telnet client supports line mode.
								// Let's turn it off.
								mSpecialReplies.push_back(0xFF);	// IAC
								mSpecialReplies.push_back(0xFA);	// SB
								mSpecialReplies.push_back(ATNetTelnetOptions::LineMode);
								mSpecialReplies.push_back(0x01);	// MODE
								mSpecialReplies.push_back(0x00);	// 0
								mSpecialReplies.push_back(0xFF);	// IAC
								mSpecialReplies.push_back(0xF0);	// SE
							}

							SendDont(c);
							break;

						default:
							SendDont(c);
							break;
					}

					state = kTS_WaitingForIAC;
					continue;

				case kTS_WaitingForWontOptionByte:
					LogF("Received WONT %u (%s)\n", c, GetTelnetOptionName(c));

					// There is no such thing as a WON'T offer, so if we receive a WON'T,
					// it has to be in response to a DO or DON'T, and the only thing we
					// need to do is update local state or send out additional other requests.
					switch(c) {
						case ATNetTelnetOptions::TransmitBinary:
							if (mbTelnetListeningMode) {
								if (mTelnetBinaryModeIncomingPending) {
									--mTelnetBinaryModeIncomingPending;

									if (mbTelnetBinaryModeIncoming) {
										mbTelnetBinaryModeIncoming = false;
									}
								}
							}
							break;

						default:
							break;
					}

					state = kTS_WaitingForIAC;
					continue;
			}

			// We need to process non-command data payloads separately because of a minor issue:
			// IAC IAC pairs are required to encode FF bytes during subnegotiation.
			switch(substate) {
			case kTSS_SubOptionCode:
				if (c == 0x18)		// TERMINAL_TYPE
					substate = kTSS_SubData_TerminalType;
				else
					substate = kTSS_SubData_Discard;
				break;

			case kTSS_SubData_TerminalType:
				if (!mbTelnetListeningMode && c == 0x01) {	// SEND
					Log("Received TERMINAL-TYPE SEND\n");

					if (mTelnetTermType.empty())
						mbTelnetSentTerminalType = true;

					const uint8 *s = mbTelnetSentTerminalType ? (const uint8 *)"UNKNOWN" : (const uint8 *)mTelnetTermType.data();
					const size_t len = mbTelnetSentTerminalType ? 7 : mTelnetTermType.size();
					mbTelnetSentTerminalType = true;

					mSpecialReplies.push_back(0xFF);		// IAC
					mSpecialReplies.push_back(0xFA);		// SB
					mSpecialReplies.push_back(0x18);		// TERMINAL-TYPE
					mSpecialReplies.push_back(0x00);		// IS
					mSpecialReplies.insert(mSpecialReplies.end(), s, s + len);
					mSpecialReplies.push_back(0xFF);		// IAC
					mSpecialReplies.push_back(0xF0);		// SE
				}

				substate = kTSS_SubData_Discard;
				break;

			case kTSS_SubData_Discard:
				break;

			case kTSS_None:
				if (mbTelnetLFConversion && !mbTelnetSawIncomingATASCII) {
					if (c == 0x9B)
						mbTelnetSawIncomingATASCII = true;
					else if (c == 0x0D)
						mbTelnetSawIncomingCR = true;
					else if (mbTelnetSawIncomingCR) {
						mbTelnetSawIncomingCR = false;

						if (c == 0x0A || (c == 0x00 && !mbTelnetBinaryModeIncoming))
							continue;
					}
				} else if (!mbTelnetBinaryModeIncoming) {
					// A CR without a following LF is stuffed as CR NUL, so we must strip the NUL in that case.
					if (c == 0x0D)
						mbTelnetSawIncomingCR = true;
					else if (mbTelnetSawIncomingCR) {
						mbTelnetSawIncomingCR = false;

						if (c == 0x00)
							continue;
					}
				}

				*dst++ = c;
				break;
			}
		}

		bytes = (uint32)(dst - mReadBuffer);
	}

	mTelnetState = state;
	mTelnetSubState = substate;

	bool logs = false;

	mMutex.Lock();
	mReadIndex = 0;
	mReadLevel = bytes;

	if (!mWorkerLog.empty()) {
		logs = true;
		mLogMessages.append(mWorkerLog);
		mWorkerLog.clear();
	}

	mMutex.Unlock();

	FlushSpecialReplies();

	if (mpCB && (bytes || logs))
		mpCB->OnReadAvail(this, bytes);
}

void ATModemDriverTCP::OnWrite() {
	// Dump any special replies into the write buffer first; these have priority.
	FlushSpecialReplies();

	if (mpCB)
		mpCB->OnWriteAvail(this);
}

void ATModemDriverTCP::OnError(int err) {
	if (!err || err == WSAEWOULDBLOCK)
		return;

	if (mpCB) {
		ATModemEvent ev = kATModemEvent_GenericError;

		if (err == WSAECONNABORTED || err == WSAECONNRESET) {
			ev = kATModemEvent_ConnectionDropped;
			mbConnected = false;
			mbReadEOF = true;
		}

		mpCB->OnEvent(this, kATModemPhase_Connected, ev);
	}
}

void ATModemDriverTCP::QueueRead() {
	for(;;) {
		mMutex.Lock();

		if (mbReadEOF || mReadIndex < mReadLevel) {
			mMutex.Unlock();
			return;
		}

		mReadIndex = 0;
		mReadLevel = 0;
		mMutex.Unlock();

		int actual = recv(mSocket, (char *)mReadBuffer, sizeof mReadBuffer, 0);

		if (actual >= 0) {
			OnRead(actual);
		} else {
			OnError(WSAGetLastError());
			break;
		}
	}
}

void ATModemDriverTCP::QueueWrite() {
	mMutex.Lock();
	for(;;) {
		if (!mbConnected) {
			// just swallow data
			mWriteQueuedBytes = 0;
			break;
		}

		if (!mWriteQueuedBytes)
			break;

		const uint32 bytesQueued = mWriteQueuedBytes;

		mMutex.Unlock();

		int actual = send(mSocket, (char *)mWriteBuffer, bytesQueued, 0);

		mMutex.Lock();

		if (actual <= 0) {
			if (actual < 0) {
				mMutex.Unlock();
				OnError(WSAGetLastError());
				return;
			}

			break;
		}

		if ((uint32)actual >= mWriteQueuedBytes) {
			mWriteQueuedBytes = 0;
		} else {
			memmove(mWriteBuffer, mWriteBuffer + actual, mWriteQueuedBytes - actual);
			mWriteQueuedBytes -= actual;
		}

		mMutex.Unlock();

		OnWrite();

		mMutex.Lock();
	}

	mMutex.Unlock();
}

void ATModemDriverTCP::FlushSpecialReplies() {
	uint32 sn = (uint32)mSpecialReplies.size();
	uint32 si = mSpecialReplyIndex;
	if (si < sn) {
		si += Write(mSpecialReplies.data() + si, sn - si, false);

		if (si >= sn) {
			si = 0;
			mSpecialReplies.clear();
		}

		mSpecialReplyIndex = si;
		QueueWrite();
	}
}

void ATModemDriverTCP::SendDo(uint8 c) {
	if (mTelnetSentDoDont[c])
		return;

	mTelnetSentDoDont[c] = true;

	LogF("Sending  DO %u (%s)\n", c, GetTelnetOptionName(c));
	SendCommand(0xFD, c);
}

void ATModemDriverTCP::SendDont(uint8 c) {
	// If we have already sent a request on this particular option, don't send
	// another one.
	if (mTelnetSentDoDont[c])
		return;

	mTelnetSentDoDont[c] = true;

	LogF("Sending  DONT %u (%s)\n", c, GetTelnetOptionName(c));
	SendCommand(0xFE, c);
}

void ATModemDriverTCP::SendWill(uint8 c) {
	LogF("Sending  WILL %u (%s)\n", c, GetTelnetOptionName(c));
	SendCommand(0xFB, c);
}

void ATModemDriverTCP::SendWont(uint8 c) {
	LogF("Sending  WONT %u (%s)\n", c, GetTelnetOptionName(c));

	SendCommand(0xFC, c);
}

void ATModemDriverTCP::SendCommand(uint8 cmd, uint8 opt) {
	uint8 c[3] = { 0xFF, cmd, opt };

	mSpecialReplies.insert(mSpecialReplies.end(), c, c+3);
}

void ATModemDriverTCP::Log(const char *msg) {
	if (mbWorkerLoggingEnabled)
		mWorkerLog.append(msg);
}

void ATModemDriverTCP::LogF(const char *format, ...) {
	if (mbWorkerLoggingEnabled) {
		va_list val;
		va_start(val, format);
		mWorkerLog.append_vsprintf(format, val);
		va_end(val);
	}
}