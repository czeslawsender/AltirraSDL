// Compatibility shim: intercepts bare #include <intrin.h> on non-MSVC compilers
#ifndef _COMPAT_INTRIN_H
#define _COMPAT_INTRIN_H

#if defined(__x86_64__) || defined(__i386__)
	#include <x86intrin.h>
	#include <cpuid.h>

	// GCC defines __cpuid(level, eax, ebx, ecx, edx) with 5 params.
	// MSVC uses __cpuid(int[4], level) with array + level.
	// Override to MSVC array-style so existing code compiles unchanged.
	#undef __cpuid
	#define __cpuid(info, level) \
		__cpuid_count((level), 0, (info)[0], (info)[1], (info)[2], (info)[3])
	#undef __cpuidex
	#define __cpuidex(info, level, count) \
		__cpuid_count((level), (count), (info)[0], (info)[1], (info)[2], (info)[3])
#elif defined(__aarch64__)
	#include <arm_neon.h>
#endif

#endif
