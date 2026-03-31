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

#ifndef f_AT_ATNATIVEUI_UINATIVEDRAW_H
#define f_AT_ATNATIVEUI_UINATIVEDRAW_H

#include <vd2/system/refcount.h>
#include <at/atcore/enumutils.h>

void ATUIInitNativeDraw();
void ATUIShutdownNativeDraw();

struct ID2D1GeometrySink;
struct ID2D1PathGeometry;
struct ID2D1HwndRenderTarget;
struct ID2D1SolidColorBrush;
struct IDWriteTextFormat;
struct IDWriteTextLayout;
struct IDWriteTextRenderer;

class ATUINativeWindowProxy;
class ATUINativeDraw;
class ATUINativeDrawSolidColorBrushHandle;
class ATUINativeDrawSolidBrush;

class ATUINativeDrawPath {
	ATUINativeDrawPath(const ATUINativeDrawPath&) = delete;
	ATUINativeDrawPath& operator=(const ATUINativeDrawPath&) = delete;

public:
	ATUINativeDrawPath();
	~ATUINativeDrawPath();

	ID2D1PathGeometry *CreatePathGeometry();

	void Begin(const vdfloat2& pt);
	void LineTo(const vdfloat2& pt);
	void CubicTo(const vdfloat2& pt1, const vdfloat2& pt2, const vdfloat2& pt3);
	void QuadTo(const vdfloat2& pt1, const vdfloat2& pt2);

	// Add an arc to the specified end point on an ellipse of the specified radii and rotation,
	// direction, and whether it goes around the long side.
	//
	// rotation:
	//		Rotation angle of the ellipse in degrees.
	//
	// ccw:
	//		True if the arc should be swept in the counter clockwise direction, clockwise
	//		otherwise.
	//
	// longArc:
	//		True if the arc segment should exceed 180 degrees.
	void ArcTo(const vdfloat2& end, const vdfloat2& radii, float rotation, bool ccw, bool longArc);

private:
	struct Private;

	void GenerateArcCurves(ID2D1GeometrySink& geometrySink, const vdfloat2 *pts);

	enum class Command : uint8 {
		Begin,
		Line,
		Quad,
		Cubic,
		Arc,
	};

	vdrefptr<ID2D1PathGeometry> mpPathGeometry;

	vdfastvector<vdfloat2> mPoints;
	vdfastvector<Command> mCommands;
};

enum class ATUINativeTextRenderOpts : uint32 {
	Default = 0,
	NoSnap = 1,
	Clip = 2
};

AT_IMPLEMENT_ENUM_FLAGS(ATUINativeTextRenderOpts);

enum class ATUINativeTextAlignment : uint8 {
	Left,
	Center,
	Right,
	Justified
};

enum class ATUINativeTextVAlignment : uint8 {
	Top,
	Middle,
	Bottom
};

class ATUINativeTextFormat {
public:
	using Align = ATUINativeTextAlignment;
	using VAlign = ATUINativeTextVAlignment;

	ATUINativeTextFormat() = default;
	ATUINativeTextFormat(const wchar_t *familyName, float size, int weight = 400);
	ATUINativeTextFormat(const ATUINativeTextFormat&) noexcept;

	ATUINativeTextFormat(ATUINativeTextFormat&& src) noexcept
		: mpFormat(src.mpFormat)
	{
		src.mpFormat = nullptr;
	}

	~ATUINativeTextFormat();

	ATUINativeTextFormat& operator=(const ATUINativeTextFormat&) noexcept;
	ATUINativeTextFormat& operator=(ATUINativeTextFormat&&) noexcept;

	bool IsValid() const { return mpFormat != nullptr; }
	IDWriteTextFormat *GetDWriteTextFormat() const { return mpFormat; }

	void SetWordWrapEnabled(bool enabled);
	void SetAlignment(Align alignment);
	void SetVerticalAlignment(VAlign alignment);

private:
	IDWriteTextFormat *mpFormat = nullptr;
};

class ATUINativeTextLayout {
public:
	ATUINativeTextLayout() = default;
	ATUINativeTextLayout(VDStringSpanW text, const ATUINativeTextFormat& format, const vdfloat2& maxSize);
	ATUINativeTextLayout(const ATUINativeTextLayout&) noexcept;

	ATUINativeTextLayout(ATUINativeTextLayout&& src) noexcept
		: mpLayout(src.mpLayout)
	{
		src.mpLayout = nullptr;
	}

