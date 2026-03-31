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

#include "stdafx.h"
#include <vd2/VDDisplay/internal/customeffectpassbase.h>
#include <vd2/VDDisplay/internal/customeffectutils.h>

////////////////////////////////////////////////////////////////////////////////

VDDCustomEffectFrameRef VDDCustomEffectFrameRef::Parse(VDStringSpanA name, uint32 currentPassIndex) {
	if (name.empty())
		return {};

	if (name[0] == '$')
		name.remove_prefix(1);

	if (name == "IN")
		return VDDCustomEffectFrameRef { true, currentPassIndex, 0 };

	if (name.size() >= 4) {
		if (name == "ORIG")
			return VDDCustomEffectFrameRef { true, 0, 0 };

		uint32 index = 0;
		bool hasIndex = false;

		if (name.back() >= '0' && name.back() <= '9') {
			hasIndex = true;

			while(!name.empty()) {
				const char ch = name.back();

				if (ch < '0' || ch > '9')
					break;

				if (index > UINT32_MAX/10)
					return {};

				index *= 10;

				uint32 digit = (uint32)(ch - '0');
				if (UINT32_MAX - index < digit)
					return {};

				index += digit;
				name.remove_suffix(1);
			}
		}

		if (name == "PREV") {
			if (hasIndex && (index < 1 || index > 6))
				throw VDException("Invalid reference from pass %u to parameter '%s'", currentPassIndex, VDStringA(name).c_str());
			
			return VDDCustomEffectFrameRef { true, 0, index + 1 };
		}

		// PASSPREV = one pass behind IN
		// PASSPREV1 = two passes behind IN, etc.
		if (name == "PASSPREV") {
			if ((hasIndex && !index) || index >= currentPassIndex)
				throw VDException("Invalid reference from pass %u to parameter '%s'", currentPassIndex, VDStringA(name).c_str());

			return VDDCustomEffectFrameRef { true, currentPassIndex - (index + 1), 0 };
		}

		if (name == "PASS") {
			if (!hasIndex || (index < 1 || index+1 > currentPassIndex))
				throw VDException("Invalid reference from pass %u to parameter '%s'", currentPassIndex, VDStringA(name).c_str());

			return VDDCustomEffectFrameRef { true, index, 0 };
		}
	}

	return {};
}

VDDCustomEffectFrameRef VDDCustomEffectFrameRef::Parse(const char *name, uint32 currentPassIndex) {
	return Parse(VDStringSpanA(name), currentPassIndex);
}

////////////////////////////////////////////////////////////////////////////////

VDDCustomEffectVarStorage::VDDCustomEffectVarStorage() {
	mData.resize(4, 0);
}

VDDCustomEffectVarStorage::~VDDCustomEffectVarStorage() = default;

VDDCustomEffectVec4Gather VDDCustomEffectVarStorage::RequestVector(ptrdiff_t dstOffset, VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex) {
	const uint32 offset = AllocateVarOffset(var, passIndex, elementIndex);

	return VDDCustomEffectVec4Gather {
		(uint32)dstOffset,
		{
			offset,
			offset + 4,
			offset + 8,
			offset + 12
		}
	};
}

VDDCustomEffectVec4x4Gather VDDCustomEffectVarStorage::RequestRowMajorMatrix(ptrdiff_t dstOffset, VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex) {
	VDDCustomEffectVec4x4Gather gather {};

	if (var != VDDCustomEffectVariable::ModelViewProj) {
		// Requesting a matrix out of a vector -- pull the vector and add three zero
		// rows. We take advantage of a known zero in storage.
		gather.mVec[0] = RequestVector(dstOffset, var, passIndex, elementIndex);
		gather.mVec[1] = VDDCustomEffectVec4Gather { (uint32)(dstOffset + 16), { kZeroOffset, kZeroOffset, kZeroOffset, kZeroOffset } };
		gather.mVec[2] = VDDCustomEffectVec4Gather { (uint32)(dstOffset + 32), { kZeroOffset, kZeroOffset, kZeroOffset, kZeroOffset } };
		gather.mVec[3] = VDDCustomEffectVec4Gather { (uint32)(dstOffset + 48), { kZeroOffset, kZeroOffset, kZeroOffset, kZeroOffset } };
	} else {
		const uint32 offset = AllocateVarOffset(var, passIndex, elementIndex);

		for(int i=0; i<4; ++i) {
			gather.mVec[i] = VDDCustomEffectVec4Gather {
				(uint32)(dstOffset + 16 * i),
				{
					(uint32)(offset + sizeof(float)*(i*4 + 0)),
					(uint32)(offset + sizeof(float)*(i*4 + 1)),
					(uint32)(offset + sizeof(float)*(i*4 + 2)),
					(uint32)(offset + sizeof(float)*(i*4 + 3))
				}
			};
		}		
	}

	return gather;
}

