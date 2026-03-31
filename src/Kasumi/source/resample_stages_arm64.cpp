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
#include <vd2/system/vdtypes.h>

#ifdef VD_CPU_ARM64
#include <intrin.h>
#include <arm_neon.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/Kasumi/resample_kernels.h>
#include "resample_stages_arm64.h"

namespace {
	void ConvertFilterTableToS16(vdblock<sint16, vdaligned_alloc<sint16>>& dst, vdspan<const sint32> src) {
		// add a bit of padding to allow for read-over
		dst.resize(src.size() + 8, 0);

		std::transform(src.begin(), src.end(), dst.begin(), [](sint32 v) { return (sint16)v; });
	}
}

///////////////////////////////////////////////////////////////////////////

VDResamplerSeparableTableRowStageNEON::VDResamplerSeparableTableRowStageNEON(const IVDResamplerFilter& filter)
	: VDResamplerRowStageSeparableTable32(filter)
{
	ConvertFilterTableToS16(mFilterBank16, mFilterBank);
}

void VDResamplerSeparableTableRowStageNEON::Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
	const size_t filterSize = mFilterBank16.size() >> 8;

	const sint16 *filters = mFilterBank16.data();

	if (filterSize == 2)
		return Filter(dst, src, w, filters, std::integral_constant<size_t, 2>(), u, dudx);
	else if (filterSize == 4)
		return Filter(dst, src, w, filters, std::integral_constant<size_t, 4>(), u, dudx);
	else if (filterSize == 6)
		return Filter(dst, src, w, filters, std::integral_constant<size_t, 6>(), u, dudx);
	else if (filterSize == 8)
		return Filter(dst, src, w, filters, std::integral_constant<size_t, 8>(), u, dudx);
	else if (filterSize & 2)
		return Filter<true>(dst, src, w, filters, filterSize, u, dudx);
	else
		return Filter<false>(dst, src, w, filters, filterSize, u, dudx);
}

