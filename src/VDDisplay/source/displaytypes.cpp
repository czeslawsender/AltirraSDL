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

#include <stdafx.h>
#include <vd2/system/math.h>
#include <vd2/VDDisplay/displaytypes.h>

float VDDScreenMaskParams::GetMaskIntensityScale() const {
	switch(mType) {
		case VDDScreenMaskType::None:
			break;

		case VDDScreenMaskType::ApertureGrille:
			// The aperture grille has one vertical slot for each color. At
			// maximum openness, each slot occupies one third of the total area.
			return mOpenness / 3.0f;

		case VDDScreenMaskType::DotTriad:
			// The dot triad is composed of equilateral triangles with 1/6th of
			// a disc centered at each vertex. The area of an equilateral triangle
			// with 1 unit sides is sqrt(3)/4, and the area of a disc of radius
			// 0.5 is pi/4, so the maximum illumination is pi/[6 sqrt(3)].
			return (mOpenness * mOpenness) * (nsVDMath::kfPi / 6.0f / sqrtf(3.0f));

		case VDDScreenMaskType::SlotMask:
			// The slot mask has one vertical slot for each color. In addition,
			// the stripes are broken vertically by gaps which are the same
			// width as the horizontal gaps.
			return (mOpenness / 3.0f) * ((2.0f + mOpenness) / 3.0f);
	}

	return 1.0f;
}
