//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2009 Avery Lee
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
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stdafx.h>
#include <math.h>
#include <vd2/Kasumi/resample_kernels.h>

///////////////////////////////////////////////////////////////////////////
//
// utility functions
//
///////////////////////////////////////////////////////////////////////////

namespace {
	inline double sinc(double x) {
		return fabs(x) < 1e-9 ? 1.0 : sin(x) / x;
	}
}

///////////////////////////////////////////////////////////////////////////
//
// VDResamplerAxis
//
///////////////////////////////////////////////////////////////////////////

void VDResamplerAxis::Init(sint32 dudxIn) {
	this->dudx = dudxIn;
}

void VDResamplerAxis::Compute(sint32 count, sint32 u0, sint32 w, sint32 kernel_width) {
	u = u0;
	dx = count;

	sint32 du_kern	= (kernel_width-1) << 16;
	sint32 u_limit	= w << 16;

	dx_precopy	= 0;
	dx_preclip	= 0;
	dx_active	= 0;
	dx_postclip	= 0;
	dx_postcopy = 0;
	dx_dualclip	= 0;

	if (dudx == 0) {
		if (u < -du_kern)
			dx_precopy = w;
		else if (u >= u_limit)
			dx_postcopy = w;
		else if (u < 0) {
			if (u + du_kern < u_limit)
				dx_preclip = w;
			else
				dx_dualclip = w;
		} else if (u + du_kern >= u_limit)
			dx_postclip = w;
		else
			dx_active = w;

		return;
	}

	[[maybe_unused]] sint32 u_start = u;

	// (desired - u0 + (dudx-1)) / dudx : first pixel >= desired

	sint32 dudx_m1_mu0	= dudx - 1 - u;
	sint32 first_preclip	= (dudx_m1_mu0 + 0x10000 - du_kern) / dudx;
	sint32 first_active		= (dudx_m1_mu0                    ) / dudx;
	sint32 first_postclip	= (dudx_m1_mu0 + u_limit - du_kern) / dudx;
	sint32 first_postcopy	= (dudx_m1_mu0 + u_limit - 0x10000) / dudx;

	// clamp
	if (first_preclip < 0)
		first_preclip = 0;
	if (first_active < first_preclip)
		first_active = first_preclip;
	if (first_postclip < first_active)
		first_postclip = first_active;
	if (first_postcopy < first_postclip)
		first_postcopy = first_postclip;
	if (first_preclip > dx)
		first_preclip = dx;
	if (first_active > dx)
		first_active = dx;
	if (first_postclip > dx)
		first_postclip = dx;
	if (first_postcopy > dx)
		first_postcopy = dx;

	// determine widths

	dx_precopy	= first_preclip;
	dx_preclip	= first_active - first_preclip;
	dx_active	= first_postclip - first_active;
	dx_postclip	= first_postcopy - first_postclip;
	dx_postcopy	= dx - first_postcopy;

	// sanity checks
	[[maybe_unused]] sint32 pos0 = dx_precopy;
	[[maybe_unused]] sint32 pos1 = pos0 + dx_preclip;
	[[maybe_unused]] sint32 pos2 = pos1 + dx_active;
	[[maybe_unused]] sint32 pos3 = pos2 + dx_postclip;

	VDASSERT(!((dx_precopy|dx_preclip|dx_active|dx_postcopy|dx_postclip) & 0x80000000));
	VDASSERT(dx_precopy + dx_preclip + dx_active + dx_postcopy + dx_postclip == dx);

	VDASSERT(!pos0			|| u_start + dudx*(pos0 - 1) <  0x10000 - du_kern);	// precopy -> preclip
	VDASSERT( pos0 >= pos1	|| u_start + dudx*(pos0    ) >= 0x10000 - du_kern);
	VDASSERT( pos1 <= pos0	|| u_start + dudx*(pos1 - 1) <  0);					// preclip -> active
	VDASSERT( pos1 >= pos2	|| u_start + dudx*(pos1    ) >= 0 || !dx_active);
	VDASSERT( pos2 <= pos1	|| u_start + dudx*(pos2 - 1) <  u_limit - du_kern || !dx_active);	// active -> postclip
	VDASSERT( pos2 >= pos3	|| u_start + dudx*(pos2    ) >= u_limit - du_kern);
	VDASSERT( pos3 <= pos2	|| u_start + dudx*(pos3 - 1) <  u_limit - 0x10000);	// postclip -> postcopy
	VDASSERT( pos3 >= dx	|| u_start + dudx*(pos3    ) >= u_limit - 0x10000);

	u += dx_precopy * dudx;

	// test for overlapping clipping regions
	if (!dx_active && kernel_width > w) {
		dx_dualclip = dx_preclip + dx_postclip;
		dx_preclip = dx_postclip = 0;
	}
}

///////////////////////////////////////////////////////////////////////////
//
// VDResamplerLinearFilter
//
///////////////////////////////////////////////////////////////////////////

