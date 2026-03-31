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
#include <vd2/system/time.h>
#include <vd2/VDDisplay/display.h>
#include <at/atnativeui/debug_win32.h>
#include "tracenative.h"

vdrefptr<IVDRefCount> ATCreateNativeTracer(ATTraceContext& context, const ATNativeTraceSettings& settings) {
	vdrefptr<ATNativeTracer> tracer(new ATNativeTracer(context, settings));

	return tracer;
}

////////////////////////////////////////////////////////////////////////////////

ATNativeTracer::ATNativeTracer(ATTraceContext& traceContext, const ATNativeTraceSettings& settings) {
	mpFrameTraceGroup = traceContext.mpCollection->AddGroup(L"Frames", kATTraceGroupType_Frames);
	mpFrameTraceChannel = mpFrameTraceGroup->AddSimpleChannel(traceContext.mBaseTime, traceContext.mBaseTickScale, L"Frames");

	mpCpuTraceGroup = traceContext.mpCollection->AddGroup(L"CPU main thread", kATTraceGroupType_Normal);

	mpCpuMainChannels[0] = new ATTraceChannelStringTable(traceContext.mBaseTime, traceContext.mBaseTickScale, L"Sim");
	mpCpuMainChannels[0]->AddString(L"Sim");

	mpCpuMainChannels[1] = new ATTraceChannelStringTable(traceContext.mBaseTime, traceContext.mBaseTickScale, L"Native Msg");
	mpCpuMainChannels[1]->AddString(L"Msg");

	mpCpuWindowMsgChannel = new ATTraceChannelFormatted(traceContext.mBaseTime, traceContext.mBaseTickScale, L"Window Msg");

	mpCpuDisplayPostChannel = new ATTraceChannelFormatted(traceContext.mBaseTime, traceContext.mBaseTickScale, L"Display Post");
	mpCpuDisplayPresentChannel = new ATTraceChannelFormatted(traceContext.mBaseTime, traceContext.mBaseTickScale, L"Display Present");

	mpCpuTraceGroup->AddChannel(mpCpuMainChannels[0]);
	mpCpuTraceGroup->AddChannel(mpCpuMainChannels[1]);
	mpCpuTraceGroup->AddChannel(mpCpuWindowMsgChannel);
	mpCpuTraceGroup->AddChannel(mpCpuDisplayPostChannel);
	mpCpuTraceGroup->AddChannel(mpCpuDisplayPresentChannel);

	g_pATProfiler = this;
}

ATNativeTracer::~ATNativeTracer() {
	mpFrameTraceChannel = nullptr;
	mpCpuWindowMsgChannel = nullptr;
	mpCpuDisplayPostChannel = nullptr;
	mpCpuDisplayPresentChannel = nullptr;

	for(auto& p : mpCpuMainChannels)
		p = nullptr;

	mpCpuTraceGroup = nullptr;

	g_pATProfiler = nullptr;
}

void ATNativeTracer::OnEvent(ATProfileEvent event) {
}

void ATNativeTracer::OnEventWithArg(ATProfileEvent event, uintptr arg) {
	if (event == kATProfileEvent_DisplayVSync) {
		const VDDVSyncProfileInfo& vsinfo = *(const VDDVSyncProfileInfo *)arg;

		const uint32 numRefreshes = vsinfo.mRefreshCounts[1] - vsinfo.mRefreshCounts[0];
		const uint64 qpcBase = vsinfo.mQpcTimes[0];
		const uint64 qpcDelta = vsinfo.mQpcTimes[1] - qpcBase;

		for(uint32 i = 0; i < numRefreshes; ++i) {
			const uint64 t1 = qpcBase + (qpcDelta * i + (numRefreshes >> 1)) / numRefreshes;
			const uint64 t2 = qpcBase + (qpcDelta * (i + 1) + (numRefreshes >> 1)) / numRefreshes;

			mpFrameTraceChannel->AddTickEvent(t1, t2, L"Frame", 0x808080);
		}
	}
}

void ATNativeTracer::BeginRegion(ATProfileRegion region) {
	switch(region) {
		case kATProfileRegion_Simulation:
		case kATProfileRegion_NativeEvents:
			mMainThreadPendingRegion = region;
			mMainThreadPendingRegionStart = VDGetPreciseTick();
			break;

		default:
			break;
	}
}

void ATNativeTracer::BeginRegionWithArg(ATProfileRegion region, uintptr arg) {
	if (region == kATProfileRegion_NativeMessage) {
		mPendingWindowMessage = (uint32)arg;
		mPendingWindowMessageStart = VDGetPreciseTick();
	} else if (region == kATProfileRegion_DisplayPost) {
		mDisplayPostPendingStart = VDGetPreciseTick();
		mDisplayPostPendingFrame = (uint32)arg;
	} else if (region == kATProfileRegion_DisplayPresent) {
		mDisplayPresentPendingStart = VDGetPreciseTick();
		mDisplayPresentPendingFrame = (uint32)arg;
	} else {
		mMainThreadPendingRegionArg = arg;
		BeginRegion(region);
	}
}

void ATNativeTracer::EndRegion(ATProfileRegion region) {
	if (region == kATProfileRegion_NativeMessage) {
		if (mPendingWindowMessage) {
			const char *s = ATGetNameForWindowMessageW32(mPendingWindowMessage);

			if (s) {
				mpCpuWindowMsgChannel->AddTickEvent(
					mPendingWindowMessageStart,
					VDGetPreciseTick(),
					[msgId = mPendingWindowMessage](VDStringW& str) {
						str = VDTextAToW(ATGetNameForWindowMessageW32(msgId));
					},
					0xE02000
				);
			} else {
				mpCpuWindowMsgChannel->AddTickEventF(mPendingWindowMessageStart, VDGetPreciseTick(), 0xE02000, L"0x%X", mPendingWindowMessage);
			}
		}
	} else if (region == kATProfileRegion_DisplayPost) {
		mpCpuDisplayPostChannel->AddTickEventF(mDisplayPostPendingStart, VDGetPreciseTick(), 0x0040FF, L"Post %u", mDisplayPostPendingFrame);
	} else if (region == kATProfileRegion_DisplayPresent) {
		mpCpuDisplayPresentChannel->AddTickEventF(mDisplayPostPendingStart, VDGetPreciseTick(), 0x0040FF, L"Present %u", mDisplayPresentPendingFrame);
	} else if (region != kATProfileRegion_Idle && mMainThreadPendingRegion == region) {
		uint32 stringIndex = 0;
		uint32 channelIndex = 0;
		uint32 color = 0xA0A0A0;

		switch(region) {
			case kATProfileRegion_Simulation:		stringIndex = 0; channelIndex = 0; color = 0xAAFF66; break;
			case kATProfileRegion_NativeEvents:		stringIndex = 0; channelIndex = 1; color = 0xE02000; break;

			default:
				break;
		}

		mpCpuMainChannels[channelIndex]->AddTickEvent(mMainThreadPendingRegionStart, VDGetPreciseTick(), stringIndex, color);
		mMainThreadPendingRegion = kATProfileRegion_Idle;
	}
}
