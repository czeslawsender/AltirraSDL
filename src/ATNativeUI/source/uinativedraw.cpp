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

#include <stdafx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vd2/system/color.h>
#include <at/atcore/comsupport_win32.h>
#include <at/atnativeui/nativewindowproxy.h>
#include <at/atnativeui/uinativedraw.h>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

ID2D1Factory *g_pATD2D1Factory;
IDWriteFactory *g_pATDWriteFactory;

void ATUIInitNativeDraw() {
	if (!g_pATD2D1Factory) {
		D2D1_FACTORY_OPTIONS options {};

#ifdef _DEBUG
		// HKCU\Software\Microsoft\Direct3D\Direct2D\DebugLayerAppControlled must be set for this
		// to work.
		options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

		[[maybe_unused]] HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, &options, (void **)&g_pATD2D1Factory);
		VDASSERT(SUCCEEDED(hr));
	}

	if (!g_pATDWriteFactory) {
		[[maybe_unused]] HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&g_pATDWriteFactory);
		VDASSERT(SUCCEEDED(hr));
	}
}

void ATUIShutdownNativeDraw() {
	if (g_pATDWriteFactory) {
		g_pATDWriteFactory->Release();
		g_pATDWriteFactory = nullptr;
	}

	if (g_pATD2D1Factory) {
		g_pATD2D1Factory->Release();
		g_pATD2D1Factory = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////

struct ATUINativeDrawPath::Private {
	static D2D1_POINT_2F TranslatePt(const vdfloat2& pt) {
		return D2D1_POINT_2F {
			.x = pt.x,
			.y = pt.y
		};
	};
};

ATUINativeDrawPath::ATUINativeDrawPath() {
}

ATUINativeDrawPath::~ATUINativeDrawPath() {
}

ID2D1PathGeometry *ATUINativeDrawPath::CreatePathGeometry() {
	if (!mpPathGeometry && !mPoints.empty()) {
		HRESULT hr = g_pATD2D1Factory->CreatePathGeometry(~mpPathGeometry);

		if (SUCCEEDED(hr)) {
			vdrefptr<ID2D1GeometrySink> geometrySink;
			hr = mpPathGeometry->Open(~geometrySink);
			if (SUCCEEDED(hr)) {
				const vdfloat2 *pts = mPoints.data();
				bool open = false;
				bool haveFigureGeo = false;

				for(Command cmd : mCommands) {
					switch(cmd) {
						case Command::Begin:
							if (!open)
								open = true;
							else {
								VDASSERT(haveFigureGeo);
								geometrySink->EndFigure(D2D1_FIGURE_END_CLOSED);
							}

							geometrySink->BeginFigure(Private::TranslatePt(*pts++), D2D1_FIGURE_BEGIN_FILLED);

							haveFigureGeo = false;
							break;

						case Command::Line:
							geometrySink->AddLine(Private::TranslatePt(*pts++));
							haveFigureGeo = true;
							break;

						case Command::Quad:
							geometrySink->AddQuadraticBezier(
								D2D1_QUADRATIC_BEZIER_SEGMENT {
									.point1 = Private::TranslatePt(pts[0]),
									.point2 = Private::TranslatePt(pts[1])
								}
							);
							haveFigureGeo = true;
							pts += 2;
							break;

						case Command::Cubic:
							geometrySink->AddBezier(
								D2D1_BEZIER_SEGMENT {
									.point1 = Private::TranslatePt(pts[0]),
									.point2 = Private::TranslatePt(pts[1]),
									.point3 = Private::TranslatePt(pts[2])
								}
							);
							haveFigureGeo = true;
							pts += 3;
							break;

						case Command::Arc: {

#if 0
							const int data = (int)(pts[1].y + 0.5f);
							const bool ccw = (data & 1) != 0;
							const bool longArc = (data & 2) != 0;

							geometrySink->AddArc(
								D2D1_ARC_SEGMENT {
									.point = translatePt(pts[2]),
									.size = D2D1_SIZE_F { pts[0].x, pts[0].y },
									.rotationAngle = pts[1].x,
									.sweepDirection = ccw ? D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE : D2D1_SWEEP_DIRECTION_CLOCKWISE,
									.arcSize = longArc ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL
								}
							);
#else
							// Sigh... WINE up to 11.0 has a broken implementation of Direct2D that not only
							// stubs ArcTo(), but doesn't even update the geometry sink state properly, leading
							// to trashed geometry. Thus, we need to emulate arcs using beziers instead.
							GenerateArcCurves(*geometrySink, pts - 1);
#endif

							haveFigureGeo = true;
							pts += 3;
							break;
						}
					}
				}

				if (open) {
					VDASSERT(haveFigureGeo);
					geometrySink->EndFigure(D2D1_FIGURE_END_CLOSED);
				}

				VDASSERT(!mCommands.empty());

				geometrySink->Close();
			}
		}

		static uint32 sCount = 0;
		if (!(++sCount % 1000))
			VDDEBUG2("Created %u path geometries\n", sCount);

	}

	return mpPathGeometry;
}

void ATUINativeDrawPath::Begin(const vdfloat2& pt) {
	mPoints.push_back(pt);
	mCommands.push_back(Command::Begin);
}

void ATUINativeDrawPath::LineTo(const vdfloat2& pt) {
	mPoints.push_back(pt);
	mCommands.push_back(Command::Line);
}

void ATUINativeDrawPath::QuadTo(const vdfloat2& pt1, const vdfloat2& pt2) {
	mPoints.push_back(pt1);
	mPoints.push_back(pt2);
	mCommands.push_back(Command::Quad);
}

void ATUINativeDrawPath::CubicTo(const vdfloat2& pt1, const vdfloat2& pt2, const vdfloat2& pt3) {
	mPoints.push_back(pt1);
	mPoints.push_back(pt2);
	mPoints.push_back(pt3);
	mCommands.push_back(Command::Cubic);
}

void ATUINativeDrawPath::ArcTo(const vdfloat2& end, const vdfloat2& radii, float rotation, bool ccw, bool longArc) {
	// We have to guard against negative radii, or D2D will fail rendering with
	// a bogus path-not-closed error. This is very hard to debug without the
	// debug layer enabled as otherwise it only triggers at EndDraw().
	if (radii.x <= 0 || radii.y <= 0) {
		if (radii.x < 0 || radii.y < 0)
			VDFAIL("Cannot add arc with zero radius");

		LineTo(end);
		return;
	}

	mPoints.push_back(nsVDMath::max(radii, vdfloat2{0,0}));
	mPoints.push_back(vdfloat2 { rotation, (ccw ? 1.0f : 0.0f) + (longArc ? 2.0f : 0.0f) });
	mPoints.push_back(end);
	mCommands.push_back(Command::Arc);
}

void ATUINativeDrawPath::GenerateArcCurves(ID2D1GeometrySink& geometrySink, const vdfloat2 *pts) {
	using namespace nsVDMath;

	// Retrieve the start and end points.
	const int data = (int)(pts[2].y + 0.5f);
	const bool ccw = (data & 1) != 0;
	vdfloat2 startPt = pts[0];
	vdfloat2 endPt = pts[3];

	// Compute basis vectors for the ellipse, distorted to a circle.
	const float angle = pts[2].x * (nsVDMath::kfTwoPi / 180.0f);
	const float cs = cosf(angle);
	const float sn = sinf(angle);
	vdfloat2 circleX = vdfloat2 {  cs, sn };
	vdfloat2 circleY = vdfloat2 { -sn, cs };

	// Transform the start/end points back into circle space.
	const vdfloat2 radii = pts[1];
	const float rx = radii.x;
	const float ry = radii.y;
	const vdfloat2 norStartPt {
		dot(startPt, circleX) / rx,
		dot(startPt, circleY) / ry
	};

	const vdfloat2 norEndPt {
		dot(endPt, circleX) / rx,
		dot(endPt, circleY) / ry
	};

	// Compute the chord and chord length between the two points.
	const vdfloat2 norChord = norEndPt - norStartPt;
	const float chordLenSq = dot(norChord, norChord);

	// If the arc is degenerate due to a near zero chord length, just use a line.
	if (chordLenSq < 1e-7f) {
		geometrySink.AddLine(Private::TranslatePt(endPt));
	} else {
		// Compute the normal length by right angle triangle, with half the chord length on one
		// side and the normal leg on the other. The half chord length may exceed 1
		// either due to user error or numerical accuracy -- clamp the normal length to 0 in
		// that case.
		const float normalLenSq = std::max<float>(0.0f, 1.0f - chordLenSq*0.25f);

		// Determine which direction the normal vector should use. If the direction is ccw, then chord
		// should be rotated left to produce the normal vector, and for cw, right.
		const vdfloat2 normal = vdfloat2 { -norChord.y, norChord.x } * sqrtf(normalLenSq / chordLenSq);
		const vdfloat2 norMidChord = (norStartPt + norEndPt) * 0.5f;
		const vdfloat2 norCenter = ccw ? norMidChord - normal : norMidChord + normal;

		// At this point we have the circle center and the start/end points on the circle in
		// normalized space. Compute the rotation basis from the start to end points as a sin/cos
		// vector, by computing a basis from the relative normalized start point.
		const vdfloat2 norRelStartPt = norStartPt - norCenter;
		const vdfloat2 norRelEndPt = norEndPt - norCenter;

		const vdfloat2 arcRotation { dot(norRelStartPt,  norRelEndPt), dot(vdfloat2 { -norRelStartPt.y, norRelStartPt.x }, norRelEndPt) };

		// Subdivide the arc rotation vector.
		//
		// At a minimum, we need to divide the arc in two to have two reasonably accurate cubic
		// curves. If the arc is longer than 180 degrees, then we should use four curves.
		float angle = atan2f(arcRotation.y, arcRotation.x);
		if (angle < 0)
			angle += nsVDMath::kfTwoPi;

		if (ccw)
			angle -= nsVDMath::kfTwoPi;

		const bool use4 = fabs(angle) > nsVDMath::kfPi * 0.5f;
		const int numCurves = use4 ? 4 : 2;
		const float angleStep = use4 ? angle / 4.0f : angle / 2.0f;

		const vdfloat2 rotationStep { cosf(angleStep), sinf(angleStep) };
		vdfloat2 norRelCurPt = norRelStartPt - norCenter;

		// Add the curves.
		vdfloat2 ellipseX = circleX * rx;
		vdfloat2 ellipseY = circleY * ry;
		vdfloat2 center = norCenter.x * ellipseX + norCenter.y * ellipseY;
		vdfloat2 p1 = norRelStartPt;

		for(int curveIndex = 0; curveIndex < numCurves; ++curveIndex) {
			vdfloat2 p4 = rotationStep * p1.x + vdfloat2 { -rotationStep.y, rotationStep.x } * p1.y;

			// Convert the arc to a cubic curve using the formulas from:
			//
			// A.Riskus, G.Litukus. An Improved Algorithm for the Approximation of a Cubic Bezier Curve
			// and its Application for Approximating Quadratic Bezier Curve. Information Technology and
			// Control, 2013, T. 42, Nr. 4.
			//
			// Includes numerical accuracy improvement by Don Hatch from:
			// https://stackoverflow.com/questions/734076/how-to-best-approximate-a-geometrical-arc-with-a-bezier-curve
			//
			// The primary part of this algorithm is computing the relative length of the tangents,
			// called k2 in the algorithm. Hatch's accuracy improvement is not explained, but it avoids
			// 0/0 by applying Lagrange's Identity:
			//		cross(a,b)^2 = dot(a,a)*dot(b,b) - dot(a,b)^2
			//
			// Both a and b are unit length in our case, reducing this to:
			//		cross(a,b)^2 = 1 - dot(a,b)^2
			//
			// Given q2 = 1 + dot(a,b) and k2 = (4/3) * (sqrt(2*q2)-q2) / cross(a,b):
			//	k2 = (4/3) * [(sqrt(2*q2) - q2) * (sqrt(2*q2) + q2)] / [cross(a,b) * (sqrt(2*q2) + q2)]
			//	   = (4/3) * [(2*q2) - q2^2] / [cross(a,b) * (sqrt(2*q2) + q2)]
			//	   = (4/3) * [1 - dot(a,b)^2] / [cross(a,b) * (sqrt(2*q2) + q2)]
			//	   = (4/3) * cross(a,b)^2 / [cross(a,b) * (sqrt(2*q2) + q2)]
			//	   = (4/3) * cross(a,b) / (sqrt(2*q2) + q2)
			//
			// We always subdivide such that no curve segment exceeds 90 degrees, so q2 >= 1 and
			// divisor >= sqrt(2)+1.
			//
			// For an exact 90deg arc, this produces the classic value of 4/3*(sqrt(2)-1). This is not
			// the best value as it is slightly worse than minimax, but it's good enough.

			float q2 = 1.0f + dot(p1, p4);
			float k2 = (4.0f/3.0f) * (p1.x*p4.y - p1.y*p4.x) / (sqrtf(2.0f * q2) + q2);

			vdfloat2 p2 = p1 + vdfloat2 { -p1.y, p1.x } * k2;
			vdfloat2 p3 = p4 - vdfloat2 { -p4.y, p4.x } * k2;

			geometrySink.AddBezier(
				D2D1_BEZIER_SEGMENT(
					Private::TranslatePt(p2.x * ellipseX + p2.y * ellipseY + center),
					Private::TranslatePt(p3.x * ellipseX + p3.y * ellipseY + center),
					Private::TranslatePt(p4.x * ellipseX + p4.y * ellipseY + center)
				)
			);

			p1 = p4;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

ATUINativeTextFormat::ATUINativeTextFormat(const wchar_t *familyName, float size, int weight) {
	g_pATDWriteFactory->CreateTextFormat(familyName ? familyName : L"MS Shell Dlg 2", nullptr, (DWRITE_FONT_WEIGHT)weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &mpFormat);
}

ATUINativeTextFormat::ATUINativeTextFormat(const ATUINativeTextFormat& src) noexcept
	: mpFormat(src.mpFormat)
{
	if (mpFormat)
		mpFormat->AddRef();
}

ATUINativeTextFormat::~ATUINativeTextFormat() {
	if (mpFormat)
		mpFormat->Release();
}

ATUINativeTextFormat& ATUINativeTextFormat::operator=(const ATUINativeTextFormat& src) noexcept {
	if (mpFormat != src.mpFormat) {
		if (mpFormat)
			mpFormat->Release();

		mpFormat = src.mpFormat;

		if (mpFormat)
			mpFormat->AddRef();
	}

	return *this;
}

ATUINativeTextFormat& ATUINativeTextFormat::operator=(ATUINativeTextFormat&& src) noexcept {
	if (mpFormat)
		mpFormat->Release();

	mpFormat = src.mpFormat;
	src.mpFormat = nullptr;

	return *this;
}

void ATUINativeTextFormat::SetWordWrapEnabled(bool enabled) {
	if (mpFormat) {
		[[maybe_unused]] HRESULT hr = mpFormat->SetWordWrapping(enabled ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
		VDASSERT(SUCCEEDED(hr));
	}
}

void ATUINativeTextFormat::SetAlignment(Align alignment) {
	if (mpFormat) {
		DWRITE_TEXT_ALIGNMENT da = DWRITE_TEXT_ALIGNMENT_LEADING;

		switch(alignment) {
			case Align::Left:
				break;

			case Align::Center:
				da = DWRITE_TEXT_ALIGNMENT_CENTER;
				break;

			case Align::Right:
				da = DWRITE_TEXT_ALIGNMENT_TRAILING;
				break;

			case Align::Justified:
				da = DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
				break;
		}

		[[maybe_unused]] HRESULT hr = mpFormat->SetTextAlignment(da);
		VDASSERT(SUCCEEDED(hr));
	}
}

void ATUINativeTextFormat::SetVerticalAlignment(VAlign alignment) {
	if (mpFormat) {
		DWRITE_PARAGRAPH_ALIGNMENT da = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;

		switch(alignment) {
			case VAlign::Top:
				break;

			case VAlign::Middle:
				da = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
				break;

			case VAlign::Bottom:
				da = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
				break;
		}

		[[maybe_unused]] HRESULT hr = mpFormat->SetParagraphAlignment(da);
		VDASSERT(SUCCEEDED(hr));
	}
}

////////////////////////////////////////////////////////////////////////////////

ATUINativeTextLayout::ATUINativeTextLayout(VDStringSpanW text, const ATUINativeTextFormat& format, const vdfloat2& maxSize) {
	IDWriteTextFormat *dformat = format.GetDWriteTextFormat();

	if (dformat)
		g_pATDWriteFactory->CreateTextLayout(text.data(), text.size(), dformat, maxSize.x, maxSize.y, &mpLayout);
}

ATUINativeTextLayout::ATUINativeTextLayout(const ATUINativeTextLayout& src) noexcept
	: mpLayout(src.mpLayout)
{
	if (mpLayout)
		mpLayout->AddRef();
}

ATUINativeTextLayout::~ATUINativeTextLayout() {
	if (mpLayout)
		mpLayout->Release();
}

ATUINativeTextLayout& ATUINativeTextLayout::operator=(const ATUINativeTextLayout& src) noexcept {
	if (mpLayout != src.mpLayout) {
		if (mpLayout)
			mpLayout->Release();

		mpLayout = src.mpLayout;

		if (mpLayout)
			mpLayout->AddRef();
	}

	return *this;
}

ATUINativeTextLayout& ATUINativeTextLayout::operator=(ATUINativeTextLayout&& src) noexcept {
	if (mpLayout)
		mpLayout->Release();

	mpLayout = src.mpLayout;
	src.mpLayout = nullptr;

	return *this;
}

vdfloat2 ATUINativeTextLayout::GetSize() const {
	vdfloat2 sz { 0, 0 };

	if (mpLayout) {
		DWRITE_TEXT_METRICS metrics {};

		HRESULT hr = mpLayout->GetMetrics(&metrics);
		if (SUCCEEDED(hr)) {
			sz.x = metrics.width;
			sz.y = metrics.height;
		}
	}

	return sz;
}

void ATUINativeTextLayout::GetTextRectsForRange(uint32 start, uint32 n, vdfastvector<vdrect32f>& rects) const {
	rects.clear();

	if (!mpLayout)
		return;

	DWRITE_HIT_TEST_METRICS fastArray[4];
	vdfastvector<DWRITE_HIT_TEST_METRICS> slowArray;
	vdspan<const DWRITE_HIT_TEST_METRICS> metrics;

	UINT actualCount = 0;
	HRESULT hr = mpLayout->HitTestTextRange(start, n, 0, 0, fastArray, 4, &actualCount);
	if (SUCCEEDED(hr)) {
		metrics = vdspan<const DWRITE_HIT_TEST_METRICS>(fastArray, fastArray + actualCount);
	} else if (hr == E_NOT_SUFFICIENT_BUFFER) {
		slowArray.resize(actualCount);

		hr = mpLayout->HitTestTextRange(start, n, 0, 0, slowArray.data(), actualCount, &actualCount);
		if (FAILED(hr))
			return;

		slowArray.resize(actualCount);
		metrics = slowArray;
	} else {
		return;
	}

	rects.resize(actualCount);
	for(UINT i = 0; i < actualCount; ++i) {
		vdrect32f& dst = rects[i];
		const DWRITE_HIT_TEST_METRICS& src = metrics[i];

		dst.set(src.left, src.top, src.left + src.width, src.top + src.height);
	}
}

ATUINativeTextLayout::HitTestResult ATUINativeTextLayout::HitTest(const vdpoint32f& pt) const {
	HitTestResult result {};

	DWRITE_HIT_TEST_METRICS metrics {};
	BOOL inside = FALSE;
	BOOL trailingHit = FALSE;
	HRESULT hr = mpLayout->HitTestPoint(pt.x, pt.y, &trailingHit, &inside, &metrics);
	if (SUCCEEDED(hr)) {
		result.mNearestPosition = metrics.textPosition;
		result.mbInside = inside;
	}

	return result;
}

vdrect32f ATUINativeTextLayout::GetTextBoundingRectForRange(uint32 start, uint32 n) const {
	vdfastvector<vdrect32f> rects;

	GetTextRectsForRange(start, n, rects);

	vdrect32f r(0, 0, 0, 0);
	bool empty = true;

	for(const vdrect32f& r2 : rects) {
		if (!r2.empty()) {
			if (empty) {
				r = r2;
				empty = false;
			} else {
				r.add(r2);
			}
		}
	}

	return r;
}

void ATUINativeTextLayout::SetTextRangeColor(uint32 start, uint32 n, const ATUINativeDrawSolidBrush *brush) {
	if (mpLayout) {
		IUnknown *unk = brush ? brush->GetTextEffect() : nullptr;

		DWRITE_TEXT_RANGE range;
		range.startPosition = start;
		range.length = n;
		[[maybe_unused]] HRESULT hr = mpLayout->SetDrawingEffect(unk, range);
		VDASSERT(SUCCEEDED(hr));
	}
}

////////////////////////////////////////////////////////////////////////////////

struct ATUINativeDraw::Helpers {
	static D2D1_COLOR_F ColorFromARGB32(uint32 c);
};

D2D1_COLOR_F ATUINativeDraw::Helpers::ColorFromARGB32(uint32 c) {
	auto fc = vdfloat32x4(VDColorARGB::FromARGB8(c | 0xFF000000));

	D2D1_COLOR_F r {};
	r.r = fc.x();
	r.g = fc.y();
	r.b = fc.z();
	r.a = fc.w();

	return r;
}

////////////////////////////////////////////////////////////////////////////////

class __declspec(uuid("FE9CF68E-7D52-4585-80A8-7A7654720A58")) ATUINativeDrawSolidColorBrushHandle final : public ATCOMBaseW32<IUnknown> {
public:
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObj) override;

	uint32 mColorARGB = 0;
	vdrefptr<ID2D1SolidColorBrush> mpSolidBrush;
};

HRESULT STDMETHODCALLTYPE ATUINativeDrawSolidColorBrushHandle::QueryInterface(REFIID iid, void **ppvObj) {
	if (!ppvObj)
		return E_POINTER;

	if (iid == __uuidof(ATUINativeDrawSolidColorBrushHandle)) {
		*ppvObj = this;
		AddRef();
		return S_OK;
	} else if (iid == __uuidof(IUnknown)) {
		*ppvObj = static_cast<IUnknown *>(this);
		AddRef();
		return S_OK;
	}

	*ppvObj = nullptr;
	return E_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////

class ATUINativeDrawTextRenderer final : public ATCOMBaseW32<IDWriteTextRenderer> {
public:
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObj) override;

	HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void *clientDrawingContext, BOOL *isDisabled) override;
	HRESULT STDMETHODCALLTYPE GetCurrentTransform(void *clientDrawingContext, DWRITE_MATRIX *transform) override;
	HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void *clientDrawingContext, FLOAT *pixelsPerDip) override;

	HRESULT STDMETHODCALLTYPE DrawGlyphRun(
		void *clientDrawingContext,
		FLOAT baselineOriginX,
		FLOAT baselineOriginY,
		DWRITE_MEASURING_MODE measuringMode,
		const DWRITE_GLYPH_RUN *glyphRun,
		const DWRITE_GLYPH_RUN_DESCRIPTION *glyphRunDescription,
		IUnknown *clientDrawingEffect
		) override;
	HRESULT STDMETHODCALLTYPE DrawUnderline(
		void *clientDrawingContext,
		FLOAT baselineOriginX,
		FLOAT baselineOriginY,
		const DWRITE_UNDERLINE *underline,
		IUnknown *clientDrawingEffect
		) override;
	HRESULT STDMETHODCALLTYPE DrawStrikethrough(
		void *clientDrawingContext,
		FLOAT baselineOriginX,
		FLOAT baselineOriginY,
		const DWRITE_STRIKETHROUGH *strikethrough,
		IUnknown *clientDrawingEffect
		) override;
	HRESULT STDMETHODCALLTYPE DrawInlineObject(
		void *clientDrawingContext,
		FLOAT originX,
		FLOAT originY,
		IDWriteInlineObject *inlineObject,
		BOOL isSideways,
		BOOL isRightToLeft,
		IUnknown *clientDrawingEffect
		) override;
};

HRESULT STDMETHODCALLTYPE ATUINativeDrawTextRenderer::QueryInterface(REFIID iid, void **ppvObj) {
	if (!ppvObj)
		return E_POINTER;

	if (iid == __uuidof(IDWriteTextRenderer)) {
		*ppvObj = static_cast<IDWriteTextRenderer *>(this);
		AddRef();
		return S_OK;
	} else if (iid == __uuidof(IDWritePixelSnapping)) {
		*ppvObj = static_cast<IDWritePixelSnapping *>(this);
		AddRef();
		return S_OK;
	} else if (iid == __uuidof(IUnknown)) {
		*ppvObj = static_cast<IUnknown *>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE ATUINativeDrawTextRenderer::IsPixelSnappingDisabled(void *clientDrawingContext, BOOL *isDisabled) {
	*isDisabled = FALSE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ATUINativeDrawTextRenderer::GetCurrentTransform(void *clientDrawingContext, DWRITE_MATRIX *transform) {
	ATUINativeDraw& draw = *(ATUINativeDraw *)clientDrawingContext;

	D2D1_MATRIX_3X2_F mx {};
	draw.mpRT->GetTransform(&mx);

	transform->dx = mx.dx;
	transform->dy = mx.dy;
	transform->m11 = mx.m11;
	transform->m12 = mx.m12;
	transform->m21 = mx.m21;
	transform->m22 = mx.m22;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ATUINativeDrawTextRenderer::GetPixelsPerDip(void *clientDrawingContext, FLOAT *pixelsPerDip) {
	ATUINativeDraw& draw = *(ATUINativeDraw *)clientDrawingContext;

	float dpiX = 96, dpiY = 96;
	draw.mpRT->GetDpi(&dpiX, &dpiY);

	*pixelsPerDip = dpiX / 96.0f;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ATUINativeDrawTextRenderer::DrawGlyphRun(
	void *clientDrawingContext,
	FLOAT baselineOriginX,
	FLOAT baselineOriginY,
	DWRITE_MEASURING_MODE measuringMode,
	const DWRITE_GLYPH_RUN *glyphRun,
	const DWRITE_GLYPH_RUN_DESCRIPTION *glyphRunDescription,
	IUnknown *clientDrawingEffect)
{
	ATUINativeDraw& draw = *(ATUINativeDraw *)clientDrawingContext;

	D2D1_POINT_2F baselineOrigin;
	baselineOrigin.x = baselineOriginX;
	baselineOrigin.y = baselineOriginY;

	draw.mpRT->DrawGlyphRun(
		baselineOrigin,
		glyphRun,
		draw.ResolveSolidBrushEffect(clientDrawingEffect),
		measuringMode
	);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ATUINativeDrawTextRenderer::DrawUnderline(
	void *clientDrawingContext,
	FLOAT baselineOriginX,
	FLOAT baselineOriginY,
	const DWRITE_UNDERLINE *underline,
	IUnknown *clientDrawingEffect)
{
	ATUINativeDraw& draw = *(ATUINativeDraw *)clientDrawingContext;

	D2D1_RECT_F r;
	r.left = baselineOriginX;
	r.top = baselineOriginY + underline->offset;
	r.right = r.left + underline->width;
	r.bottom = r.top + underline->thickness;

	draw.mpRT->FillRectangle(r, draw.ResolveSolidBrushEffect(clientDrawingEffect));

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ATUINativeDrawTextRenderer::DrawStrikethrough(
	void *clientDrawingContext,
	FLOAT baselineOriginX,
	FLOAT baselineOriginY,
	const DWRITE_STRIKETHROUGH *strikethrough,
	IUnknown *clientDrawingEffect)
{
	ATUINativeDraw& draw = *(ATUINativeDraw *)clientDrawingContext;

	D2D1_RECT_F r;
	r.left = baselineOriginX;
	r.top = baselineOriginY + strikethrough->offset;
	r.right = r.left + strikethrough->width;
	r.bottom = r.top + strikethrough->thickness;

	draw.mpRT->FillRectangle(r, draw.ResolveSolidBrushEffect(clientDrawingEffect));

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ATUINativeDrawTextRenderer::DrawInlineObject(
	void *clientDrawingContext,
	FLOAT originX,
	FLOAT originY,
	IDWriteInlineObject *inlineObject,
	BOOL isSideways,
	BOOL isRightToLeft,
	IUnknown *clientDrawingEffect)
{
	return E_NOTIMPL;
}

////////////////////////////////////////////////////////////////////////////////

ATUINativeDraw::ATUINativeDraw() {
	mpTextRenderer = new ATUINativeDrawTextRenderer;
}

ATUINativeDraw::~ATUINativeDraw() {
}

void ATUINativeDraw::Init(ATUINativeWindowProxy& window) {
	D2D1_RENDER_TARGET_PROPERTIES rtProps {};
	D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRtProps {};
	hwndRtProps.hwnd = window.GetWindowHandle();

	vdsize32 sz = window.GetClientSize();

	hwndRtProps.pixelSize.width = sz.w;
	hwndRtProps.pixelSize.height = sz.h;
	hwndRtProps.presentOptions = D2D1_PRESENT_OPTIONS_IMMEDIATELY;

	HRESULT hr = g_pATD2D1Factory->CreateHwndRenderTarget(&rtProps, &hwndRtProps, ~mpRT);
	VDVERIFY(SUCCEEDED(hr));
}

void ATUINativeDraw::Resize(int w, int h) {
	HRESULT hr = mpRT->Resize(D2D1_SIZE_U { .width = (UINT32)w, .height = (UINT32)h });
	VDVERIFY(SUCCEEDED(hr));
}

bool ATUINativeDraw::Begin() {
	mpRT->BeginDraw();

	D2D1_MATRIX_3X2_F mx {};
	mx.m11 = 1.0f;
	mx.m22 = 1.0f;
	mpRT->SetTransform(mx);

	return true;
}

void ATUINativeDraw::End() {
	[[maybe_unused]] HRESULT hr = mpRT->EndDraw();

	VDASSERT(SUCCEEDED(hr));
}

vdfloat2 ATUINativeDraw::GetTargetSize() const {
	D2D1_SIZE_F sz = mpRT->GetSize();

	return vdfloat2 { sz.width, sz.height };
}

float ATUINativeDraw::GetDpi() const {
	float dpiX = 96, dpiY = 96;

	mpRT->GetDpi(&dpiX, &dpiY);

	return dpiX;
}

void ATUINativeDraw::SetDpi(float dpi) {
	mpRT->SetDpi(dpi, dpi);
}

void ATUINativeDraw::SetTransform(const vdfloat2& scale, const vdfloat2& offset) {
	D2D1_MATRIX_3X2_F mx {};
	mx.m11 = scale.x;
	mx.m22 = scale.y;
	mx.dx = offset.x;
	mx.dy = offset.y;
	mpRT->SetTransform(mx);
}

void ATUINativeDraw::Clear(uint32 c) {
	mpRT->Clear(Helpers::ColorFromARGB32(c));
}

void ATUINativeDraw::FillRect(const vdfloat2& origin, const vdfloat2& size, uint32 c) {
	SetBrushColor(c);

	if (mpSolidBrush) {
		mpRT->FillRectangle(D2D1_RECT_F { origin.x, origin.y, origin.x + size.x, origin.y + size.y }, mpSolidBrush);
	}
}

void ATUINativeDraw::FillEllipse(const vdfloat2& center, float rx, float ry, uint32 c) {
	D2D1_ELLIPSE ell {};
	ell.point.x = center.x;
	ell.point.y = center.y;
	ell.radiusX = rx;
	ell.radiusY = ry;

	SetBrushColor(c);

	if (mpSolidBrush)
		mpRT->FillEllipse(ell, mpSolidBrush);
}

void ATUINativeDraw::DrawPath(ATUINativeDrawPath& path, uint32 c, float strokeWidth) {
	SetBrushColor(c);

	if (mpSolidBrush) {
		ID2D1PathGeometry *geo = path.CreatePathGeometry();

		if (geo)
			mpRT->DrawGeometry(geo, mpSolidBrush, strokeWidth, nullptr);
	}
}

void ATUINativeDraw::FillPath(ATUINativeDrawPath& path, uint32 c) {
	SetBrushColor(c);

	if (mpSolidBrush) {
		ID2D1PathGeometry *geo = path.CreatePathGeometry();
		if (geo)
			mpRT->FillGeometry(geo, mpSolidBrush, nullptr);
	}
}

void ATUINativeDraw::DrawString(VDStringSpanW text, const ATUINativeTextFormat& format, uint32 c, const vdrect32f& area, ATUINativeTextRenderOpts options) {
	SetBrushColor(c);

	if (mpSolidBrush && format.IsValid()) {
		mpRT->DrawText(text.data(), text.size(), format.GetDWriteTextFormat(),
			D2D1_RECT_F { area.left, area.top, area.right, area.bottom }, mpSolidBrush);
	}
}

void ATUINativeDraw::DrawString(const vdfloat2& origin, const ATUINativeTextLayout& layout, uint32 c, ATUINativeTextRenderOpts options) {
	SetBrushColor(c);

	if (mpSolidBrush && layout.IsValid()) {
		layout.GetDWriteTextLayout()->Draw(
			this,
			mpTextRenderer,
			origin.x,
			origin.y
		);

		//mpRT->DrawTextLayout(D2D1_POINT_2F { origin.x, origin.y }, layout.GetDWriteTextLayout(), mpSolidBrush);

		[[maybe_unused]] HRESULT hr = mpRT->Flush();
		VDASSERT(SUCCEEDED(hr));
	}
}

void ATUINativeDraw::SetBrushColor(uint32 c) {
	if (mSolidBrushColor != c || !mpSolidBrush) {
		HRESULT hr = mpRT->CreateSolidColorBrush(Helpers::ColorFromARGB32(c), ~mpSolidBrush);
		if (SUCCEEDED(hr))
			mSolidBrushColor = c;
	}
}

vdrefptr<ATUINativeDrawSolidColorBrushHandle> ATUINativeDraw::CreateSolidBrushHandle(uint32 c) {
	for(ATUINativeDrawSolidColorBrushHandle *h : mSolidBrushHandles) {
		if (h->mColorARGB == c)
			return vdrefptr(h);
	}

	vdrefptr<ATUINativeDrawSolidColorBrushHandle> p(new ATUINativeDrawSolidColorBrushHandle);
	p->mColorARGB = c;

	mSolidBrushHandles.push_back(p);

	return p;
}

ID2D1SolidColorBrush *ATUINativeDraw::ResolveSolidBrush(ATUINativeDrawSolidColorBrushHandle& h) {
	if (!h.mpSolidBrush) {
		HRESULT hr = mpRT->CreateSolidColorBrush(Helpers::ColorFromARGB32(h.mColorARGB), ~h.mpSolidBrush);
		if (FAILED(hr))
			return mpSolidBrush;
	}

	return h.mpSolidBrush;
}

ID2D1SolidColorBrush *ATUINativeDraw::ResolveSolidBrushEffect(IUnknown *p) {
	if (p) {
		ATUINativeDrawSolidColorBrushHandle *h = nullptr;
		HRESULT hr = p->QueryInterface(__uuidof(ATUINativeDrawSolidColorBrushHandle), (void **)&h);

		if (SUCCEEDED(hr)) {
			ID2D1SolidColorBrush *brush = ResolveSolidBrush(*h);
			h->Release();

			return brush;
		}
	}

	return mpSolidBrush;
}

////////////////////////////////////////////////////////////////////////////////

ATUINativeDrawSolidBrush::ATUINativeDrawSolidBrush() noexcept = default;
ATUINativeDrawSolidBrush::ATUINativeDrawSolidBrush(const ATUINativeDrawSolidBrush&) noexcept = default;
ATUINativeDrawSolidBrush::ATUINativeDrawSolidBrush(ATUINativeDrawSolidBrush&&) noexcept = default;
ATUINativeDrawSolidBrush::~ATUINativeDrawSolidBrush() = default;

ATUINativeDrawSolidBrush& ATUINativeDrawSolidBrush::operator=(const ATUINativeDrawSolidBrush&) noexcept = default;
ATUINativeDrawSolidBrush& ATUINativeDrawSolidBrush::operator=(ATUINativeDrawSolidBrush&&) noexcept = default;

IUnknown *ATUINativeDrawSolidBrush::GetTextEffect() const {
	return mpBrushHandle;
}

void ATUINativeDrawSolidBrush::SetColorRGB(ATUINativeDraw& nativeDraw, uint32 rgb) {
	SetColorARGB(nativeDraw, rgb | 0xFF000000);
}

void ATUINativeDrawSolidBrush::SetColorARGB(ATUINativeDraw& nativeDraw, uint32 argb) {
	if (mColor != argb) {
		mColor = argb;

		mpBrushHandle = nullptr;
	}

	if (!mpBrushHandle)
		mpBrushHandle = nativeDraw.CreateSolidBrushHandle(mColor);
}

