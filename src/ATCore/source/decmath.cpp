//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2019 Avery Lee
//	Decimal math support
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
//
//	As a special exception, this library can also be redistributed and/or
//	modified under an alternate license. See COPYING.RMT in the same source
//	archive for details.

#include <stdafx.h>
#include <cmath>
#include <at/atcore/decmath.h>

bool ATDecFloat::SetDouble(double v) {
	uint8 bias = 0x40;

	if (v < 0) {
		bias = 0xc0;
		v = -v;
	}

	if (v < 1e-98) {
		SetZero();
		return true;
	}

	static const double invln100 = 0.2171472409516259138255644594583025411471985029018332830572268916;
	double x = floor(log(v) * invln100);
	int ix = (int)x;
	double mantissa = v * pow(100.0, 4-x);

	// compensate for roundoff
	if (mantissa >= 10000000000.0) {
		mantissa /= 100.0;
		++ix;
	} else if (mantissa < 100000000.0) {
		mantissa *= 100.0;
		--ix;
	}

	// convert mantissa to integer (100000000 - 10000000000)
	sint64 imant64 = (sint64)(mantissa + 0.5);

	// renormalize if necessary after rounding
	if (imant64 == 10000000000) {
		imant64 = 100000000;
		++ix;
	}

	// check for underflow
	if (ix < -49) {
		SetZero();
		return true;
	}

	// check for overflow (1E+98 error)
	if (ix >= 49)
		return false;

	// split mantissa into bytes
	uint8 rb[5];

	rb[0] = (uint8)(imant64 / 100000000);
	uint32 imant32 = (uint32)(imant64 % 100000000);

	rb[1] = imant32 / 1000000;
	imant32 %= 1000000;

	rb[2] = imant32 / 10000;
	imant32 %= 10000;

	rb[3] = imant32 / 100;
	imant32 %= 100;

	rb[4] = imant32;

	// convert mantissa to BCD
	for(int i=0; i<5; ++i)
		mMantissa[i] = (uint8)(((rb[i] / 10) << 4) + (rb[i] % 10));

	// encode exponent
	mSignExp = bias + ix;
	return true;
}

ATDecFloat ATDecFloat::operator-() const {
	ATDecFloat r(*this);

	if (r.mSignExp)
		r.mSignExp ^= 0x80;

	return r;
}

ATDecFloat ATDecFloat::Abs() const {
	ATDecFloat r(*this);

	r.mSignExp &= 0x7f;
	return r;
}

ATDecFloatText ATDecFloat::ToString() const {
	ATDecFloatText dt;

	// 18 chars needed: -0.000000000E+100<nul>
	static_assert(std::size(dt.mBuf.ch) >= 18);

	char *dst = dt.mBuf.ch;

	if (!mSignExp || !mMantissa[0])
		*dst++ = '0';
	else {
		int exp = (mSignExp & 0x7f) * 2 - 0x80;

		if (mSignExp & 0x80)
			*dst++ = '-';

		if (mMantissa[0] >= 10) {
			*dst++ = '0' + (mMantissa[0] >> 4);
			*dst++ = '.';
			*dst++ = '0' + (mMantissa[0] & 15);
			++exp;
		} else {
			*dst++ = '0' + (mMantissa[0] & 15);
			*dst++ = '.';
		}

		for(int i=1; i<5; ++i) {
			int v = mMantissa[i];
			*dst++ = '0' + (v >> 4);
			*dst++ = '0' + (v & 15);
		}

		// cut off trailing zeroes
		while(dst[-1] == '0')
			--dst;

		// cut off trailing period
		if (dst[-1] == '.')
			--dst;

		// add exponent
		if (exp) {
			*dst++ = 'E';
			if (exp < 0) {
				*dst++ = '-';
				exp = -exp;
			} else {
				*dst++ = '+';
			}

			// E+100 or higher is not valid in the math pack, but we allow it here
			if (exp >= 100) {
				*dst++ = '1';
				exp -= 100;
			}

			*dst++ = '0' + (exp / 10);
			*dst++ = '0' + (exp % 10);
		}
	}

	*dst = 0;

	static_cast<VDStringSpanA&>(dt) = VDStringSpanA(dt.mBuf.ch, dst);
	return dt;
}

