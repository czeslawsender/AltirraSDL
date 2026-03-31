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

#ifndef f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECT_H
#define f_VD2_VDDISPLAY_INTERNAL_CUSTOMEFFECT_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/refcount.h>

struct VDDisplayCustomShaderPassInfo {
	float mTiming;
	uint32 mOutputWidth;
	uint32 mOutputHeight;
	bool mbOutputLinear;
	bool mbOutputFloat;			// output surface is float type (float or half)
	bool mbOutputHalfFloat;		// output surface is half-float
	bool mbOutputSrgb;			// output surface has hardware sRGB correction enabled
	bool mbCached;
};

class IVDDisplayCustomShaderPipeline : public IVDRefCount {
protected:
	~IVDDisplayCustomShaderPipeline() = default;

public:
	virtual bool ContainsFinalBlit() const = 0;
	virtual uint32 GetMaxPrevFrames() const = 0;
	virtual bool HasTimingInfo() const = 0;
	virtual vdspan<const VDDisplayCustomShaderPassInfo> GetPassTimings() = 0;

	virtual void IncrementFrame() = 0;
};

#endif

