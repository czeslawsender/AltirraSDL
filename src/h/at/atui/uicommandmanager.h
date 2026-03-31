//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2026 Avery Lee
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

#ifndef f_AT_ATUI_UICOMMANDMANAGER_H
#define f_AT_ATUI_UICOMMANDMANAGER_H

#include <vd2/system/linearalloc.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/vdstl_hashmap.h>
#include <vd2/system/vdstl_vectorview.h>
#include <at/atcore/notifylist.h>

struct VDAccelToCommandEntry;
class VDStringW;
struct ATUICommand;

enum ATUICmdState {
	kATUICmdState_None,
	kATUICmdState_Checked,
	kATUICmdState_RadioChecked
};

struct ATUICommandBaseOptions {
	// Request that a command be run without confirmation or notification
	// dialogs.
	bool mbQuiet = false;

	// Invoked when a command completes, asynchronously or not. If the command
	// completes immediately, this will be invoked _during_ the ExecuteCommand
	// call. Cmd/args are empty if the command is cancelled or throws an error.
	vdfunction<void(const ATUICommand *cmd, vdspan<const VDStringW> args)> mpOnCommandExecuted;
};

struct ATUICommandOptions final : public ATUICommandBaseOptions {
	vdfastvector<VDStringSpanW> mInArgs;
};

class ATUICommandContext final : public ATUICommandBaseOptions, public vdrefcount {
public:
	bool GetArg(size_t index, VDStringW& s) const;
	void SetArg(size_t index, const wchar_t *s);

	// Mark a command as completed. This is only necessary for deferred commands;
	// if the command returns without capturing the context or throwing, the
	// command is implicitly marked as completed. If a reference is taken on
	// the context at return time, then this must be called.
	void MarkCompleted(bool succeeded);

private:
	friend class ATUICommandManager;

	ATUICommandManager *mpParent = nullptr;
	const ATUICommand *mpCommand = nullptr;
	bool mbRecordArgs = false;
	bool mbCommandCompleted = false;
	bool mbCommandCancelled = false;
	vdvector<VDStringW> mInArgs;
	vdvector<VDStringW> mOutArgs;
};

typedef void (*ATUICmdExecuteFnBase)();
typedef void (*ATUICmdExecuteFnCtx)(ATUICommandContext&);
typedef bool (*ATUICmdTestFn)();
typedef ATUICmdState (*ATUICmdStateFn)();
typedef void (*ATUICmdFormatFn)(VDStringW&);

struct ATUICommandExecuteFn {
	ATUICommandExecuteFn() = default;

	template<typename T> requires std::is_convertible_v<T, ATUICmdExecuteFnBase>
	constexpr ATUICommandExecuteFn(T&& fn) : mpBase(fn), mbHasCtx(false) {}

	template<typename T> requires std::is_convertible_v<T, ATUICmdExecuteFnCtx>
	constexpr ATUICommandExecuteFn(T&& fn) : mpCtx(fn), mbHasCtx(true) {}

	void operator()(ATUICommandContext& ctx) const {
		if (mbHasCtx)
			mpCtx(ctx);
		else
			mpBase();
	}

	union {
		ATUICmdExecuteFnBase mpBase {};
		ATUICmdExecuteFnCtx mpCtx;
	};

	bool mbHasCtx = false;
};

struct ATUICommand {
	const char *mpName;
	ATUICommandExecuteFn mpExecuteFn;
	ATUICmdTestFn mpTestFn;
	ATUICmdStateFn mpStateFn;
	ATUICmdFormatFn mpFormatFn;
};

class ATUICommandManager {
	ATUICommandManager(const ATUICommandManager&);
	ATUICommandManager& operator=(const ATUICommandManager&);
public:
	ATUICommandManager();
	~ATUICommandManager();

	void RegisterCommand(const ATUICommand *cmd);
	void RegisterCommands(const ATUICommand *cmd, size_t n);

	const ATUICommand *GetCommand(const char *str) const;
	bool ExecuteCommand(const char *str, const ATUICommandOptions& ctx = ATUICommandOptions());
	bool ExecuteCommand(const ATUICommand& cmd, const ATUICommandOptions& ctx = ATUICommandOptions());
	bool ExecuteCommandNT(const char *str, const ATUICommandOptions& ctx = ATUICommandOptions()) noexcept;
	bool ExecuteCommandNT(const ATUICommand& cmd, const ATUICommandOptions& ctx = ATUICommandOptions()) noexcept;
	
	void ListCommands(vdfastvector<VDAccelToCommandEntry>& commands) const;

private:
	friend class ATUICommandContext;

	struct Node {
		Node *mpNext;
		uint32 mHash;
		const ATUICommand *mpCmd;
	};

	VDLinearAllocator mAllocator;

	vdrefptr<ATUICommandContext> mpCommandContext;

	enum { kHashTableSize = 257 };
	Node *mpHashTable[kHashTableSize];
};

#endif
