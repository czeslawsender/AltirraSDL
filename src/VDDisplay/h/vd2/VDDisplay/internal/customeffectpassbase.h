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

#ifndef f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFEcTPASS_H
#define f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFEcTPASS_H

#include <vd2/system/vectors.h>

class VDDisplayCustomShaderProps;

struct VDDCustomEffectFrameRef {
	bool mbValid;
	uint32 mPassIndex;
	uint32 mElementIndex;

	static VDDCustomEffectFrameRef Parse(VDStringSpanA name, uint32 currentPassIndex);
	static VDDCustomEffectFrameRef Parse(const char *name, uint32 currentPassIndex);
};

enum class VDDCustomEffectVariable : uint32 {
	None,
	VideoSize,			// float4(video_width, video_height, 0, 0)
	TextureSize,		// float4(texture_width, texture_height, 0, 0)
	OutputSize,			// float4(output_width, output_height, 0, 0)
	FrameCount,			// float4(frame_count, 0, 0, 0)
	FrameDirection,		// float4(frame_direction, 0, 0, 0)
	ModelViewProj,		// float4x4(model_view_projection)
};

enum class VDDCustomEffectVarClass : uint32 {
	Global,
	Texture
};

struct VDDCustomEffectVarRef {
	VDDCustomEffectVariable mVar;
	uint32 mVarIndex;
	uint32 mVarOffset;
};

struct VDDCEPassParamOffsets {
	sint32 mVideoSize = -1;
	sint32 mTextureSize = -1;
	sint32 mOutputSize = -1;
	sint32 mFrameDirection = -1;
};

struct VDDCEFrameParamOffsets {
	sint32 mFrameCount = -1;
};

struct VDDCustomEffectScalarGather {
	uint32 mDstOffset;
	uint32 mSrcOffset;
};

struct VDDCustomEffectVec4Gather {
	uint32 mDstOffset;
	uint32 mOffset[4];
};

struct VDDCustomEffectVec4x4Gather {
	VDDCustomEffectVec4Gather mVec[4];
};

struct VDDCustomEffectVarGather {
	vdfastvector<VDDCustomEffectVec4Gather> mVecGathers;
};

struct VDDCustomEffectVarAddress {
	VDDCustomEffectVariable mVar;
	uint32 mPassIndex;
	uint32 mElementIndex;

	bool operator==(const VDDCustomEffectVarAddress&) const = default;
};

struct VDDCustomEffectVarAddressHash {
	size_t operator()(const VDDCustomEffectVarAddress& v) const {
		return (uint32)v.mVar + (v.mPassIndex << 8) + (v.mElementIndex << 16);
	}
};

enum VDDCETextureType : uint8 {
	None,
	PassInput,
	Custom
};

struct VDDCETextureKey {
	VDDCETextureType mType {};
	uint8 mPassIndex {};
	uint16 mTextureIndex {};
};

struct VDDCETextureSpec {
	uint32 mTexWidth = 1;
	uint32 mTexHeight = 1;
	uint32 mImageWidth = 1;
	uint32 mImageHeight = 1;
	bool mbLinear = false;
	bool mbSrgb = false;

	VDDCEFrameParamOffsets mFrameParams {};
};

class VDDCustomEffectVarStorage {
public:
	static constexpr uint32 kZeroOffset = 0;

	VDDCustomEffectVarStorage();
	~VDDCustomEffectVarStorage();

	VDDCustomEffectVec4Gather RequestVector(ptrdiff_t dstOffset, VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex);
	VDDCustomEffectVec4x4Gather RequestRowMajorMatrix(ptrdiff_t dstOffset, VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex);
	VDDCustomEffectVec4x4Gather RequestColumnMajorMatrix(ptrdiff_t dstOffset, VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex);

	sint32 GetVarOffset(VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex) const;
	void ResolvePassParamOffsets(VDDCEPassParamOffsets& offsets, uint32 passIndex) const;
	void ResolveFrameParamOffsets(VDDCEFrameParamOffsets& offsets, uint32 passIndex, uint32 frameIndex) const;
	
	void SetVector(sint32 offset, const vdfloat4& v);
	void SetMatrix(sint32 offset, const vdfloat4x4& m);

	void GatherBools(uint32 *dst, vdspan<const VDDCustomEffectScalarGather> gathers) const;
	void GatherFloats(uint32 *dst, vdspan<const VDDCustomEffectScalarGather> gathers) const;
	void GatherVecs(uint32 *dst, vdspan<const VDDCustomEffectVec4Gather> gathers) const;

private:
	uint32 AllocateVarOffset(VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex);

	vdfastvector<float> mData;
	vdfastvector<sint32> mPassElementMap;
	vdhashmap<VDDCustomEffectVarAddress, uint32, VDDCustomEffectVarAddressHash> mVarMap;
	
};

class VDDCustomEffectPassBase {
	VDDCustomEffectPassBase(const VDDCustomEffectPassBase&) = delete;
	VDDCustomEffectPassBase& operator=(const VDDCustomEffectPassBase&) = delete;

public:
	enum ScaleType : uint32 {
		kScaleType_Source,
		kScaleType_Viewport,
		kScaleType_Absolute,
	};

	VDDCustomEffectPassBase(uint32 passIndex);
	virtual ~VDDCustomEffectPassBase();

	bool HasScalingFactor() const { return mbHasScalingFactor; }
	bool IsInputFiltered() const { return mbFilterInput; }
	bool IsOutputHalfFloat() const { return mbHalfFloatFramebuffer; }
	bool IsOutputFloat() const { return mbFloatFramebuffer; }
	bool IsOutputSrgb() const { return mbSrgbFramebuffer; }
	uint32 GetFrameCountLimit() const { return mFrameCountLimit; }

	virtual void ResetVariables(VDDCustomEffectVarStorage& varStorage, uint32 prevOutputFramesNeeded, bool outputFilteringNeeded) = 0;

protected:
	void ParseCommonProps(const VDDisplayCustomShaderProps& props);
	vdint2 ComputeRenderSize(const vdint2& srcSize, const vdint2& viewportSize) const;

	const uint32 mPassIndex;

	uint32 mFrame = 0;
	uint32 mFrameCountLimit = UINT32_C(0xFFFFFFFF);

	bool mbHasScalingFactor = false;
	ScaleType mScaleTypeX = kScaleType_Source;
	ScaleType mScaleTypeY = kScaleType_Source;
	float mScaleFactorX = 0;
	float mScaleFactorY = 0;
	bool mbSrgbFramebuffer = false;
	bool mbFloatFramebuffer = false;
	bool mbHalfFloatFramebuffer = false;
	bool mbFilterInput = false;

	VDDCEPassParamOffsets mPassParamOffsets {};
};

#endif