template<bool T_FilterSizeOddPair, typename T_FilterSize>
void VDResamplerSeparableTableRowStageNEON::Filter(void *dst, const void *src, size_t w, const sint16 *filters, T_FilterSize filterSize, uint32 u, uint32 dudx) {
	static constexpr size_t minFilterSize = std::is_same_v<T_FilterSize, size_t> ? 8 : filterSize;

	uint32 *VDRESTRICT dst32 = (uint32 *)dst;

	uint64 srcAccumLo = (uint64)u << 48;
	uint64 srcAccumHi = ((uint64)(uintptr)src >> 2) + (u >> 16);
	const uint64 srcIncLo = (uint64)dudx << 48;
	const uint64 srcIncHi = dudx >> 16;

	do {
		const uint8 *VDRESTRICT src2 = (const uint8 *)(srcAccumHi * 4);
		const sint16 *VDRESTRICT filter2 = &filters[(srcAccumLo >> 56) * filterSize];

		int32x4_t accum = vdupq_n_s32(0);

		if constexpr (minFilterSize < 4) {
			uint8x8_t px01 = vld1_u8(src2);
			int16x8_t pxw01 = vreinterpretq_s16_u16(vmovl_u8(px01));
			int16x4_t coeff01 = vld1_s16(filter2);

			accum = vmull_lane_s16(vget_low_s16(pxw01), coeff01, 0);
			accum = vmlal_high_lane_s16(accum, pxw01, coeff01, 1);
		} else {
			uint8x8_t px01 = vld1_u8(src2);
			uint8x8_t px23 = vld1_u8(src2+8);
			src2 += 16;

			int16x8_t pxw01 = vreinterpretq_s16_u16(vmovl_u8(px01));
			int16x8_t pxw23 = vreinterpretq_s16_u16(vmovl_u8(px23));

			int16x4_t coeff0123 = vld1_s16(filter2);
			filter2 += 4;

			accum = vmull_lane_s16(vget_low_s16(pxw01), coeff0123, 0);
			accum = vmlal_high_lane_s16(accum, pxw01, coeff0123, 1);
			accum = vmlal_lane_s16(accum, vget_low_s16(pxw23), coeff0123, 2);
			accum = vmlal_high_lane_s16(accum, pxw23, coeff0123, 3);
		}

		if constexpr (minFilterSize >= 6) {
			if constexpr (minFilterSize < 8) {
				uint8x8_t px01 = vld1_u8(src2);
				int16x8_t pxw01 = vreinterpretq_s16_u16(vmovl_u8(px01));
				int16x4_t coeff01 = vld1_s16(filter2);

				accum = vmlal_lane_s16(accum, vget_low_s16(pxw01), coeff01, 0);
				accum = vmlal_high_lane_s16(accum, pxw01, coeff01, 1);
			} else {
				uint8x8_t px01 = vld1_u8(src2);
				uint8x8_t px23 = vld1_u8(src2+8);
				src2 += 16;

				int16x8_t pxw01 = vreinterpretq_s16_u16(vmovl_u8(px01));
				int16x8_t pxw23 = vreinterpretq_s16_u16(vmovl_u8(px23));

				int16x4_t coeff0123 = vld1_s16(filter2);
				filter2 += 4;

				accum = vmlal_lane_s16(accum, vget_low_s16(pxw01), coeff0123, 0);
				accum = vmlal_high_lane_s16(accum, pxw01, coeff0123, 1);
				accum = vmlal_lane_s16(accum, vget_low_s16(pxw23), coeff0123, 2);
				accum = vmlal_high_lane_s16(accum, pxw23, coeff0123, 3);
			}
		}

		if constexpr (std::is_same_v<size_t, T_FilterSize>) {
			for(size_t i = 10; i < filterSize; i += 4) {
				uint8x8_t px01 = vld1_u8(src2);
				uint8x8_t px23 = vld1_u8(src2+8);
				src2 += 16;

				int16x8_t pxw01 = vreinterpretq_s16_u16(vmovl_u8(px01));
				int16x8_t pxw23 = vreinterpretq_s16_u16(vmovl_u8(px23));

				int16x4_t coeff0123 = vld1_s16(filter2);
				filter2 += 4;

				accum = vmlal_lane_s16(accum, vget_low_s16(pxw01), coeff0123, 0);
				accum = vmlal_high_lane_s16(accum, pxw01, coeff0123, 1);
				accum = vmlal_lane_s16(accum, vget_low_s16(pxw23), coeff0123, 2);
				accum = vmlal_high_lane_s16(accum, pxw23, coeff0123, 3);
			}

			if constexpr (T_FilterSizeOddPair) {
				uint8x8_t px01 = vld1_u8(src2);
				int16x8_t pxw01 = vreinterpretq_s16_u16(vmovl_u8(px01));
				int16x4_t coeff01 = vcreate_s16(*(uint32 *)filter2);

				accum = vmlal_lane_s16(accum, vget_low_s16(pxw01), coeff01, 0);
				accum = vmlal_high_lane_s16(accum, pxw01, coeff01, 1);
			}
		}

		int16x4_t accum2 = vqrshrn_n_s32(accum, 14);
		uint8x8_t accum3 = vqmovun_s16(vcombine_s16(accum2, accum2));

		*dst32++ = vget_lane_u32(vreinterpret_u32_u8(accum3), 0);

		srcAccumHi += srcIncHi + (srcAccumLo + srcIncLo < srcAccumLo ? 1 : 0);
		srcAccumLo += srcIncLo;
	} while(--w);
}

VDResamplerSeparableTableColStageNEON::VDResamplerSeparableTableColStageNEON(const IVDResamplerFilter& filter)
	: VDResamplerColStageSeparableTable32(filter)
{
	ConvertFilterTableToS16(mFilterBank16, mFilterBank);
}

void VDResamplerSeparableTableColStageNEON::Process(void *dst, const void *const *src, uint32 w, sint32 phase) {
	const size_t filtSize = mFilterBank16.size() >> 8;

	const sint16 *filter = mFilterBank16.data() + filtSize*((phase >> 8) & 0xff);

	uint8 *dst8 = (uint8 *)dst;

	if (filtSize == 2)
		return Filter(dst8, src, w, filter, std::integral_constant<size_t, 2>());
	else if (filtSize == 4)
		return Filter(dst8, src, w, filter, std::integral_constant<size_t, 4>());
	else if (filtSize == 6)
		return Filter(dst8, src, w, filter, std::integral_constant<size_t, 6>());
	else if (filtSize == 8)
		return Filter(dst8, src, w, filter, std::integral_constant<size_t, 8>());
	else
		return Filter(dst8, src, w, filter, filtSize);
}

