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

#ifndef f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECTBASE_H
#define f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECTBASE_H

#include <vd2/VDDisplay/internal/customeffectpassbase.h>
#include <vd2/VDDisplay/internal/customeffect.h>

class VDDisplayCustomShaderProps;

class VDDCustomEffectBase : public virtual IVDDisplayCustomShaderPipeline, public vdrefcount {
	VDDCustomEffectBase(const VDDCustomEffectBase&) = delete;
	VDDCustomEffectBase& operator=(const VDDCustomEffectBase&) = delete;
public:
	VDDCustomEffectBase() = default;
	virtual ~VDDCustomEffectBase() = default;

	int AddRef() override;
	int Release() override;

	void Parse(const wchar_t *path);

	void IncrementFrame() override;
	uint32 GetMaxPrevFrames() const override { return mMaxPrevFramesPerPass[0]; }

protected:
	void ParseTextures(const VDDisplayCustomShaderProps& props, const wchar_t *basePath);

	virtual void LoadTexture(const char *name, const wchar_t *path, bool linear) = 0;
	virtual void BeginPasses(uint32 numPasses) = 0;
	virtual void AddPass(uint32 passIndex, const VDDisplayCustomShaderProps& props, const char *shaderPath, const wchar_t *basePath) = 0;
	virtual void EndPasses() = 0;
	virtual void InitProfiling() = 0;

	bool mbNewFrame = false;

	vdfastvector<uint32> mMaxPrevFramesPerPass;
	vdfastvector<bool> mPassInputsFiltered;
	vdfastvector<bool> mPassInputsSrgb;

	// Used during rendering to determine which passes need to be run. [0] is set when the
	// input/original texture is invalidated, and successive entries are set as pass outputs
	// are invalidated. [i] is also equal to whether pass i-1 was run.
	vdfastvector<bool> mInputInvalidationTable;

	vdfastvector<VDDCustomEffectPassBase *> mPasses;

	VDDCustomEffectVarStorage mVarStorage;
};

#endif

