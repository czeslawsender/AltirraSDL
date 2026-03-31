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

#ifndef f_AT_CUSTOMDEVICEVMCLASSES_H
#define f_AT_CUSTOMDEVICEVMCLASSES_H

#include <vd2/system/date.h>
#include <vd2/system/refcount.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <at/atcore/deviceparentimpl.h>
#include <at/atcore/devicevideo.h>
#include <at/atvm/vm.h>

class ATDeviceCustom;
class ATMemoryLayer;
class IATDeviceControllerPort;
class IATAudioSoundGroup;
class IATAudioSampleHandle;
class IATPrinterOutput;

////////////////////////////////////////////////////////////////////////////////

struct ATDeviceCustomSleepInfo {
	uint32 mThreadIndex;
	uint64 mWakeTime;
};

struct ATDeviceCustomSleepInfoPred {
	bool operator()(const ATDeviceCustomSleepInfo& x, const ATDeviceCustomSleepInfo& y) {
		if (x.mWakeTime != y.mWakeTime)
			return x.mWakeTime > y.mWakeTime;

		return x.mThreadIndex > y.mThreadIndex;
	}
};

struct ATDeviceCustomDomain final : public ATVMDomain {
	ATDeviceCustom *mpParent;
};

////////////////////////////////////////////////////////////////////////////////

class ATDeviceCustomObject : public ATVMObject {
public:
	virtual ~ATDeviceCustomObject() = default;
};

struct ATDeviceCustomScriptThread final : public ATVMObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustom *mpParent = nullptr;
	ATVMThread mVMThread;

	sint32 VMCallIsRunning();
	void VMCallRun(const ATVMFunction *function);
	void VMCallInterrupt();
	static void VMCallSleep(sint32 cycles, ATVMDomain& domain);
	void VMCallJoin(ATVMDomain& domain);
};

struct ATDeviceCustomScriptCondition final : public ATDeviceCustomObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustom *mpParent = nullptr;
	ATVMThreadWaitQueue mWaitQueue;

	void VMCallWait(ATVMDomain& domain);
	void VMCallSignal();
	void VMCallBroadcast();
};

struct ATDeviceCustomSegment final : public ATVMObject {
	static const ATVMObjectClass kVMObjectClass;

	const char *mpName = nullptr;
	uint8 *mpData = nullptr;
	uint8 *mpInitData = nullptr;
	uint32 mSize = 0;
	uint32 mInitSize = 0;
	bool mbNonVolatile = false;
	bool mbMappable = false;
	bool mbReadOnly = false;
	bool mbSpecial = false;

	sint32 VMCallGetLength();
	void VMCallClear(sint32 value);
	void VMCallFill(sint32 offset, sint32 value, sint32 size);
	void VMCallXorConst(sint32 offset, sint32 value, sint32 size);
	void VMCallReverseBits(sint32 offset, sint32 size);
	void VMCallTranslate(sint32 destOffset, ATDeviceCustomSegment *srcSegment, sint32 srcOffset, sint32 size, ATDeviceCustomSegment *translateTable, sint32 translateOffset);
	void VMCallCopy(sint32 destOffset, ATDeviceCustomSegment *srcSegment, sint32 srcOffset, sint32 size);
	void VMCallCopyRect(sint32 destOffset, sint32 dstSkip, ATDeviceCustomSegment *srcSegment, sint32 srcOffset, sint32 srcPitch, sint32 width, sint32 height);
	void VMCallWriteByte(sint32 offset, sint32 value);
	sint32 VMCallReadByte(sint32 offset);
	void VMCallWriteWord(sint32 offset, sint32 value);
	sint32 VMCallReadWord(sint32 offset);
	void VMCallWriteRevWord(sint32 offset, sint32 value);
	sint32 VMCallReadRevWord(sint32 offset);
};

enum class ATDeviceCustomAddressAction : uint8 {
	None,
	ConstantData,
	Block,
	Network,
	Script,
	Variable
};

struct ATDeviceCustomAddressBinding {
	ATDeviceCustomAddressAction mAction = ATDeviceCustomAddressAction::None;

	union {
		uint8 mByteData;
		uint16 mScriptFunctions[2];
		uint32 mVariableIndex;
	};
};

struct ATDeviceCustomMemoryLayer final : public ATVMObject {
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustom *mpParent = nullptr;
	uint32 mId = 0;
	const char *mpName = nullptr;
	ATMemoryLayer *mpPhysLayer = nullptr;