template<typename T_FilterSize>
void VDResamplerSeparableTableColStageNEON::Filter(uint8 *VDRESTRICT dst, const void* const *src, uint32 w, const sint16 *VDRESTRICT filter, T_FilterSize filterSize) {
	static constexpr size_t filterMinSize = std::is_same_v<T_FilterSize, size_t> ? 8 : filterSize;

	int16x8_t baseFilter = vld1q_s16(filter);
	size_t xoffset = 0;

	size_t w2 = w >> 1;
	
	while(w2--) {
		int32x4_t accum1;
		int32x4_t accum2;

		int16x8_t px0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8((const uint8 *)src[0] + xoffset)));
		int16x8_t px1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8((const uint8 *)src[1] + xoffset)));

		accum1 = vmull_laneq_s16(vget_low_s16(px0), baseFilter, 0);
		accum2 = vmull_high_laneq_s16(px0, baseFilter, 0);
		accum1 = vmlal_laneq_s16(accum1, vget_low_s16(px1), baseFilter, 1);
		accum2 = vmlal_high_laneq_s16(accum2, px1, baseFilter, 1);

		if constexpr (filterMinSize >= 4) {
			px0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8((const uint8 *)src[2] + xoffset)));
			px1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8((const uint8 *)src[3] + xoffset)));

			accum1 = vmlal_laneq_s16(accum1, vget_low_s16(px0), baseFilter, 2);
			accum2 = vmlal_high_laneq_s16(accum2, px0, baseFilter, 2);
			accum1 = vmlal_laneq_s16(accum1, vget_low_s16(px1), baseFilter, 3);
			accum2 = vmlal_high_laneq_s16(accum2, px1, baseFilter, 3);
		}

		if constexpr (filterMinSize >= 6) {
			px0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8((const uint8 *)src[4] + xoffset)));
			px1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8((const uint8 *)src[5] + xoffset)));

			accum1 = vmlal_laneq_s16(accum1, vget_low_s16(px0), baseFilter, 4);
			accum2 = vmlal_high_laneq_s16(accum2, px0, baseFilter, 4);
			accum1 = vmlal_laneq_s16(accum1, vget_low_s16(px1), baseFilter, 5);
			accum2 = vmlal_high_laneq_s16(accum2, px1, baseFilter, 5);
		}

		if constexpr (filterMinSize >= 8) {
			px0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8((const uint8 *)src[6] + xoffset)));
			px1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8((const uint8 *)src[7] + xoffset)));

			accum1 = vmlal_laneq_s16(accum1, vget_low_s16(px0), baseFilter, 6);
			accum2 = vmlal_high_laneq_s16(accum2, px0, baseFilter, 6);
			accum1 = vmlal_laneq_s16(accum1, vget_low_s16(px1), baseFilter, 7);
			accum2 = vmlal_high_laneq_s16(accum2, px1, baseFilter, 7);

			if constexpr (std::is_same_v<T_FilterSize, size_t>) {
				for(size_t i = 8; i < filterSize; i += 2) {
					int16x4_t moreCoeffs = vcreate_s16(*(uint32 *)&filter[i]);

					accum1 = vmlal_lane_s16(accum1, vget_low_s16(px0), moreCoeffs, 0);
					accum2 = vmlal_high_lane_s16(accum2, px0, moreCoeffs, 0);
					accum1 = vmlal_lane_s16(accum1, vget_low_s16(px1), moreCoeffs, 1);
					accum2 = vmlal_high_lane_s16(accum2, px1, moreCoeffs, 1);
				}
			}
		}

		int16x8_t accum3 = vqrshrn_high_n_s32(vqrshrn_n_s32(accum1, 14), accum2, 14);

		vst1_u8(dst, vqmovun_s16(accum3));

		dst += 8;
		xoffset += 8;
	}

	if (w & 1) {
		int32x4_t accum;

		accum = vmull_laneq_s16(vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[0] + xoffset))))), baseFilter, 0);
		accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[1] + xoffset))))), baseFilter, 1);

		if constexpr (filterMinSize >= 4) {
			accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[2] + xoffset))))), baseFilter, 2);
			accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[3] + xoffset))))), baseFilter, 3);
		}

		if constexpr (filterMinSize >= 6) {
			accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[2] + xoffset))))), baseFilter, 4);
			accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[3] + xoffset))))), baseFilter, 5);
		}

		if constexpr (filterMinSize >= 8) {
			accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[2] + xoffset))))), baseFilter, 6);
			accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[3] + xoffset))))), baseFilter, 7);

			if constexpr (std::is_same_v<T_FilterSize, size_t>) {
				for(size_t i = 8; i < filterSize; i += 2) {
					int16x4_t moreCoeffs = vcreate_s16(*(uint32 *)&filter[i]);

					accum = vmlal_lane_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[i+0] + xoffset))))), moreCoeffs, 0);
					accum = vmlal_lane_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vcreate_u8(*(uint32 *)((const uint8 *)src[i+1] + xoffset))))), moreCoeffs, 1);
				}
			}
		}

		int16x4_t accum2 = vqrshrn_n_s32(accum, 14);

		*(uint32 *)dst = vget_lane_u32(vreinterpret_u32_u8(vqmovun_s16(vcombine_s16(accum2, accum2))), 0);

		dst += 4;
		xoffset += 4;
	}
}

