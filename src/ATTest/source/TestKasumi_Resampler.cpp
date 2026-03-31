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
#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/memory.h>
#include <vd2/system/time.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/resample.h>
#include "test.h"

AT_DEFINE_TEST(Kasumi_Resampler) {
	uint8 dst[201*201*4];
	uint8 src[201*201*4];

	static constexpr size_t kGuardHeader = 201*4 + 4;

	VDPixmap pxdst;
	pxdst.pitch = 201;
	pxdst.format = nsVDPixmap::kPixFormat_Y8;

	VDPixmap pxsrc;
	pxsrc.pitch = 201;
	pxsrc.format = nsVDPixmap::kPixFormat_Y8;

	static constexpr IVDPixmapResampler::FilterMode kFilterModes[]={
		IVDPixmapResampler::kFilterLinear,
		IVDPixmapResampler::kFilterCubic,
		IVDPixmapResampler::kFilterLanczos3,
	};

	static constexpr int kSizes[] {
		1, 2, 3, 4, 5, 7, 8,
		11, 27,
		127, 147
	};

	for(int fmodei = 0; fmodei < sizeof(kFilterModes)/sizeof(kFilterModes[0]); ++fmodei) {
		IVDPixmapResampler::FilterMode filterMode = kFilterModes[fmodei];

		pxdst.h = 16;
		pxsrc.h = 16;
		for(int dalign = 0; dalign < 3; ++dalign) {
			for(int dx : kSizes) {
				pxdst.data = dst + kGuardHeader + dalign;
				pxdst.w = dx;
				pxdst.h = dx;

				for(int sx : kSizes) {
					AT_TEST_TRACEF("Testing Y8 %dx%d -> %dx%d", sx, sx, dx, dx);
					VDMemset8(src, 0x40, sizeof src);
					VDMemset8(dst, 0xCD, sizeof dst);

					pxsrc.w = sx;
					pxsrc.h = sx;
					pxsrc.data = src + kGuardHeader;

					for(int i=0; i<sx; ++i)
						VDMemset8((char *)pxsrc.data + pxsrc.pitch*i, 0xA0, sx);

					VDPixmapResample(pxdst, pxsrc, filterMode);

					TEST_ASSERT(!VDMemCheck8(dst, 0xCD, kGuardHeader + dalign));
					for(int i=0; i<dx; ++i) {
						uint8 *p = pxdst.GetPixelRow<uint8>(i);
						TEST_ASSERT(!VDMemCheck8(p, 0xA0, dx));
						TEST_ASSERT(!VDMemCheck8(p + dx, 0xCD, 201 - dx));
					}
				}
			}

			if (fmodei > 0)
				break;
		}
	}

	pxdst.pitch = 201*4;
	pxdst.format = nsVDPixmap::kPixFormat_XRGB8888;

	pxsrc.pitch = 201*4;
	pxsrc.format = nsVDPixmap::kPixFormat_XRGB8888;

	for(int fmodei = 0; fmodei < sizeof(kFilterModes)/sizeof(kFilterModes[0]); ++fmodei) {
		IVDPixmapResampler::FilterMode filterMode = kFilterModes[fmodei];

		pxdst.h = 16;
		pxsrc.h = 16;
		for(int dalign = 0; dalign < 12; dalign += 4) {
			for(int dx : kSizes) {
				pxdst.data = dst + kGuardHeader + dalign;
				pxdst.w = dx;
				pxdst.h = dx;

				for(int sx : kSizes) {
					AT_TEST_TRACEF("Testing XRGB8888 %dx%d -> %dx%d", sx, sx, dx, dx);

					VDMemset8(src, 0x40, sizeof src);
					VDMemset8(dst, 0xCD, sizeof dst);

					pxsrc.w = sx;
					pxsrc.h = sx;
					pxsrc.data = src + kGuardHeader;

					for(int i=0; i<sx; ++i)
						VDMemset8(pxsrc.GetPixelRow<void>(i), 0xA0, sx*4);

					VDPixmapResample(pxdst, pxsrc, filterMode);

					TEST_ASSERT(!VDMemCheck8(dst, 0xCD, kGuardHeader + dalign));
					for(int i=0; i<dx; ++i) {
						const uint8 *px = pxdst.GetPixelRow<uint8>(i);

						for(int j=0; j<dx; ++j) {
							TEST_ASSERT(px[0] == 0xA0);
							TEST_ASSERT(px[1] == 0xA0);
							TEST_ASSERT(px[2] == 0xA0);
							px += 4;
						}

						TEST_ASSERT(!VDMemCheck8(px, 0xCD, (201-dx) * 4));
					}
				}
			}
		}
	}

	return 0;
}