double ATDecFloat::ToDouble() const {
	if (!mSignExp || !mMantissa[0])
		return 0.0;

	int exp = (mSignExp & 0x7f) - 0x40;
	double r = 0;

	for(int i=0; i<5; ++i) {
		int c = mMantissa[i];

		r = (r * 100.0) + (c >> 4)*10 + (c & 15);
	}

	r *= pow(100.0, (double)(exp - 4));

	if (mSignExp & 0x80)
		r = -r;

	return r;
}

bool operator<(const ATDecFloat& x, const ATDecFloat& y) {
	// check for sign difference
	if ((x.mSignExp ^ y.mSignExp) & 0x80)
		return x.mSignExp < 0x80;

	bool xlores = !(x.mSignExp & 0x80);
	bool ylores = !xlores;

	if (x.mSignExp < y.mSignExp)
		return xlores;
	if (x.mSignExp > y.mSignExp)
		return ylores;

	for(int i=0; i<5; ++i) {
		if (x.mMantissa[i] < y.mMantissa[i])
			return xlores;
		if (x.mMantissa[i] > y.mMantissa[i])
			return ylores;
	}

	// values are equal
	return false;
}

bool operator==(const ATDecFloat& x, const ATDecFloat& y) {
	return x.mSignExp == y.mSignExp
		&& x.mMantissa[0] == y.mMantissa[0]
		&& x.mMantissa[1] == y.mMantissa[1]
		&& x.mMantissa[2] == y.mMantissa[2]
		&& x.mMantissa[3] == y.mMantissa[3]
		&& x.mMantissa[4] == y.mMantissa[4];
}

bool operator>(const ATDecFloat& x, const ATDecFloat& y) {
	return y<x;
}

bool operator!=(const ATDecFloat& x, const ATDecFloat& y) {
	return !(x==y);
}

bool operator<=(const ATDecFloat& x, const ATDecFloat& y) {
	return !(x>y);
}

bool operator>=(const ATDecFloat& x, const ATDecFloat& y) {
	return !(x<y);
}

bool ATDecFloatAdd(ATDecFloat& dst, const ATDecFloat& x, const ATDecFloat& y) {
	// Extract exponents
	int xexp = x.mSignExp & 0x7f;
	int yexp = y.mSignExp & 0x7f;

	// Make sure x is larger in magnitude
	if (x.Abs() < y.Abs())
		return ATDecFloatAdd(dst, y, x);

	// Check for y=0.
	if (!y.mSignExp) {
		dst = x;
		return true;
	}

	// Denormalize y.
	ATDecFloat z(y);
	int expdiff = xexp - yexp;
	uint32 round = 0;
	if (expdiff) {
		if (expdiff > 5) {
			dst = x;
			return true;
		}

		// shift 
		while(expdiff--) {
			z.mMantissa[4] = z.mMantissa[3];
			z.mMantissa[3] = z.mMantissa[2];
			z.mMantissa[2] = z.mMantissa[1];
			z.mMantissa[1] = z.mMantissa[0];
			z.mMantissa[0] = 0;
		}
	}

	// Set mantissa.
	dst.mSignExp = x.mSignExp;

	// Check if we need to add or subtract.
	if ((x.mSignExp ^ y.mSignExp) & 0x80) {
		// subtract
		uint32 borrow = 0;

		if (round > 0x50 || (round == 0x50 && (z.mMantissa[4] & 0x01)))
			borrow = 1;

		for(int i=4; i>=0; --i) {
			sint32 lo = ((sint32)x.mMantissa[i] & 0x0f) - ((sint32)z.mMantissa[i] & 0x0f) - borrow;
			sint32 hi = ((sint32)x.mMantissa[i] & 0xf0) - ((sint32)z.mMantissa[i] & 0xf0);

			if (lo < 0) {
				lo += 10;
				hi -= 0x10;
			}

			borrow = 0;
			if (hi < 0) {
				hi += 0xA0;
				borrow = 1;
			}

			dst.mMantissa[i] = (uint8)(lo + hi);
		}

		// a borrow out isn't possible
		VDASSERT(!borrow);

		// renormalize as necessary
		for(int i=0; i<5; ++i) {
			if (dst.mMantissa[0])
				break;

			--dst.mSignExp;
			if ((dst.mSignExp & 0x7f) < 64-49) {
				dst.SetZero();
				return true;
			}

			dst.mMantissa[0] = dst.mMantissa[1];
			dst.mMantissa[1] = dst.mMantissa[2];
			dst.mMantissa[2] = dst.mMantissa[3];
			dst.mMantissa[3] = dst.mMantissa[4];
			dst.mMantissa[4] = 0;
		}

		// check for zero
		if (!dst.mMantissa[0])
			dst.mSignExp = 0;
	} else {
		// add
		uint32 carry = 0;

		for(int i=4; i>=0; --i) {
			uint32 lo = ((uint32)x.mMantissa[i] & 0x0f) + ((uint32)z.mMantissa[i] & 0x0f) + carry;
			uint32 hi = ((uint32)x.mMantissa[i] & 0xf0) + ((uint32)z.mMantissa[i] & 0xf0);

			if (lo >= 10) {
				lo -= 10;
				hi += 0x10;
			}

			carry = 0;
			if (hi >= 0xA0) {
				hi -= 0xA0;
				carry = 1;
			}

			dst.mMantissa[i] = (uint8)(lo + hi);
		}

		// if we had a carry out, we need to renormalize again
		if (carry) {
			++dst.mSignExp;

			// check for overflow
			if ((dst.mSignExp & 0x7f) > 48+64)
				return false;

			// renormalize
			uint32 carry2 = 0;
			for(int i=3; i>=0; --i) {
				uint32 r = dst.mMantissa[i] + carry2;

				if ((r & 0x0f) >= 0x0A)
					r += 0x06;
				if ((r & 0xf0) >= 0xA0)
					r += 0x60;

				carry2 = r >> 8;
				dst.mMantissa[i+1] = (uint8)r;
			}

			// Unlike base 2 FP, it isn't possible for this to require another renormalize.
			dst.mMantissa[0] = carry + carry2;
		}
	}

	return true;
}

