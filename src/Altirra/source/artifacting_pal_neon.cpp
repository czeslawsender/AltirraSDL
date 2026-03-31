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

#if defined(VD_CPU_ARM64)
#include <intrin.h>

void ATArtifactPALLuma_NEON(uint32 *dst0, const uint8 *src, uint32 n, const uint32 *kernels0) {
	n >>= 3;

	int16x8_t x0;
	int16x8_t x1 = vmovq_n_s16(0);

	uint32 *VDRESTRICT dst = dst0;
	const uint32 *VDRESTRICT kernels = kernels0;

	do {
		const uint8 p0 = *src++;
		const uint8 p1 = *src++;
		const uint8 p2 = *src++;
		const uint8 p3 = *src++;

		const uint32 *VDRESTRICT f0 = kernels + 64U*p0;
		const uint32 *VDRESTRICT f1 = kernels + 64U*p1;
		const uint32 *VDRESTRICT f2 = kernels + 64U*p2;
		const uint32 *VDRESTRICT f3 = kernels + 64U*p3;

		x0 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f0[0])));
		x1 = vreinterpretq_s16_u32(vld1q_u32(&f0[4]));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f1[8])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f1[12])));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f2[16])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f2[20])));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f3[24])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f3[28])));

		vst1q_u32(dst, vreinterpretq_u32_s16(x0));
		dst += 4;

		const uint8 p4 = *src++;
		const uint8 p5 = *src++;
		const uint8 p6 = *src++;
		const uint8 p7 = *src++;

		const uint32 *VDRESTRICT f4 = kernels + 64U*p4;
		const uint32 *VDRESTRICT f5 = kernels + 64U*p5;
		const uint32 *VDRESTRICT f6 = kernels + 64U*p6;
		const uint32 *VDRESTRICT f7 = kernels + 64U*p7;

		x0 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f4[32])));
		x1 = vreinterpretq_s16_u32(vld1q_u32(&f4[36]));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f5[40])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f5[44])));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f6[48])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f6[52])));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f7[56])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f7[60])));

		vst1q_u32(dst, vreinterpretq_u32_s16(x0));
		dst += 4;
	} while(--n);

	vst1q_u32(dst, vreinterpretq_u32_s16(x1));
}

void ATArtifactPALLumaTwin_NEON(uint32 *dst0, const uint8 *src, uint32 n, const uint32 *kernels0) {
	n >>= 3;

	int16x8_t x0;
	int16x8_t x1 = vmovq_n_s16(0);

	uint32 *VDRESTRICT dst = dst0;
	const uint32 *VDRESTRICT kernels = kernels0;
	do {
		const uint8 p0 = src[0];
		const uint8 p2 = src[2];

		const uint32 *VDRESTRICT f0 = kernels + 32U*p0;
		const uint32 *VDRESTRICT f2 = kernels + 32U*p2;

		x0 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f0[0])));
		x1 = vreinterpretq_s16_u32(vld1q_u32(&f0[4]));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f2[8])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f2[12])));

		vst1q_u32(dst, vreinterpretq_u32_s16(x0));
		dst += 4;

		const uint8 p4 = src[4];
		const uint8 p6 = src[6];

		const uint32 *VDRESTRICT f4 = kernels + 32U*p4;
		const uint32 *VDRESTRICT f6 = kernels + 32U*p6;

		x0 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f4[16])));
		x1 = vreinterpretq_s16_u32(vld1q_u32(&f4[20]));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f6[24])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f6[28])));

		vst1q_u32(dst, vreinterpretq_u32_s16(x0));
		dst += 4;
		src += 8;
	} while(--n);

	vst1q_u32(dst, vreinterpretq_u32_s16(x1));
}

