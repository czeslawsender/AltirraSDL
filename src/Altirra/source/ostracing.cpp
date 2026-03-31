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

#ifdef ATNRELEASE
#include <windows.h>
#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>
#include "ostracing.h"

TRACELOGGING_DEFINE_PROVIDER(
	g_hATWinTraceLoggingProvider,
	"Altirra",
	// {E7B2EEAB-1112-4C7C-A96D-B66D5E0E8F01}
	(0xe7b2eeab, 0x1112, 0x4c7c, 0xa9, 0x6d, 0xb6, 0x6d, 0x5e, 0xe, 0x8f, 0x1));

TraceLoggingActivity<g_hATWinTraceLoggingProvider, 0, 4> g_ATOSTracingActivitySimulate;

bool g_ATOSTracingEnabled;

void ATInitOSTracing() {
	if (!g_ATOSTracingEnabled) {
		g_ATOSTracingEnabled = true;

		TraceLoggingRegister(g_hATWinTraceLoggingProvider);
	}
}

void ATShutdownOSTracing() {
	if (g_ATOSTracingEnabled) {
		g_ATOSTracingEnabled = false;

		TraceLoggingUnregister(g_hATWinTraceLoggingProvider);
	}
}

void ATOSTraceSimulateBegin() {
	if (g_ATOSTracingEnabled) {
		TraceLoggingWriteStart(g_ATOSTracingActivitySimulate, "Simulate");
	}
}

void ATOSTraceSimulateEnd() {
	if (g_ATOSTracingEnabled) {
		TraceLoggingWriteStop(g_ATOSTracingActivitySimulate, "Simulate");
	}
}
#else
void ATOSTraceSimulateBegin() {}
void ATOSTraceSimulateEnd() {}
#endif
