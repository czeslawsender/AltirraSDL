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
#include <windows.h>
#include <at/atcore/asyncdispatcher.h>
#include <at/atcore/deviceimpl.h>
#include <at/atcore/deviceserial.h>
#include <at/atcore/propertyset.h>

void ATCreateDevicePipeSerial(const ATPropertySet& pset, IATDevice **dev);

extern const ATDeviceDefinition g_ATDeviceDefPipeSerial = { "pipeserial", "pipeserial", L"Named pipe serial port", ATCreateDevicePipeSerial };

class ATDevicePipeSerial final : public ATDeviceT<IATDeviceSerial>, public VDThread
{
public:
	ATDevicePipeSerial();
	~ATDevicePipeSerial();

public:
	void GetDeviceInfo(ATDeviceInfo& info) override;
	void Init() override;
	void Shutdown() override;
	void GetSettingsBlurb(VDStringW& buf) override;
	void GetSettings(ATPropertySet& pset) override;
	bool SetSettings(const ATPropertySet& pset) override;
	void ColdReset() override;
	bool GetErrorStatus(uint32 idx, VDStringW& error) override;

public:
	void SetOnStatusChange(const vdfunction<void(const ATDeviceSerialStatus&)>& fn) override;
	void SetTerminalState(const ATDeviceSerialTerminalState&) override;
	ATDeviceSerialStatus GetStatus() override;
	void SetOnReadReady(vdfunction<void()> fn) override;
	bool Read(uint32& baudRate, uint8& c) override;
	bool Read(uint32 baudRate, uint8& c, bool& framingError) override;
	void Write(uint32 baudRate, uint8 c) override;
	void FlushBuffers() override;

private:
	void ThreadRun() override;

	void OnPipeStatusChanged();
	void UpdatePipe();
	bool InitPipe();
	void ShutdownPipe();

	bool CheckPipeName();
	void SetPipePath();
	void UpdateStatus();

	vdfunction<void()> mpOnReadReady;
	IATAsyncDispatcher *mpAsyncDispatcher = nullptr;
	uint64 mAsyncCallback = 0;

	bool mbCanRead = true;
	bool mbLastConnected = false;
	uint32 mBaudRate = 1;

	enum class Status : uint8 {
		NotSet,
		Error,
		NotConnected,
		Connected
	};

	Status mStatus = Status::NotSet;
	VDStringW mError;
	VDStringW mPipeName;
	VDStringW mPipePath;

	HANDLE mhPipe = nullptr;
	HANDLE mhRecvEvent = nullptr;
	HANDLE mhSendEvent = nullptr;
	HANDLE mhAttentionEvent = nullptr;

	VDRWLock mMutex;
	uint32 mRecvLevel = 0;
	uint32 mRecvReadOffset = 0;
	uint32 mSendLevel = 0;
	uint32 mSendWriteOffset = 0;
	bool mbConnected = false;
	bool mbRecvReadBlocked = false;
	bool mbRecvWriteBlocked = false;
	bool mbSendReadBlocked = false;
	bool mbExitRequested = false;

	static constexpr uint32 kRecvBufferSize = 256;
	static constexpr uint32 kSendBufferSize = 256;

	uint8 mReadBuffer[kRecvBufferSize] {};
	uint8 mWriteBuffer[kSendBufferSize] {};

	static constexpr wchar_t kDefaultPipeName[] = L"AltirraSerial";
};

ATDevicePipeSerial::ATDevicePipeSerial() {
	mPipeName = kDefaultPipeName;
	SetPipePath();
}

ATDevicePipeSerial::~ATDevicePipeSerial() {
	Shutdown();
}

void ATDevicePipeSerial::GetDeviceInfo(ATDeviceInfo& info) {
	info.mpDef = &g_ATDeviceDefPipeSerial;
}

void ATDevicePipeSerial::Init() {
	mpAsyncDispatcher = GetService<IATAsyncDispatcher>();

	UpdatePipe();
}

void ATDevicePipeSerial::Shutdown() {
	ShutdownPipe();

	if (mpAsyncDispatcher) {
		mpAsyncDispatcher->Cancel(&mAsyncCallback);
		mpAsyncDispatcher = nullptr;
	}
}