VDResamplerLinearFilter::VDResamplerLinearFilter(double twofc)
	: mScale(twofc)
	, mTaps((int)ceil(1.0 / twofc) * 2)
{
}

int VDResamplerLinearFilter::GetFilterWidth() const {
	return mTaps;
}

double VDResamplerLinearFilter::EvaluateFilter(double t) const {
	t = 1.0f - fabs(t)*mScale;

	return t + fabs(t);
}

void VDResamplerLinearFilter::GenerateFilter(float *dst, double offset) const {
	double pos = -((double)((mTaps>>1)-1) + offset) * mScale;

	for(unsigned i=0; i<mTaps; ++i) {
		double t = 1.0 - fabs(pos);

		*dst++ = (float)(t+fabs(t));
		pos += mScale;
	}
}

void VDResamplerLinearFilter::GenerateFilterBank(float *dst) const {
	for(int offset=0; offset<256; ++offset) {
		GenerateFilter(dst, offset * (1.0f / 256.0f));
		dst += mTaps;
	}
}

///////////////////////////////////////////////////////////////////////////
//
// VDResamplerSharpLinearFilter
//
///////////////////////////////////////////////////////////////////////////

VDResamplerSharpLinearFilter::VDResamplerSharpLinearFilter(double factor)
	: mScale(factor)
{
}

int VDResamplerSharpLinearFilter::GetFilterWidth() const {
	return 2;
}

double VDResamplerSharpLinearFilter::EvaluateFilter(double t) const {
	t = (0.5f - fabs(t)) * mScale + 0.5f;

	if (t < 0.0f)
		t = 0.0f;

	if (t > 1.0f)
		t = 1.0f;

	return t;
}

void VDResamplerSharpLinearFilter::GenerateFilter(float *dst, double offset) const {
	double t = (0.5 - fabs(offset)) * mScale + 0.5f;

	if (t < 0.0f)
		t = 0.0f;

	if (t > 1.0f)
		t = 1.0f;

	dst[0] = (float)t;
	dst[1] = 1.0f - (float)t;
}

void VDResamplerSharpLinearFilter::GenerateFilterBank(float *dst) const {
	for(int offset=0; offset<256; ++offset) {
		GenerateFilter(dst, offset * (1.0f / 256.0f));
		dst += 2;
	}
}

///////////////////////////////////////////////////////////////////////////
//
// VDResamplerCubicFilter
//
///////////////////////////////////////////////////////////////////////////

VDResamplerCubicFilter::VDResamplerCubicFilter(double twofc, double A)
	: mScale(twofc)
	, mA0( 1.0  )
	, mA2(-3.0-A)
	, mA3( 2.0+A)
	, mB0(-4.0*A)
	, mB1( 8.0*A)
	, mB2(-5.0*A)
	, mB3(     A)
	, mTaps((int)ceil(2.0 / twofc)*2)
{
}

int VDResamplerCubicFilter::GetFilterWidth() const { return mTaps; }

double VDResamplerCubicFilter::EvaluateFilter(double t) const {
	t = fabs(t)*mScale;

	if (t < 1.0)
		return mA0 + (t*t)*(mA2 + t*mA3);
	else if (t < 2.0)
		return mB0 + t*(mB1 + t*(mB2 + t*mB3));
	else
		return 0;
}

void VDResamplerCubicFilter::GenerateFilter(float *dst, double offset) const {
	double pos = -((double)((mTaps>>1)-1) + offset) * mScale;

	for(unsigned i=0; i<mTaps; ++i) {
		double t = fabs(pos);
		double v = 0;

		if (t < 1.0)
			v = mA0 + (t*t)*(mA2 + t*mA3);
		else if (t < 2.0)
			v = mB0 + t*(mB1 + t*(mB2 + t*mB3));

		*dst++ = (float)v;
		pos += mScale;
	}
}

void VDResamplerCubicFilter::GenerateFilterBank(float *dst) const {
	for(int offset=0; offset<256; ++offset) {
		GenerateFilter(dst, offset * (1.0f / 256.0f));
		dst += mTaps;
	}
}

///////////////////////////////////////////////////////////////////////////
//
// VDResamplerLanczos3Filter
//
///////////////////////////////////////////////////////////////////////////

VDResamplerLanczos3Filter::VDResamplerLanczos3Filter(double twofc)
	: mScale(twofc)
	, mTaps((int)ceil(3.0 / twofc)*2)
{
}

int VDResamplerLanczos3Filter::GetFilterWidth() const {
	return mTaps;
}

double VDResamplerLanczos3Filter::EvaluateFilter(double t) const {
	static const double pi  = 3.1415926535897932384626433832795;	// pi
	static const double pi3 = 1.0471975511965977461542144610932;	// pi/3

	t *= mScale;

	if (fabs(t) < 3.0)
		return sinc(pi*t) * sinc(pi3*t);
	else
		return 0.0;
}

