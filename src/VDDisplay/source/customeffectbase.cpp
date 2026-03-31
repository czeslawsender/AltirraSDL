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
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/VDDisplay/internal/customeffectbase.h>
#include <vd2/VDDisplay/internal/customeffectutils.h>
#include <vd2/VDDisplay/internal/options.h>

int VDDCustomEffectBase::AddRef() {
	return vdrefcount::AddRef();
}

int VDDCustomEffectBase::Release() {
	return vdrefcount::Release();
}

void VDDCustomEffectBase::Parse(const wchar_t *path) {
	// parse the .cgp file
	VDDisplayCustomShaderProps props;

	{
		VDTextInputFile tf(path);
		props.ParseFromFile(tf);
	}

	// compute base path for all referencing
	const VDStringW basePath = VDFileSplitPathLeftSpan(VDStringSpanW(path));

	// load all textures
	ParseTextures(props, basePath.c_str());

	// determine pass count
	uint32 passCount = 0;

	auto shaderCountSpecified = props.TryGetInt(VDDCsPropKeyView("shaders", nullptr));

	if (shaderCountSpecified.has_value()) {
		int v = shaderCountSpecified.value();

		if (v <= 0 || v > 100)
			throw VDException("Invalid 'shader' value");

		passCount = (uint32)v;
	} else {
		while(props.GetString(VDDCsPropKeyView("shader", passCount)))
			++passCount;
	}

	if (!passCount)
		throw VDException("Custom shader pipeline contains no passes. There should be at least a shader0= property pointing to a .fx file for pass 0.");

	// parse and init passes
	mPassInputsFiltered.resize(passCount + 1, false);
	mPassInputsSrgb.resize(passCount + 1, false);
	mMaxPrevFramesPerPass.resize(passCount, 0);

	BeginPasses(passCount);

	for(uint32 passIndex = 0; passIndex < passCount; ++passIndex) {
		const char *shaderPath = props.GetString(VDDCsPropKeyView("shader", passIndex));

		if (!shaderPath)
			throw VDException("Missing entry 'shader%u'", passIndex);

		AddPass(passIndex, props, shaderPath, basePath.c_str());

		mPassInputsFiltered[passIndex] = mPasses.back()->IsInputFiltered();
		mPassInputsSrgb[passIndex + 1] = mPasses.back()->IsOutputSrgb();
	}

	mInputInvalidationTable.resize(passCount + 1);

	EndPasses();

	mVarStorage.SetMatrix(
		mVarStorage.GetVarOffset(VDDCustomEffectVariable::ModelViewProj, 0, 0),
		vdfloat4x4::identity()
	);

	uint32 passIndex = 0;
	for(VDDCustomEffectPassBase *pass : mPasses) {
		pass->ResetVariables(mVarStorage, mMaxPrevFramesPerPass[passIndex], mPassInputsFiltered[passIndex + 1]);

		++passIndex;
	}

	// set up profiling if enabled
	const bool profile = VDDInternalOptions::sbShowCustomShaderStats || props.GetBool(VDDCsPropKeyView("shader_show_stats", nullptr), false);
	if (profile)
		InitProfiling();
}

void VDDCustomEffectBase::IncrementFrame() {
	mbNewFrame = true;
}

void VDDCustomEffectBase::ParseTextures(const VDDisplayCustomShaderProps& props, const wchar_t *basePath) {
	const char *texturesProp = props.GetString(VDDCsPropKeyView("textures", nullptr));
	if (!texturesProp)
		return;

	VDStringRefA texturesList(texturesProp);
	const VDCharMaskA whitespace(" \r\n\t\v");

	while(!texturesList.empty()) {
		VDStringRefA textureName;
		if (!texturesList.split(';', textureName)) {
			textureName = texturesList;
			texturesList.clear();
		}

		textureName = textureName.trim(whitespace);
		if (textureName.empty())
			continue;

		const VDStringA textureNameBuf(textureName);
		const char *texPath = props.GetString(VDDCsPropKeyView(textureName));
		if (!texPath)
			throw MyError("No path specified for texture: %s", textureNameBuf.c_str());

		VDStringA linearKey;
		linearKey.sprintf("%s_linear", textureNameBuf.c_str());
		bool linear = props.GetBool(VDDCsPropKeyView(linearKey), true);

		LoadTexture(textureNameBuf.c_str(), VDMakePath(VDStringSpanW(basePath), VDTextU8ToW(VDStringSpanA(texPath))).c_str(), linear);
	}
}