void ATDevicePipeSerial::GetSettingsBlurb(VDStringW& buf) {
	buf.sprintf(L"listen at %ls at %u baud", mPipePath.c_str(), mBaudRate);
}

void ATDevicePipeSerial::GetSettings(ATPropertySet& pset) {
	pset.Clear();

	if (!mPipeName.empty())
		pset.SetString("pipe_name", mPipeName.c_str());

	pset.SetUint32("baud_rate", mBaudRate);
}

bool ATDevicePipeSerial::SetSettings(const ATPropertySet& pset) {
	mBaudRate = std::clamp<uint32>(pset.GetUint32("baud_rate", 31250), 1, 1000000);

	VDStringW newPipeName(pset.GetString("pipe_name", kDefaultPipeName));
	if (mPipeName != newPipeName) {
		mPipeName = newPipeName;

		SetPipePath();
		UpdatePipe();
	}

	return true;
}

void ATDevicePipeSerial::ColdReset() {
	FlushBuffers();
}

bool ATDevicePipeSerial::GetErrorStatus(uint32 idx, VDStringW& error) {
	if (idx || mStatus == Status::Connected)
		return false;

	switch(mStatus) {
		case Status::Error:
			error = mError;
			break;

		case Status::Connected:
			break;

		case Status::NotConnected:
			error = L"No incoming connection";
			break;
	}

	return true;
}

void ATDevicePipeSerial::SetOnStatusChange(const vdfunction<void(const ATDeviceSerialStatus&)>& fn) {
}

void ATDevicePipeSerial::SetTerminalState(const ATDeviceSerialTerminalState& state) {
}

ATDeviceSerialStatus ATDevicePipeSerial::GetStatus() {
	return {};
}

void ATDevicePipeSerial::SetOnReadReady(vdfunction<void()> fn) {
	mpOnReadReady = std::move(fn);
}

bool ATDevicePipeSerial::Read(uint32& baudRate, uint8& c) {
	baudRate = 0;
	c = 0;

	if (!mhPipe) {
		mbCanRead = false;
		return false;
	}

	bool anyRead = false;
	bool unblockRead = false;

	vdsyncexclusive(mMutex) {
		if (mRecvLevel) {
			c = mReadBuffer[mRecvReadOffset];
			if (++mRecvReadOffset >= kRecvBufferSize)
				mRecvReadOffset = 0;

			if (--mRecvLevel == kRecvBufferSize/2 && mbRecvWriteBlocked) {
				mbRecvWriteBlocked = false;
				unblockRead = true;
			}

			anyRead = true;
		} else if (!mbRecvReadBlocked) {
			mbRecvReadBlocked = true;
		}
	}

	if (!anyRead) {
		mbCanRead = false;
		return false;
	}

	if (unblockRead)
		VDVERIFY(SetEvent(mhAttentionEvent));


	baudRate = mBaudRate;
	return true;
}

bool ATDevicePipeSerial::Read(uint32 baudRate, uint8& c, bool& framingError) {
	framingError = false;

	uint32 transmitRate;
	if (!Read(transmitRate, c))
		return false;

	// check for more than a 5% discrepancy in baud rates between modem and serial port
	if (abs((int)baudRate - (int)transmitRate) * 20 > (int)transmitRate) {
		// baud rate mismatch -- return some bogus character and flag a framing error
		c = 'U';
		framingError = true;
	}

	return true;
}

void ATDevicePipeSerial::Write(uint32 baudRate, uint8 c) {
	if (!mhPipe)
		return;

	// drop byte if it is >5% from baud rate
	if (baudRate && abs((int)mBaudRate - (int)baudRate) * 20 > (int)mBaudRate)
		return;

	// enqueue byte, if there is room
	bool unblock = false;

	vdsyncexclusive(mMutex) {
		if (mbConnected && mSendLevel < kSendBufferSize) {
			mWriteBuffer[mSendWriteOffset] = c;

			if (++mSendWriteOffset >= kSendBufferSize)
				mSendWriteOffset = 0;
			
			if (mSendLevel == 0 && mbSendReadBlocked) {
				mbSendReadBlocked = false;
				unblock = true;
			}

			++mSendLevel;
		}
	}

	if (unblock)
		VDVERIFY(SetEvent(mhAttentionEvent));
}

