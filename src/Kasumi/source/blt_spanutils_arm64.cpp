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

#ifdef VD_CPU_ARM64
namespace nsVDPixmapSpanUtils {
	void VDCDECL horiz_compress2x_centered_NEON(uint8 *dst, const uint8 *src, sint32 w) {
		if (w == 1) {
			*dst = *src;
			return;
		}

		if (w == 2) {
			*dst = (uint8)((src[0] + src[1] + 1) >> 1);
			return;
		}

		*dst++ = (uint8)((4*src[0] + 3*src[1] + src[2] + 4) >> 3);
		--w;
		++src;

		uint16x8_t coeff = vdupq_n_u8(0x03);

		while(w >= 32) {
			uint8x16x2_t v0 = vld2q_u8(src);
			uint8x16x2_t v1 = vld2q_u8(src + 2);

			uint16x8_t accum1 = vaddl_u8(vget_low_u8(v0.val[0]), vget_low_u8(v1.val[1]));
			uint16x8_t accum2 = vaddl_high_u8(v0.val[0], v1.val[1]);

			accum1 = vmlal_u8(accum1, vget_low_u8(v0.val[1]), vget_low_u8(coeff));
			accum2 = vmlal_high_u8(accum2, v0.val[1], coeff);

			accum1 = vmlal_u8(accum1, vget_low_u8(v1.val[0]), vget_low_u8(coeff));
			accum2 = vmlal_high_u8(accum2, v1.val[0], coeff);

			vst1q_u8(dst, vcombine_u8(vqrshrn_n_u16(accum1, 3), vqrshrn_n_u16(accum2, 3)));

			w -= 32;
			src += 32;
			dst += 16;
		}

		if (w >= 16) {
			uint8x8x2_t v0 = vld2_u8(src);
			uint8x8x2_t v1 = vld2_u8(src + 2);

			uint16x8_t accum = vaddl_u8(v0.val[0], v1.val[1]);
			accum = vmlal_u8(accum, v0.val[1], vget_low_u8(coeff));
			accum = vmlal_u8(accum, v1.val[0], vget_low_u8(coeff));

			vst1_u8(dst, vqrshrn_n_u16(accum, 3));

			w -= 16;
			src += 16;
			dst += 8;
		}

		while(w >= 4) {
			w -= 2;
			*dst++ = (uint8)(((src[0] + src[3]) + 3*(src[1] + src[2]) + 4) >> 3);
			src += 2;
		}

		switch(w) {
		case 3:
			*dst++ = (uint8)((src[0] + 3*src[1] + 4*src[2] + 4) >> 3);
			break;
		case 2:
			*dst++ = (uint8)((src[0] + 7*src[1] + 4) >> 3);
			break;
		}
	}
}
#endif
