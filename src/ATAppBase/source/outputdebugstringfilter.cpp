//	Altirra - Atari 800/800XL/5200 emulator
//	Application base library - OutputDebugString hook
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
#include <vd2/system/vdtypes.h>

#if defined(WIN32) && defined(ATNRELEASE) && defined(VD_CPU_X64)

#include <windows.h>
#include <intrin.h>
#include <vd2/system/binary.h>
#include <vd2/system/VDString.h>

extern "C" {
	void *g_pATOutputDebugStringWFwd;
}

extern "C" void __stdcall ATPatchedOutputDebugStringW(const wchar_t *str);
extern "C" void __cdecl ATPatchedOutputDebugStringWFwd1(const wchar_t *str);

void ATSetOutputDebugStringFilter() {
	HMODULE kernel32 = GetModuleHandleW(L"kernelbase");
	FARPROC fp = GetProcAddress(kernel32, "OutputDebugStringW");

	if (!fp)
		return;

	// Hook OutputDebugStringW() because Microsoft can't be bothered to clean
	// up the debug spew emitted by release build Windows 11 DLLs.
	//
	// For now, we're lazy and just look for the current pattern in Windows
	// 11 x64:
	//
	//	12 x int3
	//	48 8B C4  mov rax,rsp
	//
	// We need to replace it with:
	//
	//	48 B8 xxxxxxxx xxxxxxxx		movabs rax, addr
	//	FF E0						jmp rax
	//	EB F2						jmp *-14

	MEMORY_BASIC_INFORMATION mbi {};
	if (!VirtualQuery(fp, &mbi, sizeof mbi))
		return;

	// check that the region is in executable image code
	if (!(mbi.State & MEM_COMMIT))
		return;

	if (!(mbi.AllocationProtect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
		return;

	if (mbi.Type != MEM_IMAGE)
		return;

	// check that we have 12 bytes before and 3 bytes after the entry point
	// within the same memory region
	const uintptr_t fpAddr = (uintptr_t)fp;
	const uintptr_t regionBase = (uintptr_t)mbi.BaseAddress;

	if (fpAddr < regionBase)
		return;
	
	const uint32_t regionOffset = fpAddr - regionBase;
	if (regionOffset < 12)
		return;

	if (regionOffset >= mbi.RegionSize || mbi.RegionSize - regionOffset < 3)
		return;

	// double check that all bytes are as we expect
	static constexpr uint8 kCheckBytes[] {
		0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
		0x48, 0x8B, 0xC4
	};

	if (!memcmp(fp, kCheckBytes, 15))
		return;

	// change protection
	char *patchBase = (char *)fp - 12;
	DWORD oldProtect = 0;

	if (!VirtualProtect(patchBase, 15, PAGE_EXECUTE_READWRITE, &oldProtect))
		return;

	// cache the forwarding pointer
	g_pATOutputDebugStringWFwd = patchBase + 15;

	// write the patch instructions
	VDWriteUnalignedLEU16(patchBase, 0xB848);
	VDWriteUnalignedLEU64(patchBase+2, (uint64)ATPatchedOutputDebugStringW);
	VDWriteUnalignedLEU16(patchBase+10, 0xE0FF);

	// interlocked write the final JMP instruction
	_InterlockedExchange16((volatile short *)(patchBase + 12), (short)0xF2EB);

	// restore protection
	DWORD dummy = 0;
	VirtualProtect(patchBase, 15, oldProtect, &dummy);
}

extern "C" void __stdcall ATPatchedOutputDebugStringW(const wchar_t *str) {
	struct Prefix {
		constexpr Prefix(const wchar_t *s) : mpStr(s) {
			mLen = 0;

			while(s[mLen])
				++mLen;

			mLen *= sizeof(wchar_t);
		}

		size_t mLen = 0;
		const wchar_t *mpStr = nullptr;
	};

	// We are looking to reject strings like this:
	//
	// onecore\windows\directx\database\helperlibrary\lib\directxdatabasequeryimpl.cpp(291)\directxdatabasehelper.dll!00007FFD2934A12D: (caller: 00007FFD29349F8F) ReturnHr(1) tid(207c) 80070490 Element not found.

	static constexpr Prefix kPrefixes[] {
		L"onecore",
		L"shell\\",
		L"mincore\\",
	};

	bool found = false;

	for(const Prefix& prefix : kPrefixes) {
		if (!memcmp(str, prefix.mpStr, prefix.mLen)) {
			found = true;
			break;
		}
	}

	if (found) {
		const wchar_t *check = wcsstr(str, L": (caller: ");

		if (check) {
			if (wcsstr(check, L"ReturnHr("))
				return;

			if (wcsstr(check, L"LogHr("))
				return;
		}
	}

	ATPatchedOutputDebugStringWFwd1(str);
}

#else

void ATSetOutputDebugStringFilter() {
}

#endif