VDDCustomEffectVec4x4Gather VDDCustomEffectVarStorage::RequestColumnMajorMatrix(ptrdiff_t dstOffset, VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex) {
	VDDCustomEffectVec4x4Gather gather = RequestRowMajorMatrix(dstOffset, var, passIndex, elementIndex);

	return VDDCustomEffectVec4x4Gather {
		{
			{
				(uint32)dstOffset,
				{
					gather.mVec[0].mOffset[0],
					gather.mVec[1].mOffset[0],
					gather.mVec[2].mOffset[0],
					gather.mVec[3].mOffset[0],
				},
			},
			{
				(uint32)(dstOffset + 16),
				{
					gather.mVec[0].mOffset[1],
					gather.mVec[1].mOffset[1],
					gather.mVec[2].mOffset[1],
					gather.mVec[3].mOffset[1],
				},
			},
			{
				(uint32)(dstOffset + 32),
				{
					gather.mVec[0].mOffset[2],
					gather.mVec[1].mOffset[2],
					gather.mVec[2].mOffset[2],
					gather.mVec[3].mOffset[2],
				},
			},
			{
				(uint32)(dstOffset + 48),
				{
					gather.mVec[0].mOffset[3],
					gather.mVec[1].mOffset[3],
					gather.mVec[2].mOffset[3],
					gather.mVec[3].mOffset[3],
				},
			},
		}
	};
}

sint32 VDDCustomEffectVarStorage::GetVarOffset(VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex) const {
	// Most parameters do not care about elementIndex
	if (var != VDDCustomEffectVariable::FrameCount)
		elementIndex = 0;

	const auto it = mVarMap.find(VDDCustomEffectVarAddress { var, passIndex, elementIndex });

	return it != mVarMap.end() ? (sint32)it->second : -1;
}

void VDDCustomEffectVarStorage::ResolvePassParamOffsets(VDDCEPassParamOffsets& offsets, uint32 passIndex) const {
	offsets.mVideoSize		= GetVarOffset(VDDCustomEffectVariable::VideoSize		, passIndex, 0);
	offsets.mTextureSize	= GetVarOffset(VDDCustomEffectVariable::TextureSize		, passIndex, 0);
	offsets.mOutputSize		= GetVarOffset(VDDCustomEffectVariable::OutputSize		, passIndex, 0);
	offsets.mFrameDirection	= GetVarOffset(VDDCustomEffectVariable::FrameDirection	, passIndex, 0);
}

void VDDCustomEffectVarStorage::ResolveFrameParamOffsets(VDDCEFrameParamOffsets& offsets, uint32 passIndex, uint32 frameIndex) const {
	offsets.mFrameCount		= GetVarOffset(VDDCustomEffectVariable::FrameCount		, passIndex, frameIndex);
}

void VDDCustomEffectVarStorage::SetVector(sint32 offset, const vdfloat4& v) {
	if (offset >= 0)
		memcpy((char *)mData.data() + offset, &v, 16);
}

void VDDCustomEffectVarStorage::SetMatrix(sint32 offset, const vdfloat4x4& m) {
	if (offset >= 0)
		memcpy((char *)mData.data() + offset, &m, 64);
}