	uint32 mAddressBase = 0;
	uint32 mSize = 0;
	ATDeviceCustomAddressBinding *mpReadBindings = nullptr;
	ATDeviceCustomAddressBinding *mpWriteBindings = nullptr;
	ATDeviceCustomSegment *mpSegment = nullptr;
	uint32 mSegmentOffset = 0;
	uint32 mMaxOffset = 0;

	bool mbAutoRD4 = false;
	bool mbAutoRD5 = false;
	bool mbAutoCCTL = false;
	bool mbAutoPBI = false;
	bool mbAutoOS = false;
	bool mbAutoBASIC = false;
	bool mbAutoSelfTest = false;
	bool mbRD5Active = false;
	bool mbIsWriteThrough = false;

	uint8 mEnabledModes = 0;

	void VMCallSetOffset(sint32 offset);
	void VMCallSetSegmentAndOffset(ATDeviceCustomSegment *seg, sint32 offset);
	void VMCallSetModes(sint32 read, sint32 write);
	void VMCallSetReadOnly(sint32 ro);
	void VMCallSetBaseAddress(sint32 baseAddr);
};

struct ATDeviceCustomNetwork final : public ATVMObject {
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustom *mpParent = nullptr;

	sint32 VMCallSendMessage(sint32 param1, sint32 param2);
	sint32 VMCallPostMessage(sint32 param1, sint32 param2);
};

struct ATDeviceCustomSIO final : public ATVMObject {
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustom *mpParent = nullptr;
	bool mbValid = false;
	bool mbDataFrameReceived = false;
	uint32 mSendChecksum = 0;
	uint32 mRecvChecksum = 0;
	uint8 mRecvLast = 0;

	void Reset();
	void VMCallAck();
	void VMCallNak();
	void VMCallError();
	void VMCallComplete();
	void VMCallSendFrame(ATDeviceCustomSegment *seg, sint32 offset, sint32 length);
	void VMCallRecvFrame(sint32 length);
	void VMCallDelay(sint32 cycles);
	void VMCallEnableRaw(sint32 enable);
	void VMCallSetProceed(sint32 asserted);
	void VMCallSetInterrupt(sint32 asserted);
	sint32 VMCallCommandAsserted();
	sint32 VMCallMotorAsserted();
	void VMCallSendRawByte(sint32 c, sint32 cyclesPerBit, ATVMDomain& domain);
	sint32 VMCallRecvRawByte(ATVMDomain& domain);
	sint32 VMCallWaitCommand(ATVMDomain& domain);
	sint32 VMCallWaitCommandOff(ATVMDomain& domain);
	sint32 VMCallWaitMotorChanged(ATVMDomain& domain);
	void VMCallResetRecvChecksum();
	void VMCallResetSendChecksum();
	sint32 VMCallGetRecvChecksum();
	sint32 VMCallCheckRecvChecksum();
	sint32 VMCallGetSendChecksum();
	void VMCallSetExternalClock(sint32 initialOffset, sint32 period);
};

struct ATDeviceCustomRawSendInfo {
	sint32 mThreadIndex;
	uint32 mCyclesPerBit;
	uint8 mByte;
};

struct ATDeviceCustomSIOCommand {
	ATDeviceCustomSegment *mpAutoTransferSegment = nullptr;
	const void *mpAutoTransferBlob = nullptr;
	uint32 mAutoTransferOffset = 0;
	uint32 mAutoTransferLength = 0;
	bool mbAutoTransferWrite = false;
	bool mbAllowAccel = false;
	const ATVMFunction *mpScript = nullptr;
};

struct ATDeviceCustomSIODevice final : public ATVMObject {
	static const ATVMObjectClass kVMObjectClass;

	bool mbAllowAccel = false;
	ATDeviceCustomSIOCommand *mpCommands[256] {};
};

enum class ATDeviceCustomSerialFenceId : uint32 {
	AutoReceive = 1,
	ScriptReceive,
	ScriptDelay
};

enum class ATDeviceCustomNetworkCommand : uint8 {
	None,
	DebugReadByte,
	ReadByte,
	WriteByte,
	ColdReset,
	WarmReset,
	Error,
	ScriptEventSend,
	ScriptEventPost,
	Init
};