///////////////////////////////////////////////////////////////////////////

VDResamplerSeparableTableRowStage8NEON::VDResamplerSeparableTableRowStage8NEON(const IVDResamplerFilter& filter)
	: VDResamplerRowStageSeparableTable32(filter)
{
	mbUseFastLerp = VDResamplerFilterHasNoOvershoot(mFilterBank.data(), mFilterBank.size());
}

void VDResamplerSeparableTableRowStage8NEON::Init(const VDResamplerAxis& axis, uint32 srcw) {
	mSrcWidth = srcw;

	const uint32 ksize = (int)mFilterBank.size() >> 8;
	const uint32 kquads = (ksize + 3) >> 2;
	const uint32 ksize4 = kquads * 4;

	if (srcw < ksize4)
		mTempBuffer.resize(ksize4, 0);
	
	const sint32 dstw = axis.dx_preclip + axis.dx_active + axis.dx_postclip + axis.dx_dualclip;
	mRowFilters.resize((kquads * 4 + 4) * dstw);

	sint16 *rowFilter = mRowFilters.data();
	memset(rowFilter, 0, mRowFilters.size() * sizeof(mRowFilters[0]));

	uint32 xstart = 0;

	// 2-tap is a special case that we can optimize. For it we encode groups of 4 destination pixels
	// with solely 2-tap filters, instead of the padded 4*N-tap filters we use for wider sizes. Any
	// remaining pixels at the end are done conventionally. We can also simplify some checks for
	// this case as 1-pixel sources are special cased as a memset(), so we'll always be able to fit
	// the 2-tap kernel within the source.
	//
	// A fast group of 4 pixels takes 32 bytes, while regular encoding for a 2-tap filter takes
	// 16 bytes, or 64 bytes for 4 pixels. This means that the fast group takes less space, and we
	// don't need to resize the row filter buffer as long as we're OK with a bit of unused space.
	//
	// When SSSE3 is available, the filter has no overshoot, and we are interpolating, we can use
	// an even faster path where we only expand to 16-bit instead of 32-bit for intermediate values
	// and use shuffling instead of gathering. In this case, the fast groups are 8 pixels wide.

	mNumFastGroups = 0;

	// We can use SSSE3 fast lerp as long as the gather doesn't have to pull more than 16 consecutive
	// samples. This works for any step factor of 2.0 or below.
	if (axis.dudx > 0x20000)
		mbUseFastLerp = false;

	if (ksize == 2) {
		if (mbUseFastLerp) {
			uint32 fastGroups = dstw >> 3;

			mFastLerpOffsets.resize(fastGroups);

			for(uint32 i = 0; i < fastGroups; ++i) {
				alignas(16) uint16 offsets[8];

				for(uint32 j = 0; j < 8; ++j) {
					sint32 u = axis.u + axis.dudx * (i*8 + j);
					sint32 rawSrcOffset = u >> 16;
					sint32 srcOffset = rawSrcOffset;

					const sint32 *VDRESTRICT filter = &mFilterBank[ksize * ((u >> 8) & 0xFF)];

					if (srcOffset > (sint32)srcw - 2) {
						srcOffset = (sint32)srcw - 2;
						rowFilter[8] = -0x8000;
					} else {
						uint8 f0 = (uint8)((filter[0] + 64) >> 7);
						uint8 f1 = (uint8)(0x80 - f0);
						rowFilter[8] = (sint16)(uint16)(((uint32)f1 << 8) + f0);
					}
		
					if (srcOffset < 0) {
						srcOffset = 0;
						rowFilter[8] = 0x0080;
					}

					offsets[j] = srcOffset;

					++rowFilter;
				}

				mFastLerpOffsets[i] = offsets[0];

				uint16x8_t absOffsets = vld1q_u16(offsets);
				uint16x8_t relOffsets = vsubq_u16(absOffsets, vdupq_laneq_u16(absOffsets, 0));
				uint8x8_t shuffles = vmovn_u16(relOffsets);

				uint16x8_t shuffles2 = vaddq_u16(
					vreinterpretq_u16_u8(
						vcombine_u8(
							vzip1_u8(shuffles, shuffles),
							vzip2_u8(shuffles, shuffles)
						)
					),
					vdupq_n_u16(0x0100)
				);

				vst1q_s16(rowFilter - 8, vreinterpretq_s16_u16(shuffles2));

				rowFilter += 8;
			}

			mNumFastGroups = fastGroups;

			xstart = fastGroups << 3;
		} else {
			uint32 fastGroups = dstw >> 2;

			for(uint32 i = 0; i < fastGroups; ++i) {
				for(uint32 j = 0; j < 4; ++j) {
					sint32 u = axis.u + axis.dudx * (i*4 + j);
					sint32 rawSrcOffset = u >> 16;
					sint32 srcOffset = rawSrcOffset;

					if (srcOffset > (sint32)srcw - 2)
						srcOffset = (sint32)srcw - 2;
		
					if (srcOffset < 0)
						srcOffset = 0;

					rowFilter[0] = srcOffset;

					const sint32 *VDRESTRICT filter = &mFilterBank[ksize * ((u >> 8) & 0xFF)];
					for(uint32 k = 0; k < 2; ++k) {
						sint32 tapSrcOffset = rawSrcOffset + k;

						if (tapSrcOffset < 0)
							tapSrcOffset = 0;
						if (tapSrcOffset >= (sint32)srcw)
							tapSrcOffset = (sint32)srcw - 1;

						rowFilter[8 + tapSrcOffset - srcOffset] += filter[k];
					}

					rowFilter += 2;
				}

				rowFilter += 8;
			}

			mNumFastGroups = fastGroups;

			xstart = fastGroups << 2;
		}
	}

	for(sint32 x = xstart; x < dstw; ++x) {
		sint32 u = axis.u + axis.dudx * x;
		sint32 rawSrcOffset = u >> 16;
		sint32 srcOffset = rawSrcOffset;

		// We need both these clamps in the case where the raw kernel fits within the source, but
		// the 4-expanded kernel doesn't. In that case, we push the kernel as far left as it can.
		// The source is copied and padded in Process() to prevent read overruns.
		if (srcOffset > (sint32)srcw - (sint32)ksize4)
			srcOffset = (sint32)srcw - (sint32)ksize4;
		
		if (srcOffset < 0)
			srcOffset = 0;

		const sint32 *VDRESTRICT filter = &mFilterBank[ksize * ((u >> 8) & 0xFF)];

		*rowFilter++ = srcOffset;
		*rowFilter++ = 0;
		*rowFilter++ = 0;
		*rowFilter++ = 0;

		for(uint32 i = 0; i < ksize; ++i) {
			sint32 tapSrcOffset = rawSrcOffset + i;

			if (tapSrcOffset < 0)
				tapSrcOffset = 0;
			if (tapSrcOffset >= (sint32)srcw)
				tapSrcOffset = (sint32)srcw - 1;

			rowFilter[tapSrcOffset - srcOffset] += filter[i];
		}

		rowFilter += kquads*4;
	}
}

