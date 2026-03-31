//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2024-2025 Avery Lee
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
#include <bit>
#include <vd2/system/math.h>
#include <test.h>

namespace {
	template<typename T_Src, typename T_Dst, typename T_Fn>
	void TestFunction(T_Fn&& function, const char *name, std::initializer_list<std::pair<T_Src, T_Dst>> testVectors) {
		for(const auto& testVector : testVectors) {
			const auto r = function(testVector.first);

			if constexpr (std::is_same_v<T_Src, float>) {
				AT_TEST_TRACEF("Testing %s: %08X (%.9g) -> %.17g", name, std::bit_cast<uint32>(testVector.first), testVector.first, (double)r);
			} else if constexpr (std::is_same_v<T_Src, double>) {
				AT_TEST_TRACEF("Testing %s: %016llX (%.9g) -> %.17g", name, std::bit_cast<uint64>(testVector.first), testVector.first, (double)r);
			}

			AT_TEST_ASSERTF(r == testVector.second, "Test vector %s failed for value: %.17g", name, (double)testVector.first);
		}
	}
}

DEFINE_TEST(System_Math) {
	TestFunction<float, int>(static_cast<int(*)(float)>(VDRoundToInt), "VDRoundToInt(float)", {
		{ 0.00f, 0 },
		{ 0.45f, 0 },
		{ 0.55f, 1 },
		{ 1.00f, 1 },
		{ 1.45f, 1 },
		{ 1.55f, 2 },
		{ 2.00f, 2 },
		{ -0.45f, 0 },
		{ -0.55f, -1 },
		{ -1.00f, -1 },
		{ -1.45f, -1 },
		{ -1.55f, -2 },
		{ -2.0f, -2 },
		{ 16777216.0f, 16777216 },
		{ -16777216.0f, -16777216 },
		{ 0x1.0p30f, (int)(1 << 30) },
		{ -0x1.0p30f, -(int)(1 << 30) },
	});

	TestFunction<double, int>(static_cast<int(*)(double)>(VDRoundToInt), "VDRoundToInt(double)", {
		{ 0.00, 0 },
		{ 0.45, 0 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 1 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, -1 },
		{ -1.00, -1 },
		{ -1.45, -1 },
		{ -1.55, -2 },
		{ -2.00, -2 },
		{ 16777216.0, 16777216 },
		{ -16777216.0, -16777216 },
		{ 0x1.2345678p28, 0x12345678 },
		{ -0x1.2345678p28, -0x12345678 },
	});

	TestFunction<float, sint32>(static_cast<sint32(*)(float)>(VDRoundToIntFast), "VDRoundToIntFast(float)", {
		{ 0.00f, 0 },
		{ 0.45f, 0 },
		{ 0.55f, 1 },
		{ 1.00f, 1 },
		{ 1.45f, 1 },
		{ 1.55f, 2 },
		{ 2.00f, 2 },
		{ -0.45f, 0 },
		{ -0.55f, -1 },
		{ -1.00f, -1 },
		{ -1.45f, -1 },
		{ -1.55f, -2 },
		{ -2.0f, -2 },
		{ 4194303, 4194303 },
		{ -4194303.0f, -4194303 },
	});

	TestFunction<double, sint32>(static_cast<sint32(*)(double)>(VDRoundToIntFastFullRange), "VDRoundToIntFastFullRange(double)", {
		{ 0.00, 0 },
		{ 0.45, 0 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 1 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, -1 },
		{ -1.00, -1 },
		{ -1.45, -1 },
		{ -1.55, -2 },
		{ -2.00, -2 },
		{ 16777216.0, 16777216 },
		{ -16777216.0, -16777216 },
		{ 0x1.2345678p28, 0x12345678 },
		{ -0x1.2345678p28, -0x12345678 },
	});

	TestFunction<float, sint32>(static_cast<sint32(*)(float)>(VDRoundToInt32), "VDRoundToInt32(float)", {
		{ 0.00f, 0 },
		{ 0.45f, 0 },
		{ 0.55f, 1 },
		{ 1.00f, 1 },
		{ 1.45f, 1 },
		{ 1.55f, 2 },
		{ 2.00f, 2 },
		{ -0.45f, 0 },
		{ -0.55f, -1 },
		{ -1.00f, -1 },
		{ -1.45f, -1 },
		{ -1.55f, -2 },
		{ -2.0f, -2 },
		{ 8388607.0f, 8388607 },
		{ -8388607.0f, -8388607 },
		{ 0x1.0p30f, (int)(1 << 30) },
		{ -0x1.0p30f, -(int)(1 << 30) },
		});

	TestFunction<double, sint32>(static_cast<sint32(*)(double)>(VDRoundToInt32), "VDRoundToInt(double)", {
		{ 0.00, 0 },
		{ 0.45, 0 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 1 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, -1 },
		{ -1.00, -1 },
		{ -1.45, -1 },
		{ -1.55, -2 },
		{ -2.00, -2 },
		{ 16777216.0, 16777216 },
		{ -16777216.0, -16777216 },
		{ 0x1.2345678p28, 0x12345678 },
		{ -0x1.2345678p28, -0x12345678 },
		});

	TestFunction<float, sint64>(static_cast<sint64(*)(float)>(VDRoundToInt64), "VDRoundToInt64(float)", {
		{ 0.00f, 0 },
		{ 0.45f, 0 },
		{ 0.55f, 1 },
		{ 1.00f, 1 },
		{ 1.45f, 1 },
		{ 1.55f, 2 },
		{ 2.00f, 2 },
		{ -0.45f, 0 },
		{ -0.55f, -1 },
		{ -1.00f, -1 },
		{ -1.45f, -1 },
		{ -1.55f, -2 },
		{ -2.0f, -2 },
		{ 16777216.0f, 16777216 },
		{ -16777216.0f, -16777216 },
		{ 0x1.0p48f, ((sint64)1 << 48) },
		{ -0x1.0p48f, -((sint64)1 << 48) },
		});

	TestFunction<double, sint64>(static_cast<sint64(*)(double)>(VDRoundToInt64), "VDRoundToInt64(double)", {
		{ 0.00, 0 },
		{ 0.45, 0 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 1 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, -1 },
		{ -1.00, -1 },
		{ -1.45, -1 },
		{ -1.55, -2 },
		{ -2.00, -2 },
		{ 16777216.0, 16777216 },
		{ -16777216.0, -16777216 },
		{ 0x1.23456789ABCDEp52, INT64_C(0x123456789ABCDE) },
		{ -0x1.23456789ABCDEp52, -INT64_C(0x123456789ABCDE) },
		});

	TestFunction<float, float>(static_cast<float(*)(float)>(VDRoundFast), "VDRoundFast(float)", {
		{ 0.00f, 0.0f },
		{ 0.45f, 0.0f },
		{ 0x1.FFFFFEp-2f, 0.0f },
		{ 0x1.000002p-1f, 1.0f },
		{ 0.55f, 1.0f },
		{ 1.00f, 1.0f },
		{ 1.45f, 1.0f },
		{ 1.55f, 2.0f },
		{ 2.00f, 2.0f },
		{ -0.45f, 0.0f },
		{ -0x1.FFFFFEp-2f, -0.0f },
		{ -0x1.000002p-1f, -1.0f },
		{ -0.55f, -1.0f },
		{ -1.00f, -1.0f },
		{ -1.45f, -1.0f },
		{ -1.55f, -2.0f },
		{ -2.00f, -2.0f },
		{ 4194302.0f, 4194302.0f },
		{ 4194302.25f, 4194302.0f },
		{ 4194302.75f, 4194303.0f },
		{ 4194303.0f, 4194303.0f },
		{ 4194303.25f, 4194303.0f },
		{ 4194303.75f, 4194304.0f },
		{ 4194304.0f, 4194304.0f },
		{ 4194304.25f, 4194304.0f },
		{ 4194304.75f, 4194305.0f },
		{ 4194305.0f, 4194305.0f },
		{ 4194305.25f, 4194305.0f },
		{ 4194305.75f, 4194306.0f },
		{ 4194306.0f, 4194306.0f },
		{ 8388607.0f, 8388607.0f },
		{ 8388608.0f, 8388608.0f },
		{ 8388609.0f, 8388609.0f },
		{ 16777215.0f, 16777215.0f },
		{ 16777216.0f, 16777216.0f },
		{ 16777217.0f, 16777217.0f },
		{ 33554432.0f, 33554432.0f },
		{ -4194302.0f, -4194302.0f },
		{ -4194302.25f, -4194302.0f },
		{ -4194302.75f, -4194303.0f },
		{ -4194303.0f, -4194303.0f },
		{ -4194303.25f, -4194303.0f },
		{ -4194303.75f, -4194304.0f },
		{ -4194304.0f, -4194304.0f },
		{ -4194304.25f, -4194304.0f },
		{ -4194304.75f, -4194305.0f },
		{ -4194305.0f, -4194305.0f },
		{ -4194305.25f, -4194305.0f },
		{ -4194305.75f, -4194306.0f },
		{ -4194306.0f, -4194306.0f },
		{ -8388607.0f, -8388607.0f },
		{ -8388608.0f, -8388608.0f },
		{ -8388609.0f, -8388609.0f },
		{ -16777215.0f, -16777215.0f },
		{ -16777216.0f, -16777216.0f },
		{ -16777217.0f, -16777217.0f },
		{ -33554432.0f, -33554432.0f },
		{ -0x1p25f, -0x1p25f },
		{ 0x1p25f, 0x1p25f },
		{ -0x1p64f, -0x1p64f },
		{ 0x1p64f, 0x1p64f },
		});

	TestFunction<double, sint64>(static_cast<sint64(*)(double)>(VDFloorToInt64), "VDFloorToInt64(double)", {
		{ 0.00, 0 },
		{ 0.45, 0 },
		{ 0.55, 0 },
		{ 1.00, 1 },
		{ 1.45, 1 },
		{ 1.55, 1 },
		{ 2.00, 2 },
		{ -0.45, -1 },
		{ -0.55, -1 },
		{ -1.0, -1 },
		{ -1.45, -2 },
		{ -1.55, -2 },
		{ -2.0, -2 },
		{  0x7FFFFFFF'FFFFFC00.0p0, INT64_C( 0x7FFFFFFF'FFFFFC00) },
		{ -0x7FFFFFFF'FFFFFC00.0p0, INT64_C(-0x7FFFFFFF'FFFFFC00) },
		{ -0x80000000'00000000.0p0, INT64_C(-0x7FFFFFFF'FFFFFFFF) - 1 },
	});

	TestFunction<double, sint32>(static_cast<sint32(*)(double)>(VDCeilToInt), "VDCeilToInt(double)", {
		{ 0.00, 0 },
		{ 0.45, 1 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 2 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, 0 },
		{ -1.45, -1 },
		{ -1.55, -1 },
		{ -2.0, -2 },
		{ 2147483647.0, 2147483647 },
		{ -2147483647.0, -2147483647 },
		{ -2147483648.0, -2147483647 - 1 },
	});

	TestFunction<float, sint32>(static_cast<sint32(*)(float)>(VDCeilToInt32), "VDCeilToInt32(float)", {
		{ 0.00f, 0 },
		{ 0.45f, 1 },
		{ 0.55f, 1 },
		{ 1.00f, 1 },
		{ 1.45f, 2 },
		{ 1.55f, 2 },
		{ 2.00f, 2 },
		{ -0.45f, 0 },
		{ -0.55f, 0 },
		{ -1.45f, -1 },
		{ -1.55f, -1 },
		{ -2.0f, -2 },
		{ 16777216.0f, 16777216 },
		{ -16777216.0f, -16777216 },
		{ -2147483648.0f, -2147483647 - 1 },
	});

	TestFunction<double, sint32>(static_cast<sint32(*)(double)>(VDCeilToInt32), "VDRoundToInt(double)", {
		{ 0.00, 0 },
		{ 0.45, 1 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 2 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, 0 },
		{ -1.45, -1 },
		{ -1.55, -1 },
		{ -2.0, -2 },
		{ 2147483647.0, 2147483647 },
		{ -2147483647.0, -2147483647 },
		{ -2147483648.0, -2147483647 - 1 },
	});

	TestFunction<double, sint64>(static_cast<sint64(*)(double)>(VDCeilToInt64), "VDCeilToInt64(double)", {
		{ 0.00, 0 },
		{ 0.45, 1 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 2 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, 0 },
		{ -1.45, -1 },
		{ -1.55, -1 },
		{ -2.0, -2 },
		{  0x7FFFFFFF'FFFFFC00.0p0, INT64_C( 0x7FFFFFFF'FFFFFC00) },
		{ -0x7FFFFFFF'FFFFFC00.0p0, INT64_C(-0x7FFFFFFF'FFFFFC00) },
		{ -0x80000000'00000000.0p0, INT64_C(-0x7FFFFFFF'FFFFFFFF) - 1 },
	});

	TestFunction<double, sint32>(static_cast<sint32(*)(double)>(VDCeilToInt), "VDCeilToInt(double)", {
		{ 0.00, 0 },
		{ 0.45, 1 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 2 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, 0 },
		{ -1.45, -1 },
		{ -1.55, -1 },
		{ -2.0, -2 },
		{ 2147483647.0, 2147483647 },
		{ -2147483647.0, -2147483647 },
		{ -2147483648.0, -2147483647 - 1 },
	});

	TestFunction<float, sint32>(static_cast<sint32(*)(float)>(VDCeilToInt32), "VDCeilToInt32(float)", {
		{ 0.00f, 0 },
		{ 0.45f, 1 },
		{ 0.55f, 1 },
		{ 1.00f, 1 },
		{ 1.45f, 2 },
		{ 1.55f, 2 },
		{ 2.00f, 2 },
		{ -0.45f, 0 },
		{ -0.55f, 0 },
		{ -1.45f, -1 },
		{ -1.55f, -1 },
		{ -2.0f, -2 },
		{ 16777216.0f, 16777216 },
		{ -16777216.0f, -16777216 },
		{ -2147483648.0f, -2147483647 - 1 },
	});

	TestFunction<double, sint32>(static_cast<sint32(*)(double)>(VDCeilToInt32), "VDCeilToInt(double)", {
		{ 0.00, 0 },
		{ 0.45, 1 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 2 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, 0 },
		{ -1.45, -1 },
		{ -1.55, -1 },
		{ -2.0, -2 },
		{ 2147483647.0, 2147483647 },
		{ -2147483647.0, -2147483647 },
		{ -2147483648.0, -2147483647 - 1 },
	});

	TestFunction<double, sint64>(static_cast<sint64(*)(double)>(VDCeilToInt64), "VDCeilToInt64(double)", {
		{ 0.00, 0 },
		{ 0.45, 1 },
		{ 0.55, 1 },
		{ 1.00, 1 },
		{ 1.45, 2 },
		{ 1.55, 2 },
		{ 2.00, 2 },
		{ -0.45, 0 },
		{ -0.55, 0 },
		{ -1.45, -1 },
		{ -1.55, -1 },
		{ -2.0, -2 },
		{  0x7FFFFFFF'FFFFFC00.0p0, INT64_C( 0x7FFFFFFF'FFFFFC00) },
		{ -0x7FFFFFFF'FFFFFC00.0p0, INT64_C(-0x7FFFFFFF'FFFFFC00) },
		{ -0x80000000'00000000.0p0, INT64_C(-0x7FFFFFFF'FFFFFFFF) - 1 },
	});

	TestFunction<float, sint16>(VDClampedRoundFixedToInt16Fast, "VDClampedRoundFixedToInt16Fast", {
//		{ -1000.0f, -32768 },
		{ -1.0f, -32767 },
		{ -0x0.FFFFFEp0f, -32767 },
		{ -32766.51f/32767.0f, -32767 },
		{ -32766.49f/32767.0f, -32766 },
		{ -32766.0f/32767.0f, -32766 },
		{ -0.51f/32767.0f, -1 },
		{ -0.49f/32767.0f, 0 },
		{ -1e-30f, 0 },
		{ 0.0f, 0 },
		{ 1e-30f, 0 },
		{ 0.49f/32767.0f, 0 },
		{ 0.51f/32767, 1 },
		{ 1.0f/32767.0f, 1 },
		{ 32766.0f/32767.0f, 32766 },
		{ 32766.49f/32767.0f, 32766 },
		{ 32766.51f/32767.0f, 32767 },
		{ 0x0.FFFFFEp0f, 32767 },
		{ 1.0f, 32767 },
		{ 1.01f, 32767 },
		{ 1000.0f, 32767 },
	});

	TestFunction<float, uint8>(VDClampedRoundFixedToUint8Fast, "VDClampedRoundFixedToUint8Fast", {
		{ -1000.0f, 0 },
		{ -0.1f, 0 },
		{ -1e-30f, 0 },
		{ 0.0f, 0 },
		{ 1e-30f, 0 },
		{ 0.49f/255.0f, 0 },
		{ 0.51f/255.0f, 1 },
		{ 1.0f/255.0f, 1 },
		{ 254.0f/255.0f, 254 },
		{ 254.49f/255.0f, 254 },
		{ 254.51f/255.0f, 255 },
		{ 0x0.FFFFFEp0f, 255 },
		{ 1.0f, 255 },
		{ 1.01f, 255 },
		{ 1000.0f, 255 },
	});

	return 0;
}