struct ATDeviceCustomPBIDevice final : public ATVMObject {
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustomPBIDevice(ATDeviceCustom& parent) : mParent(parent) {}

	void VMCallAssertIrq();
	void VMCallNegateIrq();

private:
	ATDeviceCustom& mParent;
};

struct ATDeviceCustomClock final : public ATVMObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustom *mpParent = nullptr;
	uint64 mLocalTimeCaptureTimestamp = 0;
	VDExpandedDate mLocalTime;

	void Reset();

	void VMCallCaptureLocalTime();
	sint32 VMCallLocalYear();
	sint32 VMCallLocalMonth();
	sint32 VMCallLocalDay();
	sint32 VMCallLocalDayOfWeek();
	sint32 VMCallLocalHour();
	sint32 VMCallLocalMinute();
	sint32 VMCallLocalSecond();
};

struct ATDeviceCustomConsole final : public ATVMObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	static void VMCallSetConsoleButtonState(sint32 button, sint32 depressed);
	static void VMCallSetKeyState(sint32 key, sint32 state);
	static void VMCallPushBreak();
};

struct ATDeviceCustomControllerPort final : public ATVMObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	vdrefptr<IATDeviceControllerPort> mpControllerPort;

	void Init();
	void Shutdown();

	void Enable();
	void Disable();

	void ResetPotPositions();

	void VMCallSetPaddleA(sint32 pos);
	void VMCallSetPaddleB(sint32 pos);
	void VMCallSetTrigger(sint32 asserted);
	void VMCallSetDirs(sint32 directionMask);
};

struct ATDeviceCustomDebug final : public ATVMObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	static void VMCallLog(const char *str);
	static void VMCallLogInt(const char *str, sint32 v);
};

class ATDeviceCustomImage final : public ATVMObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	VDPixmapBuffer mPixmapBuffer;

	// The LSB of this counter is a dirty flag while the higher bits are a
	// counter. The counter field is only incremented if the dirty flag is
	// observed and cleared, which prevents unobserved dirties from wrapping
	// the counter.
	uint32 mChangeCounter = 0;

	void Init(sint32 w, sint32 h);

	void VMCallClear(sint32 color);
	sint32 VMCallGetPixel(sint32 x, sint32 y);
	void VMCallPutPixel(sint32 x, sint32 y, sint32 color);
	void VMCallFillRect(sint32 x, sint32 y, sint32 w, sint32 h, sint32 color);
	void VMCallInvertRect(sint32 x, sint32 y, sint32 w, sint32 h);
	void VMCallBlt(sint32 dx, sint32 dy, ATDeviceCustomImage *src, sint32 sx, sint32 sy, sint32 w, sint32 h);
	void VMCallBltExpand1(sint32 dx, sint32 dy, ATDeviceCustomSegment *src, sint32 srcOffset, sint32 srcPitch, sint32 w, sint32 h, sint32 c0, sint32 c1);
	void VMCallBltTileMap(
		sint32 x,
		sint32 y,
		ATDeviceCustomImage *imageSrc,
		sint32 tileWidth,
		sint32 tileHeight,
		ATDeviceCustomSegment *tileSrc,
		sint32 tileOffset,
		sint32 tilePitch,
		sint32 tileMapWidth,
		sint32 tileMapHeight);
};

class ATDeviceCustomVideoOutput final : public ATVMObject, public IATDeviceVideoOutput {
	ATDeviceCustomVideoOutput(const ATDeviceCustomVideoOutput&) = delete;
	ATDeviceCustomVideoOutput& operator=(const ATDeviceCustomVideoOutput&) = delete;
public:
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustomVideoOutput(ATDeviceCustom *parent, IATDeviceVideoManager *vidMgr, const char *tag, const wchar_t *displayName);
	~ATDeviceCustomVideoOutput();

	VDStringA mTag;
	VDStringW mDisplayName;
	VDPixmap mFrameBuffer {};
	ATDeviceVideoInfo mVideoInfo {};

	ATDeviceCustom *mpParent = nullptr;
	IATDeviceVideoManager *mpVideoMgr = nullptr;
	ATDeviceCustomImage *mpFrameBufferImage = nullptr;
	const ATVMFunction *mpOnComposite = nullptr;
	const ATVMFunction *mpOnPreCopy = nullptr;
	const ATDeviceCustomSegment *mpCopySource = nullptr;
	sint32 mCopyOffset = 0;
	sint32 mCopySkip = 0;