bool ATDecFloatMul(ATDecFloat& dst, const ATDecFloat& x, const ATDecFloat& y) {
	// compute new exponent
	uint8 sign = (x.mSignExp^y.mSignExp) & 0x80;
	sint32 exp = (uint32)(x.mSignExp & 0x7f) + (uint32)(y.mSignExp & 0x7f) - 0x80;

	// convert both mantissae to binary
	int xb[5];
	int yb[5];

	for(int i=0; i<5; ++i) {
		int xm = x.mMantissa[i];
		int ym = y.mMantissa[i];
		xb[i] = ((xm & 0xf0) >> 4)*10 + (xm & 0x0f);
		yb[i] = ((ym & 0xf0) >> 4)*10 + (ym & 0x0f);
	}

	// compute result
	int rb[10] = {0};

	for(int i=0; i<5; ++i) {
		int xbi = xb[i];
		for(int j=0; j<5; ++j)
			rb[i+j] += xbi * yb[j];
	}

	// propagate carries
	int carry = 0;
	for(int i=9; i>=0; --i) {
		rb[i] += carry;
		carry = rb[i] / 100;
		rb[i] %= 100;
	}

	// shift if necessary
	if (carry) {
		++exp;

		rb[5] = rb[4];
		rb[4] = rb[3];
		rb[3] = rb[2];
		rb[2] = rb[1];
		rb[1] = rb[0];
		rb[0] = carry;
	}

	// check for underflow
	if (exp < -49) {
		dst.SetZero();
		return true;
	}

	// check for overflow
	if (exp > 49)
		return false;

	// convert digits back to BCD
	for(int i=0; i<5; ++i) {
		int rbi = rb[i];
		dst.mMantissa[i] = (uint8)(((rbi/10) << 4) + (rbi % 10));
	}

	// encode exponent
	dst.mSignExp = (uint8)(sign + exp + 0x40);

	return true;
}