void ATDevicePipeSerial::FlushBuffers() {
	uint32 baudRate = 0;
	uint8 c = 0;

	for(uint32 i = 0; i < kRecvBufferSize; ++i) {
		if (!Read(baudRate, c))
			break;
	}
}

void ATDevicePipeSerial::ThreadRun() {
	OVERLAPPED recvOp {};
	OVERLAPPED sendOp {};
	HANDLE handles[3] {
		mhRecvEvent,
		mhSendEvent,
		mhAttentionEvent
	};

	bool connected = false;
	bool connectPending = false;
	bool prevConnected = false;

	bool recvPending = false;
	bool sendPending = false;
	bool recvWriteBlockedLocal = false;
	bool sendReadBlockedLocal = false;
	uint32 recvWriteOffset = 0;
	uint32 sendReadOffset = 0;

	uint32 recvBytesPending = 0;
	uint32 sendBytesPending = 0;

	for(;;) {
		if (prevConnected != connected) {
			prevConnected = connected;

			// if we are switching back to disconnected, make sure the pipe is clear (though it
			// may already be disconnected)
			if (!connected) {
				(void)CancelIo(mhPipe);
				(void)DisconnectNamedPipe(mhPipe);

				sendPending = false;
				recvPending = false;
				recvWriteBlockedLocal = false;
				sendReadBlockedLocal = false;
			}

			vdsyncexclusive(mMutex) {
				mbConnected = connected;

				// flush data on disconnect
				if (!connected) {
					recvWriteOffset = mRecvReadOffset;
					sendReadOffset = mSendWriteOffset;
					mRecvLevel = 0;
					mSendLevel = 0;

					recvBytesPending = 0;
					sendBytesPending = 0;

					mbRecvReadBlocked = true;
					mbRecvWriteBlocked = false;
					mbSendReadBlocked = true;
				}
			}

			mpAsyncDispatcher->Queue(&mAsyncCallback, [this] { OnPipeStatusChanged(); });
		}

		if (recvBytesPending) {
			bool unblock = false;

			vdsyncexclusive(mMutex) {
				if (mbRecvReadBlocked) {
					mbRecvReadBlocked = false;
					unblock = true;
				}

				mRecvLevel += recvBytesPending;
				VDASSERT(mRecvLevel <= kRecvBufferSize);
			}

			if (unblock)
				mpAsyncDispatcher->Queue(&mAsyncCallback, [this] { OnPipeStatusChanged(); });

			recvWriteOffset += recvBytesPending;
			if (recvWriteOffset >= kRecvBufferSize)
				recvWriteOffset = 0;

			recvBytesPending = 0;
		}

		if (sendBytesPending) {
			vdsyncexclusive(mMutex) {
				VDASSERT(mSendLevel >= sendBytesPending);
				mSendLevel -= sendBytesPending;
			}

			sendReadOffset += sendBytesPending;
			if (sendReadOffset >= kRecvBufferSize)
				sendReadOffset = 0;

			sendBytesPending = 0;
		}

		// If we aren't connected, queue a connect call. Only do this if we don't have a send
		// or receive operation pending, as may be waiting for it to drain.
		if (!connected && !connectPending && !recvPending && !sendPending) {
			memset(&recvOp, 0, sizeof recvOp);
			recvOp.hEvent = mhRecvEvent;

			VDVERIFY(ResetEvent(mhRecvEvent));

			// Per the docs, the pipe may become connected before the call to ConnectNamedPipe(),
			// in which case it will 'fail' with ERROR_PIPE_CONNECTED. We should treat this
			// the same way as success.
			DWORD err = 0;
			if (ConnectNamedPipe(mhPipe, &recvOp))
				err = ERROR_PIPE_CONNECTED;
			else
				err = GetLastError();

			if (err == ERROR_PIPE_CONNECTED) {
				connected = true;
			} else {
				const DWORD err = GetLastError();

				if (err != ERROR_IO_PENDING) {
					VDFAIL("Connect failed");
					break;
				}

				connectPending = true;
			}

			continue;
		}

		if (connected) {
			if (!recvPending && !recvWriteBlockedLocal) {
				uint32 recvLevel = 0;

				vdsyncexclusive(mMutex) {
					recvLevel = mRecvLevel;

					if (recvLevel >= kRecvBufferSize) {
						mbRecvWriteBlocked = true;
						recvWriteBlockedLocal = true;
					}
				}

				uint32 recvSize = kRecvBufferSize - std::max<uint32>(recvLevel, recvWriteOffset);

				if (recvSize) {
					DWORD avail = 0;
					if (!PeekNamedPipe(mhPipe, nullptr, 0, nullptr, &avail, nullptr)) {
						connected = false;
						continue;
					}

					if (!avail)
						avail = 1;

					if (recvSize > avail)
						recvSize = avail;

					if (recvSize) {
						memset(&recvOp, 0, sizeof recvOp);
						recvOp.hEvent = mhRecvEvent;

						VDVERIFY(ResetEvent(mhRecvEvent));

						DWORD actual = 0;
						if (ReadFile(mhPipe, &mReadBuffer[recvWriteOffset], recvSize, &actual, &recvOp)) {
							VDVERIFY(ResetEvent(mhRecvEvent));
							recvBytesPending = actual;
							continue;
						} else {
							DWORD err = GetLastError();

							if (err != ERROR_IO_PENDING) {
								connected = false;
								continue;
							}

							recvPending = true;
						}
					}
				}
			}

			if (!sendPending && !sendReadBlockedLocal) {
				uint32 sendLevel = 0;

				vdsyncexclusive(mMutex) {
					sendLevel = mSendLevel;

					if (sendLevel == 0) {
						sendReadBlockedLocal = true;
						mbSendReadBlocked = true;
					}
				}

				const uint32 sendSize = std::min<uint32>(sendLevel, kSendBufferSize - sendReadOffset);

				if (sendSize) {
					memset(&sendOp, 0, sizeof sendOp);
					sendOp.hEvent = mhSendEvent;

					VDVERIFY(ResetEvent(mhSendEvent));

					DWORD actual = 0;
					if (WriteFile(mhPipe, &mWriteBuffer[sendReadOffset], sendSize, &actual, &sendOp)) {
						VDVERIFY(ResetEvent(mhSendEvent));

						sendBytesPending = actual;
						continue;
					} else {
						DWORD err = GetLastError();

						if (err != ERROR_IO_PENDING) {
							connected = false;
							continue;
						}

						sendPending = true;
					}
				}
			}
		}

		const DWORD waitResult = WaitForMultipleObjects(3, handles, FALSE, INFINITE);

		if (waitResult == WAIT_OBJECT_0) {
			// read event
			DWORD actual = 0;

			if (!GetOverlappedResult(mhPipe, &recvOp, &actual, TRUE)) {
				connected = false;
				connectPending = false;
				recvPending = false;
				continue;
			}

			if (connectPending) {
				connectPending = false;
				connected = true;
			} else {
				recvBytesPending = actual;
				recvPending = false;
			}
		} else if (waitResult == WAIT_OBJECT_0 + 1) {
			// write event
			DWORD actual = 0;

			if (!GetOverlappedResult(mhPipe, &sendOp, &actual, TRUE)) {
				connected = false;
				sendPending = false;
				continue;
			}

			VDASSERT(!sendBytesPending);
			sendBytesPending = actual;
			sendPending = false;
		} else if (waitResult == WAIT_OBJECT_0 + 2) {
			// attention event
			vdsyncexclusive(mMutex) {
				if (mbExitRequested)
					break;

				recvWriteBlockedLocal = mbRecvWriteBlocked;
				sendReadBlockedLocal = mbSendReadBlocked;
			}
		} else {
			VDFAIL("Wait failed");
			break;
		}
	}

	CancelIo(mhPipe);

	if (recvPending || connectPending) {
		DWORD actual = 0;

		(void)GetOverlappedResult(mhPipe, &recvOp, &actual, TRUE);
	}

	if (sendPending) {
		DWORD actual = 0;

		(void)GetOverlappedResult(mhPipe, &sendOp, &actual, TRUE);
	}
}