void ATArtifactPALChroma_NEON(uint32 *dst0, const uint8 *src, uint32 n, const uint32 *kernels0) {
	int16x8_t x0;
	int16x8_t x1 = vmovq_n_s16(0);
	int16x8_t x2 = vmovq_n_s16(0);
	int16x8_t x3 = vmovq_n_s16(0);

	uint32 *VDRESTRICT dst = dst0;
	const uint32 *VDRESTRICT kernels = kernels0;

	uint32 n2 = n >> 3;
	do {
		const uint8 p0 = *src++;
		const uint8 p1 = *src++;
		const uint8 p2 = *src++;
		const uint8 p3 = *src++;

		const uint32 *VDRESTRICT f0 = kernels + 128U*p0;
		const uint32 *VDRESTRICT f1 = kernels + 128U*p1;
		const uint32 *VDRESTRICT f2 = kernels + 128U*p2;
		const uint32 *VDRESTRICT f3 = kernels + 128U*p3;

		x0 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f0[0])));
		x1 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f0[4])));
		x2 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f0[8])));
		x3 = vreinterpretq_s16_u32(vld1q_u32(&f0[12]));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f1[16])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f1[20])));
		x2 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f1[24])));
		x3 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f1[28])));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f2[32])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f2[36])));
		x2 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f2[40])));
		x3 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f2[44])));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f3[48])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f3[52])));
		x2 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f3[56])));
		x3 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f3[60])));

		vst1q_u32(dst, vreinterpretq_u32_s16(x0));
		dst += 4;

		const uint8 p4 = *src++;
		const uint8 p5 = *src++;
		const uint8 p6 = *src++;
		const uint8 p7 = *src++;

		const uint32 *VDRESTRICT f4 = kernels + 128U*p4;
		const uint32 *VDRESTRICT f5 = kernels + 128U*p5;
		const uint32 *VDRESTRICT f6 = kernels + 128U*p6;
		const uint32 *VDRESTRICT f7 = kernels + 128U*p7;

		x0 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f4[64])));
		x1 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f4[68])));
		x2 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f4[72])));
		x3 = vreinterpretq_s16_u32(vld1q_u32(&f4[76]));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f5[80])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f5[84])));
		x2 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f5[88])));
		x3 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f5[92])));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f6[96])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f6[100])));
		x2 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f6[104])));
		x3 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f6[108])));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f7[112])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f7[116])));
		x2 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f7[120])));
		x3 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f7[124])));

		vst1q_u32(dst, vreinterpretq_u32_s16(x0));
		dst += 4;

	} while(--n2);

	vst1q_u32(dst, vreinterpretq_u32_s16(x1));
}

void ATArtifactPALChromaTwin_NEON(uint32 *dst0, const uint8 *src, uint32 n, const uint32 *kernels0) {
	int16x8_t x0;
	int16x8_t x1 = vmovq_n_s16(0);
	int16x8_t x2 = vmovq_n_s16(0);
	int16x8_t x3 = vmovq_n_s16(0);

	uint32 *VDRESTRICT dst = dst0;
	const uint32 *VDRESTRICT kernels = kernels0;
	uint32 n2 = n >> 3;
	do {
		const uint8 p0 = src[0];
		const uint8 p2 = src[2];

		const uint32 *VDRESTRICT f0 = kernels + 64U*p0;
		const uint32 *VDRESTRICT f2 = kernels + 64U*p2;

		x0 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f0[0])));
		x1 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f0[4])));
		x2 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f0[8])));
		x3 = vreinterpretq_s16_u32(vld1q_u32(&f0[12]));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f2[16])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f2[20])));
		x2 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f2[24])));
		x3 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f2[28])));

		vst1q_u32(dst, vreinterpretq_u32_s16(x0));
		dst += 4;

		const uint8 p4 = src[4];
		const uint8 p6 = src[6];

		const uint32 *f4 = kernels + 64U*p4;
		const uint32 *f6 = kernels + 64U*p6;

		x0 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f4[32])));
		x1 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f4[36])));
		x2 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f4[40])));
		x3 = vreinterpretq_s16_u32(vld1q_u32(&f4[44]));

		x0 = vaddq_s16(x0, vreinterpretq_s16_u32(vld1q_u32(&f6[48])));
		x1 = vaddq_s16(x1, vreinterpretq_s16_u32(vld1q_u32(&f6[52])));
		x2 = vaddq_s16(x2, vreinterpretq_s16_u32(vld1q_u32(&f6[56])));
		x3 = vaddq_s16(x3, vreinterpretq_s16_u32(vld1q_u32(&f6[60])));

		vst1q_u32(dst, vreinterpretq_u32_s16(x0));
		dst += 4;
		src += 8;
	} while(--n2);

	vst1q_u32(dst, vreinterpretq_u32_s16(x1));
	dst += 4;
}

