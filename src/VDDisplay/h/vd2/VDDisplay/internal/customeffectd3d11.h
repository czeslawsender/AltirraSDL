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

#ifndef f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECTD3D11_H
#define f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECTD3D11_H

#include <vd2/system/refcount.h>
#include <vd2/system/vectors.h>
#include <vd2/VDDisplay/internal/customeffect.h>

class IVDTTexture2D;
class IVDTContext;
class VDDisplayNodeContext3D;
struct VDDisplaySourceTexMapping;
struct VDDRenderView;

class IVDDisplayCustomEffectD3D11 : public virtual IVDDisplayCustomShaderPipeline {
public:
	virtual void PreRun() = 0;
	virtual void Run(IVDTTexture2D *const *srcTextures, const vdint2& texSize, const vdint2& imageSize, const vdint2& viewportSize) = 0;
	virtual void RunFinal(const VDDRenderView& renderView, const vdrect32f& dstRect, const vdint2& viewportSize) = 0;
	virtual void PostRun() = 0;

	virtual VDDisplaySourceTexMapping ComputeFinalOutputMapping(const vdint2& srcSize, const vdint2& viewportSize) const = 0;
	virtual IVDTTexture2D *GetFinalOutput() = 0;
};

vdrefptr<IVDDisplayCustomEffectD3D11> VDDisplayParseCustomEffectD3D11(IVDTContext& context, VDDisplayNodeContext3D& dctx, const wchar_t *path);

#endif
