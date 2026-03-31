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

#ifndef f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECTIMPLD3D11_H
#define f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECTIMPLD3D11_H

#include <vd2/system/vdstl.h>
#include <vd2/Tessa/Context.h>
#include <vd2/VDDisplay/internal/customeffectd3d11.h>
#include <vd2/VDDisplay/internal/customeffectbase.h>
#include <vd2/VDDisplay/internal/customeffectpassbase.h>
#include <vd2/VDDisplay/internal/customeffectutils.h>
#include "displaynode3d.h"

class VDDCustomEffectCompilerD3D;
class VDDisplayContext3D;
class VDDCustomEffectD3D11;

class VDDCustomEffectPassD3D11 final : public VDDCustomEffectPassBase {
public:
	struct TextureSpec : public VDDCETextureSpec {
		IVDTTexture2D *mpTexture = nullptr;
		VDDPoolTextureIndex mTexturePoolIndex {};
	};

	VDDCustomEffectPassD3D11(VDDCustomEffectD3D11& parent, IVDTContext& context, VDDisplayNodeContext3D& dctx, VDDisplayCommandList3D& cmdList, uint32 passIndex);

	void Init(
		const char *shaderPath,
		const VDDisplayCustomShaderProps& propLookup,
		const vdhashmap<VDStringA, uint32>& customTextureLookup,
		vdspan<const TextureSpec> customTextures,
		vdspan<const bool> passInputsFiltered,
		const wchar_t *basePath,
		vdspan<uint32> maxPrevFrames,
		VDDCustomEffectVarStorage& varStorage
	);

	void ResetVariables(VDDCustomEffectVarStorage& varStorage, uint32 prevFramesRequired, bool outputFiltered) override;

	VDDisplaySourceTexMapping ComputeOutputMapping(const vdint2& srcSize, const vdint2& viewportSize) const;

	void IncrementFrame();
	void SetPassUncacheable();

	bool Run(
		const vdrect32f *dstRect,
		vdspan<const VDDTexSpecView<const TextureSpec>> texSpecGrid,
		const vdint2& viewportSize,
		bool lastStage,
		vdspan<bool> inputInvalidationTable,
		VDDCustomEffectVarStorage& varStorage
	);

	VDDPoolRenderViewId GetRenderViewId() const {
		return mRenderViewId;
	}

	VDDPoolCommandIndex GetCommandIndex() const {
		return mCommandIndex;
	}

	VDDTexSpecView<const TextureSpec> GetOutputTexSpecs() const {
		return { std::from_range, mOutputTextures };
	}

private:
	struct SamplerBinding {
		uint32 mSamplerIndex = 0;
		VDDCETextureKey mTextureKey;
	};

	struct TextureBinding {
		uint32 mTextureIndex = 0;
		VDDPoolTextureIndex mPoolTextureIndex {};
		VDDCETextureKey mTextureKey;
	};

	struct ConstantBufferInfo {
		uint32 mCbIndex = 0;
		vdfastvector<uint32> mConstantBuffer;
		vdfastvector<VDDCustomEffectScalarGather> mFloatGathers;
	};

	struct IoBinding {
		VDStringA mSemantic;
		uint32 mSemanticIndex = 0;
		uint8 mComponentMask = 0;
		uint32 mRegisterIndex = 0;
	};

	struct ShaderInfo {
		vdvector<ConstantBufferInfo> mConstantBuffers;
		vdvector<SamplerBinding> mSamplers;
		vdvector<TextureBinding> mTextures;
		vdvector<IoBinding> mInputBindings;
		vdvector<IoBinding> mOutputBindings;
		uint32 mTextureSlotCount = 0;
		uint32 mSamplerSlotCount = 0;
	};

	struct Vertex {
		float x, y, z, w;
		uint32 mColor;
		vdfloat2 mUV0;
		vdfloat2 mUV1;
	};

	bool ProcessShader(
		VDDCustomEffectCompilerD3D& compiler,
		bool isPixelShader,
		const void *shader, size_t shaderSize, ShaderInfo& shaderInfo,
		const vdhashmap<VDStringA, uint32>& customTextureLookup,
		vdspan<const TextureSpec> customTextures,
		vdspan<uint32> maxPrevFrames, VDDCustomEffectVarStorage& varStorage);