bool ATDecFloatDiv(ATDecFloat& dst, const ATDecFloat& x, const ATDecFloat& y) {
	// check for zero divisor
	if (!y.mSignExp || !y.mMantissa[0])
		return false;

	// check for zero dividend
	if (!x.mSignExp || !x.mMantissa[0]) {
		dst.SetZero();
		return true;
	}

	// compute new exponent
	uint8 sign = (x.mSignExp^y.mSignExp) & 0x80;
	sint32 exp = (uint32)(x.mSignExp & 0x7f) - (uint32)(y.mSignExp & 0x7f);

	// convert both mantissae to binary
	uint64 xb = 0;
	uint64 yb = 0;

	for(int i=0; i<5; ++i) {
		int xm = x.mMantissa[i];
		int ym = y.mMantissa[i];

		xb = (xb * 100) + ((xm & 0xf0) >> 4)*10 + (xm & 0x0f);
		yb = (yb * 100) + ((ym & 0xf0) >> 4)*10 + (ym & 0x0f);
	}

	// do division
	xb *= 10000;
	uint32 v1 = (uint32)(xb / yb);

	xb = (xb % yb) * 1000000;
	uint32 v2 = (uint32)(xb / yb);

	// split digits to base 100
	uint8 rb[6];
	rb[0] = v1 / 10000;		v1 %= 10000;
	rb[1] = v1 / 100;		v1 %= 100;
	rb[2] = v1;
	rb[3] = v2 / 10000;		v2 %= 10000;
	rb[4] = v2 / 100;		v2 %= 100;
	rb[5] = v2;

	// check if we need to renormalize
	if (!rb[0]) {
		rb[0] = rb[1];
		rb[1] = rb[2];
		rb[2] = rb[3];
		rb[3] = rb[4];
		rb[4] = rb[5];
		rb[5] = 0;
		--exp;
	}

	// convert digits back to BCD
	for(int i=0; i<5; ++i) {
		int rbi = rb[i];
		dst.mMantissa[i] = (uint8)(((rbi/10) << 4) + (rbi % 10));
	}

	// check for underflow or overflow
	if (exp < -49) {
		dst.SetZero();
		return true;
	}

	if (exp > 49)
		return false;

	// encode exponent
	dst.mSignExp = (uint8)(sign + exp + 0x40);

	return true;
}

////////////////////////////////////////////////////////////////////////////////

ATDecFloatOpt& ATDecFloatOpt::operator+=(const ATDecFloatOpt& other) {
	ATDecFloat r;
	if (mbValid && other.mbValid) {
		mbValid = ATDecFloatAdd(r, mValue, other.mValue);
		mValue = r;
	}

	return *this;
}

ATDecFloatOpt& ATDecFloatOpt::operator-=(const ATDecFloatOpt& other) {
	ATDecFloat r;
	if (mbValid && other.mbValid) {
		mbValid = ATDecFloatAdd(r, mValue, -other.mValue);
		mValue = r;
	}

	return *this;
}

ATDecFloatOpt& ATDecFloatOpt::operator*=(const ATDecFloatOpt& other) {
	ATDecFloat r;
	if (mbValid && other.mbValid) {
		mbValid = ATDecFloatMul(r, mValue, other.mValue);
		mValue = r;
	}

	return *this;
}

ATDecFloatOpt& ATDecFloatOpt::operator/=(const ATDecFloatOpt& other) {
	ATDecFloat r;
	if (mbValid && other.mbValid) {
		mbValid = ATDecFloatDiv(r, mValue, other.mValue);
		mValue = r;
	}

	return *this;
}

ATDecFloatOpt ATDecFloatOpt::operator+(const ATDecFloatOpt& other) const {
	ATDecFloatOpt v;
	v.mbValid = mbValid && other.mbValid && ATDecFloatAdd(v.mValue, mValue, other.mValue);

	return v;
}

ATDecFloatOpt ATDecFloatOpt::operator-(const ATDecFloatOpt& other) const {
	ATDecFloatOpt v;
	v.mbValid = mbValid && other.mbValid && ATDecFloatAdd(v.mValue, mValue, -other.mValue);

	return v;
}

ATDecFloatOpt ATDecFloatOpt::operator*(const ATDecFloatOpt& other) const {
	ATDecFloatOpt v;
	v.mbValid = mbValid && other.mbValid && ATDecFloatMul(v.mValue, mValue, other.mValue);

	return v;
}

ATDecFloatOpt ATDecFloatOpt::operator/(const ATDecFloatOpt& other) const {
	ATDecFloatOpt v;
	v.mbValid = mbValid && other.mbValid && ATDecFloatDiv(v.mValue, mValue, other.mValue);

	return v;
}

