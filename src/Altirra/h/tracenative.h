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

#ifndef f_AT_TRACENATIVE_H
#define f_AT_TRACENATIVE_H

#include <vd2/system/refcount.h>
#include <at/atcore/profile.h>
#include "trace.h"

class ATNativeTracer final : public vdrefcounted<IVDRefCount>, public IATProfiler {
public:
	ATNativeTracer(ATTraceContext& traceContext, const ATNativeTraceSettings& settings);
	~ATNativeTracer();

	void OnEvent(ATProfileEvent event) override;
	void OnEventWithArg(ATProfileEvent event, uintptr arg) override;
	void BeginRegion(ATProfileRegion region) override;
	void BeginRegionWithArg(ATProfileRegion region, uintptr arg) override;
	void EndRegion(ATProfileRegion region) override;

private:
	ATTraceGroup *mpFrameTraceGroup = nullptr;
	ATTraceGroup *mpCpuTraceGroup = nullptr;
	vdrefptr<ATTraceChannelSimple> mpFrameTraceChannel;
	vdrefptr<ATTraceChannelStringTable> mpCpuMainChannels[3];
	vdrefptr<ATTraceChannelFormatted> mpCpuWindowMsgChannel;
	vdrefptr<ATTraceChannelFormatted> mpCpuDisplayPostChannel;
	vdrefptr<ATTraceChannelFormatted> mpCpuDisplayPresentChannel;

	ATProfileRegion mMainThreadPendingRegion = kATProfileRegion_Idle;
	uint64 mMainThreadPendingRegionStart = 0;
	uintptr mMainThreadPendingRegionArg = 0;
	uint64 mPendingWindowMessageStart = 0;
	uint32 mPendingWindowMessage = 0;

	uint64 mDisplayPostPendingStart = 0;
	uint32 mDisplayPostPendingFrame = 0;
	uint64 mDisplayPresentPendingStart = 0;
	uint32 mDisplayPresentPendingFrame = 0;
};

#endif