void VDResamplerSeparableTableRowStage8NEON::Process(void *dst, const void *src, uint32 w) {
	// get one degenerate case out of the way
	if (mSrcWidth == 1) {
		memset(dst, *(const uint8 *)src, w);
		return;
	}

	// if the source is narrower than the 4-padded filter kernel, copy the source to a temp buffer
	// so we can safely read over to an integral number of quads
	if (!mTempBuffer.empty()) {
		memcpy(mTempBuffer.data(), src, mSrcWidth);
		src = mTempBuffer.data();
	}

	const sint16 *VDRESTRICT rowFilter = mRowFilters.data();
	const uint32 ksize = mFilterBank.size() >> 8;
	const uint32 kquads = (ksize + 3) >> 2;
	uint8 *VDRESTRICT dst8 = (uint8 *)dst;

#if 0		// reference code
	while(w--) {
		const uint8 *VDRESTRICT src2 = (const uint8 *)src + (uint16)rowFilter[0];

		rowFilter += 4;

		sint32 accum = 0x2000;
		for(uint32 i=0; i<kquads; ++i) {
			accum += rowFilter[0] * (sint32)src2[0] + rowFilter[1] * (sint32)src2[1] + rowFilter[2] * (sint32)src2[2] + rowFilter[3] * (sint32)src2[3];
			rowFilter += 4;
			src2 += 4;
		}

		accum >>= 14;

		if (accum < 0)
			accum = 0;
		if (accum > 255)
			accum = 255;

		*dst8++ = (uint8)accum;
	}
#else
	uint32 fastGroups = mNumFastGroups;
	if (fastGroups) {
		if (mbUseFastLerp) {
			const uint16 *VDRESTRICT offsets = mFastLerpOffsets.data();

			do {
				uint8x16_t srcVector = vld1q_u8((const uint8 *)src + (*offsets++));
				uint8x16_t gatherVector = vqtbl1q_u8(srcVector, vreinterpretq_u8_s16(vld1q_s16(rowFilter)));
				uint8x16_t filterVector = vreinterpretq_u8_s16(vld1q_s16(rowFilter + 8));
				int16x8_t accum =
					vpaddq_u16(
						vmull_u8(vget_low_u8(gatherVector), vget_low_u8(filterVector)),
						vmull_high_u8(gatherVector, filterVector)
					);

				vst1_u8(dst8, vqshrun_n_s16(accum, 7));

				rowFilter += 16;
				dst8 += 8;
			} while(--fastGroups);

			w &= 7;
		} else {
			do {
				// gather four pairs of adjacent samples to filter
				size_t offset0 = *(const uint32 *)(rowFilter + 0);
				size_t offset1 = *(const uint32 *)(rowFilter + 2);
				size_t offset2 = *(const uint32 *)(rowFilter + 4);
				size_t offset3 = *(const uint32 *)(rowFilter + 6);

				uint16x4_t srcVector = vdup_n_u16(*(const uint16 *)((const uint8 *)src + offset0));
				srcVector = vset_lane_u16(*(const uint16 *)((const uint8 *)src + offset1), srcVector, 1);
				srcVector = vset_lane_u16(*(const uint16 *)((const uint8 *)src + offset2), srcVector, 2);
				srcVector = vset_lane_u16(*(const uint16 *)((const uint8 *)src + offset3), srcVector, 3);

				// filter and then pairwise add
				int16x8_t srcVector2 = vreinterpretq_s16_u16(vmovl_u8(srcVector));
				int16x8_t coeffs = vld1q_s16(rowFilter + 8);
				int32x4_t accum = vpaddq_s32(
					vmull_s16(vget_low_s16(srcVector2), vget_low_s16(coeffs)),
					vmull_high_s16(srcVector2, coeffs)
				);

				int16x4_t accum2 = vqrshrn_n_s32(accum, 14);
				uint8x8_t accum3 = vqmovun_s16(vcombine_s16(accum2, accum2));

				*(uint32 *)dst8 = vget_lane_u32(vreinterpret_u32_u8(accum3), 0);

				rowFilter += 16;
				dst8 += 4;
			} while(--fastGroups);

			w &= 3;
		}
	}

	if (kquads == 1) {
		while(w--) {
			const uint8 *VDRESTRICT src2 = (const uint8 *)src + (uint16)rowFilter[0];
			int16x4_t v = vreinterpret_s16_u16(vget_low_u8(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)src2)))));
			int16x4_t coeffs = vld1_s16(rowFilter + 4);
			int32x4_t accum = vmull_s16(v, coeffs);

			accum = vdupq_n_s32(vaddvq_s32(accum));
			int16x4_t accum1 = vqrshrn_n_s32(accum, 14);
			uint8x8_t accum2 = vqmovun_s16(vcombine_s16(accum1, accum1));

			*dst8++ = (uint8)vget_lane_u8(accum2, 0);
			rowFilter += 8;
		}
	} else {
		while(w--) {
			const uint8 *VDRESTRICT src2 = (const uint8 *)src + (uint16)rowFilter[0];

			rowFilter += 4;

			int32x4_t accum = vdupq_n_s32(0);
			uint32 i = kquads;
			do {
				int16x4_t v = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)src2)))));
				int16x4_t coeffs = vld1_s16(rowFilter);
				accum = vmlal_s16(accum, v, coeffs);

				rowFilter += 4;
				src2 += 4;
			} while(--i);

			accum = vdupq_n_s32(vaddvq_s32(accum));
			int16x4_t accum1 = vqrshrn_n_s32(accum, 14);
			uint8x8_t accum2 = vqmovun_s16(vcombine_s16(accum1, accum1));

			*dst8++ = (uint8)vget_lane_u8(accum2, 0);
		}
	}