void VDDCustomEffectVarStorage::GatherBools(uint32 *dst, vdspan<const VDDCustomEffectScalarGather> gathers) const {
	const char *VDRESTRICT src = (const char *)mData.data();

	for(const VDDCustomEffectScalarGather& vecGather : gathers) {
		uint32 *VDRESTRICT dst2 = (uint32 *)((char *)dst + vecGather.mDstOffset);
		const float *VDRESTRICT src2 = (const float *)(src + vecGather.mSrcOffset);

		*dst2 = (*src2 != 0);
	}
}

void VDDCustomEffectVarStorage::GatherFloats(uint32 *dst, vdspan<const VDDCustomEffectScalarGather> gathers) const {
	const char *VDRESTRICT src = (const char *)mData.data();

	for(const VDDCustomEffectScalarGather& vecGather : gathers) {
		uint32 *VDRESTRICT dst2 = (uint32 *)((char *)dst + vecGather.mDstOffset);
		const uint32 *VDRESTRICT src2 = (const uint32 *)(src + vecGather.mSrcOffset);

		*dst2 = *src2;
	}
}

void VDDCustomEffectVarStorage::GatherVecs(uint32 *dst, vdspan<const VDDCustomEffectVec4Gather> gathers) const {
	const char *VDRESTRICT src = (const char *)mData.data();

	for(const VDDCustomEffectVec4Gather& vecGather : gathers) {
		uint32 *VDRESTRICT dst2 = (uint32 *)((char *)dst + vecGather.mDstOffset);

		memcpy(&dst2[0], src + vecGather.mOffset[0], sizeof(dst2[0]));
		memcpy(&dst2[1], src + vecGather.mOffset[1], sizeof(dst2[1]));
		memcpy(&dst2[2], src + vecGather.mOffset[2], sizeof(dst2[2]));
		memcpy(&dst2[3], src + vecGather.mOffset[3], sizeof(dst2[3]));
	}
}

uint32 VDDCustomEffectVarStorage::AllocateVarOffset(VDDCustomEffectVariable var, uint32 passIndex, uint32 elementIndex) {
	const auto r = mVarMap.insert_as(VDDCustomEffectVarAddress { var, passIndex, elementIndex });

	if (r.second) {
		uint32 varSize = 4;

		if (var == VDDCustomEffectVariable::ModelViewProj)
			varSize = 16;

		size_t index = mData.size();
		mData.resize(index + varSize, 0);

		r.first->second = (uint32)(index * 4);
	}

	return r.first->second;
}

////////////////////////////////////////////////////////////////////////////////

VDDCustomEffectPassBase::VDDCustomEffectPassBase(uint32 passIndex)
	: mPassIndex(passIndex)
{
}

VDDCustomEffectPassBase::~VDDCustomEffectPassBase() {
}