ATDecFloatOpt ATDecFloatOpt::PlyEvl(vdspan<const ATDecFloat> coeffs) const {
	if (!mbValid)
		return ATDecFloatOpt();

	ATDecFloatOpt r(ATDecFloat::Zero());

	for(const ATDecFloat& c : coeffs)
		r = r * mValue + c;

	return r;
}

ATDecFloatOpt ATDecFloatOpt::Log() const {
	// Typically, this would be a multiplication by precomputed 1/log10(e), but
	// we are using division to exactly match the standard math pack.
	ATDecFloatOpt x = Log10();

	return x / ATDecFloat::kLog10E;
}

ATDecFloatOpt ATDecFloatOpt::Log10() const {
	// The algorithm used here exactly matches the one used by the standard
	// math pack, and by extension, is depended upon by Turbo Basic XL. The
	// algorithm and precise constants come from "BASIC Scientific Routines
	// Volume I" by F. R. Ruckdeschel, Table VI.11, Series approximation
	// for log10(X) over the interval 1 <= X <= 10 (corrected). It uses
	// a minimax polynomial f(Z) for Z = (X-c)/(X+c) for C = sqrt(10).
	// This polynomial only has odd powers.
	//
	// The one additional factor is how to reduce the range to within [1,10].
	// This is done by chopping the exponent and then dividing by 10 if
	// necessary.

	// error on non-positive numbers
	if (mValue.mSignExp == 0 || mValue.mSignExp >= 0x80)
		return {};

	ATDecFloat x = mValue;

	// strip exponent and rescale to [1, 100)
	x.mSignExp = 0x40;

	// divide by 10 if needed; truncate as div would
	int intPow10 = (mValue.mSignExp - 0x40) * 2;

	if (x.mMantissa[0] >= 0x10) {
		for(int i = 3; i >= 0; --i)
			x.mMantissa[i + 1] = (x.mMantissa[i + 1] >> 4) | (x.mMantissa[i] << 4);

		x.mMantissa[0] >>= 4;
		++intPow10;
	}

	// change form
	static constexpr ATDecFloat sqrt10 { 0x40, { 0x03, 0x16, 0x22, 0x77, 0x66 }};
	ATDecFloatOpt z = (x - sqrt10) / (x + sqrt10);
	ATDecFloatOpt z2 = z * z;

	// evaluate odd powers
	static constexpr ATDecFloat kCoeffs[] {
		{ 0x3F, { 0x49, 0x15, 0x57, 0x11, 0x08 } },	// A10 =  0.4915571108
		{ 0xBF, { 0x51, 0x70, 0x49, 0x47, 0x08 } },	// A9  = -0.5170494708
		{ 0x3F, { 0x39, 0x20, 0x57, 0x61, 0x95 } },	// A8  =  0.3920576195
		{ 0xBF, { 0x04, 0x39, 0x63, 0x03, 0x55 } },	// A7  = -0.0439630355
		{ 0x3F, { 0x10, 0x09, 0x30, 0x12, 0x64 } },	// A6  =  0.1009301264
		{ 0x3F, { 0x09, 0x39, 0x08, 0x04, 0x60 } },	// A5  =  0.0939080460
		{ 0x3F, { 0x12, 0x42, 0x58, 0x47, 0x42 } },	// A4  =  0.1242584742
		{ 0x3F, { 0x17, 0x37, 0x12, 0x06, 0x08 } },	// A3  =  0.1737120608
		{ 0x3F, { 0x28, 0x95, 0x29, 0x71, 0x17 } },	// A2  =  0.2895297117
		{ 0x3F, { 0x86, 0x85, 0x88, 0x96, 0x44 } },	// A1  =  0.8685889644
	};

	static constexpr ATDecFloat half { 0x3F, { 0x50, 0x00, 0x00, 0x00, 0x00 } };

	auto r = z2.PlyEvl(kCoeffs) * z + half;

	// add in original exponent times two, plus one if we divided above
	if (intPow10) {
		ATDecFloat xp {};
		xp.mSignExp = intPow10 < 0 ? 0xC0 : 0x40;

		intPow10 = abs(intPow10);
		xp.mMantissa[0] = intPow10 + (intPow10 / 10)*6;

		r += xp;
	}

	return r;
}