void ATArtifactPALFinal_NEON(uint32 *dst0, const uint32 *ybuf, const uint32 *ubuf, const uint32 *vbuf, uint32 *ulbuf, uint32 *vlbuf, uint32 n) {
	n >>= 2;

	const uint32 *VDRESTRICT usrc = ubuf + 4;
	const uint32 *VDRESTRICT vsrc = vbuf + 4;
	const uint32 *VDRESTRICT ysrc = ybuf;
	uint32 *VDRESTRICT uprev = ulbuf;
	uint32 *VDRESTRICT vprev = vlbuf;

	static constexpr sint16 coeffs[] {
		-3182*2,	// -co_ug / co_ub * 32768
		-8346*2,	// -co_vg / co_vr * 32768
		0,
		0
	};

	const int16x4_t co = vld1_s16(coeffs);

	uint8 *VDRESTRICT dst = (uint8 *)dst0;

	do {
		int16x8_t up = vreinterpretq_s16_u32(vld1q_u32(uprev));
		int16x8_t vp = vreinterpretq_s16_u32(vld1q_u32(vprev));
		int16x8_t u = vreinterpretq_s16_u32(vld1q_u32(usrc));
		int16x8_t v = vreinterpretq_s16_u32(vld1q_u32(vsrc));

		usrc += 4;
		vsrc += 4;

		vst1q_u32(uprev, vreinterpretq_u32_s16(u));
		vst1q_u32(vprev, vreinterpretq_u32_s16(v));
		uprev += 4;
		vprev += 4;

		u = vaddq_s16(u, up);
		v = vaddq_s16(v, vp);

		int16x8_t y = vreinterpretq_s16_u32(vld1q_u32(ysrc));
		ysrc += 4;

		int16x8_t r = vaddq_s16(y, v);
		int16x8_t b = vaddq_s16(y, u);

		// vqdmulah would be useful here, but it requires ARMv8.1.
		int16x8_t gu = vqdmulhq_lane_s16(u, co, 0);
		int16x8_t gv = vqdmulhq_lane_s16(v, co, 1);

		int16x8_t g = vaddq_s16(vaddq_s16(y, gu), gv);

		uint8x8x4_t pixels;

		// Rounding is already present in the luma input, so we deliberately use
		// the unrounded narrowing shift.
		pixels.val[0] = vqshrun_n_s16(b, 6);
		pixels.val[1] = vqshrun_n_s16(g, 6);
		pixels.val[2] = vqshrun_n_s16(r, 6);
		pixels.val[3] = vmov_n_u8(0);

		vst4_u8(dst, pixels);
		dst += 32;
	} while(--n);
}

