//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2024 Avery Lee
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

#ifndef f_VD2_VDDDISPLAY_DISPLAYTYPES_H
#define f_VD2_VDDDISPLAY_DISPLAYTYPES_H

#include <vd2/system/vdtypes.h>

enum class VDDScreenMaskType : uint8 {
	None,
	ApertureGrille,
	DotTriad,
	SlotMask,
};

struct VDDScreenMaskParams {
	VDDScreenMaskType mType = VDDScreenMaskType::None;
	float mSourcePixelsPerDot = 0;
	float mOpenness = 0;

	// If true, intensity is boosted to compensate for the average energy loss
	// of the screen mask. This has no effect if the screen mask is disabled.
	bool mbScreenMaskIntensityCompensation = false;

	bool operator==(const VDDScreenMaskParams&) const = default;

	// Returns the intensity scaling imposed by the given mask type and
	// parameters. A value of 0.5 means that the screen mask halves the
	// average linear intensity of the output. This is used for intensity
	// compensation.
	float GetMaskIntensityScale() const;
};

#endif