ATDecFloatOpt ATDecFloatOpt::Exp() const {
	return ((*this) * ATDecFloat::kLog10E).Exp10();
}

ATDecFloatOpt ATDecFloatOpt::Exp10() const {
	// The algorithm used here exactly matches the one used by the standard
	// math pack, and by extension, is depended upon by Turbo Basic XL. The
	// algorithm and precise constants come from "BASIC Scientific Routines
	// Volume I" by F. R. Ruckdeschel, Table VI.13, Series approximation
	// for 10^X over the interval 0 <= X <= 1.

	// check for invalid input
	if (!mbValid)
		return *this;

	// For negative values, calculate with positive exponent and then invert.
	// This is both inefficient and reports FP errors instead of underflowing,
	// but we need to do this for bug parity.
	if (mValue.mSignExp & 0x80) {
		auto r = (-*this).Exp10();
		return r.IsValid() ? ATDecFloat::One() / r : ATDecFloat::Zero();
	}

	// check for input outside of [-100, 100]
	int exp = mValue.mSignExp & 0x7F;
	if (exp >= 0x41) {
		// overflow if input >= 100, underflow if <= -100
		if (mValue.mSignExp & 0x80)
			return ATDecFloatOpt(ATDecFloat::Zero());
		else
			return {};
	}

	ATDecFloatOpt x = *this;

	int intVal = 0;
	if (exp == 0x40) {
		intVal = mValue.mMantissa[0] - (mValue.mMantissa[0] >> 4)*6;

		// The standard math pack suboptimally uses a _rounded_ integer conversion
		// to do range reduction, which has two downsides: it moves the input to
		// the polynomial out of the optimal range. It also causes exp10() to
		// return an entirely erroneous result for values >65535.
		bool rounded = false;
		if (mValue.mMantissa[1] >= 0x50) {
			++intVal;
			rounded = true;
		}

		x.mValue.mMantissa[0] = 1;
		x -= ATDecFloat::One();

		if (rounded)
			x -= ATDecFloat::One();
	}

	static constexpr ATDecFloat kCoeffs[] {
		{ 0x3D, { 0x17, 0x94, 0x19, 0x00, 0x00 } },	// A9 = 0.0000179419
		{ 0x3D, { 0x57, 0x33, 0x05, 0x00, 0x00 } },	// A8 = 0.0000573305
		{ 0x3E, { 0x05, 0x54, 0x76, 0x62, 0x00 } },	// A7 = 0.0005547662
		{ 0x3E, { 0x32, 0x19, 0x62, 0x27, 0x00 } },	// A6 = 0.0032196227
		{ 0x3F, { 0x01, 0x68, 0x60, 0x30, 0x36 } },	// A5 = 0.0168603036
		{ 0x3F, { 0x07, 0x32, 0x03, 0x27, 0x41 } },	// A4 = 0.0732032741
		{ 0x3F, { 0x25, 0x43, 0x34, 0x56, 0x75 } },	// A3 = 0.2543345675
		{ 0x3F, { 0x66, 0x27, 0x37, 0x30, 0x50 } },	// A2 = 0.6627373050
		{ 0x40, { 0x01, 0x15, 0x12, 0x92, 0x55 } },	// A1 = 1.1512925485
		{ 0x3F, { 0x99, 0x99, 0x99, 0x99, 0x99 } },	// constant 1 term
	};

	ATDecFloatOpt y = x.PlyEvl(kCoeffs);
	ATDecFloatOpt z = y*y;

	if (intVal & 1)
		z *= ATDecFloat { 0x40, { 0x10, 0x00, 0x00, 0x00, 0x00 } };

	int newExp = z.mValue.mSignExp + (intVal >> 1);

	if (newExp < 64 - 49)
		return ATDecFloatOpt(ATDecFloat::Zero());
	
	if (newExp >= 64 + 49)
		return {};

	z.mValue.mSignExp = (uint8)newExp;

	return z;
}

ATDecFloatText ATDecFloatOpt::ToString() const {
	if (mbValid)
		return mValue.ToString();
	else
		return ATDecFloatText(ATDecFloatInvalid());
}

