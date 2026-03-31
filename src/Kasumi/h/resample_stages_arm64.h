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

#ifndef f_VD2_KASUMI_RESAMPLE_STAGES_ARM64_H
#define f_VD2_KASUMI_RESAMPLE_STAGES_ARM64_H

#include "resample_stages_reference.h"

struct VDResamplerAxis;

#ifdef VD_CPU_ARM64

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (ARM64)
//
///////////////////////////////////////////////////////////////////////////

class VDResamplerSeparableTableRowStageNEON final : public VDResamplerRowStageSeparableTable32 {
public:
	VDResamplerSeparableTableRowStageNEON(const IVDResamplerFilter& filter);

	void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) override;

private:
	template<bool T_FilterSizeOddPair = false, typename T_FilterSize>
	void Filter(void *dst, const void *src, size_t w, const sint16 *filters, T_FilterSize filterSize, uint32 u, uint32 dudx);

	vdblock<sint16, vdaligned_alloc<sint16>> mFilterBank16;
};

class VDResamplerSeparableTableColStageNEON final : public VDResamplerColStageSeparableTable32 {
public:
	VDResamplerSeparableTableColStageNEON(const IVDResamplerFilter& filter);

	void Process(void *dst, const void *const *src, uint32 w, sint32 phase) override;

private:
	template<typename T_FilterSize>
	void Filter(uint8 *VDRESTRICT dst, const void* const *src, uint32 w, const sint16 *VDRESTRICT filter, T_FilterSize filterSize);

	vdblock<sint16, vdaligned_alloc<sint16>> mFilterBank16;
};

class VDResamplerSeparableTableRowStage8NEON final : public VDResamplerRowStageSeparableTable32, public IVDResamplerSeparableRowStage2 {
public:
	VDResamplerSeparableTableRowStage8NEON(const IVDResamplerFilter& filter);
	
	IVDResamplerSeparableRowStage2 *AsRowStage2() override { return this; } 

	void Init(const VDResamplerAxis& axis, uint32 srcw) override;
	void Process(void *dst, const void *src, uint32 w) override;

	void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) override;

private:
	vdblock<sint16, vdaligned_alloc<sint16, 16>> mRowFilters;
	vdblock<uint8> mTempBuffer;
	vdblock<uint16> mFastLerpOffsets;
	uint32 mSrcWidth;
	uint32 mNumFastGroups;
	bool mbUseFastLerp;
};

class VDResamplerSeparableTableColStage8NEON final : public VDResamplerColStageSeparableTable32 {
public:
	VDResamplerSeparableTableColStage8NEON(const IVDResamplerFilter& filter);

	void Process(void *dst, const void *const *src, uint32 w, sint32 phase);

private:
	vdblock<sint16, vdaligned_alloc<sint16>> mFilterBank16;
	bool mbUseFastLerp;
};
#endif

#endif