void VDDCustomEffectPassBase::ParseCommonProps(const VDDisplayCustomShaderProps& props) {
	static constexpr const char *kScaleTypePropNames[]={
		"scale_type",
		"scale_type_x",
		"scale_type_y",
	};

	ScaleType scaleTypes[3];
	uint32 scaleTypesFound = 0;

	for(uint32 i=0; i<3; ++i) {
		scaleTypes[i] = kScaleType_Source;

		const char *scaleTypeProp = props.GetString(VDDCsPropKeyView(kScaleTypePropNames[i], mPassIndex));
		if (scaleTypeProp) {
			VDStringSpanA scaleTypeStr(scaleTypeProp);

			if (scaleTypeStr == "source")
				scaleTypes[i] = kScaleType_Source;
			else if (scaleTypeStr == "viewport")
				scaleTypes[i] = kScaleType_Viewport;
			else if (scaleTypeStr == "absolute")
				scaleTypes[i] = kScaleType_Absolute;
			else
				throw MyError("Pass %u has invalid scale mode: \"%s\"", mPassIndex, scaleTypeProp);

			scaleTypesFound |= 1 << i;
		}
	}

	if (scaleTypesFound & 1) {
		mScaleTypeX = scaleTypes[0];
		mScaleTypeY = scaleTypes[0];
	} else {
		mScaleTypeX = scaleTypes[1];
		mScaleTypeY = scaleTypes[2];
	}

	mbHasScalingFactor = (scaleTypesFound != 0);
	mScaleFactorX = 1.0f;
	mScaleFactorY = 1.0f;

	if (mbHasScalingFactor) {
		float scaleFactors[3];
		uint32 scaleFactorsFound = 0;

		static constexpr const char *kScaleFactorPropNames[]={
			"scale",
			"scale_x",
			"scale_y",
		};

		for(uint32 i=0; i<3; ++i) {
			const char *scaleProp = props.GetString(VDDCsPropKeyView(kScaleFactorPropNames[i], mPassIndex));
			float factor = 1.0f;

			if (scaleProp) {
				if (i == 0 && mScaleTypeX != mScaleTypeY)
					throw MyError("Pass %u: can't use a single scale factor with mixed scale types", mPassIndex);

				char dummy;
				if (1 != sscanf(scaleProp, "%g%c", &factor, &dummy) || !(factor > 0) || !(factor < 16384.0f))
					throw MyError("Pass %u has invalid scale factor: %s", mPassIndex, scaleProp);

				scaleFactorsFound |= 1 << i;
			}

			scaleFactors[i] = factor;
		}

		if (scaleFactorsFound == 0) {
			mScaleTypeX = kScaleType_Source;
			mScaleTypeY = kScaleType_Source;
			mScaleFactorX = 1.0f;
			mScaleFactorY = 1.0f;
		} else if (scaleFactorsFound & 1) {
			mScaleFactorX = scaleFactors[0];
			mScaleFactorY = scaleFactors[0];
		} else {
			if (scaleFactorsFound & 2)
				mScaleFactorX = scaleFactors[1];
			else
				mScaleTypeX = kScaleType_Source;

			if (scaleFactorsFound & 4)
				mScaleFactorY = scaleFactors[2];
			else
				mScaleTypeY = kScaleType_Source;
		}
	}

	const char *frameCountModProp = props.GetString(VDDCsPropKeyView("frame_count_mod", mPassIndex));
	if (frameCountModProp) {
		unsigned mod;
		char dummy;
		if (1 != sscanf(frameCountModProp, "%u%c", &mod, &dummy) || mod == 0)
			throw VDException("Pass %u has invalid frame_count_mod value: %s", mPassIndex, frameCountModProp);

		mFrameCountLimit = mod - 1;
	}

	mbSrgbFramebuffer = props.GetBool(VDDCsPropKeyView("srgb_framebuffer", mPassIndex), false);
	mbFloatFramebuffer = props.GetBool(VDDCsPropKeyView("float_framebuffer", mPassIndex), false);

	if (mbFloatFramebuffer) {
		if (mbSrgbFramebuffer)
			throw VDException("Pass %u error: cannot request floating-point sRGB framebuffer", mPassIndex);

		mbHalfFloatFramebuffer = props.GetBool(VDDCsPropKeyView("halffloat_framebuffer", mPassIndex), false);
	}

	mbFilterInput = props.GetBool(VDDCsPropKeyView("filter_linear", mPassIndex), true);
}

vdint2 VDDCustomEffectPassBase::ComputeRenderSize(const vdint2& srcSize, const vdint2& viewportSize) const {
	vdint2 renderSize {};

	switch(mScaleTypeX) {
		case kScaleType_Source:
			renderSize.x = VDRoundToInt((float)srcSize.x * mScaleFactorX);
			break;

		case kScaleType_Viewport:
			renderSize.x = VDRoundToInt((float)viewportSize.x * mScaleFactorX);
			break;

		case kScaleType_Absolute:
			renderSize.x = VDRoundToInt(mScaleFactorX);
			break;

		default:
			renderSize.x = 1;
			break;
	}

	switch(mScaleTypeY) {
		case kScaleType_Source:
			renderSize.y = VDRoundToInt((float)srcSize.y * mScaleFactorY);
			break;

		case kScaleType_Viewport:
			renderSize.y = VDRoundToInt((float)viewportSize.y * mScaleFactorY);
			break;

		case kScaleType_Absolute:
			renderSize.y = VDRoundToInt(mScaleFactorY);
			break;

		default:
			renderSize.y = 1;
			break;
	}

	if (renderSize.x < 1)
		renderSize.x = 1;

	if (renderSize.y < 1)
		renderSize.y = 1;

	return renderSize;
}