#endif
}

void VDResamplerSeparableTableRowStage8NEON::Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
	// if we're hitting this path, it must be for dualclip, so perf is not significant
	uint8 *VDRESTRICT dst = (uint8 *)dst0;
	const uint8 *src = (const uint8 *)src0;
	const unsigned ksize = (int)mFilterBank.size() >> 8;
	const sint32 *filterBase = mFilterBank.data();

	do {
		const uint8 *VDRESTRICT src2 = src + (u>>16);
		const sint32 *VDRESTRICT filter = filterBase + ksize*((u>>8)&0xff);
		u += dudx;

		int accum = 0x2000;
		for(unsigned i = ksize; i; --i) {
			uint8 p = *src2++;
			sint32 coeff = *filter++;

			accum += (sint32)p * coeff;
		}

		accum >>= 14;

		if (accum < 0)
			accum = 0;

		if (accum > 255)
			accum = 255;

		*dst++ = (uint8)accum;
	} while(--w);
}

VDResamplerSeparableTableColStage8NEON::VDResamplerSeparableTableColStage8NEON(const IVDResamplerFilter& filter)
	: VDResamplerColStageSeparableTable32(filter)
{
	size_t n = mFilterBank.size();
	mbUseFastLerp = VDResamplerFilterHasNoOvershoot(mFilterBank.data(), n);

	mFilterBank16.resize((mFilterBank.size() + 7) & ~7, 0);

	for(size_t i = 0; i < n; ++i)
		mFilterBank16[i] = (sint16)mFilterBank[i];
}