void VDResamplerLanczos3Filter::GenerateFilter(float *dst, double offset) const {
	static const double pi  = 3.1415926535897932384626433832795;	// pi
	static const double pi3 = 1.0471975511965977461542144610932;	// pi/3

	double t = -(((double)((mTaps>>1)-1) + offset) * mScale);

	for(unsigned i=0; i<mTaps; ++i) {
		double v = 0;

		if (fabs(t) < 3.0)
			v = sinc(pi*t) * sinc(pi3*t);

		*dst++ = (float)v;
		t += mScale;
	}
}

void VDResamplerLanczos3Filter::GenerateFilterBank(float *dst) const {
	float *VDRESTRICT dst2 = dst;

	// We need to generate 256 filters at offsets of [0..255]/256. However, calling
	// GenerateFilter() in a loop is pretty slow due to all of the sinc() calls. The
	// angles are evenly spaced, so we can iterate them much more quickly. To aid
	// this, the loops are transposed so we evaluate one tap of the filter at a time
	// across all 256 phases.
	//
	// In the unscaled domain, taps are 1.0 apart and filters are 1/256 apart. The
	// scaling factor (mScale) changes this to mScale and mScale/256 instead.
	// GenerateFilter() negates the offset so we actually step by -mScale/256 instead.

	const int numTaps = mTaps;

	static const double pi  = 3.1415926535897932384626433832795;	// pi
	static const double pi3 = 1.0471975511965977461542144610932;	// pi/3

	struct vec2d {
		double x, y;
	};

	double t0 = -(double)((numTaps>>1)-1) * mScale;
	for(int i=0; i<numTaps; ++i) {
		double tinc1 = -pi * mScale / 256.0;
		double tinc3 = -pi3 * mScale / 256.0;

		double csinc1 = cos(tinc1);
		double sninc1 = sin(tinc1);
		double csinc3 = cos(tinc3);
		double sninc3 = sin(tinc3);

		double cs1a = cos(pi*t0);
		double cs3a = cos(pi3*t0);
		double sn1a = sin(pi*t0);
		double sn3a = sin(pi3*t0);

		double cs1b = cs1a*csinc1 - sn1a*sninc1;
		double sn1b = sn1a*csinc1 + cs1a*sninc1;
		double cs3b = cs3a*csinc3 - sn3a*sninc3;
		double sn3b = sn3a*csinc3 + cs3a*sninc3;

		vec2d cs1 { cs1a, cs1b };
		vec2d cs3 { cs3a, cs3b };
		vec2d sn1 { sn1a, sn1b };
		vec2d sn3 { sn3a, sn3b };

		double csinc1b = cos(tinc1 * 2.0f);
		double sninc1b = sin(tinc1 * 2.0f);
		double csinc3b = cos(tinc3 * 2.0f);
		double sninc3b = sin(tinc3 * 2.0f);

		double tinc = -mScale / 256.0f;
		double tinc2 = tinc*2;
		vec2d t { t0, t0 + tinc };
		for(int offset = 0; offset < 128; ++offset) {
			double v1 = 0;
			double v2 = 0;

			if (fabs(t.x) < 3.0) {
				if (fabs(t.x) < 1e-9)
					v1 = 1.0;
				else
					v1 = sn1.x * sn3.x / ((pi*pi3) * (t.x*t.x));
			}

			if (fabs(t.y) < 3.0) {
				if (fabs(t.y) < 1e-9)
					v2 = 1.0;
				else
					v2 = sn1.y * sn3.y / ((pi*pi3) * (t.y*t.y));
			}

			dst2[i + offset*numTaps*2] = (float)v1;
			dst2[i + offset*numTaps*2 + numTaps] = (float)v2;

			vec2d cnext1 = { cs1.x*csinc1b - sn1.x*sninc1b, cs1.y*csinc1b - sn1.y*sninc1b };
			vec2d snext1 = { sn1.x*csinc1b + cs1.x*sninc1b, sn1.y*csinc1b + cs1.y*sninc1b };
			cs1 = cnext1;
			sn1 = snext1;

			vec2d cnext3 = { cs3.x*csinc3b - sn3.x*sninc3b, cs3.y*csinc3b - sn3.y*sninc3b };
			vec2d snext3 = { sn3.x*csinc3b + cs3.x*sninc3b, sn3.y*csinc3b + cs3.y*sninc3b };
			cs3 = cnext3;
			sn3 = snext3;

			t.x += tinc2;
			t.y += tinc2;
		}

		t0 += mScale;
	}

#if 0
	// check against reference
	vdblock<float> tmp(256 * numTaps);
	for(int offset=0; offset<256; ++offset) {
		GenerateFilter(&tmp[offset * numTaps], offset * (1.0f / 256.0f));
	}

	for(int i=0, n = 256*numTaps; i < n; ++i) {
		if (fabsf(tmp[i] - dst[i]) > 0.75f / 16384.0f)
			__debugbreak();
	}
#endif
}