AT_DEFINE_TEST_NONAUTO(Kasumi_ResamplerBench) {
	static constexpr struct TestInfo {
		int mSrcSize;
		int mDstSize;
		int mFormat;
		IVDPixmapResampler::FilterMode mFilterMode;
	} kTests[] {
		{  256,  256, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterLinear },
		{  256, 1024, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterLinear },
		{  256, 4096, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterLinear },
		{  256,  256, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterLinear },
		{ 1024,  256, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterLinear },
		{ 4096,  256, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterLinear },
		{  900, 1100, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterLinear },
		{ 1100,  900, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterLinear },

		{  256,  256, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterCubic },
		{  256, 1024, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterCubic },
		{  256, 4096, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterCubic },
		{  256,  256, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterCubic },
		{ 1024,  256, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterCubic },
		{ 4096,  256, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterCubic },
		{  900, 1100, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterCubic },
		{ 1100,  900, nsVDPixmap::kPixFormat_Y8, IVDPixmapResampler::FilterMode::kFilterCubic },

		{  256,  256, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterLinear },
		{  256, 1024, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterLinear },
		{  256, 4096, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterLinear },
		{  256,  256, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterLinear },
		{ 1024,  256, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterLinear },
		{ 4096,  256, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterLinear },
		{  900, 1100, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterLinear },
		{ 1100,  900, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterLinear },

		{  256,  256, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterCubic },
		{  256, 1024, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterCubic },
		{  256, 4096, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterCubic },
		{  256,  256, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterCubic },
		{ 1024,  256, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterCubic },
		{ 4096,  256, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterCubic },
		{  900, 1100, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterCubic },
		{ 1100,  900, nsVDPixmap::kPixFormat_XRGB8888, IVDPixmapResampler::FilterMode::kFilterCubic },
	};

	for(const TestInfo& testInfo : kTests) {
		VDPixmapBuffer pxsrc(testInfo.mSrcSize, testInfo.mSrcSize, testInfo.mFormat);
		VDPixmapBuffer pxdst(testInfo.mDstSize, testInfo.mDstSize, testInfo.mFormat);

		memset(pxsrc.base(), 0, pxsrc.size());
		memset(pxdst.base(), 0, pxdst.size());

		vdautoptr<IVDPixmapResampler> p { VDCreatePixmapResampler() };
		p->SetFilters(testInfo.mFilterMode, testInfo.mFilterMode, false);
		p->Init(pxdst.w, pxdst.h, pxdst.format, pxsrc.w, pxsrc.h, pxsrc.format);

		sint64 minTime = (sint64)1 << 62;

		for(int i=0; i<20; ++i) {
			uint64 t0 = VDGetPreciseTick();
			p->Process(pxdst, pxsrc);

			sint64 dt = (sint64)(VDGetPreciseTick() - t0);

			if (dt < minTime)
				minTime = dt;
		}

		double secs = (double)minTime * VDGetPreciseSecondsPerTick();

		printf("%4dx%-4d -> %4dx%-4d | %-10s | %d | %5.1f ms | %7.1f Mpx/sec | %7.1f Mpx/sec\n"
			, pxsrc.w
			, pxsrc.h
			, pxdst.w
			, pxdst.h
			, VDPixmapGetInfo(testInfo.mFormat).name
			, testInfo.mFilterMode
			, secs * 1000.0
			, (pxsrc.w * pxsrc.h) / (secs * 1000000.0)
			, (pxdst.w * pxdst.h) / (secs * 1000000.0)
		);
	}

	return 0;
}
