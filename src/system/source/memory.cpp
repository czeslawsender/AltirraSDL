//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include <stdafx.h>
#include <malloc.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#if defined(VD_CPU_AMD64) || defined(VD_CPU_X86)
#include <x86intrin.h>
#endif
#endif
#include <vd2/system/atomic.h>
#include <vd2/system/memory.h>
#include <vd2/system/seh.h>
#include <vd2/system/cpuaccel.h>

void *VDAlignedMallocThrow(size_t n, unsigned alignment) {
	void *p = VDAlignedMalloc(n, alignment);

	if (!p)
		throw std::bad_alloc();

	return p;
}

void *VDAlignedMalloc(size_t n, unsigned alignment) vdnoexcept {
#ifdef VD_COMPILER_MSVC
	return _aligned_malloc(n, alignment);
#else
	void *p = malloc(n + sizeof(void *) + alignment - 1);

	if (p) {
		void *alignedp = (void *)(((uintptr)p + sizeof(void *) + alignment - 1) & ~((uintptr)alignment - 1));

		((void **)alignedp)[-1] = p;
		p = alignedp;
	}

	return p;
#endif
}

void VDAlignedFree(void *p) vdnoexcept {
#ifdef VD_COMPILER_MSVC
	_aligned_free(p);
#else
	free(((void **)p)[-1]);
#endif
}

void *VDAlignedVirtualAlloc(size_t n) {
#ifdef _WIN32
	return VirtualAlloc(NULL, n, MEM_COMMIT, PAGE_READWRITE);
#else
	// Store size before the allocation so munmap can retrieve it
	size_t total = n + sizeof(size_t) + 4095;
	total &= ~(size_t)4095;
	void *raw = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (raw == MAP_FAILED)
		return nullptr;
	*(size_t *)raw = total;
	return (char *)raw + 4096; // page-aligned user pointer
#endif
}

void VDAlignedVirtualFree(void *p) {
#ifdef _WIN32
	VirtualFree(p, 0, MEM_RELEASE);
#else
	if (!p) return;
	void *raw = (char *)p - 4096;
	size_t total = *(size_t *)raw;
	munmap(raw, total);
#endif
}

void __cdecl VDSwapMemoryScalar(void *p0, void *p1, size_t bytes) {
	uint32 *dst0 = (uint32 *)p0;
	uint32 *dst1 = (uint32 *)p1;

	while(bytes >= 4) {
		uint32 a = *dst0;
		uint32 b = *dst1;

		*dst0++ = b;
		*dst1++ = a;

		bytes -= 4;
	}

	char *dstb0 = (char *)dst0;
	char *dstb1 = (char *)dst1;

	while(bytes--) {
		char a = *dstb0;
		char b = *dstb1;

		*dstb0++ = b;
		*dstb1++ = a;
	}
}

#if defined(VD_CPU_AMD64) || defined(VD_CPU_X86)
	void __cdecl VDSwapMemorySSE(void *p0, void *p1, size_t bytes) {
		if (((uint32)(size_t)p0 | (uint32)(size_t)p1) & 15)
			return VDSwapMemoryScalar(p0, p1, bytes);

		__m128 *pv0 = (__m128 *)p0;
		__m128 *pv1 = (__m128 *)p1;

		size_t veccount = bytes >> 4;
		if (veccount) {
			do {
				__m128 v0 = *pv0;
				__m128 v1 = *pv1;

				*pv0++ = v1;
				*pv1++ = v0;
			} while(--veccount);
		}

		uint32 left = bytes & 15;
		if (left) {
			uint8 *pb0 = (uint8 *)pv0;
			uint8 *pb1 = (uint8 *)pv1;
			do {
				uint8 b0 = *pb0;
				uint8 b1 = *pb1;

				*pb0++ = b1;
				*pb1++ = b0;
			} while(--left);
		}
	}
#endif

void (__cdecl *VDSwapMemory)(void *p0, void *p1, size_t bytes) = VDSwapMemoryScalar;

void VDInvertMemory(void *p, unsigned bytes) {
	char *dst = (char *)p;

	if (!bytes)
		return;

	while((intptr_t)dst & 3) {
		*dst = ~*dst;
		++dst;

		if (!--bytes)
			return;
	}

	unsigned lcount = bytes >> 2;

	if (lcount)
		do {
			*(long *)dst = ~*(long *)dst;
			dst += 4;
		} while(--lcount);

	bytes &= 3;

	while(bytes--) {
		*dst = ~*dst;
		++dst;
	}
}