namespace {
	template<unsigned T_Rows>
	void FilterColumns_NEON(void *dst0, const uint8 *const *src, const sint16 *filter, uint32 n) {
		static_assert(T_Rows >= 2 && T_Rows <= 8);
		static_assert((T_Rows & 1) == 0);

		uint8 *VDRESTRICT dst = (uint8 *)dst0;

		const uint8 *rows[T_Rows];
		for(unsigned i = 0; i < T_Rows; ++i)
			rows[i] = src[i];

		int16x8_t rowFilter = vld1q_s16(filter);

		uint32 xoffset = 0;

		if constexpr(T_Rows == 2) {
			uint32 n2 = n >> 1;
			n &= 1;

			uint32 xoffsetLimit1 = n2 * 8;
			while(xoffset < xoffsetLimit1) {
				// load source samples from the two rows
				int16x8_t x0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(rows[0] + xoffset)));
				int16x8_t x1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(rows[1] + xoffset)));

				// filter two sets of four pairs of pixels
				int32x4_t accum1 = vmull_laneq_s16(vget_low_s16(x0), rowFilter, 0);
				int32x4_t accum2 = vmull_high_laneq_s16(x0, rowFilter, 0);

				accum1 = vmlal_laneq_s16(accum1, vget_low_s16(x1), rowFilter, 1);
				accum2 = vmlal_high_laneq_s16(accum2, x1, rowFilter, 1);

				int16x8_t accum3 = vqrshrn_high_n_s32(vqrshrn_n_s32(accum1, 14), accum2, 14);

				vst1_u8(dst + xoffset, vqmovun_s16(accum3));

				xoffset += 8;
			}
		}

		uint32 xoffsetLimit2 = xoffset + n*4;
		while(xoffset < xoffsetLimit2) {
			// this section is critical and must be unrolled
			int32x4_t accum = vmull_laneq_s16(vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(rows[0] + xoffset)))))), rowFilter, 0);
			accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(rows[1] + xoffset)))))), rowFilter, 1);

			if constexpr(T_Rows >= 4) {
				accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(rows[2] + xoffset)))))), rowFilter, 2);
				accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(rows[3] + xoffset)))))), rowFilter, 3);
			}

			if constexpr(T_Rows >= 6) {
				accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(rows[4] + xoffset)))))), rowFilter, 4);
				accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(rows[5] + xoffset)))))), rowFilter, 5);
			}

			if constexpr(T_Rows >= 8) {
				accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(rows[6] + xoffset)))))), rowFilter, 6);
				accum = vmlal_laneq_s16(accum, vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(rows[7] + xoffset)))))), rowFilter, 7);
			}

			int16x4_t accum2 = vqrshrn_n_s32(accum, 14);
			uint8x8_t accum3 = vqmovun_s16(vcombine_s16(accum2, accum2));

			*(uint32 *)(dst + xoffset) = vget_lane_u32(vreinterpret_u32_u8(accum3), 0);

			xoffset += 4;
		}
	}

	void FilterColumnsLerp_NEON(void *dst0, const uint8 *const *src, const sint16 *filter, uint32 n) {
		uint8 *VDRESTRICT dst = (uint8 *)dst0;

		const uint8 *VDRESTRICT row0 = src[0];
		const uint8 *VDRESTRICT row1 = src[1];

		// reduce filter from 2.14 to 1.7
		uint8 f0 = (uint8)((filter[0] + 64) >> 7);
		uint8 f1 = (uint8)(0x80 - f0);
		uint8x8_t coeff0 = vdup_n_u8(f0);
		uint8x8_t coeff1 = vdup_n_u8(f1);

		uint32 xoffset = 0;
		uint32 n2 = n >> 1;

		uint32 xoffsetLimit1 = n2 * 8;
		while(xoffset < xoffsetLimit1) {
			// load and interleave source samples from the two rows
			// filter two sets of four pairs of pixels
			uint16x8_t accum =
				vmlal_u8(
					vmull_u8(vld1_u8(row0 + xoffset), coeff0),
					vld1_u8(row1 + xoffset),
					coeff1
				);

			uint8x8_t accum2 = vqrshrun_n_s16(accum, 7);
			vst1_u8(dst + xoffset, accum2);

			xoffset += 8;
		}

		if (n & 1) {
			uint8x8_t v0 = vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(row0 + xoffset)));
			uint8x8_t v1 = vreinterpret_u8_u32(vdup_n_u32(*(uint32 *)(row1 + xoffset)));

			// filter two sets of four pairs of pixels
			uint16x8_t accum = vmlal_u8(vmull_u8(v0, coeff0), v1, coeff1);

			uint8x8_t accum2 = vqrshrn_n_u16(accum, 7);
			*(uint32 *)(dst + xoffset) = vget_lane_u32(vreinterpret_u32_u8(accum2), 0);
		}
	}
}