template<bool T_UseSignedPalette>
void ATArtifactPAL32_NEON(void *dst, void *delayLine, uint32 n) {
	// For this path, we assume that the alpha channel holds precomputed luminance. This works because
	// the only source of raw RGB32 input is VBXE, and though it outputs 21-bit RGB, it can only do so
	// from 4 x 256 palettes. All we need to do is average the YRGB pixels between the delay line and
	// the current line, and then recorrect the luminance back.

	uint32 *VDRESTRICT dst32 = (uint32 *)dst;
	uint32 *VDRESTRICT delay32 = (uint32 *)delayLine;

	uint8x16_t x40b = vmovq_n_u8(0x40);

	const uint32 n4 = n >> 2;
	const uint32 n1 = n & 3;

	for(uint32 i=0; i<n4; ++i) {
		uint8x16_t prev = vreinterpretq_u8_u32(vld1q_u32(delay32));
		uint8x16_t next = vreinterpretq_u8_u32(vld1q_u32(dst32));

		if (vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u8_u16(vceqq_u8(prev, next)), 4)), 0) != ~(uint64)0) {
			vst1q_u32(delay32, next);

			uint8x16_t avg = vrhaddq_u8(prev, next);
			uint32x4_t ydiff = vreinterpretq_u32_u8(vsubq_u8(next, avg));
			uint32x4_t ydiff2 = vsriq_n_u32(ydiff, ydiff, 8);
			uint8x16_t ydiffrgb = vreinterpretq_u8_u32(vsriq_n_u32(ydiff2, ydiff2, 16));

			uint8x16_t final = vsqaddq_u8(avg, ydiffrgb);

			if constexpr (T_UseSignedPalette) {
				final = vqsubq_u8(final, x40b);
				final = vqaddq_u8(final, final);
			}

			vst1q_u32(dst32, vreinterpretq_u32_u8(final));
		} else if constexpr (T_UseSignedPalette) {
			uint8x16_t final = next;

			final = vqsubq_u8(final, x40b);
			final = vqaddq_u8(final, final);

			vst1q_u32(dst32, vreinterpretq_u32_u8(final));
		}

		delay32 += 4;
		dst32 += 4;
	}

	for(uint32 i=0; i<n1; ++i) {
		uint32 prev32 = *delay32;
		uint32 next32 = *dst32;

		if (prev32 != next32) {
			*delay32 = next32;

			uint8x8_t next = vcreate_u8(next32);
			uint8x8_t prev = vcreate_u8(prev32);
			uint8x8_t avg = vrhadd_u8(prev, next);
			uint32x2_t ydiff = vreinterpret_u32_u8(vsub_u8(next, avg));
			uint32x2_t ydiff2 = vsri_n_u32(ydiff, ydiff, 8);
			uint8x8_t ydiffrgb = vreinterpret_u8_u32(vsri_n_u32(ydiff2, ydiff2, 16));

			uint8x8_t final = vsqadd_u8(avg, ydiffrgb);

			if constexpr (T_UseSignedPalette) {
				final = vqsub_u8(final, vget_low_u8(x40b));
				final = vqadd_u8(final, final);
			}

			*dst32 = vget_lane_u32(vreinterpret_u32_u8(final), 0);
		} else if constexpr (T_UseSignedPalette) {
			uint8x8_t final = vcreate_u8(next32);

			final = vqsub_u8(final, vget_low_u8(x40b));
			final = vqadd_u8(final, final);

			*dst32 = vget_lane_u32(vreinterpret_u32_u8(final), 0);
		}

		++delay32;
		++dst32;
	}
}

void ATArtifactPAL32_NEON(void *dst, void *delayLine, uint32 n, bool compressExtendedRange) {
	if (compressExtendedRange)
		ATArtifactPAL32_NEON<true>(dst, delayLine, n);
	else
		ATArtifactPAL32_NEON<false>(dst, delayLine, n);
}

void ATArtifactPALFinalMono_NEON(uint32 *dst0, const uint32 *ybuf, uint32 n, const uint32 palette0[256]) {
	const uint32 *VDRESTRICT ysrc = ybuf;
	const uint32 *VDRESTRICT palette = palette0;
	uint32 *VDRESTRICT dst = dst0;

	n >>= 2;

	// Compute coefficients.
	//
	// Luma is signed 12.6 and we're going to do a high multiply followed by a rounded halving
	// for a total right shift of 17 bits. This means that we need to convert our tint color to
	// a normalized 5.11 fraction.

	for(uint32 i=0; i<n; ++i) {
		const int16x8_t y = vreinterpretq_s16_u32(vld1q_u32(ysrc));
		ysrc += 4;

		// convert signed 12.6 to u8
		const uint8x8_t indices = vqrshrun_n_s16(y, 6);

		// convert 8 pixels
		uint32x4x2_t pixels;
		pixels.val[0] = vdupq_n_u32(palette[vget_lane_u8(indices, 0)]);
		pixels.val[0] = vsetq_lane_u32(palette[vget_lane_u8(indices, 1)], pixels.val[0], 1);
		pixels.val[0] = vsetq_lane_u32(palette[vget_lane_u8(indices, 2)], pixels.val[0], 2);
		pixels.val[0] = vsetq_lane_u32(palette[vget_lane_u8(indices, 3)], pixels.val[0], 3);
		pixels.val[1] = vdupq_n_u32(palette[vget_lane_u8(indices, 4)]);
		pixels.val[1] = vsetq_lane_u32(palette[vget_lane_u8(indices, 5)], pixels.val[1], 1);
		pixels.val[1] = vsetq_lane_u32(palette[vget_lane_u8(indices, 6)], pixels.val[1], 2);
		pixels.val[1] = vsetq_lane_u32(palette[vget_lane_u8(indices, 7)], pixels.val[1], 3);
		vst1q_u32_x2(dst, pixels);
		dst += 8;
	}
}

#endif
