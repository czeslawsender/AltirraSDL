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
#include "console.h"
#include "trace.h"

class ATTraceLogger final : public vdrefcounted<IVDRefCount> {
public:
	ATTraceLogger(ATTraceContext& traceContext);
	~ATTraceLogger();

private:
	void Log(const char *msg, uint64 timestamp);

	vdrefptr<ATTraceChannelStringTable> mpTraceChannel;
};

ATTraceLogger::ATTraceLogger(ATTraceContext& traceContext) {
	ATTraceGroup *group = traceContext.mpCollection->AddGroup(L"Log", kATTraceGroupType_Log);

	mpTraceChannel = new ATTraceChannelStringTable(traceContext.mBaseTime, traceContext.mBaseTickScale, L"Log"); 
	group->AddChannel(mpTraceChannel);

	ATConsoleSetTraceLogger(std::bind_front(&ATTraceLogger::Log, this));
}

ATTraceLogger::~ATTraceLogger() {
	ATConsoleSetTraceLogger(nullptr);
}

void ATTraceLogger::Log(const char *msg, uint64 timestamp) {
	VDStringW s = VDTextU8ToW(VDStringSpanA(msg));

	if (!s.empty() && s.back() == L'\n')
		s.pop_back();

	const auto stringId = mpTraceChannel->AddString(std::move(s));

	mpTraceChannel->AddTickEvent(timestamp, timestamp, stringId, kATTraceColor_Default);
}

////////////////////////////////////////////////////////////////////////////////

vdrefptr<IVDRefCount> ATCreateTraceLogger(ATTraceContext& traceContext) {
	return vdrefptr(new ATTraceLogger(traceContext));
}

