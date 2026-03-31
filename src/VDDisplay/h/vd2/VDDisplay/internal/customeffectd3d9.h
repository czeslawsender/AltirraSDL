//	VirtualDub - Video processing and capture application
//	Display library - custom D3D9 shader support
//	Copyright (C) 1998-2016 Avery Lee
//
//	This program is free software; you can redistribute it and/or
//	modify it under the terms of the GNU General Public License
//	as published by the Free Software Foundation; either version 2
//	of the License, or (at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECTD3D9_H
#define f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECTD3D9_H

#include <vd2/system/refcount.h>
#include <vd2/system/vectors.h>
#include <vd2/VDDisplay/internal/customeffect.h>

class VDD3D9Manager;
struct IDirect3DTexture9;

class IVDDisplayCustomEffectD3D9 : public virtual IVDDisplayCustomShaderPipeline {
public:
	virtual void Run(IDirect3DTexture9 *const *srcTextures, const vdint2& texSize, const vdint2& imageSize, const vdint2& viewportSize) = 0;
	virtual void RunFinal(const vdrect32f& dstRect, const vdint2& viewportSize) = 0;

	virtual IDirect3DTexture9 *GetFinalOutput(uint32& imageWidth, uint32& imageHeight) = 0;
};

vdrefptr<IVDDisplayCustomEffectD3D9> VDDisplayParseCustomShaderPipeline(VDD3D9Manager *d3d9mgr, const wchar_t *path);

#endif