	~ATUINativeTextLayout();

	ATUINativeTextLayout& operator=(const ATUINativeTextLayout&) noexcept;
	ATUINativeTextLayout& operator=(ATUINativeTextLayout&&) noexcept;

	bool IsValid() const { return mpLayout != nullptr; }
	IDWriteTextLayout *GetDWriteTextLayout() const { return mpLayout; }

	vdfloat2 GetSize() const;

	struct HitTestResult {
		uint32 mNearestPosition = 0;
		bool mbInside = false;
	};

	HitTestResult HitTest(const vdpoint32f& pt) const;

	void GetTextRectsForRange(uint32 start, uint32 n, vdfastvector<vdrect32f>& rects) const;
	vdrect32f GetTextBoundingRectForRange(uint32 start, uint32 n) const;

	void SetTextRangeColor(uint32 start, uint32 n, const ATUINativeDrawSolidBrush *brush);

private:
	IDWriteTextLayout *mpLayout = nullptr;
};

// Native drawing context.
//
// Coordinates are in Direct2D device independent pixels (DIPs), which are in 96 DPI.
//
class ATUINativeDraw {
	friend class ATUINativeDrawSolidBrush;
	friend class ATUINativeDrawTextRenderer;

	ATUINativeDraw(const ATUINativeDraw&) = delete;
	ATUINativeDraw& operator=(const ATUINativeDraw&) = delete;

public:
	ATUINativeDraw();
	~ATUINativeDraw();

	void Init(ATUINativeWindowProxy& window);
	void Resize(int w, int h);
	bool Begin();
	void End();

	vdfloat2 GetTargetSize() const;

	float GetDpi() const;
	void SetDpi(float dpi);
	void SetTransform(const vdfloat2& scale, const vdfloat2& offset);

	void Clear(uint32 c);
	void FillRect(const vdfloat2& origin, const vdfloat2& size, uint32 c);
	void FillEllipse(const vdfloat2& center, float rx, float ry, uint32 c);
	void DrawPath(ATUINativeDrawPath& path, uint32 c, float strokeWidth);
	void FillPath(ATUINativeDrawPath& path, uint32 c);

	void DrawString(VDStringSpanW text, const ATUINativeTextFormat& format, uint32 c, const vdrect32f& area, ATUINativeTextRenderOpts options = ATUINativeTextRenderOpts::Default);
	void DrawString(const vdfloat2& origin, const ATUINativeTextLayout& layout, uint32 c, ATUINativeTextRenderOpts options = ATUINativeTextRenderOpts::Default);

private:
	struct Helpers;

	void SetBrushColor(uint32 c);
	vdrefptr<ATUINativeDrawSolidColorBrushHandle> CreateSolidBrushHandle(uint32 c);
	ID2D1SolidColorBrush *ResolveSolidBrush(ATUINativeDrawSolidColorBrushHandle& h);
	ID2D1SolidColorBrush *ResolveSolidBrushEffect(IUnknown *p);

	vdrefptr<ID2D1HwndRenderTarget> mpRT;
	vdrefptr<ID2D1SolidColorBrush> mpSolidBrush;
	uint32 mSolidBrushColor = 0;
	vdrefptr<IDWriteTextRenderer> mpTextRenderer;

	vdfastvector<ATUINativeDrawSolidColorBrushHandle *> mSolidBrushHandles;
};

class ATUINativeDrawSolidBrush {
public:
	ATUINativeDrawSolidBrush() noexcept;
	ATUINativeDrawSolidBrush(const ATUINativeDrawSolidBrush&) noexcept;
	ATUINativeDrawSolidBrush(ATUINativeDrawSolidBrush&&) noexcept;
	~ATUINativeDrawSolidBrush();

	ATUINativeDrawSolidBrush& operator=(const ATUINativeDrawSolidBrush&) noexcept;
	ATUINativeDrawSolidBrush& operator=(ATUINativeDrawSolidBrush&&) noexcept;

	IUnknown *GetTextEffect() const;

	void SetColorRGB(ATUINativeDraw& nativeDraw, uint32 rgb);
	void SetColorARGB(ATUINativeDraw& nativeDraw, uint32 argb);

private:
	uint32 mColor = 0xFF000000;
	vdrefptr<ATUINativeDrawSolidColorBrushHandle> mpBrushHandle;
};

#endif