void ATDevicePipeSerial::OnPipeStatusChanged() {
	bool canRead {};
	bool connected {};

	vdsyncexclusive(mMutex) {
		canRead = (mRecvLevel > 0);
		connected = mbConnected;
	}

	if (mbLastConnected != connected) {
		mbLastConnected = connected;

		UpdateStatus();
	}

	if (canRead && !mbCanRead) {
		mbCanRead = true;

		if (mpOnReadReady)
			mpOnReadReady();
	}
}

void ATDevicePipeSerial::UpdatePipe() {
	ShutdownPipe();

	if (!mpAsyncDispatcher)
		return;

	if (!InitPipe())
		ShutdownPipe();

	UpdateStatus();
}

bool ATDevicePipeSerial::InitPipe() {
	mError.clear();

	if (!CheckPipeName()) {
		mError = L"Pipe name is reserved and can't be used.";
		return false;
	}

	mhRecvEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	mhSendEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	mhAttentionEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	if (!mhRecvEvent || !mhSendEvent || !mhAttentionEvent) {
		mError = L"Unable to allocate resources.";
		return false;
	}

	mhPipe = CreateNamedPipe(
		mPipePath.c_str(),
		PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
		1,
		256,
		256,
		0,
		nullptr);

	if (mhPipe == INVALID_HANDLE_VALUE)
		mhPipe = nullptr;

	if (!mhPipe) {
		mError = L"Unable to create pipe.";
		return false;
	}

	return ThreadStart();
}

