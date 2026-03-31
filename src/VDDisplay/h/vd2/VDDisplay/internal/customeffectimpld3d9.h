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

#include <vd2/VDDisplay/direct3d.h>
#include <vd2/VDDisplay/internal/customeffectbase.h>
#include <vd2/VDDisplay/internal/customeffectpassbase.h>
#include <vd2/VDDisplay/internal/customeffectutils.h>

class VDDisplayCustomShaderD3D9 final : public VDDCustomEffectPassBase, public VDD3D9Client {
public:
	struct TextureSpec : public VDDCETextureSpec {
		IDirect3DTexture9 *mpTexture = nullptr;
	};

	VDDisplayCustomShaderD3D9(uint32 passIndex, VDD3D9Manager *d3d9mgr);
	~VDDisplayCustomShaderD3D9();

	void Init(
		const char *shaderPath,
		const VDDisplayCustomShaderProps& propLookup,
		const vdhashmap<VDStringA, TextureSpec>& customTextureLookup,
		vdspan<const bool> passInputsFiltered,
		vdspan<const bool> passInputsSrgb,
		const wchar_t *basePath,
		vdspan<uint32> maxPrevFrames,
		VDDCustomEffectVarStorage& varStorage
	);

	void ResetVariables(VDDCustomEffectVarStorage& varStorage, uint32 prevOutputFramesNeeded, bool outputFilteringNeeded) override;

	bool Run(const vdrect32f *dstRect, vdspan<const VDDTexSpecView<const TextureSpec>> texSpecGrid, const vdint2& viewportSize, bool lastStage,
		vdspan<bool> inputInvalidationTable,
		VDDCustomEffectVarStorage& varStorage);

	VDDTexSpecView<const TextureSpec> GetOutputTexSpecs() const;

public:
	void OnPreDeviceReset() override;
	void OnPostDeviceReset() override;

private:
	struct RenderState {
		uint32 mState;
		uint32 mValue;
	};

	struct SamplerState {
		uint32 mStage;
		uint32 mState;
		uint32 mValue;
	};

	struct TextureBinding {
		uint32 mStage;
		VDDCETextureKey mTexKey;
	};

	struct UploadSpan {
		uint32 mStart;
		uint32 mCount;
		uint32 mSrcOffset;
	};

	struct ShaderInfo {
		vdfastvector<UploadSpan> mUploadSpansB;
		vdfastvector<UploadSpan> mUploadSpansI;
		vdfastvector<UploadSpan> mUploadSpansF;
		vdfastvector<uint32> mConstantBuffer;
		vdfastvector<VDDCustomEffectScalarGather> mConstantBufferBoolGathers;
		vdfastvector<VDDCustomEffectVec4Gather> mConstantBufferVecGathers;
	};

	void UpdateVariables(ShaderInfo& shaderInfo, VDDCustomEffectVarStorage& varStorage);

	void ConvertBoolData(vdspan<uint32> dst, const float *src);

	template<class T, HRESULT (__stdcall IDirect3DDevice9::*T_UploadFn)(UINT, T *, UINT)>
	void UploadShaderData(IDirect3DDevice9 *dev, const vdfastvector<UploadSpan>& spans, const void *src);

	bool ProcessShader(const uint32 *shader, uint32 shaderSize, ShaderInfo& shaderInfo, const vdhashmap<VDStringA, TextureSpec>& customTextureLookup, vdspan<uint32> maxPrevFrames, VDDCustomEffectVarStorage& varStorage);

	VDD3D9Manager *const mpD3DMgr;
	vdrefptr<IDirect3DVertexShader9> mpVertexShader;
	vdrefptr<IDirect3DPixelShader9> mpPixelShader;
	uint32 mPrevSrcWidth = 0;
	uint32 mPrevSrcHeight = 0;

	bool mbVSConstantTableInited = false;
	bool mbPSConstantTableInited = false;
	bool mbPassCanBeCached = false;

	vdfastvector<uint8> mPassInputReferenceTable;

	VDD3DXConstantTable mVertexShaderConstantTable;
	VDD3DXConstantTable mPixelShaderConstantTable;

	vdfastvector<RenderState> mRenderStates;
	vdfastvector<SamplerState> mSamplerStates;

	ShaderInfo mVertexShaderInfo = {};
	ShaderInfo mPixelShaderInfo = {};

	vdfastvector<TextureBinding> mTextureBindings;

	VDDCEPassParamOffsets mPassParamOffsets {};

	struct OutputTexture : public TextureSpec {
		vdrefptr<IDirect3DTexture9> mpTextureRef;
		vdrefptr<IDirect3DSurface9> mpSurfaceRef;
	};

	vdvector<OutputTexture> mOutputTextures;

	// aliased against master custom texture table -- not refcounted
	vdfastvector<TextureSpec> mCustomTextures;
};

class VDDisplayCustomShaderPipelineD3D9 final : public VDDCustomEffectBase, public IVDDisplayCustomEffectD3D9, public VDD3D9Client {
public:
	VDDisplayCustomShaderPipelineD3D9(VDD3D9Manager *d3d9mgr);
	~VDDisplayCustomShaderPipelineD3D9();

	bool ContainsFinalBlit() const override;
	bool HasTimingInfo() const override;
	vdspan<const VDDisplayCustomShaderPassInfo> GetPassTimings() override;

	void Run(IDirect3DTexture9 *const *srcTextures, const vdint2& texSize, const vdint2& imageSize, const vdint2& viewportSize) override;
	void RunFinal(const vdrect32f& dstRect, const vdint2& viewportSize) override;

	IDirect3DTexture9 *GetFinalOutput(uint32& imageWidth, uint32& imageHeight) override;

public:
	void OnPreDeviceReset() override;
	void OnPostDeviceReset() override;

private:
	void CreateQueries();
	void DestroyQueries();
	void LoadTexture(const char *name, const wchar_t *path, bool linear) override;
	void BeginPasses(uint32 numPasses) override;
	void AddPass(uint32 passIndex, const VDDisplayCustomShaderProps& props, const char *shaderPath, const wchar_t *basePath) override;
	void EndPasses() override;
	void InitProfiling() override;

	VDD3D9Manager *const mpManager;

	vdfastvector<VDDisplayCustomShaderD3D9::TextureSpec> mOrigTexSpecs;
	vdvector<VDDTexSpecView<const VDDisplayCustomShaderD3D9::TextureSpec>> mTexSpecGrid;

	vdhashmap<VDStringA, VDDisplayCustomShaderD3D9::TextureSpec> mCustomTextures;

	vdfastvector<VDDisplayCustomShaderPassInfo> mPassInfos;
	vdfastvector<IDirect3DQuery9 *> mpTimingQueries;
	vdfastvector<UINT64> mTimingValues;
	uint32 mNextTimingValueIssue = 0;
	uint32 mNextTimingValueQuery = 0;
	bool mbIssueQueries = false;
	bool mbQueryQueries = false;
	float mTicksToSeconds = 0;
};

#endif