namespace {
	uintptr VDGetSystemPageSize() {
#ifdef _WIN32
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		static uintptr pageSize = sysInfo.dwPageSize;
#else
		static uintptr pageSize = (uintptr)sysconf(_SC_PAGESIZE);
#endif
		return pageSize;
	}
}

bool VDIsValidReadRegion(const void *p0, size_t bytes) {
	if (!bytes)
		return true;

	if (!p0)
		return false;

	uintptr pageSize = VDGetSystemPageSize();
	uintptr p = (uintptr)p0;
	uintptr pLimit = p + (bytes-1);

	vd_seh_guard_try {
		for(;;) {
			*(volatile char *)p;

			if (pLimit - p < pageSize)
				break;

			p += pageSize;
		}
	} vd_seh_guard_except {
		return false;
	}

	return true;
}

bool VDIsValidWriteRegion(void *p0, size_t bytes) {
	if (!bytes)
		return true;

	if (!p0)
		return false;

	// Note: Unlike IsValidWritePtr(), this is threadsafe.

	uintptr pageSize = VDGetSystemPageSize();
	uintptr p = (uintptr)p0;
	uintptr pLimit = p + (bytes-1);
	p &= ~(uintptr)3;

	vd_seh_guard_try {

		for(;;) {
			VDAtomicInt::staticCompareExchange((volatile int *)p, 0xa5, 0xa5);

			if (pLimit - p < pageSize)
				break;

			p += pageSize;
		}
	} vd_seh_guard_except {
		return false;
	}

	return true;
}

bool VDCompareRect(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, size_t w, size_t h) {
	if (!w || !h)
		return false;

	do {
		if (memcmp(dst, src, w))
			return true;

		dst = (char *)dst + dstpitch;
		src = (const char *)src + srcpitch;
	} while(--h);

	return false;
}

const void *VDMemCheck8(const void *src, uint8 value, size_t count) {
	const uint8 *src8 = (const uint8 *)src;

	if (count >= 32) {
#if VD_PTR_SIZE > 4
		const uint64 v64 = (uint64)value * UINT64_C(0x0101010101010101);

		size_t count8 = count >> 3;
		do {
			uint64 testVal;
			memcpy(&testVal, src8, 8);

			if (testVal != v64) [[unlikely]] {
				for(int i=0; i<7; ++i) {
					if (*src8 != value)
						break;

					++src8;
				}
				return src8;
			}

			src8 += 8;
		} while(--count8);

		count &= 7;
#else
		const uint32 v32 = (uint32)value * UINT64_C(0x01010101);

		size_t count4 = count >> 2;
		do {
			uint32 testVal;
			memcpy(&testVal, src8, 4);

			if (testVal != v32) [[unlikely]] {
				for(int i=0; i<3; ++i) {
					if (*src8 != value)
						break;

					++src8;
				}
				return src8;
			}

			src8 += 4;
		} while(--count4);

		count &= 3;
#endif
	}

	if (count) {
		do {
			if (*src8 != value)
				return src8;

			++src8;
		} while(--count);
	}

	return NULL;
}

void VDMemset8(void *dst, uint8 value, size_t count) {
	if (count)
		memset(dst, value, count);
}

void VDMemset16(void *dst, uint16 value, size_t count) {
	if (count) {
		uint16 *dst2 = (uint16 *)dst;

		do {
			*dst2++ = value;
		} while(--count);
	}
}

void VDMemset24(void *dst, uint32 value, size_t count) {
	if (count) {
		uint8 *dst2 = (uint8 *)dst;
		uint8 c0 = (uint8)value;
		uint8 c1 = (uint8)(value >> 8);
		uint8 c2 = (uint8)(value >> 16);

		do {
			*dst2++ = c0;
			*dst2++ = c1;
			*dst2++ = c2;
		} while(--count);
	}
}

void VDMemset32(void *dst, uint32 value, size_t count) {
	if (count) {
		uint32 *dst2 = (uint32 *)dst;

		do {
			*dst2++ = value;
		} while(--count);
	}
}

void VDMemset64(void *dst, uint64 value, size_t count) {
	if (count) {
		uint64 *dst2 = (uint64 *)dst;

		do {
			*dst2++ = value;
		} while(--count);
	}
}