void ATDevicePipeSerial::ShutdownPipe() {
	if (isThreadActive()) {
		vdsyncexclusive(mMutex) {
			mbExitRequested = true;
		}

		VDVERIFY(SetEvent(mhAttentionEvent));
		ThreadWait();
	}

	if (mhPipe) {
		VDVERIFY(CloseHandle(mhPipe));
		mhPipe = nullptr;
	}

	if (mhRecvEvent) {
		VDVERIFY(CloseHandle(mhRecvEvent));
		mhRecvEvent = nullptr;
	}

	if (mhSendEvent) {
		VDVERIFY(CloseHandle(mhSendEvent));
		mhSendEvent = nullptr;
	}

	if (mhAttentionEvent) {
		VDVERIFY(CloseHandle(mhAttentionEvent));
		mhAttentionEvent = nullptr;
	}
}

bool ATDevicePipeSerial::CheckPipeName() {
	if (mPipeName.empty())
		return false;

	// trap invalid characters
	// reject names consisting solely of periods
	bool nonPeriod = false;

	for(wchar_t c : mPipeName) {
		if (c < 32)
			return false;

		switch(c) {
			case L'<':
			case L'>':
			case L':':
			case L'"':
			case L'/':
			case L'\\':
			case L'|':
			case L'?':
			case L'*':
				return false;
		}

		if (c != L'.')
			nonPeriod = true;
	}

	if (!nonPeriod)
		return false;
	

	return !mPipeName.empty();
}

void ATDevicePipeSerial::SetPipePath() {
	mPipePath = VDStringW(L"\\\\.\\pipe\\") + mPipeName;
}

void ATDevicePipeSerial::UpdateStatus() {
	Status status = Status::NotConnected;

	if (!mError.empty())
		status = Status::Error;
	else if (mhPipe && mbLastConnected)
		status = Status::Connected;

	if (mStatus != status) {
		mStatus = status;

		NotifyStatusChanged();
	}
}

///////////////////////////////////////////////////////////////////////////

void ATCreateDevicePipeSerial(const ATPropertySet& pset, IATDevice **dev) {
	vdrefptr<ATDevicePipeSerial> p(new ATDevicePipeSerial);

	*dev = p;
	(*dev)->AddRef();
}