	uint32 mActivityCounter = 0;

	void VMCallSetPAR(sint32 numerator, sint32 denominator);
	void VMCallSetImage(ATDeviceCustomImage *image);
	void VMCallMarkActive();
	void VMCallSetPassThrough(sint32 enabled);
	void VMCallSetTextArea(sint32 cols, sint32 rows);
	void VMCallSetCopyTextSource(ATDeviceCustomSegment *seg, sint32 offset, sint32 skip);

public:
	const char *GetName() const override;
	const wchar_t *GetDisplayName() const override;
	void Tick(uint32 hz300ticks) override;
	void UpdateFrame() override;
	const VDPixmap& GetFrameBuffer() override;
	const ATDeviceVideoInfo& GetVideoInfo() override;
	vdpoint32 PixelToCaretPos(const vdpoint32& pixelPos) override;
	vdrect32 CharToPixelRect(const vdrect32& r) override;
	int ReadRawText(uint8 *dst, int x, int y, int n) override;
	uint32 GetActivityCounter() override;
};

class ATDeviceCustomSoundParams final : public ATDeviceCustomObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	void VMCallSetPan(sint32 x, sint32 y);
	void VMCallSetVolume(sint32 x, sint32 y);
	void VMCallSetRate(sint32 x, sint32 y);
	void VMCallSetLooping(int enabled);

	float mPan = 0.0f;
	float mVolume = 1.0f;
	float mRate = 1.0f;
	bool mbLooping = false;
};

class ATDeviceCustomSound final : public ATDeviceCustomObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustomSound(ATDeviceCustom& parent);

	void SetSoundData(vdspan<const sint16> data, float samplingRate);

	void VMCallPlay(const ATDeviceCustomSoundParams *params);
	void VMCallStopAll();

private:
	ATDeviceCustom& mParent;

	vdrefptr<IATAudioSoundGroup> mpSoundGroup;
	vdrefptr<IATAudioSampleHandle> mpAudioSample;
};

////////////////////////////////////////////////////////////////////////////////

class ATDeviceCustomSerialPort final : public ATDeviceCustomObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustomSerialPort(ATDeviceCustom& parent);
	~ATDeviceCustomSerialPort();

	void Shutdown();

	sint32 VMCallRead(sint32 baud);
	sint32 VMCallReadWait(sint32 baud, ATVMDomain& domain);
	void VMCallWrite(sint32 byte, sint32 baud);
	sint32 VMCallWaitError(ATVMDomain& domain);
	sint32 VMCallBaudToCyclesPerByte(sint32 baud);

private:
	enum class ErrorType : uint8 {
		Framing = 1
	};

	void RaiseError(ErrorType errorType);
	void OnDeviceAttach();
	void OnDeviceDetach();
	void OnReadReady();

	ATDeviceCustom& mParent;
	ATDeviceBusSingleChild mDeviceBus;

	ATVMThreadWaitQueue mReadQueue;
	ATVMThreadWaitQueue mErrorQueue;

	uint32 mPendingErrorMask = 0;
	sint32 mWaitBaudRate = -1;
};

////////////////////////////////////////////////////////////////////////////////

class ATDeviceCustomPrinterPort final : public ATDeviceCustomObject {
public:
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustomPrinterPort(ATDeviceCustom& parent);
	~ATDeviceCustomPrinterPort();

	void Shutdown();

	void VMCallWriteRaw(sint32 ch);
	void VMCallWriteUnicode(sint32 ch);

private:
	void OnDeviceAttach();
	void OnDeviceDetach();
	IATPrinterOutput *GetPrinterOutput();

	ATDeviceCustom& mParent;
	ATDeviceBusSingleChild mDeviceBus;
	vdrefptr<IATPrinterOutput> mpPrinterOutput;
};

////////////////////////////////////////////////////////////////////////////////

class ATDeviceCustomEmulatorObj final : public vdrefcounted<ATVMObject> {
public:
	static const ATVMObjectClass kVMObjectClass;

	ATDeviceCustomEmulatorObj();

	void Shutdown();

	void VMCallRunCommand(const char *command, ATVMDomain& domain);

private:
	struct UIStep;

	VDStringA mPendingCommand;
	bool mbValid = true;

	vdfunction<void(ATVMThread& thread)> mpCancelFn;
};

#endif
