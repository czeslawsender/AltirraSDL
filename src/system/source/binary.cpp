//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 2025 Avery Lee, All Rights Reserved.
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
#include <vd2/system/binary.h>

static_assert(VDSwizzleU16(0x1234) == 0x3412);
static_assert(VDSwizzleS16(0x1234) == 0x3412);
static_assert(VDSwizzleU32(0x12345678) == 0x78563412);
static_assert(VDSwizzleS32(0x12345678) == 0x78563412);
static_assert(VDSwizzleU64(0x123456789ABCDEF0) == 0xF0DEBC9A78563412);
static_assert(VDSwizzleS64(0x123456789ABCDEF0) == 0xF0DEBC9A78563412);