void VDResamplerSeparableTableColStage8NEON::Process(void *dst0, const void *const *src0, uint32 w, sint32 phase) {
	uint8 *VDRESTRICT dst = (uint8 *)dst0;
	const uint8 *const *VDRESTRICT src = (const uint8 *const *)src0;
	const unsigned ksize = (unsigned)mFilterBank.size() >> 8;
	const sint16 *VDRESTRICT filter = &mFilterBank16[((phase>>8)&0xff) * ksize];

	int w4 = w & ~3;

	if (w4) {
		switch(ksize) {
			case 2:
				if (mbUseFastLerp)
					FilterColumnsLerp_NEON(dst, src, filter, w >> 2);
				else
					FilterColumns_NEON<2>(dst, src, filter, w >> 2);
				break;

			case 4:
				FilterColumns_NEON<4>(dst, src, filter, w >> 2);
				break;

			case 6:
				FilterColumns_NEON<6>(dst, src, filter, w >> 2);
				break;

			case 8:
				FilterColumns_NEON<8>(dst, src, filter, w >> 2);
				break;

			default:
				{
					uint32 xoffset = 0;

					for(int i=0; i<w4; i += 4) {
						int32x4_t accum = vdupq_n_s32(0);

						size_t j = 0;

						do {
							int16x4_t c0 = vreinterpret_s16_u16(
								vget_low_u16(
									vmovl_u8(
										vreinterpret_u8_u32(
											vdup_n_u32(
												*(uint32 *)(src[j+0] + xoffset)
											)
										)
									)
								)
							);

							int16x4_t c1 = vreinterpret_s16_u16(
								vget_low_u16(
									vmovl_u8(
										vreinterpret_u8_u32(
											vdup_n_u32(
												*(uint32 *)(src[j+1] + xoffset)
											)
										)
									)
								)
							);

							int16x4_t coeff = vreinterpret_s16_u32(vdup_n_u32(*(uint32 *)&filter[j]));
							accum = vmlal_lane_s16(accum, c1, coeff, 1);
							accum = vmlal_lane_s16(accum, c0, coeff, 0);

							j += 2;
						} while(j < ksize);

						int16x4_t accum2 = vqrshrn_n_s32(accum, 14);
						uint8x8_t accum3 = vqmovun_s16(vcombine_s16(accum2, accum2));

						*(uint32 *)(dst + xoffset) = vget_lane_u32(vreinterpret_u32_u8(accum3), 0);

						xoffset += 4;
					}
				}
				break;
		}
	}

	for(uint32 i=w4; i<w; ++i) {
		int b = 0x2000;
		const sint16 *filter2 = filter;
		const uint8 *const *src2 = src;

		for(unsigned j = ksize; j; j -= 2) {
			sint32 p0 = (*src2++)[i];
			sint32 p1 = (*src2++)[i];
			sint32 coeff0 = filter2[0];
			sint32 coeff1 = filter2[1];
			filter2 += 2;

			b += p0*coeff0;
			b += p1*coeff1;
		}

		b >>= 14;

		if ((uint32)b >= 0x00000100)
			b = ~b >> 31;

		dst[i] = (uint8)b;
	}
}
#endif