	void ProcessConstantBuffer(ID3D11ShaderReflectionConstantBuffer& cb, ConstantBufferInfo& cbInfo, VDDCustomEffectVarStorage& varStorage);
	void ParseFrameStruct(uint32 varPassIndex, uint32 varElementIndex, const D3D11_SHADER_VARIABLE_DESC& varDesc, ID3D11ShaderReflectionType& type, const D3D11_SHADER_TYPE_DESC& typeDesc, ConstantBufferInfo& cbInfo, VDDCustomEffectVarStorage& varStorage);

	VDDCustomEffectD3D11& mParent;
	IVDTContext& mContext;
	VDDisplayNodeContext3D& mDisplayContext;
	VDDisplayCommandList3D& mCommandList;

	bool mbPassCanCache = false;

	VDDPoolCommandIndex mCommandIndex {};
	VDDPoolRenderViewId mRenderViewId {};

	vdrefptr<IVDTVertexProgram> mpVertexShader;
	vdrefptr<IVDTFragmentProgram> mpPixelShader;
	vdfastvector<uint8> mPassInputReferenceList;

	struct OutputTexture : public TextureSpec {
		vdrefptr<IVDTTexture2D> mpTextureRef;
	};

	vdvector<OutputTexture> mOutputTextures;
	vdrefptr<IVDTVertexFormat> mpVertexFormat;

	ShaderInfo mVertexShaderInfo;
	ShaderInfo mPixelShaderInfo;
};

class VDDCustomEffectD3D11 final : public VDDCustomEffectBase, public IVDDisplayCustomEffectD3D11 {
public:
	VDDCustomEffectD3D11(IVDTContext& context, VDDisplayNodeContext3D& dctx);
	~VDDCustomEffectD3D11();

	bool ContainsFinalBlit() const override;
	bool HasTimingInfo() const override;
	vdspan<const VDDisplayCustomShaderPassInfo> GetPassTimings() override;

	void PreRun() override;
	void Run(IVDTTexture2D *const *srcTextures, const vdint2& texSize, const vdint2& imageSize, const vdint2& viewportSize) override;
	void RunFinal(const VDDRenderView& renderView, const vdrect32f& dstRect, const vdint2& viewportSize) override;
	void PostRun() override;

	VDDisplaySourceTexMapping ComputeFinalOutputMapping(const vdint2& srcSize, const vdint2& viewportSize) const override;
	IVDTTexture2D *GetFinalOutput() override;

	void LoadTexture(const char *name, const wchar_t *path, bool linear) override;
	void BeginPasses(uint32 numPasses) override;
	void AddPass(uint32 passIndex, const VDDisplayCustomShaderProps& props, const char *shaderPath, const wchar_t *basePath) override;
	void EndPasses() override;
	void InitProfiling() override;

private:
	friend class VDDCustomEffectPassD3D11;

	void ShutdownProfiling();

	IVDTContext& mContext;
	VDDisplayNodeContext3D& mDisplayContext;

	vdfastvector<VDDisplayCustomShaderPassInfo> mPassInfos;
	vdfastvector<VDDCustomEffectPassD3D11::TextureSpec> mOrigTexSpecs;

	vdvector<VDDTexSpecView<const VDDCustomEffectPassD3D11::TextureSpec>> mTexSpecGrid;

	vdhashmap<VDStringA, uint32> mCustomTextureLookup;
	vdvector<VDDCustomEffectPassD3D11::TextureSpec> mCustomTextures;

	vdrefptr<IVDTSamplerState> mpSSBorderPoint;
	vdrefptr<IVDTSamplerState> mpSSBorderLinear;

	bool mbIssuingQueries = false;
	bool mbPollingQueries = false;
	uint32 mPollQueryIndex = 0;
	double mTimestampFrequency = 0;
	vdfastvector<uint64> mTimestamps;

	vdrefptr<IVDTTimestampFrequencyQuery> mpTimestampFrequencyQuery;
	vdvector<vdrefptr<IVDTTimestampQuery>> mpTimestampQueries;

	VDDisplayCommandList3D mCommandList;
};

#endif
