//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2018 Avery Lee
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

#ifndef f_AT_DEBUGGEREXP_H
#define f_AT_DEBUGGEREXP_H

#include <stdarg.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/error.h>
#include <at/atdebugger/expression.h>
#include <at/atdebugger/target.h>
#include <at/atcpu/execstate.h>

class VDStringA;
class ATCPUEmulator;
class ATCPUEmulatorMemory;
class ATAnticEmulator;
class ATMMUEmulator;
class IATDebuggerSymbolLookup;

struct ATDebugExpEvalContext {
	IATDebugTarget *mpTarget = nullptr;
	ATAnticEmulator *mpAntic = nullptr;
	ATMMUEmulator *mpMMU = nullptr;
	const sint32 *mpTemporaries = nullptr;

	uint32 (*mpClockFn)(void *p) = nullptr;
	void *mpClockFnData = nullptr;

	uint32 (*mpCpuClockFn)(void *p) = nullptr;
	void *mpCpuClockFnData = nullptr;

	uint32 (*mpXPCFn)(void *p) = nullptr;
	void *mpXPCFnData = nullptr;

	uint32 (*mpTapePosFn)(void *p) = nullptr;
	void *mpTapePosData = nullptr;

	sint32 (*mpHwWriteRegFn)(void *p, sint32 addr) = nullptr;
	void *mpHwWriteRegFnData = nullptr;

	bool mbAccessValid {};
	bool mbAccessReadValid {};
	bool mbAccessWriteValid {};
	sint32 mAccessAddress {};
	uint8 mAccessValue {};
	const vdfunction<bool(const ATDebugExpEvalContext&, sint32&)> *mpAccessValueFn = nullptr;
};

struct ATDebugExpEvalCache {
	bool mbExecStateValid = false;
	ATDebugDisasmMode mExecMode;
	ATCPUExecState mExecState;

	const ATCPUExecState *GetExecState(const ATDebugExpEvalContext& ctx);
};

struct ATDebuggerExprParseOpts {
	bool mbDefaultHex;
	bool mbAllowUntaggedHex;
};

ATDebugExpNode *ATDebuggerParseExpression(const char *s, IATDebuggerSymbolLookup *dbg, const ATDebuggerExprParseOpts& opts, ATDebugExpEvalContext *immContext = nullptr);
ATDebugExpNode *ATDebuggerInvertExpression(ATDebugExpNode *node);

class ATDebuggerExprParseException : public MyError {
public:
	template<class... Args>
	ATDebuggerExprParseException(Args&&... args)
		: MyError(std::forward<Args>(args)...) {}
};

#endif