void VDMemset128(void *dst, const void *src0, size_t count) {
	if (count) {
		const uint32 *src = (const uint32 *)src0;
		uint32 a0 = src[0];
		uint32 a1 = src[1];
		uint32 a2 = src[2];
		uint32 a3 = src[3];

		uint32 *dst2 = (uint32 *)dst;

		do {
			dst2[0] = a0;
			dst2[1] = a1;
			dst2[2] = a2;
			dst2[3] = a3;
			dst2 += 4;
		} while(--count);
	}
}

void VDMemsetPointer(void *dst, const void *value, size_t count) {
#if VD_PTR_SIZE == 4
	VDMemset32(dst, (uint32)(size_t)value, count);
#elif VD_PTR_SIZE == 8
	VDMemset64(dst, (uint64)(size_t)value, count);
#else
	#error Unknown pointer size
#endif
}

void VDMemset8Rect(void *dst, ptrdiff_t pitch, uint8 value, size_t w, size_t h) {
	if (w>0 && h>0) {
		do {
			memset(dst, value, w);
			dst = (char *)dst + pitch;
		} while(--h);
	}
}

void VDMemset16Rect(void *dst, ptrdiff_t pitch, uint16 value, size_t w, size_t h) {
	if (w>0 && h>0) {
		do {
			VDMemset16(dst, value, w);
			dst = (char *)dst + pitch;
		} while(--h);
	}
}

void VDMemset24Rect(void *dst, ptrdiff_t pitch, uint32 value, size_t w, size_t h) {
	if (w>0 && h>0) {
		do {
			VDMemset24(dst, value, w);
			dst = (char *)dst + pitch;
		} while(--h);
	}
}

void VDMemset32Rect(void *dst, ptrdiff_t pitch, uint32 value, size_t w, size_t h) {
	if (w>0 && h>0) {
		do {
			VDMemset32(dst, value, w);
			dst = (char *)dst + pitch;
		} while(--h);
	}
}

#if defined(_WIN32) && defined(VD_CPU_X86) && defined(VD_COMPILER_MSVC)
	extern "C" void __cdecl VDFastMemcpyPartialMMX2(void *dst, const void *src, size_t bytes);

	void __cdecl VDFastMemcpyPartialScalar(void *dst, const void *src, size_t bytes) {
		memcpy(dst, src, bytes);
	}

	void __cdecl VDFastMemcpyFinishScalar() {
	}

	void __cdecl VDFastMemcpyFinishMMX2() {
		_mm_empty();
		_mm_sfence();
	}

	void (__cdecl *VDFastMemcpyPartial)(void *dst, const void *src, size_t bytes) = VDFastMemcpyPartialScalar;
	void (__cdecl *VDFastMemcpyFinish)() = VDFastMemcpyFinishScalar;

	void VDFastMemcpyAutodetect() {
		long exts = CPUGetEnabledExtensions();

		if (exts & CPUF_SUPPORTS_SSE) {
			VDFastMemcpyPartial = VDFastMemcpyPartialMMX2;
			VDFastMemcpyFinish	= VDFastMemcpyFinishMMX2;
			VDSwapMemory		= VDSwapMemorySSE;
		} else {
			VDFastMemcpyPartial = VDFastMemcpyPartialScalar;
			VDFastMemcpyFinish	= VDFastMemcpyFinishScalar;
			VDSwapMemory		= VDSwapMemoryScalar;
		}
	}

#else
	void VDFastMemcpyPartial(void *dst, const void *src, size_t bytes) {
		memcpy(dst, src, bytes);
	}

	void VDFastMemcpyFinish() {
	}

	void VDFastMemcpyAutodetect() {
	}
#endif

void VDMemcpyRect(void *dst, ptrdiff_t dststride, const void *src, ptrdiff_t srcstride, size_t w, size_t h) {
	if (w <= 0 || h <= 0)
		return;

	if (w == srcstride && w == dststride)
		VDFastMemcpyPartial(dst, src, w*h);
	else {
		char *dst2 = (char *)dst;
		const char *src2 = (const char *)src;

		do {
			VDFastMemcpyPartial(dst2, src2, w);
			dst2 += dststride;
			src2 += srcstride;
		} while(--h);
	}
	VDFastMemcpyFinish();
}

bool VDMemcpyGuarded(void *dst, const void *src, size_t bytes) {
	vd_seh_guard_try {
		memcpy(dst, src, bytes);
	} vd_seh_guard_except {
		return false;
	}

	return true;
}
