//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2017 Avery Lee
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
#include <vd2/system/binary.h>
#include <vd2/system/constexpr.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/registry.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/zip.h>
#include <vd2/Dita/services.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/resample.h>
#include <vd2/Riza/bitmap.h>
#include <at/atcore/configvar.h>
#include <at/atcore/progress.h>
#include <at/atdebugger/target.h>
#include <at/atio/cassetteimage.h>
#include <at/atnativeui/dialog.h>
#include <at/atnativeui/messageloop.h>
#include <at/atnativeui/theme.h>
#include <at/atnativeui/theme_win32.h>
#include <at/atnativeui/uiframe.h>
#include <at/atnativeui/uinativecontrol.h>
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include "cassette.h"
#include "console.h"
#include "debugger.h"
#include "oshelper.h"
#include "resource.h"
#include "savestateio.h"
#include "simulator.h"
#include "trace.h"
#include "tracecpu.h"
#include "traceio.h"
#include "tracetape.h"
#include "tracevideo.h"
#include "uicommondialogs.h"
#include "uihistoryview.h"
#include "profiler.h"
#include "profilerui.h"

extern ATSimulator g_sim;

///////////////////////////////////////////////////////////////////////////

void ATUIShowDialogTapeEditorAtLocation(uint32 sample, float pixelsPerSample);
void ATUINotifyTraceViewerThemeChanged();
vdrefptr<ATTraceCollection> ATLoadTraceFromAtari800(const wchar_t *file);

namespace {
	struct TraceViewerTheme {
		ATConfigVarRGBColor mPanelBackground;
		ATConfigVarRGBColor mTimescaleBackground;
		ATConfigVarRGBColor mTimescaleText;
		ATConfigVarRGBColor mChannelBackground;
		ATConfigVarRGBColor mChannelText;
		ATConfigVarRGBColor mChannelOutline;
		ATConfigVarRGBColor mEventBackground;
		ATConfigVarRGBColor mEventVBlank;
		ATConfigVarRGBColor mEventVSync;
		ATConfigVarRGBColor mEventOutline;
	};

	static const TraceViewerTheme kTraceViewerThemeDark {
		.mPanelBackground		= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.panel_bg", 0x303030, ATUINotifyThemeChanged),
		.mTimescaleBackground	= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.timescale_bg", 0x202020, ATUINotifyThemeChanged),
		.mTimescaleText			= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.timescale_text", 0xD0D0D0, ATUINotifyThemeChanged),
		.mChannelBackground		= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.channel_bg", 0x222424, ATUINotifyThemeChanged),
		.mChannelText			= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.channel_text", 0xD0D0D0, ATUINotifyThemeChanged),
		.mChannelOutline		= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.channel_outline", 0x505050, ATUINotifyThemeChanged),
		.mEventBackground		= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.event_bg", 0x202020, ATUINotifyThemeChanged),
		.mEventVBlank			= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.event_vblank", 0x2C2C2C, ATUINotifyThemeChanged),
		.mEventVSync			= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.event_vsync", 0x404040, ATUINotifyThemeChanged),
		.mEventOutline			= ATConfigVarRGBColor("ui.theme.trace_viewer.dark.event_outline", 0x707070, ATUINotifyThemeChanged),
	};

	static const TraceViewerTheme kTraceViewerThemeLight {
		.mPanelBackground		= ATConfigVarRGBColor("ui.theme.trace_viewer.light.panel_bg", 0xD8D8D8, ATUINotifyThemeChanged),
		.mTimescaleBackground	= ATConfigVarRGBColor("ui.theme.trace_viewer.light.timescale_bg", 0xE8E8E8, ATUINotifyThemeChanged),
		.mTimescaleText			= ATConfigVarRGBColor("ui.theme.trace_viewer.light.timescale_text", 0x000000, ATUINotifyThemeChanged),
		.mChannelBackground		= ATConfigVarRGBColor("ui.theme.trace_viewer.light.channel_bg", 0xE6E4E4, ATUINotifyThemeChanged),
		.mChannelText			= ATConfigVarRGBColor("ui.theme.trace_viewer.light.channel_text", 0x000000, ATUINotifyThemeChanged),
		.mChannelOutline		= ATConfigVarRGBColor("ui.theme.trace_viewer.light.channel_outline", 0x505050, ATUINotifyThemeChanged),		// currently not used in light mode
		.mEventBackground		= ATConfigVarRGBColor("ui.theme.trace_viewer.light.event_bg", 0xF0F0F0, ATUINotifyThemeChanged),
		.mEventVBlank			= ATConfigVarRGBColor("ui.theme.trace_viewer.light.event_vblank", 0xC0C0C0, ATUINotifyThemeChanged),
		.mEventVSync			= ATConfigVarRGBColor("ui.theme.trace_viewer.light.event_vsync", 0xE8E8E8, ATUINotifyThemeChanged),
		.mEventOutline			= ATConfigVarRGBColor("ui.theme.trace_viewer.light.event_outline", 0x000000, ATUINotifyThemeChanged),
	};

	const TraceViewerTheme& GetTraceViewerTheme() {
		return ATUIIsDarkThemeActive() ? kTraceViewerThemeDark : kTraceViewerThemeLight;
	}

	void RecolorToolstripImage(VDPixmap& px, std::initializer_list<uint32> indices) {
		if (!ATUIIsDarkThemeActive() || px.format != nsVDPixmap::kPixFormat_XRGB8888)
			return;

		uint32 tileSize = (uint32)px.h;		// tile width/height = tilestrip height
		uint32 n = (uint32)px.w / tileSize;

		for(uint32 idx : indices) {
			if (idx >= n)
				continue;

			for(uint32 y = 0; y < tileSize; ++y) {
				uint32 *p = (uint32 *)((char *)px.data + px.pitch * y) + tileSize * idx;

				for(uint32 x = 0; x < tileSize; ++x)
					p[x] = (p[x] & 0xFF000000) | 0xC0C0C0;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

template<typename T>
class ATGDIObjectW32 {
public:
	vdnothrow ATGDIObjectW32() vdnoexcept : mHandle(nullptr), mpBlock(nullptr){}
	vdnothrow ATGDIObjectW32(T handle) vdnoexcept : ATGDIObjectW32() { reset(handle); }
	vdnothrow ATGDIObjectW32(const ATGDIObjectW32&) vdnoexcept;

	vdnothrow ATGDIObjectW32(ATGDIObjectW32&& src) vdnoexcept {
		mHandle = src.mHandle;
		mpBlock = src.mpBlock;
		src.mHandle = nullptr;
		src.mpBlock = nullptr;
	}

	~ATGDIObjectW32();

	vdnothrow ATGDIObjectW32& operator=(const ATGDIObjectW32&) vdnoexcept;
	vdnothrow ATGDIObjectW32& operator=(ATGDIObjectW32&& src) vdnoexcept;

	T get() const { return mHandle; }
	vdnothrow void reset() vdnoexcept;
	void reset(T h);

private:
	struct RefCountBlock {
		T mHandle;
		VDAtomicInt mRefCount;

		vdnothrow ~RefCountBlock() vdnoexcept;
	};

	T mHandle;
	RefCountBlock *mpBlock;
};

template<typename T>
vdnothrow ATGDIObjectW32<T>::RefCountBlock::~RefCountBlock() vdnoexcept {
	VDVERIFY(DeleteObject(mHandle));
}

template<typename T>
ATGDIObjectW32<T>::~ATGDIObjectW32() {
	reset();
}

template<typename T>
vdnothrow ATGDIObjectW32<T>::ATGDIObjectW32(const ATGDIObjectW32& src) vdnoexcept
	: mHandle(src.mHandle)
	, mpBlock(src.mpBlock)
{
	if (mpBlock)
		++mpBlock->mRefCount;
}

template<typename T>
vdnothrow ATGDIObjectW32<T>& ATGDIObjectW32<T>::operator=(const ATGDIObjectW32& src) vdnoexcept {
	if (src.mpBlock != mpBlock) {
		if (mpBlock && !--mpBlock->mRefCount)
			delete mpBlock;

		mpBlock = src.mpBlock;
		mHandle = src.mHandle;

		if (mpBlock)
			++mpBlock->mRefCount;
	}

	return *this;
}

template<typename T>
vdnothrow ATGDIObjectW32<T>& ATGDIObjectW32<T>::operator=(ATGDIObjectW32&& src) vdnoexcept {
	if (&src != this) {
		reset();

		mHandle = src.mHandle;
		mpBlock = src.mpBlock;
		src.mHandle = nullptr;
		src.mpBlock = nullptr;
	}

	return *this;
}

template<typename T>
vdnothrow void ATGDIObjectW32<T>::reset() vdnoexcept {
	if (mpBlock && !--mpBlock->mRefCount)
		delete mpBlock;

	mHandle = nullptr;
	mpBlock = nullptr;
}

template<typename T>
void ATGDIObjectW32<T>::reset(T h) {
	RefCountBlock *b = h ? new RefCountBlock { h, 1 } : nullptr;
	reset();

	mHandle = h;
	mpBlock = b;
}

typedef ATGDIObjectW32<HFONT> ATGDIFontHandleW32;

///////////////////////////////////////////////////////////////////////////

class ATGDICachedImageW32 {
	ATGDICachedImageW32(const ATGDICachedImageW32&) = delete;
	ATGDICachedImageW32& operator=(const ATGDICachedImageW32&) = delete;

public:
	ATGDICachedImageW32() = default;
	~ATGDICachedImageW32();

	HDC GetHDC() const { return mhdc; }
	uint32 GetWidth() const { return mWidth; }
	uint32 GetHeight() const { return mHeight; }

	void Clear();
	void Load(HDC targetDC, const VDPixmap& px);

private:
	bool InitBitmap(HDC targetDC, uint32 w, uint32 h);

	HDC mhdc = nullptr;
	HBITMAP mhbm = nullptr;
	HGDIOBJ mhbmOld = nullptr;

	uint32 mWidth = 0;
	uint32 mHeight = 0;
};

ATGDICachedImageW32::~ATGDICachedImageW32() {
	Clear();
}

void ATGDICachedImageW32::Clear() {
	if (mhdc) {
		if (mhbmOld) {
			SelectObject(mhdc, mhbmOld);
			mhbmOld = nullptr;
		}

		DeleteDC(mhdc);
		mhdc = nullptr;
	}

	if (mhbm) {
		DeleteObject(mhbm);
		mhbm = nullptr;
	}
}

void ATGDICachedImageW32::Load(HDC targetDC, const VDPixmap& px) {
	if (mhdc && (mWidth != (uint32)px.w || mHeight != (uint32)px.h))
		Clear();

	if (!mhdc && !InitBitmap(targetDC, px.w, px.h))
		return;

	BITMAPINFO bi = {
		{
			sizeof(BITMAPINFOHEADER),
			(LONG)px.w,
			(LONG)px.h,
			1,
			32,
			0,
			(DWORD)(px.w*px.h*4)
		}
	};

	if (px.format != nsVDPixmap::kPixFormat_XRGB8888 || px.pitch != -(sint32)(px.w * 4)) {
		VDPixmapLayout layout;
		VDMakeBitmapCompatiblePixmapLayout(layout, px.w, px.h, nsVDPixmap::kPixFormat_XRGB8888, 0);

		VDPixmapBuffer px2;
		px2.init(layout);

		VDPixmapBlt(px2, px);

		SetDIBitsToDevice(mhdc, 0, 0, px.w, px.h, 0, 0, 0, px.h, px2.base(), &bi, DIB_RGB_COLORS);
	} else {
		SetDIBitsToDevice(mhdc, 0, 0, px.w, px.h, 0, 0, 0, px.h, (char *)px.data + px.pitch * (px.h - 1), &bi, DIB_RGB_COLORS);
	}
}

bool ATGDICachedImageW32::InitBitmap(HDC targetDC, uint32 w, uint32 h) {
	mhdc = CreateCompatibleDC(targetDC);

	if (mhdc) {
		mhbm = CreateCompatibleBitmap(targetDC, w, h);
		if (mhbm) {
			mhbmOld = SelectObject(mhdc, mhbm);
			if (mhbmOld)
				return true;
		}
	}

	Clear();
	return false;
}

///////////////////////////////////////////////////////////////////////////

void ATTraceLoadDefaults(ATTraceSettings& settings) {
	VDRegistryAppKey key("Debugger", false);

	settings = {};
	settings.mbTraceVideo = key.getBool("Trace: Enable video", true);
	settings.mTraceVideoDivisor = key.getInt("Trace: Video divisor", 1);
	settings.mbTraceCpuInsns = key.getBool("Trace: Enable CPU insns", true);
	settings.mbTraceBasic = key.getBool("Trace: Enable BASIC", false);
	settings.mbAutoLimitTraceMemory = key.getBool("Trace: Auto-limit trace memory", true);
	settings.mVideoFrameSize = std::clamp<uint32>(key.getInt("Trace: Video frame size", 128), 16, 512);
}

void ATTraceSaveDefaults(const ATTraceSettings& settings) {
	VDRegistryAppKey key("Debugger");

	key.setBool("Trace: Enable video", settings.mbTraceVideo);
	key.setInt("Trace: Video divisor", settings.mTraceVideoDivisor);
	key.setBool("Trace: Enable CPU insns", settings.mbTraceCpuInsns);
	key.setBool("Trace: Enable BASIC", settings.mbTraceBasic);
	key.setBool("Trace: Auto-limit trace memory", settings.mbAutoLimitTraceMemory);
	key.setInt("Trace: Video frame size", settings.mVideoFrameSize);
}

///////////////////////////////////////////////////////////////////////////

class ATUITraceTooltipControl final : public ATUINativeControl<> {
public:
	bool IsScreenPtInAnchorArea(const vdpoint32& spt) const;

	void ShowWithText(const vdrect32& r, const wchar_t *s);

private:
	bool OnMessage(ATUINativeControlMessage& msg) override;

	void OnPaint();

	void Relayout();

	VDStringW mText;
	HFONT mhFont = nullptr;
	uint32 mFontDpi = 0;
	vdrect32 mAnchorArea { 0, 0, 0, 0 };
	RECT mTextArea {};
};

bool ATUITraceTooltipControl::IsScreenPtInAnchorArea(const vdpoint32& spt) const {
	return mAnchorArea.contains(spt);
}

void ATUITraceTooltipControl::ShowWithText(const vdrect32& r, const wchar_t *s) {
	mText = s;
	mAnchorArea = r;

	Relayout();
	BringToFrontTopmost();
	Show();
}

bool ATUITraceTooltipControl::OnMessage(ATUINativeControlMessage& msg) {
	switch(msg.mMsg) {
		case WM_PAINT:
			OnPaint();
			return 0;

		default:
			break;
	}

	return ATUINativeControl::OnMessage(msg);
}

void ATUITraceTooltipControl::OnPaint() {
	PAINTSTRUCT ps {};
	HDC hdc = BeginPaint(mhwnd, &ps);

	if (hdc) {
		const int savedDC = SaveDC(hdc);
		if (savedDC) {
			SelectObject(hdc, GetStockObject(DC_PEN));
			SelectObject(hdc, GetStockObject(DC_BRUSH));

			SetDCPenColor(hdc, RGB(0, 0, 0));
			SetDCBrushColor(hdc, RGB(240, 240, 240));

			const vdrect32& r = GetClientArea();

			Rectangle(hdc, r.left, r.top, r.right, r.bottom);

			if (mhFont && !mText.empty()) {
				SelectObject(hdc, mhFont);
				DrawText(hdc, mText.c_str(), (int)mText.size(), &mTextArea, DT_NOPREFIX | DT_LEFT);
			}

			RestoreDC(hdc, savedDC);
		}

		EndPaint(mhwnd, &ps);
	}
}

void ATUITraceTooltipControl::Relayout() {
	const POINT center {
		(mAnchorArea.left + mAnchorArea.right) >> 1,
		(mAnchorArea.top + mAnchorArea.bottom) >> 1
	};

	HMONITOR hmon = MonitorFromPoint(center, MONITOR_DEFAULTTONEAREST);
	uint32 dpi = ATUIGetMonitorDpiW32(hmon);

	if (mFontDpi != dpi) {
		mFontDpi = dpi;

		if (mhFont) {
			DeleteObject(mhFont);
			mhFont = nullptr;
		}
	}

	if (!mhFont) {
		mhFont = CreateFontW(
			-MulDiv(8, mFontDpi, 72),
			0, 0, 0, 0,
			FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			L"MS Shell Dlg");

		if (!mhFont)
			return;
	}

	sint32 w = 0;
	sint32 h = 0;
	bool succeeded = false;

	HDC hdc = GetDC(mhwnd);
	if (hdc) {
		const int savedDC = SaveDC(hdc);
		if (savedDC) {
			SelectObject(hdc, mhFont);

			RECT r {};
			if (DrawTextW(hdc, mText.c_str(), (int)mText.size(), &r, DT_CALCRECT | DT_NOPREFIX)) {
				succeeded = true;
				w = r.right - r.left;
				h = r.bottom - r.top;
			}

			RestoreDC(hdc, savedDC);
		}

		ReleaseDC(mhwnd, hdc);
	}

	if (succeeded) {
		int edgeSize = ATUIGetDpiScaledSystemMetricForWindowW32(mhwnd, SM_CXEDGE);

		w += edgeSize * 4;
		h += edgeSize * 4;

		sint32 x = mAnchorArea.right + edgeSize * 10;
		sint32 y = mAnchorArea.bottom + edgeSize * 10;

		mTextArea.left = edgeSize * 2;
		mTextArea.top = edgeSize * 2;
		mTextArea.right = mTextArea.left + w;
		mTextArea.bottom = mTextArea.top + h;

		SetArea(vdrect32(x, y, x + w, y + h));
	}
}

///////////////////////////////////////////////////////////////////////////

class ATUIDialogTraceSettings final : public VDDialogFrameW32 {
public:
	ATUIDialogTraceSettings(ATTraceSettings& settings);

	bool OnLoaded() override;
	void OnDataExchange(bool write) override;

private:
	void UpdateEnables();
	
	ATTraceSettings& mSettings;
	VDUIProxyButtonControl mVideoButton;
	VDUIProxyComboBoxControl mVideoRateCombo;
	VDUIProxyComboBoxControl mVideoSizeCombo;
};

ATUIDialogTraceSettings::ATUIDialogTraceSettings(ATTraceSettings& settings)
	: VDDialogFrameW32(IDD_TRACE_SETTINGS)
	, mSettings(settings)
{
	mVideoButton.SetOnClicked([this] { UpdateEnables(); });
}

bool ATUIDialogTraceSettings::OnLoaded() {
	AddProxy(&mVideoButton, IDC_TRACE_VIDEO);
	AddProxy(&mVideoRateCombo, IDC_VIDEO_RATE);
	mVideoRateCombo.AddItem(L"All frames");
	mVideoRateCombo.AddItem(L"Every two frames");
	mVideoRateCombo.AddItem(L"Every three frames");

	AddProxy(&mVideoSizeCombo, IDC_VIDEO_SIZE);
	mVideoSizeCombo.AddItem(L"Small (default)");
	mVideoSizeCombo.AddItem(L"Medium");
	mVideoSizeCombo.AddItem(L"Large");

	return VDDialogFrameW32::OnLoaded();
}

void ATUIDialogTraceSettings::OnDataExchange(bool write) {
	ExchangeControlValueBoolCheckbox(write, IDC_TRACE_VIDEO, mSettings.mbTraceVideo);
	ExchangeControlValueBoolCheckbox(write, IDC_TRACE_HISTORY, mSettings.mbTraceCpuInsns);
	ExchangeControlValueBoolCheckbox(write, IDC_TRACE_BASIC, mSettings.mbTraceBasic);
	ExchangeControlValueBoolCheckbox(write, IDC_LIMIT_MEMORY, mSettings.mbAutoLimitTraceMemory);

	if (write) {
		mSettings.mTraceVideoDivisor = mVideoRateCombo.GetSelection() + 1;

		switch(mVideoSizeCombo.GetSelection()) {
			case 0:
			default:
				mSettings.mVideoFrameSize = 128;
				break;

			case 1:
				mSettings.mVideoFrameSize = 192;
				break;

			case 2:
				mSettings.mVideoFrameSize = 256;
				break;
		}
	} else {
		switch(mSettings.mTraceVideoDivisor) {
			case 0:
			case 1:
				mVideoRateCombo.SetSelection(0);
				break;

			case 2:
				mVideoRateCombo.SetSelection(1);
				break;

			case 3:
			default:
				mVideoRateCombo.SetSelection(2);
				break;
		}

		if (mSettings.mVideoFrameSize >= 256)
			mVideoSizeCombo.SetSelection(2);
		else if (mSettings.mVideoFrameSize >= 192)
			mVideoSizeCombo.SetSelection(1);
		else
			mVideoSizeCombo.SetSelection(0);

		UpdateEnables();
	}
}

void ATUIDialogTraceSettings::UpdateEnables() {
	mVideoRateCombo.SetEnabled(mVideoButton.GetChecked());
}

///////////////////////////////////////////////////////////////////////////

struct ATUITraceViewerGroup;

struct ATUITraceViewerChannel {
	enum Type {
		kType_Default,
		kType_Video,
		kType_Tape,
		kType_Log,
	};

	sint32 mPosY = 0;
	sint32 mBaseHeight = 0;
	sint32 mHeight = 0;
	Type mType {};
	ATUITraceViewerGroup *mpGroup = nullptr;
	vdrefptr<IATTraceChannel> mpChannel;

	double mFrameTime0 = 0;
	double mAvgSecondsPerFrame = 0;

	static constexpr uint32 kNumCachedImages = 32;
	ATGDICachedImageW32 mCachedBitmaps[kNumCachedImages];
	sint32 mCachedImageIndices[kNumCachedImages];
};

struct ATUITraceViewerGroup {
	VDStringW mName;
	sint32 mPosY = 0;
	sint32 mBaseHeight = 0;
	sint32 mHeight = 0;
	float mVariableHeightScale = 1.0f;
	bool mbVariableHeight = false;

	vdrefptr<ATTraceGroup> mpGroup;
	vdfastvector<ATUITraceViewerChannel *> mChannels;
};

class IATUITraceViewer {
public:
	virtual void ScrollDeltaPixels(sint32 dx, sint32 dy) = 0;
	virtual void ZoomDeltaSteps(double centerTime, sint32 steps) = 0;
	virtual void SetFocusTime(double t) = 0;
	virtual void SetSelection(double startTime, double endTime) = 0;
	virtual void SetSelectionMode(bool selMode) = 0;
	virtual void SetGroupHeightScale(int groupIndex, float scale) = 0;
};

struct ATUITraceViewerContext {
	int mDpi = 0;
	bool mbSelectionMode = false;

	ATGDIFontHandleW32 mChannelFont;
	TEXTMETRICW mChannelFontMetrics;
	ATGDIFontHandleW32 mTimestampFont;
	TEXTMETRICW mTimestampFontMetrics;
	ATGDIFontHandleW32 mEventFont;
	TEXTMETRICW mEventFontMetrics;

	IATUITraceViewer *mpParent;
	vdfastvector<ATUITraceViewerGroup *> mGroups;

	vdrefptr<ATTraceChannelCPUHistory> mpCPUHistoryChannel;
};

///////////////////////////////////////////////////////////////////////////

class ATUITraceViewerCPUHistoryView final : public VDDialogFrameW32, private IATUIHistoryModel {
public:
	ATUITraceViewerCPUHistoryView(ATUITraceViewerContext& context);
	~ATUITraceViewerCPUHistoryView();

	void SetCPUTraceChannel(ATTraceChannelCPUHistory *channel, const ATCPUTimestampDecoder& timestampDecoder);
	void SetCenterTime(double t);

	void SetTimestampMode(ATUIHistoryTimestampMode mode);
	void SetTimestampOrigin(double tick);

	void SetOnTimestampModeChanged(vdfunction<void(ATUIHistoryTimestampMode)> fn);
	void SetOnTimestampOriginChanged(vdfunction<void(double)> fn);

private:
	double DecodeTapeSample(uint32 cycle) override;
	double DecodeTapeSeconds(uint32 cycle) override;
	uint32 ConvertRawTimestamp(uint32 rawCycle) override;
	float ConvertRawTimestampDeltaF(sint32 rawCycleDelta) override;
	ATCPUBeamPosition DecodeBeamPosition(uint32 cycle) override;
	bool IsInterruptPositionVBI(uint32 cycle) override;
	bool UpdatePreviewNode(ATCPUHistoryEntry&) override;
	uint32 ReadInsns(const ATCPUHistoryEntry **ppInsns, uint32 startIndex, uint32 n) override;
	void OnEsc() override;
	void OnInsnSelected(uint32 index) override;
	void JumpToInsn(uint32 pc) override;
	void JumpToSource(uint32 pc) override;

private:
	bool OnLoaded() override;

	void OnSetFocus() override;
	void OnFontsChanged();

	void OnTimestampModeChanged(ATUIHistoryTimestampMode mode);
	void OnTimestampOriginChanged(uint32 cyc, uint32 ucyc);

	void RebuildInsnView();
	void PopulateFromNewChannel();

	ATUITraceViewerContext& mContext;
	vdrefptr<ATTraceChannelCPUHistory> mpChannel;
	ATTraceChannelCPUHistoryCursor mHistoryCursor;
	double mCenterTime = -1;
	double mViewStartTime = 0;
	double mViewEndTime = -1;
	uint32 mViewStartCycle = 0;
	uint32 mNotifyRecursionCounter = 0;

	vdrefptr<IATUIHistoryView> mpHistoryView;
	vdfunction<void()> mpFontCallback;

	vdfunction<void(ATUIHistoryTimestampMode)> mpOnTimestampModeChanged;
	vdfunction<void(double)> mpOnTimestampOriginChanged;

	ATCPUTimestampDecoder mTimestampDecoder;
};

ATUITraceViewerCPUHistoryView::ATUITraceViewerCPUHistoryView(ATUITraceViewerContext& context)
	: VDDialogFrameW32(IDD_TRACEVIEWER_CPUHISTORY)
	, mContext(context)
{
	mpFontCallback = [this]() { OnFontsChanged(); };
	ATConsoleAddFontNotification(&mpFontCallback);
}

ATUITraceViewerCPUHistoryView::~ATUITraceViewerCPUHistoryView() {
	ATConsoleRemoveFontNotification(&mpFontCallback);
}

void ATUITraceViewerCPUHistoryView::SetCPUTraceChannel(ATTraceChannelCPUHistory *channel, const ATCPUTimestampDecoder& timestampDecoder) {
	if (mpChannel != channel) {
		mpChannel = channel;
		mTimestampDecoder = timestampDecoder;

		PopulateFromNewChannel();
	}
}

void ATUITraceViewerCPUHistoryView::SetCenterTime(double t) {
	if (mNotifyRecursionCounter)
		return;
	
	++mNotifyRecursionCounter;

	// check if time is within current view
	if (t < mViewStartTime || t > mViewEndTime) {
		// recenter view
		if (mCenterTime != t) {
			mCenterTime = t;

			RebuildInsnView();
		}
	}

	if (mpChannel) {
		// look up and select insn
		mpHistoryView->SelectInsn(mpChannel->FindEvent(mHistoryCursor, t));
	}

	--mNotifyRecursionCounter;
}

void ATUITraceViewerCPUHistoryView::SetTimestampMode(ATUIHistoryTimestampMode mode) {
	if (mpHistoryView)
		mpHistoryView->SetTimestampMode(mode);
}

void ATUITraceViewerCPUHistoryView::SetTimestampOrigin(double t) {
	if (mpHistoryView && mpChannel) {
		const ATCPUHistoryEntry *he = nullptr;
		if (mpChannel->ReadHistoryEvents(mHistoryCursor, &he, 0, 1) && he) {
			double foundTime = mpChannel->GetEventTime(mHistoryCursor, 0);
			const sint32 dcyc = (sint32)VDRoundToInt64((t - foundTime) / mpChannel->GetSecondsPerTick());

			mpHistoryView->SetTimestampOrigin(he->mCycle + dcyc, he->mUnhaltedCycle + dcyc);
		}
	}
}

void ATUITraceViewerCPUHistoryView::SetOnTimestampModeChanged(vdfunction<void(ATUIHistoryTimestampMode)> fn) {
	mpOnTimestampModeChanged = std::move(fn);
}

void ATUITraceViewerCPUHistoryView::SetOnTimestampOriginChanged(vdfunction<void(double)> fn) {
	mpOnTimestampOriginChanged = std::move(fn);
}

double ATUITraceViewerCPUHistoryView::DecodeTapeSample(uint32 cycle) {
	return 0;
}

double ATUITraceViewerCPUHistoryView::DecodeTapeSeconds(uint32 cycle) {
	return 0;
}

uint32 ATUITraceViewerCPUHistoryView::ConvertRawTimestamp(uint32 rawCycle) {
	return rawCycle;
}

float ATUITraceViewerCPUHistoryView::ConvertRawTimestampDeltaF(sint32 rawCycleDelta) {
	return mpChannel ? (float)((double)rawCycleDelta * mpChannel->GetSecondsPerTick()) : 0;
}

ATCPUBeamPosition ATUITraceViewerCPUHistoryView::DecodeBeamPosition(uint32 cycle) {
	return mTimestampDecoder.GetBeamPosition(cycle);
}

bool ATUITraceViewerCPUHistoryView::IsInterruptPositionVBI(uint32 cycle) {
	return mTimestampDecoder.IsInterruptPositionVBI(cycle);
}

bool ATUITraceViewerCPUHistoryView::UpdatePreviewNode(ATCPUHistoryEntry& he) {
	return false;
}

uint32 ATUITraceViewerCPUHistoryView::ReadInsns(const ATCPUHistoryEntry **ppInsns, uint32 startIndex, uint32 n) {
	return mpChannel->ReadHistoryEvents(mHistoryCursor, ppInsns, startIndex, n);
}

void ATUITraceViewerCPUHistoryView::OnEsc() {
}

void ATUITraceViewerCPUHistoryView::OnInsnSelected(uint32 index) {
	if (mNotifyRecursionCounter)
		return;

	if (mpChannel) {
		++mNotifyRecursionCounter;
		mContext.mpParent->SetFocusTime(mpChannel->GetEventTime(mHistoryCursor, index));
		--mNotifyRecursionCounter;
	}
}

void ATUITraceViewerCPUHistoryView::JumpToInsn(uint32 pc) {
}

void ATUITraceViewerCPUHistoryView::JumpToSource(uint32 pc) {
}

bool ATUITraceViewerCPUHistoryView::OnLoaded() {
	ATUICreateHistoryView((VDGUIHandle)mhdlg, ~mpHistoryView);

	mpHistoryView->SetHistoryModel(this);

	mpHistoryView->SetOnTimestampModeChanged(
		std::bind_front(&ATUITraceViewerCPUHistoryView::OnTimestampModeChanged, this)
	);

	mpHistoryView->SetOnTimestampOriginChanged(
		std::bind_front(&ATUITraceViewerCPUHistoryView::OnTimestampOriginChanged, this)
	);

	OnFontsChanged();

	if (mpHistoryView)
		SetFocus(mpHistoryView->AsNativeWindow()->GetHandleW32());

	const vdsize32 sz = GetClientArea().size();
	mResizer.Add(mpHistoryView->AsNativeWindow()->GetHandleW32(), 0, 0, sz.w, sz.h, mResizer.kMC | mResizer.kAvoidFlicker);

	PopulateFromNewChannel();

	return true;
}

void ATUITraceViewerCPUHistoryView::OnSetFocus() {
	if (mpHistoryView)
		mpHistoryView->AsNativeWindow()->Focus();
}

void ATUITraceViewerCPUHistoryView::OnFontsChanged() {
	if (mpHistoryView)
		mpHistoryView->SetFonts(ATConsoleGetPropFontW32(), ATConsoleGetPropFontLineHeightW32(), ATGetConsoleFontW32(), ATGetConsoleFontLineHeightW32());
}

void ATUITraceViewerCPUHistoryView::OnTimestampModeChanged(ATUIHistoryTimestampMode mode) {
	if (mpOnTimestampModeChanged)
		mpOnTimestampModeChanged(mode);
}

void ATUITraceViewerCPUHistoryView::OnTimestampOriginChanged(uint32 cyc, uint32 ucyc) {
	if (!mpOnTimestampOriginChanged)
		return;

	const ATCPUHistoryEntry *he = nullptr;

	if (mpChannel->ReadHistoryEvents(mHistoryCursor, &he, 0, 1) && he)
		mpOnTimestampOriginChanged(mViewStartTime + (double)(sint32)(cyc - he->mCycle) * mpChannel->GetSecondsPerTick());
}

void ATUITraceViewerCPUHistoryView::RebuildInsnView() {
	if (!mpHistoryView || !mpChannel)
		return;

	mHistoryCursor = mpChannel->StartHistoryIteration(mCenterTime, -200000);

	uint32 n = mpChannel->ReadHistoryEvents(mHistoryCursor, nullptr, 0, 400000);

	if (n) {
		mViewStartTime = mpChannel->GetEventTime(mHistoryCursor, 0);
		mViewEndTime = mpChannel->GetEventTime(mHistoryCursor, n - 1);
	} else {
		mViewStartTime = 0;
		mViewEndTime = -1;
	}

	const ATCPUHistoryEntry *he = nullptr;
	if (mpChannel->ReadHistoryEvents(mHistoryCursor, &he, 0, 1) && he) {
		mViewStartCycle = he->mCycle;
	}

	mpHistoryView->ClearInsns();
	mpHistoryView->UpdateInsns(0, n);
}

void ATUITraceViewerCPUHistoryView::PopulateFromNewChannel() {
	if (mpHistoryView) {
		mpHistoryView->ClearInsns();

		if (mpChannel && !mpChannel->IsEmpty()) {
			mpHistoryView->SetDisasmMode(mpChannel->GetDisasmMode(), mpChannel->GetSubCycles(), true);
			auto cursor = mpChannel->StartHistoryIteration(0, 0);

			const ATCPUHistoryEntry *he = nullptr;
			if (mpChannel->ReadHistoryEvents(cursor, &he, 0, 1)) {
				mpHistoryView->SetTimestampBase(he->mCycle, he->mUnhaltedCycle);
			}
		}
	}

	mViewStartTime = 0;
	mViewEndTime = -1;
	mCenterTime = -1;
}

///////////////////////////////////////////////////////////////////////////

class ATUITraceViewerCPUProfileView final : public VDDialogFrameW32 {
public:
	ATUITraceViewerCPUProfileView(ATUITraceViewerContext& context);
	~ATUITraceViewerCPUProfileView();

	void SetCPUTraceChannel(ATTraceChannelCPUHistory *channel, const ATCPUTimestampDecoder& timestampDecoder);

	void ClearSelectedRange();
	void SetSelectedRange(double startTime, double endTime);

private:
	bool OnLoaded() override;
	void OnSize() override;

	void OnFontsChanged();
	void RemakeView();

	void OnToolbarClicked(uint32 id);
	void UpdateProfilingModeImage();

	enum : uint32 {
		kToolbarId_Refresh = 1000,
		kToolbarId_Link,
		kToolbarId_Mode,
		kToolbarId_Options,
		kToolbarId_Range,
	};

	vdrefptr<ATTraceChannelCPUHistory> mpChannel;
	ATTraceChannelCPUHistoryCursor mHistoryCursor;
	double mViewStartTime = 0;
	double mViewEndTime = -1;
	double mSelectionStartTime = 0;
	double mSelectionEndTime = -1;
	bool mbGlobalAddressesEnabled = false;

	ATProfileMode mProfileMode = kATProfileMode_Insns;

	vdrefptr<IATUIProfileView> mpProfileView;
	ATProfileCounterMode mProfileCounterModes[2] {};
	ATProfileSession mSession;
	vdrefptr<ATProfileMergedFrame> mpMergedFrame;

	ATCPUTimestampDecoder mTimestampDecoder {};
	VDUIProxyToolbarControl mToolbar;
};

ATUITraceViewerCPUProfileView::ATUITraceViewerCPUProfileView(ATUITraceViewerContext&)
	: VDDialogFrameW32(IDD_TRACEVIEWER_CPUPROFILE)
{
	mToolbar.SetOnClicked([this](uint32 id) { OnToolbarClicked(id); });
	mToolbar.SetDarkModeEnabled(true);
}

ATUITraceViewerCPUProfileView::~ATUITraceViewerCPUProfileView() {
}

void ATUITraceViewerCPUProfileView::SetCPUTraceChannel(ATTraceChannelCPUHistory *channel, const ATCPUTimestampDecoder& timestampDecoder) {
	if (mpChannel != channel) {
		mpChannel = channel;

		mViewStartTime = 0;
		mViewEndTime = -1;
		mTimestampDecoder = timestampDecoder;

		RemakeView();
	}
}

void ATUITraceViewerCPUProfileView::ClearSelectedRange() {
	mSelectionStartTime = 0;
	mSelectionEndTime = -1;
}

void ATUITraceViewerCPUProfileView::SetSelectedRange(double startTime, double endTime) {
	mSelectionStartTime = startTime;
	mSelectionEndTime = endTime;
}

bool ATUITraceViewerCPUProfileView::OnLoaded() {
	AddProxy(&mToolbar, IDC_TOOLBAR);

	mToolbar.Clear();
	mToolbar.AddButton(kToolbarId_Refresh, 8, nullptr);
	mToolbar.AddButton(kToolbarId_Link, 9, nullptr);
	mToolbar.AddSeparator();
	mToolbar.AddDropdownButton(kToolbarId_Mode, 2, nullptr);
	mToolbar.AddDropdownButton(kToolbarId_Options, -1, L"Options");
	mToolbar.AddButton(kToolbarId_Range, -1, L"");

	mToolbar.SetItemVisible(kToolbarId_Range, false);

	const sint32 iconSize = GetDpiScaledMetric(SM_CXSMICON);

	VDPixmapBuffer pximg;
	if (ATLoadImageResource(IDB_TOOLBAR_PROFILER2, pximg)) {
		const uint32 n = pximg.w / pximg.h;

		// Some icons need recolorization for dark mode.
		RecolorToolstripImage(pximg, { 0, 2, 3, 4, 5, 6, 7, 9 });

		mToolbar.InitImageList(n, iconSize, iconSize);
		mToolbar.AddImages(n, pximg);
	}

	UpdateProfilingModeImage();

	ATUICreateProfileView(~mpProfileView);

	mpProfileView->Create(this, 100);
	mpProfileView->AsUINativeWindow()->Focus();

	return true;
}

void ATUITraceViewerCPUProfileView::OnSize() {
	const vdsize32 sz = GetClientArea().size();

	mToolbar.SetPosition(vdpoint32(0, 0));
	mToolbar.AutoSize();
	const vdrect32 toolbarArea = mToolbar.GetArea();

	if (mpProfileView)
		mpProfileView->AsUINativeWindow()->SetArea(vdrect32(0, toolbarArea.bottom, sz.w, sz.h));
}

void ATUITraceViewerCPUProfileView::OnFontsChanged() {
}

void ATUITraceViewerCPUProfileView::RemakeView() {
	if (!mpProfileView || !mpChannel)
		return;

	{
		ATCPUProfileBuilder builder;
		builder.Init(mProfileMode, mProfileCounterModes[0], mProfileCounterModes[1]);
		builder.SetGlobalAddressesEnabled(mbGlobalAddressesEnabled);

		auto cursor = mpChannel->StartHistoryIteration(0, 0);

		uint32 startEventIdx = 0;
		uint32 endEventIdx = mpChannel->GetEventCount();

		if (mSelectionEndTime > mSelectionStartTime) {
			startEventIdx = mpChannel->FindEvent(cursor, mSelectionStartTime);
			endEventIdx = mpChannel->FindEvent(cursor, mSelectionEndTime);

			int digits = 2;
			double deltaTime = mSelectionEndTime - mSelectionStartTime;

			while(digits < 6 && deltaTime < 0.02) {
				++digits;
				deltaTime *= 10.0;
			}

			VDStringW s;
			s.sprintf(L"Range: %.*fs - %.*fs \u00D7", digits, mSelectionStartTime, digits, mSelectionEndTime);
			mToolbar.SetItemText(kToolbarId_Range, s.c_str());
			mToolbar.SetItemVisible(kToolbarId_Range, true);
		} else {
			mToolbar.SetItemVisible(kToolbarId_Range, false);
		}

		const ATCPUHistoryEntry *hents[256];
		uint32 pos = startEventIdx;
		uint32 cycle = 0;
		uint32 unhaltedCycle = 0;

		if (mpChannel->ReadHistoryEvents(cursor, hents, pos, 1)) {
			cycle = hents[0]->mCycle;
			unhaltedCycle = hents[0]->mUnhaltedCycle;
		}

		builder.OpenFrame(cycle, unhaltedCycle, mTimestampDecoder);

		const bool useGlobalAddrs = mpChannel->GetDisasmMode() == kATDebugDisasmMode_6502;

		while(pos < endEventIdx) {
			uint32 n = mpChannel->ReadHistoryEvents(cursor, hents, pos, std::min<uint32>(endEventIdx - pos, (uint32)vdcountof(hents)));
			if (n < 2)
				break;
			
			pos += n - 1;

			builder.Update(mTimestampDecoder, hents, n - 1, useGlobalAddrs);
		}

		if (pos && mpChannel->ReadHistoryEvents(cursor, hents, pos - 1, 1)) {
			cycle = hents[0]->mCycle;
			unhaltedCycle = hents[0]->mUnhaltedCycle;
		}

		builder.CloseFrame(cycle, unhaltedCycle, true);
		builder.Finalize();
		builder.TakeSession(mSession);
	}

	ATProfileMergeFrames(mSession, 0, 1, ~mpMergedFrame);
	mpProfileView->SetData(&mSession, mpMergedFrame, mpMergedFrame);
}

void ATUITraceViewerCPUProfileView::OnToolbarClicked(uint32 id) {
	switch(id) {
		case kToolbarId_Refresh:
			RemakeView();
			break;

		case kToolbarId_Link:
			break;

		case kToolbarId_Mode:
			{
				static constexpr ATProfileMode kProfilerModes[]={
					kATProfileMode_Insns,
					kATProfileMode_Functions,
					kATProfileMode_BasicBlock,
					kATProfileMode_CallGraph,
				};

				static constexpr const wchar_t *kProfilerModeLabels[]={
					L"Instructions",
					L"Functions",
					L"Basic Blocks",
					L"Call Graph",
					nullptr
				};

				const sint32 idx = mToolbar.ShowDropDownMenu(kToolbarId_Mode, kProfilerModeLabels);

				if ((uint32)idx < vdcountof(kProfilerModes)) {
					mProfileMode = kProfilerModes[idx];

					UpdateProfilingModeImage();
					RemakeView();
				}
			}
			break;

		case kToolbarId_Options:
			{
				HMENU hmenu = LoadMenu(NULL, MAKEINTRESOURCE(IDR_PROFILE_OPTIONS_MENU));

				if (!hmenu)
					break;

				const auto counterModeMenuIds = ATUIGetProfilerCounterModeMenuIds();
				uint32 activeMask = 0;

				for(const auto cm : mProfileCounterModes) {
					if (cm) {
						activeMask |= (1 << (cm - 1));
						VDCheckMenuItemByCommandW32(hmenu, counterModeMenuIds[cm - 1], true);
					}
				}

				if (mProfileCounterModes[vdcountof(mProfileCounterModes) - 1]) {
					for(uint32 i=0; i<counterModeMenuIds.size(); ++i) {
						if (!(activeMask & (1 << i)))
							VDEnableMenuItemByCommandW32(hmenu, counterModeMenuIds[i], false);
					}
				}

				VDEnableMenuItemByCommandW32(hmenu, ID_FRAMETRIGGER_NONE, false);
				VDEnableMenuItemByCommandW32(hmenu, ID_FRAMETRIGGER_VBLANK, false);
				VDEnableMenuItemByCommandW32(hmenu, ID_FRAMETRIGGER_PCADDRESS, false);
				VDCheckMenuItemByCommandW32(hmenu, ID_MENU_ENABLEGLOBALADDRESSES, mbGlobalAddressesEnabled);

				const uint32 selectedId = mToolbar.ShowDropDownMenu(kToolbarId_Options, GetSubMenu(hmenu, 0));

				DestroyMenu(hmenu);

				for(uint32 i = 0, n = (uint32)counterModeMenuIds.size(); i < n; ++i) {
					if (selectedId == counterModeMenuIds[i]) {
						const ATProfileCounterMode selectedMode = (ATProfileCounterMode)(i + 1);

						// check if we already have this mode
						auto it = std::find(std::begin(mProfileCounterModes), std::end(mProfileCounterModes), selectedMode);
						if (it == std::end(mProfileCounterModes)) {
							// we don't -- add it
							it = std::find(std::begin(mProfileCounterModes), std::end(mProfileCounterModes), kATProfileCounterMode_None);
							if (it != std::end(mProfileCounterModes))
								*it = selectedMode;
						} else {
							// we do -- remove it
							*it = kATProfileCounterMode_None;
							std::rotate(it, it+1, std::end(mProfileCounterModes));
						}
						break;
					}
				}

				if (selectedId == ID_MENU_ENABLEGLOBALADDRESSES) {
					mbGlobalAddressesEnabled = !mbGlobalAddressesEnabled;
					RemakeView();
				}
			}
			break;

		case kToolbarId_Range:
			ClearSelectedRange();
			RemakeView();
			break;
	}
}

void ATUITraceViewerCPUProfileView::UpdateProfilingModeImage() {
	switch(mProfileMode) {
		case kATProfileMode_Insns:
		default:
			mToolbar.SetItemImage(kToolbarId_Mode, 2);
			break;

		case kATProfileMode_Functions:
			mToolbar.SetItemImage(kToolbarId_Mode, 3);
			break;

		case kATProfileMode_CallGraph:
			mToolbar.SetItemImage(kToolbarId_Mode, 5);
			break;

		case kATProfileMode_BasicBlock:
			mToolbar.SetItemImage(kToolbarId_Mode, 6);
			break;

		case kATProfileMode_BasicLines:
			mToolbar.SetItemImage(kToolbarId_Mode, 4);
			break;
	}
}

///////////////////////////////////////////////////////////////////////////

class ATUITraceViewerLogView final : public VDDialogFrameW32 {
public:
	enum class TimestampMode : uint8 {
		None,
		BeamPosition,
		Cycle,
		Microseconds
	};

	ATUITraceViewerLogView(ATUITraceViewerContext& context);
	~ATUITraceViewerLogView();

	void SetOnEntryActivated(vdfunction<void(double)> fn) {
		mpOnEntryActivated = std::move(fn);
	}

	void SetOnTimestampOriginChanged(vdfunction<void(double)> fn) {
		mpOnTimestampOriginChanged = std::move(fn);
	}

	void SetOnTimestampModeChanged(vdfunction<void(TimestampMode)> fn) {
		mpOnTimestampModeChanged = std::move(fn);
	}

	void SetLogTraceChannel(ATTraceChannelTickBased *channel, const ATCPUTimestampDecoder& timestampDecoder);

	void SetTimestampMode(TimestampMode timestampMode, bool notify);
	void ResetTimestampOrigin(bool notify);
	void SetTimestampOriginTime(double t, bool notify);
	void SetTimestampOriginTick(uint64 tick, bool notify);
	void SetTimestampOriginFromEntry(int idx, bool notify);

	void SelectNearestEntryToTime(double t);

private:
	bool OnLoaded() override;
	void OnDestroy() override;
	void OnSize() override;

	void OnFontsChanged();
	void OnItemContextMenu(const vdpoint32& cpt, int idx);

	void CopyItems(vdspan<const int> indices) const;
	void RemakeView(bool preserveScrollAndSel);

	vdrefptr<ATTraceChannelTickBased> mpChannel;

	int mInhibitSelection = 0;
	TimestampMode mTimestampMode = TimestampMode::None;
	uint64 mTimestampBase = 0;
	uint64 mTimestampOrigin = 0;
	double mSecondsPerTick = 1;
	double mTicksPerSecond = 1;

	ATCPUTimestampDecoder mTimestampDecoder {};
	vdfastvector<double> mEntryTimes;

	vdfunction<void(double)> mpOnEntryActivated;
	vdfunction<void(double)> mpOnTimestampOriginChanged;
	vdfunction<void(TimestampMode)> mpOnTimestampModeChanged;

	VDUIProxyListBoxControl mListBoxView;

	vdfunction<void()> mpFontCallback;
};

ATUITraceViewerLogView::ATUITraceViewerLogView(ATUITraceViewerContext&)
	: VDDialogFrameW32(IDD_TRACEVIEWER_LOG)
{
	mListBoxView.SetOnItemDoubleClicked(
		[this](int index) {
			if ((size_t)index < mEntryTimes.size()) {
				if (mpOnEntryActivated) {
					++mInhibitSelection;
					mpOnEntryActivated(mEntryTimes[index]);
					--mInhibitSelection;
				}
			}
		}
	);

	mListBoxView.SetOnContextMenu(std::bind_front(&ATUITraceViewerLogView::OnItemContextMenu, this));
}

ATUITraceViewerLogView::~ATUITraceViewerLogView() {
}

void ATUITraceViewerLogView::SetLogTraceChannel(ATTraceChannelTickBased *channel, const ATCPUTimestampDecoder& timestampDecoder) {
	if (mpChannel != channel) {
		mpChannel = channel;
		mTimestampDecoder = timestampDecoder;

		if (channel) {
			mTimestampBase = channel->GetBaseTick();
			mTimestampOrigin = mTimestampBase;
			mSecondsPerTick = channel->GetSecondsPerTick();
			mTicksPerSecond = 1.0 / mSecondsPerTick;
		}

		RemakeView(false);
	}
}

void ATUITraceViewerLogView::SetTimestampMode(TimestampMode timestampMode, bool notify) {
	if (mTimestampMode != timestampMode) {
		mTimestampMode = timestampMode;

		if (notify && mpOnTimestampModeChanged)
			mpOnTimestampModeChanged(timestampMode);

		RemakeView(true);
	}
}

void ATUITraceViewerLogView::ResetTimestampOrigin(bool notify) {
	SetTimestampOriginTick(mTimestampBase, notify);
}

void ATUITraceViewerLogView::SetTimestampOriginTime(double t, bool notify) {
	if (!mpChannel)
		return;

	SetTimestampOriginTick(
		mTimestampBase + VDRoundToInt64(t * mTicksPerSecond),
		notify
	);
}

void ATUITraceViewerLogView::SetTimestampOriginTick(uint64 t, bool notify) {
	if (t < mTimestampBase)
		t = mTimestampBase;

	if (mTimestampOrigin != t) {
		mTimestampOrigin = t;

		if (notify && mpOnTimestampOriginChanged) {
			mpOnTimestampOriginChanged((double)(sint64)(t - mTimestampBase) * mSecondsPerTick);
		}

		RemakeView(true);
	}
}

void ATUITraceViewerLogView::SetTimestampOriginFromEntry(int idx, bool notify) {
	if (idx >= 0 && (size_t)idx < mEntryTimes.size()) {
		const double t = mEntryTimes[idx];

		SetTimestampOriginTick(mTimestampBase + VDRoundToInt64(t / mpChannel->GetSecondsPerTick()), notify);
	}
}

void ATUITraceViewerLogView::SelectNearestEntryToTime(double t) {
	if (mInhibitSelection || !IsValid())
		return;

	auto it = std::lower_bound(mEntryTimes.begin(), mEntryTimes.end(), t);

	if (it != mEntryTimes.begin()) {
		if (it == mEntryTimes.end() || t - *std::prev(it) <= *it - t)
			--it;
	}

	if (it != mEntryTimes.end()) {
		const int index = (int)(it - mEntryTimes.begin());

		mListBoxView.SetSelection(index);
		mListBoxView.EnsureItemVisible(index);
	}
}

bool ATUITraceViewerLogView::OnLoaded() {
	AddProxy(&mListBoxView, IDC_LIST);
	OnFontsChanged();

	RemakeView(false);

	mpFontCallback = [this] { OnFontsChanged(); };
	ATConsoleAddFontNotification(&mpFontCallback);

	return true;
}

void ATUITraceViewerLogView::OnDestroy() {
	VDDialogFrameW32::OnDestroy();
	ATConsoleAddFontNotification(&mpFontCallback);
}

void ATUITraceViewerLogView::OnSize() {
	const vdrect32 r = GetClientArea();

	mListBoxView.SetArea(r);
}

void ATUITraceViewerLogView::OnFontsChanged() {
	SendMessage(mListBoxView.GetWindowHandle(), WM_SETFONT, (WPARAM)ATGetConsoleFontW32(), TRUE);
}

void ATUITraceViewerLogView::OnItemContextMenu(const vdpoint32& cpt, int idx) {
	const bool haveAny = !mEntryTimes.empty();
	const bool haveRelativeTimestamp = mTimestampMode != TimestampMode::None
		&& mTimestampMode != TimestampMode::BeamPosition;

	vdpoint32 spt = TransformClientToScreen(cpt);

	const uint32 cmdId = ActivateCommandPopupMenuReturnId(spt.x, spt.y, IDR_PERFANALYZER_LOG_MENU,
		[&](uint32 id, VDMenuItemInitializer& initer) {
			switch(id) {
				case ID_MENU_COPYSELECTED:
					initer.SetEnabled(idx >= 0);
					break;

				case ID_MENU_COPYALL:
					initer.SetEnabled(haveAny);
					break;

				case ID_MENU_RESETTIMESTAMPORIGIN:
					initer.SetEnabled(mTimestampOrigin != 0
						&& haveRelativeTimestamp);
					break;

				case ID_MENU_SETTIMESTAMPORIGIN:
					initer.SetEnabled(idx >= 0 && haveRelativeTimestamp);
					break;

				case ID_TIMESTAMPMODE_NONE:
					if (mTimestampMode == TimestampMode::None)
						initer.SetRadioChecked();
					break;

				case ID_TIMESTAMPMODE_BEAMPOSITION:
					if (mTimestampMode == TimestampMode::BeamPosition)
						initer.SetRadioChecked();
					break;

				case ID_TIMESTAMPMODE_CYCLE:
					if (mTimestampMode == TimestampMode::Cycle)
						initer.SetRadioChecked();
					break;

				case ID_TIMESTAMPMODE_MICROSECONDS:
					if (mTimestampMode == TimestampMode::Microseconds)
						initer.SetRadioChecked();
					break;
			}
		}
	);

	switch(cmdId) {
		case ID_MENU_COPYSELECTED:
			CopyItems(mListBoxView.GetSelectedIndices());
			break;

		case ID_MENU_COPYALL: {
			const size_t n = mEntryTimes.size();
			vdfastvector<int> indices(n);

			for(size_t i = 0; i < n; ++i)
				indices[i] = (int)i;

			CopyItems(indices);
			break;
		}

		case ID_MENU_RESETTIMESTAMPORIGIN:
			ResetTimestampOrigin(true);
			break;

		case ID_MENU_SETTIMESTAMPORIGIN:
			SetTimestampOriginFromEntry(idx, true);
			break;

		case ID_TIMESTAMPMODE_NONE:
			SetTimestampMode(TimestampMode::None, true);
			break;

		case ID_TIMESTAMPMODE_BEAMPOSITION:
			SetTimestampMode(TimestampMode::BeamPosition, true);
			break;

		case ID_TIMESTAMPMODE_CYCLE:
			SetTimestampMode(TimestampMode::Cycle, true);
			break;

		case ID_TIMESTAMPMODE_MICROSECONDS:
			SetTimestampMode(TimestampMode::Microseconds, true);
			break;
	}
}

void ATUITraceViewerLogView::CopyItems(vdspan<const int> indices) const {
	if (indices.empty())
		return;

	VDStringW s;
	for(int idx : indices) {
		if (!s.empty())
			s += L"\r\n";

		s += mListBoxView.GetItemText(idx);
	}

	if (indices.size() > 1)
		s += L"\r\n";

	ATCopyTextToClipboard(GetWindowHandle(), s.c_str());
}


void ATUITraceViewerLogView::RemakeView(bool preserveScrollAndSel) {
	vdfastvector<int> prevSel;
	int prevTopIndex = -1;

	if (preserveScrollAndSel) {
		prevSel = mListBoxView.GetSelectedIndices();
		prevTopIndex = mListBoxView.GetScrollPos();
	}

	mListBoxView.SetRedraw(false);
	mListBoxView.Clear();
	mEntryTimes.clear();

	if (mpChannel) {
		mpChannel->StartIteration(0, DBL_MAX, 0);

		ATTraceEvent ev;
		VDStringW s;
		const uint64 baseTick = mpChannel->GetBaseTick();
		const double secondsPerTick = mpChannel->GetSecondsPerTick();
		const double ticksPerSecond = 1.0 / secondsPerTick;
		const double timeOrigin = (double)(sint64)(mTimestampOrigin - mTimestampBase) * secondsPerTick;

		while(mpChannel->GetNextEvent(ev)) {
			s.clear();

			switch(mTimestampMode) {
				case TimestampMode::None:
				default:
					break;

				case TimestampMode::BeamPosition: {
					const auto beamPos = mTimestampDecoder.GetBeamPosition((uint32)(baseTick + VDRoundToInt64(ticksPerSecond * ev.mEventStart)));

					s.sprintf(L"(%3d:%3d,%3d) ", beamPos.mFrame, beamPos.mY, beamPos.mX);
					break;
				}

				case TimestampMode::Cycle: {
					uint64 cyc = (uint64)(baseTick + VDRoundToInt64(ticksPerSecond * ev.mEventStart));

					s.sprintf(L"(T%+-9d) ", (int)(cyc - mTimestampOrigin));
					break;
				}

				case TimestampMode::Microseconds: {
					s.sprintf(L"(%9.6f) ", ev.mEventStart - timeOrigin);
					break;
				}
			}

			s += ev.mpName;

			mListBoxView.AddItem(s.c_str());
			mEntryTimes.emplace_back(ev.mEventStart);
		}
	}

	if (preserveScrollAndSel) {
		mListBoxView.SetSelectedIndices(prevSel);
		mListBoxView.SetScrollPos(prevTopIndex);
	}

	mListBoxView.SetRedraw(true);
}

///////////////////////////////////////////////////////////////////////////

class ATUITraceViewerChannelView final : public VDDialogFrameW32 {
public:
	ATUITraceViewerChannelView(ATUITraceViewerContext& context);

	void VScrollToPixel(sint32 pos);

	void OnChannelsChanged();

private:
	bool OnPaint() override;
	void OnMouseDownL(int x, int y) override;
	void OnMouseUpL(int x, int y) override;
	void OnMouseMove(int x, int y) override;
	void OnCaptureLost() override;
	bool OnSetCursor(int x, int y, ATUICursorImage& image) override;
	void OnDpiChanged() override;
	sint32 GetBackgroundColor() const override { return GetTraceViewerTheme().mChannelBackground; }

	int GetSplitterIndex(int y) const;

	ATUITraceViewerContext& mContext;
	sint32 mScrollY = 0;

	bool mbDragging = false;
	int mDragSplitIndex = 0;
	int mDragInitialHeight = 0;
	int mDragInitialClientY = 0;
};

ATUITraceViewerChannelView::ATUITraceViewerChannelView(ATUITraceViewerContext& context)
	: VDDialogFrameW32(IDD_TRACEVIEWER_CHANNELS)
	, mContext(context)
{
}

void ATUITraceViewerChannelView::VScrollToPixel(sint32 pos) {
	if (mScrollY != pos) {
		sint32 delta = pos - mScrollY;
		mScrollY = pos;

		ScrollWindow(mhwnd, 0, -delta, nullptr, nullptr);
	}
}

void ATUITraceViewerChannelView::OnChannelsChanged() {
	Invalidate();
}

bool ATUITraceViewerChannelView::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhdlg, &ps);
	int savedDC = SaveDC(hdc);

	SelectObject(hdc, GetStockObject(DC_PEN));
	SelectObject(hdc, GetStockObject(NULL_BRUSH));
	SelectObject(hdc, mContext.mChannelFont.get());

	const TraceViewerTheme& theme = GetTraceViewerTheme();
	const vdsize32& sz = GetClientArea().size();

	for(const auto *group : mContext.mGroups) {
		RECT rch = { 0, group->mPosY - mScrollY, sz.w, group->mPosY + group->mHeight - mScrollY };

		SetBkMode(hdc, OPAQUE);
		SetDCBrushColor(hdc, GetSysColor(COLOR_3DFACE));

		if (ATUIIsDarkThemeActive()) {
			SetDCPenColor(hdc, VDSwizzleU32(theme.mChannelOutline) >> 8);
			Rectangle(hdc, rch.left, rch.top, rch.right, rch.bottom);
		} else
			DrawEdge(hdc, &rch, BDR_RAISED, BF_RECT | BF_FLAT);

		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, VDSwizzleU32(theme.mChannelText) >> 8);

		SetTextAlign(hdc, TA_LEFT | TA_TOP);
		ExtTextOutW(hdc, rch.left + 10, rch.top + 10, 0, nullptr, group->mName.c_str(), group->mName.size(), nullptr);

		for(const auto *ch : group->mChannels) {
			const wchar_t *name = ch->mpChannel->GetName();

			RECT r;
			r.left = rch.left + 10;
			r.right = rch.right - 10;
			r.top = rch.top + ch->mPosY;
			r.bottom = r.top + ch->mHeight;

			DrawText(hdc, name, wcslen(name), &r, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX); 
		}
	}

	RestoreDC(hdc, savedDC);
	EndPaint(mhdlg, &ps);
	return true;
}

void ATUITraceViewerChannelView::OnMouseDownL(int x, int y) {
	const int splitterIndex = GetSplitterIndex(y);

	if (splitterIndex >= 0) {
		mbDragging = true;
		mDragSplitIndex = splitterIndex;

		ATUITraceViewerGroup *group = mContext.mGroups[splitterIndex];

		mDragInitialHeight = group->mHeight;
		mDragInitialClientY = y;

		SetCapture();
		return;
	}

	y += mScrollY;

	for(const auto *group : mContext.mGroups) {
		const uint32 yoffset = (uint32)(y - group->mPosY);

		if (yoffset < (uint32)group->mHeight) {
			for(const auto *ch : group->mChannels) {
				if ((uint32)(yoffset - ch->mPosY) < (uint32)ch->mHeight) {
					IATTraceChannel *tch = ch->mpChannel;

					if (auto *tchVideo = vdpoly_cast<IATTraceChannelVideo *>(tch)) {
						VDStringW msg;
						const uint64 traceSize = tchVideo->GetTraceSize();
						const double duration = tchVideo->AsTraceChannel()->GetDuration();
						
						msg.sprintf(L"Video trace: %.1f MB (%.2f MB/sec)"
							, (double)traceSize / 1048576.0
							, duration > 0 ? (double)traceSize / 1048576.0 / duration : 0
						);
						ShowInfo(msg.c_str());
						return;
					}
					break;
				}
			}

			if (group->mName == L"CPU") {
				if (auto *tchCPU = vdpoly_cast<ATTraceChannelCPUHistory *>(mContext.mpCPUHistoryChannel)) {
					VDStringW msg;
					const uint64 traceSize = tchCPU->GetTraceSize();
					const double duration = tchCPU->GetDuration();
					const uint32 eventCount = tchCPU->GetEventCount();
						
					msg.sprintf(L"CPU trace: %.1f MB (%.2f MB/sec), %u events (%.1f bytes/event)"
						, (double)traceSize / 1048576.0
						, duration > 0 ? (double)traceSize / 1048576.0 / duration : 0
						, eventCount
						, eventCount > 0 ? (double)traceSize / (double)eventCount : 0
					);
					ShowInfo(msg.c_str());
				}
			}
			break;
		}
	}
}

void ATUITraceViewerChannelView::OnMouseUpL(int x, int y) {
	if (mbDragging) {
		OnMouseMove(x, y);

		mbDragging = false;
		ReleaseCapture();
	}
}

void ATUITraceViewerChannelView::OnMouseMove(int x, int y) {
	if (!mbDragging || mDragSplitIndex < 0)
		return;

	if ((unsigned)mDragSplitIndex >= mContext.mGroups.size())
		return;

	ATUITraceViewerGroup *group = mContext.mGroups[mDragSplitIndex];

	const int targetHeight = std::max<int>(1, mDragInitialHeight + (y - mDragInitialClientY));
	
	if (group->mHeight != targetHeight)
		mContext.mpParent->SetGroupHeightScale(mDragSplitIndex, (float)targetHeight / (float)group->mBaseHeight);
}

void ATUITraceViewerChannelView::OnCaptureLost() {
	mbDragging = false;
}

bool ATUITraceViewerChannelView::OnSetCursor(int x, int y, ATUICursorImage& image) {
	if (mbDragging) {
		image = kATUICursorImage_SizeVert;
		return true;
	}

	if (GetSplitterIndex(y) >= 0) {
		image = kATUICursorImage_SizeVert;
		return true;
	}

	return false;
}

void ATUITraceViewerChannelView::OnDpiChanged() {
	Invalidate();
}

int ATUITraceViewerChannelView::GetSplitterIndex(int y) const {
	const int splitterHalfWidth = 5;
	int index = 0;

	y += mScrollY;

	for(const auto *group : mContext.mGroups) {
		if (group->mbVariableHeight && abs(y - (group->mPosY + group->mHeight)) < splitterHalfWidth)
			return index;

		++index;
	}
	
	return -1;
}

///////////////////////////////////////////////////////////////////////////

class ATUITraceViewerEventView final : public VDDialogFrameW32 {
public:
	ATUITraceViewerEventView(ATUITraceViewerContext& context);

	void SetFrameChannel(IATTraceChannel *channel);

	void SetSelectionMode(bool selMode);
	void ClearSelection();
	void SetSelection(double startTime, double endTime);

	void RescaleView(double newStartTime, double secondsPerPixel);
	void ScrollToTime(double newStartTime);
	void ScrollDeltaPixels(double newStartTime, sint32 dx, sint32 dy);

	void VScrollToPixel(sint32 pos);

private:
	bool OnLoaded() override;
	void OnDestroy() override;
	void OnSize() override;
	void OnMouseMove(int x, int y) override;
	void OnMouseDownL(int x, int y) override;
	void OnMouseUpL(int x, int y) override;
	bool OnMouseDownR(int x, int y) override;
	bool OnMouseUpR(int x, int y) override;
	void OnMouseWheel(int x, int y, sint32 delta) override;
	void OnMouseLeave() override;
	void OnMouseHover() override;
	void OnContextMenu(uint32 id, int x, int y) override;
	bool OnSetCursor(ATUICursorImage& image) override;
	void OnCaptureLost() override;
	bool OnShouldErase() override;
	bool OnPaint() override;
	void OnDpiChanged() override;

private:
	struct SelectedPixelRange {
		sint32 x1;
		sint32 x2;
	};

	void InvalidateSelectedRange(double t1, double t2);
	void InvalidateSelectedPixelRange(const SelectedPixelRange& range);
	SelectedPixelRange SelectedTimeRangeToPixelRange(double t1, double t2) const;
	void RecomputeEndTime();
	double PixelToTime(sint32 x) const;
	sint32 TimeToPixel(double t) const;
	ATUITraceViewerChannel *PointToTraceChannel(const vdpoint32& cpt) const;

	void HideTooltip();

	ATUITraceViewerContext& mContext;
	sint32 mWidth = 0;
	sint32 mHeight = 0;
	double mStartTime = 0;
	double mEndTime = 0;
	double mSecondsPerPixel = 1.0f / 10.0f;

	sint32 mScrollY = 0;
	sint32 mWheelAccum = 0;
	sint32 mDragAnchorX = 0;
	sint32 mDragAnchorY = 0;
	bool mbDragLeft = false;
	bool mbDragRight = false;
	bool mbDragSelectionMode = false;
	bool mbSuppressContextMenu = false;
	bool mbSelectionMode = false;
	bool mbSelectionValid = false;

	double mSelectStart = 0;
	double mSelectEnd = 0;
	uint32 mNotifyRecursionCounter = 0;

	vdrefptr<IATTraceChannel> mpFrameChannel;

	VDPixmapBuffer mImageBufferSrc;
	VDPixmapBuffer mImageBufferDst;
	vdsize32 mImageResamplerSrcSize;
	vdsize32 mImageResamplerDstSize;
	vdautoptr<IVDPixmapResampler> mpImageResampler;
	vdfastvector<uint32> mTapeBlitBuffer;

	HDC mhdcSel = nullptr;
	HBITMAP mhbmSel = nullptr;
	HBITMAP mhbmSelOld = nullptr;

	vdrefptr<ATUITraceTooltipControl> mpTooltipControl;

	vdfunction<void()> mThemeChangedFn;
};

ATUITraceViewerEventView::ATUITraceViewerEventView(ATUITraceViewerContext& context)
	: VDDialogFrameW32(IDD_TRACEVIEWER_EVENTS)
	, mContext(context)
{
	mThemeChangedFn = [this] { Invalidate(); };
}

void ATUITraceViewerEventView::SetFrameChannel(IATTraceChannel *channel) {
	mpFrameChannel = channel;
}

void ATUITraceViewerEventView::SetSelectionMode(bool selMode) {
	mbSelectionMode = selMode;
}

void ATUITraceViewerEventView::ClearSelection() {
	if (mbSelectionValid) {
		mbSelectionValid = false;

		InvalidateSelectedRange(mSelectStart, mSelectEnd);

		++mNotifyRecursionCounter;
		mContext.mpParent->SetSelection(0, -1);
		--mNotifyRecursionCounter;
	}
}

void ATUITraceViewerEventView::SetSelection(double startTime, double endTime) {
	if (mNotifyRecursionCounter)
		return;

	if (mbSelectionValid && mSelectStart == startTime && mSelectEnd == endTime)
		return;

	if (!mbSelectionValid) {
		InvalidateSelectedRange(startTime, endTime);
	} else {
		const SelectedPixelRange oldPixelRange = SelectedTimeRangeToPixelRange(mSelectStart, mSelectEnd);
		const SelectedPixelRange newPixelRange = SelectedTimeRangeToPixelRange(startTime, endTime);

		// check if the old and new range overlap
		if (oldPixelRange.x1 < newPixelRange.x2
			&& newPixelRange.x1 < oldPixelRange.x2
			&& oldPixelRange.x1 < oldPixelRange.x2
			&& newPixelRange.x1 < newPixelRange.x2)
		{
			// they do -- invalidate min to min and max to max
			InvalidateSelectedPixelRange({ std::min(oldPixelRange.x1, newPixelRange.x1), std::max(oldPixelRange.x1, newPixelRange.x1) });
			InvalidateSelectedPixelRange({ std::min(oldPixelRange.x2, newPixelRange.x2), std::max(oldPixelRange.x2, newPixelRange.x2) });
		} else {
			// they don't -- invalidate the old and new times
			InvalidateSelectedPixelRange(oldPixelRange);
			InvalidateSelectedPixelRange(newPixelRange);
		}
	}

	mSelectStart = startTime;
	mSelectEnd = endTime;
	mbSelectionValid = true;

	++mNotifyRecursionCounter;
	mContext.mpParent->SetSelection(startTime, endTime);
	--mNotifyRecursionCounter;
}

void ATUITraceViewerEventView::RescaleView(double newStartTime, double secondsPerPixel) {
	mStartTime = newStartTime;
	mSecondsPerPixel = secondsPerPixel;
	RecomputeEndTime();

	InvalidateRect(mhdlg, nullptr, TRUE);
}

void ATUITraceViewerEventView::ScrollToTime(double newStartTime) {
	mStartTime = newStartTime;
	RecomputeEndTime();

	InvalidateRect(mhdlg, nullptr, TRUE);
}

void ATUITraceViewerEventView::ScrollDeltaPixels(double newStartTime, sint32 dx, sint32 dy) {
	mStartTime = newStartTime;
	RecomputeEndTime();
	ScrollWindow(mhdlg, dx, dy, nullptr, nullptr);
}

void ATUITraceViewerEventView::VScrollToPixel(sint32 pos) {
	if (mScrollY != pos) {
		sint32 delta = pos - mScrollY;
		mScrollY = pos;

		ScrollWindow(mhwnd, 0, -delta, nullptr, nullptr);
	}
}

bool ATUITraceViewerEventView::OnLoaded() {
	if (HDC hdc = GetDC(mhdlg)) {
		mhdcSel = CreateCompatibleDC(hdc);

		if (mhdcSel) {
			BITMAPINFO bi = {};
			bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bi.bmiHeader.biWidth = 1;
			bi.bmiHeader.biHeight = 1;
			bi.bmiHeader.biBitCount = 32;
			bi.bmiHeader.biPlanes = 1;
			bi.bmiHeader.biSizeImage = 4;
			bi.bmiHeader.biCompression = BI_RGB;

			void *bits;
			mhbmSel = CreateDIBSection(mhdcSel, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
			if (mhbmSel) {
				mhbmSelOld = (HBITMAP)SelectObject(mhdcSel, mhbmSel);

				*(uint32 *)bits = 0x84BDFF;
			}
		}

		ReleaseDC(mhdlg, hdc);
	}

	VDDialogFrameW32::OnLoaded();

	OnSize();
	ATUIRegisterThemeChangeNotification(&mThemeChangedFn);

	return false;
}

void
ATUITraceViewerEventView::OnDestroy() {
	HideTooltip();

	ATUIUnregisterThemeChangeNotification(&mThemeChangedFn);

	if (mhdcSel) {
		if (mhbmSelOld) {
			SelectObject(mhdcSel, mhbmSelOld);
			mhbmSelOld = nullptr;
		}

		DeleteDC(mhdcSel);
	}

	if (mhbmSel) {
		DeleteObject(mhbmSel);
		mhbmSel = nullptr;
	}

	VDDialogFrameW32::OnDestroy();
}

void ATUITraceViewerEventView::OnSize() {
	VDDialogFrameW32::OnSize();

	const auto& sz = GetClientArea();
	mWidth = sz.width();
	mHeight = sz.height();

	RecomputeEndTime();
}

void ATUITraceViewerEventView::OnMouseMove(int x, int y) {
	if (mpTooltipControl && !mpTooltipControl->IsScreenPtInAnchorArea(TransformClientToScreen(vdpoint32(x, y))))
		HideTooltip();

	const sint32 dx = x - mDragAnchorX;
	const sint32 dy = y - mDragAnchorY;

	if (!mbDragLeft && !mbDragRight) {
		bool checkLeft = GetKeyState(VK_LBUTTON) < 0;
		bool checkRight = GetKeyState(VK_RBUTTON) < 0;

		if (checkLeft || checkRight) {
			int dragRadius = GetDpiScaledMetric(SM_CXDRAG);

			if (abs(dx) > dragRadius || abs(dy) > dragRadius) {
				if (checkLeft)
					mbDragLeft = true;
				else
					mbDragRight = true;

				mbSuppressContextMenu = true;

				SetCapture();

				SetCurrentCursor(mbDragSelectionMode ? kATUICursorImage_IBeam : kATUICursorImage_SizeHoriz);
			}
		} else {
			TRACKMOUSEEVENT ev {};
			ev.cbSize = sizeof(TRACKMOUSEEVENT);
			ev.dwFlags = TME_HOVER | TME_LEAVE;
			ev.dwHoverTime = HOVER_DEFAULT;
			ev.hwndTrack = mhwnd;
			TrackMouseEvent(&ev);
		}
	}

	if (mbDragLeft || mbDragRight) {
		mDragAnchorX = x;
		mDragAnchorY = y;

		if (mbDragSelectionMode)
			SetSelection(mSelectStart, PixelToTime(x));
		else if (dx|dy)
			mContext.mpParent->ScrollDeltaPixels(dx, 0);
	}
}

void ATUITraceViewerEventView::OnMouseDownL(int x, int y) {
	if (mbDragRight)
		return;

	mDragAnchorX = x;
	mDragAnchorY = y;
	mbDragSelectionMode = mbSelectionMode;

	if (mbDragSelectionMode) {
		double t = PixelToTime(x);
		SetSelection(t, t);
	}
}

void ATUITraceViewerEventView::OnMouseUpL(int x, int y) {
	if (mbDragLeft) {
		mbDragLeft = false;

		ReleaseCapture();

		if (mbDragSelectionMode)
			SetSelection(mSelectStart, PixelToTime(x));
	} else {
		mContext.mpParent->SetFocusTime(PixelToTime(x));
	}
}

bool ATUITraceViewerEventView::OnMouseDownR(int x, int y) {
	if (mbDragLeft)
		return false;

	mDragAnchorX = x;
	mDragAnchorY = y;
	mbDragSelectionMode = false;
	mbSuppressContextMenu = false;
	return true;
}

bool ATUITraceViewerEventView::OnMouseUpR(int x, int y) {
	if (mbDragRight) {
		mbDragRight = false;

		ReleaseCapture();
		return true;
	} else {
		return false;
	}
}

void ATUITraceViewerEventView::OnMouseWheel(int x, int y, sint32 delta) {
	mWheelAccum += delta;

	const int steps = mWheelAccum / 120;
	if (steps) {
		mWheelAccum -= steps * 120;

		mContext.mpParent->ZoomDeltaSteps(mStartTime + mSecondsPerPixel * (double)x, steps);
	}
}

void ATUITraceViewerEventView::OnMouseLeave() {
	HideTooltip();
}

void ATUITraceViewerEventView::OnMouseHover() {
	const DWORD dwPos = GetMessagePos();
	const vdpoint32 spt { (short)LOWORD(dwPos), (short)HIWORD(dwPos) };
	const vdpoint32 cpt = TransformScreenToClient(spt);

	ATUITraceViewerChannel *ch = PointToTraceChannel(cpt);

	if (!ch || ch->mType != ATUITraceViewerChannel::kType_Log)
		return;

	const double t = PixelToTime(cpt.x);
	const double dt = 0.5 * mSecondsPerPixel * (double)ch->mHeight;
	auto& channel = *ch->mpChannel;

	channel.StartIterationNearest(t);

	ATTraceEvent ev;
	if (channel.GetNextEvent(ev) && fabs(ev.mEventStart - t) < dt) {
		if (!mpTooltipControl) {
			mpTooltipControl = new ATUITraceTooltipControl;
			if (!mpTooltipControl->Create(0, 0, 0, 0, false)) {
				mpTooltipControl = nullptr;
				return;
			}
		}

		sint32 cx = TimeToPixel(ev.mEventStart);
		sint32 cy = ch->mpGroup->mPosY + ch->mPosY - mScrollY;
		sint32 cx1 = cx - (ch->mHeight >> 1);
		sint32 cx2 = cx1 + ch->mHeight;

		vdrect32 cr(cx1, cy, cx2, cy + ch->mHeight);

		mpTooltipControl->ShowWithText(TransformClientToScreen(cr), ev.mpName);
	}
}

void ATUITraceViewerEventView::OnContextMenu(uint32 id, int x, int y) {
	if (y < 0 || mbSuppressContextMenu)
		return;

	vdpoint32 cpt = TransformScreenToClient(vdpoint32(x, y));
	ATUITraceViewerChannel *ch = PointToTraceChannel(cpt);

	if (!ch || ch->mType != ATUITraceViewerChannel::kType_Tape)
		return;

	ATTraceChannelTape *tapeChannel = static_cast<ATTraceChannelTape *>(&*ch->mpChannel);
	const double timeSeconds = PixelToTime(cpt.x);

	tapeChannel->StartIteration(timeSeconds, tapeChannel->GetDuration(), 0);

	ATTraceEvent ev;
	if (tapeChannel->GetNextEvent(ev)) {
		const auto& tapeEvent = tapeChannel->GetLastEvent();
		const double samplesPerSec = tapeChannel->GetSamplesPerSec();
		uint32 pos = VDClampToUint32((sint64)(0.5 + (timeSeconds - ev.mEventStart) * samplesPerSec) + (sint64)tapeEvent.mPosition);

		switch(ActivateCommandPopupMenuReturnId(x, y, IDR_PERFANALYZER_TAPE_MENU, nullptr)) {
			case ID_CONTEXT_GOTOTAPEEDITOR: {
				float pixelsPerTapeSample = 1.0f / (mSecondsPerPixel * samplesPerSec);
				ATUIShowDialogTapeEditorAtLocation(pos, pixelsPerTapeSample);
				break;
			}
		}
	}
}

bool ATUITraceViewerEventView::OnSetCursor(ATUICursorImage& image) {
	image = (mbDragLeft || mbDragRight ? mbDragSelectionMode : mbSelectionMode) ? kATUICursorImage_IBeam : kATUICursorImage_Arrow;
	return true;
}

void ATUITraceViewerEventView::OnCaptureLost() {
	mbDragLeft = false;
	mbDragRight = false;
}

bool ATUITraceViewerEventView::OnShouldErase() {
	return false;
}

bool ATUITraceViewerEventView::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc0 = BeginPaint(mhdlg, &ps);

	BP_PAINTPARAMS bufParams { sizeof(bufParams), 0, nullptr, nullptr };
	HDC hdc {};

	const HPAINTBUFFER hPaintBuffer = BeginBufferedPaint(hdc0, &ps.rcPaint, BPBF_COMPATIBLEBITMAP, &bufParams, &hdc);
	if (!hPaintBuffer)
		hdc = hdc0;

	int savedDC = SaveDC(hdc);
	const vdsize32& sz = GetClientArea().size();

	SelectObject(hdc, GetStockObject(DC_PEN));
	SelectObject(hdc, GetStockObject(DC_BRUSH));
	SelectObject(hdc, mContext.mEventFont.get());

	const TraceViewerTheme& theme = GetTraceViewerTheme();

	SetDCBrushColor(hdc, theme.mEventBackground);
	FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(DC_BRUSH));

	const sint32 clipX1 = ps.rcPaint.left;
	const sint32 clipX2 = ps.rcPaint.right;
	const sint32 clipY1 = ps.rcPaint.top;
	const sint32 clipY2 = ps.rcPaint.bottom;

	const double startTime = mStartTime;
	const double endTime = mEndTime;

	double eventStartClip = startTime - mSecondsPerPixel * 10;
	double eventEndClip = endTime + mSecondsPerPixel * 10;
	double labelStartClip = startTime - mSecondsPerPixel * (double)sz.w;
	double labelEndClip = endTime + mSecondsPerPixel * (double)sz.w;

	ATTraceEvent ev;

	// Draw frame boundaries and VBLANK regions.
	//
	// We cheat a little here to draw the blanking regions since we know that we're dealing
	// with only either NTSC or PAL/SECAM timings.

	if (mpFrameChannel) {
		mpFrameChannel->StartIteration(eventStartClip, eventEndClip, mSecondsPerPixel * 4);

		SetDCPenColor(hdc, VDSwizzleU32(theme.mEventVSync) >> 8);
		SetDCBrushColor(hdc, VDSwizzleU32(theme.mEventVBlank) >> 8);

		while(mpFrameChannel->GetNextEvent(ev)) {
			if (ev.mpName) {
				const double duration = ev.mEventStop - ev.mEventStart;
				const bool isPal = ev.mEventStop - ev.mEventStart > 1.0f / 55.0f;
				const double vblEndFrac = isPal ? 8.0f / 312.0f : 8.0f / 262.0f;
				const double vblStartFrac = isPal ? 248.0f / 312.0f : 248.0f / 262.0f;

				const double x0f = (ev.mEventStart - mStartTime) / mSecondsPerPixel;
				const sint32 x0 = VDFloorToInt32(x0f);
				const sint32 xve = VDFloorToInt32(x0f + (vblEndFrac * duration) / mSecondsPerPixel);
				const sint32 xvs = VDFloorToInt32(x0f + (vblStartFrac * duration) / mSecondsPerPixel);
				const sint32 x1 = VDFloorToInt32((ev.mEventStop - mStartTime) / mSecondsPerPixel);

				MoveToEx(hdc, x0, 0, nullptr);
				LineTo(hdc, x0, sz.h);

				if (xve > x0 + 1) {
					RECT r = { x0 + 1, 0, xve, sz.h };
					FillRect(hdc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
				}

				if (xvs < x1) {
					RECT r = { xvs, 0, x1, sz.h };
					FillRect(hdc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
				}
			}
		}
	}

	if (!mContext.mGroups.empty()) {
		for(const auto *group : mContext.mGroups) {
			const sint32 groupY = group->mPosY - mScrollY;

			for(auto *ch : group->mChannels) {
				const sint32 y = ch->mPosY + groupY;

				// check if channel is vertically clipped
				if (y >= clipY2)
					continue;

				if (y + ch->mHeight <= clipY1)
					continue;

				SetDCPenColor(hdc, RGB(160, 160, 160));
				MoveToEx(hdc, 0, y, nullptr);
				LineTo(hdc, sz.w, y);

				if (ch->mType == ATUITraceViewerChannel::kType_Video) {
					IATTraceChannelVideo *vch = vdpoly_cast<IATTraceChannelVideo *>(ch->mpChannel.get());
					const sint32 imgh = ch->mHeight - (3 * mContext.mDpi + 48) / 96;
					const sint32 imgw = (imgh * 4 + 1) / 3;
					const sint32 imgy = groupY + ch->mPosY + 2;
					double secondsPerFrame = mSecondsPerPixel * (double)imgw;

					if (ch->mAvgSecondsPerFrame > 0) {
						secondsPerFrame = ch->mAvgSecondsPerFrame * powf(2.0f, ceilf(log2f(secondsPerFrame / ch->mAvgSecondsPerFrame)));
					}

					// We are binning the timeline into equally spaced bins <secondsPerFrame> apart and binning
					// frame times by that, choosing the nearest frame to represent each bin. However, the frames
					// themselves take half a frame time on either side of that, so we expand the bracket by half
					// a frame on either side.
					double frameTime0 = ch->mFrameTime0 - secondsPerFrame * 0.5;
					double frameStartIndex = floor((eventStartClip - frameTime0) / secondsPerFrame - 0.5);
					double frameEndIndex = ceil((eventEndClip - frameTime0) / secondsPerFrame + 0.5);
					sint32 frameCount = (sint32)(frameEndIndex - frameStartIndex + 0.5);
					double lastFrameTime = -DBL_MAX;

					for(sint32 i = 0; i < frameCount; ++i) {
						double frameStartBracketTime = frameTime0 + (frameStartIndex + (double)i) * secondsPerFrame;
						double frameTime;
						sint32 frameBufferIndex = vch->GetNearestFrameIndex(frameStartBracketTime, frameStartBracketTime + secondsPerFrame, frameTime);
						
						// skip if no frame lies within this bracket
						if (frameBufferIndex < 0)
							continue;

						// compute the blit position for this frame
						const sint32 imgx = (sint32)((frameTime - mStartTime) / mSecondsPerPixel - (double)imgw * 0.5);

						// skip if outside of clip range
						if (imgx <= clipX1 - imgw || imgx >= clipX2)
							continue;

						// check if we just drew this frame
						if (frameTime == lastFrameTime)
							continue;

						lastFrameTime = frameTime;

						// select cache buffer
						const uint32 cacheIndex = (uint32)frameBufferIndex % ch->kNumCachedImages;
						ATGDICachedImageW32& cachedBitmap = ch->mCachedBitmaps[cacheIndex];

						// re-render cache image if not valid
						if (!cachedBitmap.GetHDC() || ch->mCachedImageIndices[cacheIndex] != frameBufferIndex || cachedBitmap.GetWidth() != imgw || cachedBitmap.GetHeight() != imgh) {
							const VDPixmap *px = &vch->GetFrameBufferByIndex(frameBufferIndex);

							mImageBufferSrc.init(px->w, px->h, nsVDPixmap::kPixFormat_XRGB8888);
							VDPixmapBlt(mImageBufferSrc, *px);

							VDPixmapLayout layout;
							VDMakeBitmapCompatiblePixmapLayout(layout, imgw, imgh, nsVDPixmap::kPixFormat_XRGB8888, 0);
							mImageBufferDst.init(layout);

							vdsize32 srcSize { px->w, px->h };
							vdsize32 dstSize { imgw, imgh };

							if (!mpImageResampler) {
								mpImageResampler = VDCreatePixmapResampler();
								mpImageResampler->SetFilters(IVDPixmapResampler::kFilterLinear, IVDPixmapResampler::kFilterLinear, false);
							}

							if (mImageResamplerSrcSize != srcSize || mImageResamplerDstSize != dstSize) {
								mpImageResampler->Init(dstSize.w, dstSize.h, nsVDPixmap::kPixFormat_XRGB8888, srcSize.w, srcSize.h, nsVDPixmap::kPixFormat_XRGB8888);
							}

							mpImageResampler->Process(mImageBufferDst, mImageBufferSrc);

							cachedBitmap.Load(hdc, mImageBufferDst);
							ch->mCachedImageIndices[cacheIndex] = frameBufferIndex;
						}

						HDC hdcCached = cachedBitmap.GetHDC();
						if (hdcCached)
							BitBlt(hdc, imgx, imgy, imgw, imgh, hdcCached, 0, 0, SRCCOPY);
					}
				} else if (ch->mType == ATUITraceViewerChannel::kType_Log) {
					const sint32 eventY1 = y + (2 * mContext.mDpi + 48) / 96;
					const sint32 eventY2 = y + ch->mHeight - (2 * mContext.mDpi + 48) / 96;
					const sint32 eventHeight = eventY2 - eventY1;
					const sint32 eventWidth = eventHeight;
					const sint32 eventLeft = eventWidth >> 1;
					const sint32 eventRight = eventWidth - eventLeft;

					ch->mpChannel->StartIteration(mStartTime - mSecondsPerPixel * (double)eventRight, endTime + mSecondsPerPixel * (double)eventLeft, 0);

					SetDCPenColor(hdc, RGB(0, 0, 0));

					sint32 lastDotX = std::numeric_limits<sint32>::min();
					while(ch->mpChannel->GetNextEvent(ev)) {
						const double evStart = std::max<double>(ev.mEventStart, eventStartClip);
						sint32 x = (sint32)((evStart - mStartTime) / mSecondsPerPixel);

						if (lastDotX != x) {
							lastDotX = x;

							const uint32 bgColor = VDSwizzleU32(ev.mBgColor) >> 8;

							SetDCBrushColor(hdc, bgColor);
							Ellipse(hdc, x - eventLeft, eventY1, x + eventRight, eventY2);
						}
					}
				} else {
					const sint32 eventY1 = y + (2 * mContext.mDpi + 48) / 96;
					const sint32 eventY2 = y + ch->mHeight - (2 * mContext.mDpi + 48) / 96;
					const sint32 eventTextY1 = y + (4 * mContext.mDpi + 48) / 96;
					const sint32 eventTextY2 = y + ch->mHeight - (4 * mContext.mDpi + 48) / 96;

					ch->mpChannel->StartIteration(mStartTime, endTime, mSecondsPerPixel * 4);

					if (ch->mType == ATUITraceViewerChannel::kType_Tape) {
						IATCassetteImage *image = g_sim.GetCassette().GetImage();

						if (image) {
							SetDCBrushColor(hdc, RGB(0x20, 0x20, 0x20));

							ATTraceChannelTape *tapeChannel = static_cast<ATTraceChannelTape *>(&*ch->mpChannel);
							const bool turbo = tapeChannel->IsTurbo();
							const double samplesPerSec32 = tapeChannel->GetSamplesPerSec() * 4294967296.0;

							while(ch->mpChannel->GetNextEvent(ev)) {
								double evStart = std::max<double>(ev.mEventStart, eventStartClip);
								double evEnd = std::min<double>(ev.mEventStop, eventEndClip);
								sint32 x1 = (sint32)((evStart - mStartTime) / mSecondsPerPixel);
								sint32 x2 = (sint32)((evEnd - mStartTime) / mSecondsPerPixel) + 1;

								if (x1 < clipX1)
									x1 = clipX1;

								if (x2 > clipX2)
									x2 = clipX2;

								const auto& tapeEvent = tapeChannel->GetLastEvent();
								uint64 pos32 = (uint64)(0.5 + (mStartTime + mSecondsPerPixel * (double)(x1 + 0.5f) - ev.mEventStart) * samplesPerSec32) + ((uint64)tapeEvent.mPosition << 32);
								uint64 posinc32 = (uint64)(0.5 + mSecondsPerPixel * samplesPerSec32);

								const sint32 channelh = eventY2 - eventY1;
								const sint32 blity = channelh / 6;
								const sint32 blith = channelh - blity*2;
								const uint32 prevPos = (uint32)((pos32 - posinc32) >> 32);
								bool lastBit = pos32 < posinc32 || image->GetBit(prevPos, turbo);

								while(x1 < x2) {
									const sint32 blitw = std::min<sint32>(x2 - x1, 4096);

									RECT channelRect = { x1, eventY1, x2, eventY2 };
									FillRect(hdc, &channelRect, (HBRUSH)GetStockObject(DC_BRUSH));

									mTapeBlitBuffer.clear();
									mTapeBlitBuffer.resize(blitw * blith, 0xFF202020);

									uint32 *pLo = mTapeBlitBuffer.data();
									uint32 *pHi = pLo + blitw * (blith - 1);

									static constexpr auto kColorTable = VDCxArray<uint32, 32>::from_index(
										[](size_t i) {
											const float f1 = (float)i / 31.0f;
											const float f0 = 1.0f - f1;

											// #70A0FF
											const uint32 r = (uint32)(0.5f + VDCxSqrt(112.0f * 112.0f * f1 + 32.0f * 32.0f * f0));
											const uint32 g = (uint32)(0.5f + VDCxSqrt(160.0f * 160.0f * f1 + 32.0f * 32.0f * f0));
											const uint32 b = (uint32)(0.5f + VDCxSqrt(255.0f * 255.0f * f1 + 32.0f * 32.0f * f0));

											return UINT32_C(0xFF000000) + (r << 16) + (g << 8) + b;
										}
									);

									static constexpr auto kColorTable2 = VDCxArray<uint32, 32>::from_index(
										[](size_t i) {
											const float f1 = 0.15f + 0.85f * (float)i / 31.0f;
											const float f0 = 1.0f - f1;

											// #70A0FF
											const uint32 r = (uint32)(0.5f + VDCxSqrt(112.0f * 112.0f * f1 + 32.0f * 32.0f * f0));
											const uint32 g = (uint32)(0.5f + VDCxSqrt(160.0f * 160.0f * f1 + 32.0f * 32.0f * f0));
											const uint32 b = (uint32)(0.5f + VDCxSqrt(255.0f * 255.0f * f1 + 32.0f * 32.0f * f0));

											return UINT32_C(0xFF000000) + (r << 16) + (g << 8) + b;
										}
									);

									uint32 bitWidth = std::max<uint32>(1, (uint32)((posinc32 + UINT32_C(0xFFFFFFFF)) >> 32));

									if (bitWidth < 2) {
										const uint32 color = 0xFF70A0FF;

										for(sint32 x=0; x<blitw; ++x) {
											const uint32 pos = (uint32)(pos32 >> 32);
											const bool bit = image->GetBit(pos, turbo);
											pos32 += posinc32;

											if (bit != lastBit) {
												uint32 *p = pLo + x;

												for(sint32 y=0; y<blith; ++y) {
													*p = color;
													p += blitw;
												}
											} if (bit)
												pHi[x] = color;
											else
												pLo[x] = color;

											lastBit = bit;
										}
									} else {
										const uint32 bitCountScale = (31 << 12) / bitWidth;
										for(sint32 x=0; x<blitw; ++x) {
											const uint32 pos = (uint32)(pos32 >> 32);
											const auto info = image->GetTransitionInfo(pos, bitWidth, turbo);

											const uint32 markFrac = (info.mMarkBits * bitCountScale + 2048) >> 12;
											pos32 += posinc32;

											if (info.mTransitionBits > 0) {
												const uint32 transitionFrac = (info.mTransitionBits * bitCountScale + 2048) >> 12;
												uint32 *p = pLo + x;

												const uint32 color = kColorTable2[transitionFrac];
												for(sint32 y=0; y<blith; ++y) {
													*p = color;
													p += blitw;
												}
											}
										
											pHi[x] = kColorTable[markFrac];
											pLo[x] = kColorTable[31 - markFrac];
										}
									}

									BITMAPINFO bi = {
										{
											sizeof(BITMAPINFOHEADER),
											(LONG)blitw,
											(LONG)blith,
											1,
											32,
											0,
											(DWORD)(blith*blith*4)
										}
									};

									SetDIBitsToDevice(hdc, x1, eventY1 + blity, blitw, blith, 0, 0, 0, blith, mTapeBlitBuffer.data(), &bi, DIB_RGB_COLORS);

									x1 += blitw;
								}
							}
						}
					} else {
						SetDCPenColor(hdc, theme.mEventOutline);

						SetBkMode(hdc, OPAQUE);
						SetDCBrushColor(hdc, RGB(160, 255, 192));
						SetTextAlign(hdc, TA_TOP | TA_CENTER);

						while(ch->mpChannel->GetNextEvent(ev)) {
							double evStart = std::max<double>(ev.mEventStart, eventStartClip);
							double evEnd = std::min<double>(ev.mEventStop, eventEndClip);
							sint32 x1 = (sint32)((evStart - mStartTime) / mSecondsPerPixel);
							sint32 x2 = (sint32)((evEnd - mStartTime) / mSecondsPerPixel) + 1;

							if (ev.mpName) {
								const uint32 bgColor = VDSwizzleU32(ev.mBgColor) >> 8;
								SetBkColor(hdc, bgColor);
								SetDCBrushColor(hdc, bgColor);
								Rectangle(hdc, x1, eventY1, x2, eventY2);

								double labelTime = (ev.mEventStart + ev.mEventStop) * 0.5;

								if (labelTime > labelStartClip && labelTime < labelEndClip) {
									RECT rtx = { x1 + 1, eventTextY1, x2 - 1, eventTextY2};

									SetTextColor(hdc, VDSwizzleU32(ev.mFgColor) >> 8);
									ExtTextOutW(hdc, (sint32)floor((labelTime - mStartTime) / mSecondsPerPixel), eventTextY1, ETO_CLIPPED, &rtx, ev.mpName, wcslen(ev.mpName), nullptr);
								}
							} else {
								const uint32 mutedBgColor = 0x606060 + ((ev.mBgColor & 0xFEFEFE) >> 1);
								SetBkColor(hdc, VDSwizzleU32(mutedBgColor) >> 8);
								RECT rev = { x1, eventY1, x2, eventY2 };
								ExtTextOutW(hdc, 0, y, ETO_OPAQUE, &rev, L"", 0, nullptr);
							}
						}
					}
				}
			}
		}

		// draw line after last group
		ATUITraceViewerGroup *lastGroup = mContext.mGroups.back();
		sint32 lastY = lastGroup->mPosY + lastGroup->mHeight - mScrollY;

		MoveToEx(hdc, 0, lastY, nullptr);
		LineTo(hdc, sz.w, lastY);
	}

	if (mbSelectionValid) {
		const SelectedPixelRange selRange = SelectedTimeRangeToPixelRange(mSelectStart, mSelectEnd);

		if (selRange.x1 < selRange.x2)
			AlphaBlend(hdc, selRange.x1, 0, selRange.x2 - selRange.x1, sz.h, mhdcSel, 0, 0, 1, 1, BLENDFUNCTION { AC_SRC_OVER, 0, 128, 0 });
	}

	RestoreDC(hdc, savedDC);

	if (hPaintBuffer)
		EndBufferedPaint(hPaintBuffer, TRUE);

	EndPaint(mhdlg, &ps);
	return true;
}

void ATUITraceViewerEventView::OnDpiChanged() {
	InvalidateRect(mhdlg, nullptr, TRUE);
}

void ATUITraceViewerEventView::InvalidateSelectedRange(double t1, double t2) {
	InvalidateSelectedPixelRange(SelectedTimeRangeToPixelRange(t1, t2));
}

void ATUITraceViewerEventView::InvalidateSelectedPixelRange(const SelectedPixelRange& range) {
	if (range.x1 != range.x2) {
		RECT r = { range.x1, 0, range.x2, mHeight };
		InvalidateRect(mhdlg, &r, TRUE);
	}
}

ATUITraceViewerEventView::SelectedPixelRange ATUITraceViewerEventView::SelectedTimeRangeToPixelRange(double t1, double t2) const {
	sint32 x1 = TimeToPixel(std::min(t1, t2));
	sint32 x2 = TimeToPixel(std::max(t1, t2));

	if (x1 == x2)
		++x2;

	return SelectedPixelRange { x1, x2 };
}

void ATUITraceViewerEventView::RecomputeEndTime() {
	mEndTime = mStartTime + mSecondsPerPixel * mWidth;
}

double ATUITraceViewerEventView::PixelToTime(sint32 x) const {
	return ((double)x + 0.5) * mSecondsPerPixel + mStartTime;
}

sint32 ATUITraceViewerEventView::TimeToPixel(double t) const {
	t = std::max<double>(t - mStartTime, -5 * mSecondsPerPixel);
	t = std::min<double>(t, mEndTime - mStartTime + mSecondsPerPixel * 5);
	return (sint32)floor(t / mSecondsPerPixel);
}

ATUITraceViewerChannel *ATUITraceViewerEventView::PointToTraceChannel(const vdpoint32& cpt) const {
	sint32 channelY = cpt.y + mScrollY;

	for(const auto *group : mContext.mGroups) {
		const sint32 groupY = group->mPosY;

		if (channelY < groupY || (channelY - groupY) >= group->mHeight)
			continue;

		for(auto *ch : group->mChannels) {
			const sint32 chY = ch->mPosY + groupY;

			if (channelY >= chY && (channelY - chY) < ch->mHeight)
				return ch;
		}

		break;
	}

	return nullptr;
}

void ATUITraceViewerEventView::HideTooltip() {
	if (mpTooltipControl) {
		mpTooltipControl->Destroy();
		mpTooltipControl = nullptr;
	}
}

///////////////////////////////////////////////////////////////////////////

class ATUITraceViewerTimescaleView final : public VDDialogFrameW32 {
public:
	ATUITraceViewerTimescaleView(ATUITraceViewerContext& context);

	sint32 GetIdealHeight() const;

	void ScrollToTime(double newStartTime);
	void ScrollDeltaPixels(double newStartTime, sint32 dx);
	void RescaleView(double newStartTime, double secondsPerPixel, double secondsPerDivision);

private:
	bool OnPaint() override;
	void OnDpiChanged() override;
	sint32 GetBackgroundColor() const override { return GetTraceViewerTheme().mTimescaleBackground; }

private:
	ATUITraceViewerContext& mContext;

	double mStartTime = 0;
	double mSecondsPerPixel = 1.0f / 10.0f;
	double mSecondsPerDivision = 1.0f;
	int mDivisionDecimals = 0;
};

ATUITraceViewerTimescaleView::ATUITraceViewerTimescaleView(ATUITraceViewerContext& context)
	: VDDialogFrameW32(IDD_TRACEVIEWER_TIMESCALE)
	, mContext(context)
{
}

sint32 ATUITraceViewerTimescaleView::GetIdealHeight() const {
	return mContext.mTimestampFontMetrics.tmHeight + (14 * mContext.mDpi + 48) / 96;
}

void ATUITraceViewerTimescaleView::ScrollToTime(double newStartTime) {
	mStartTime = newStartTime;
	InvalidateRect(mhdlg, nullptr, TRUE);
}

void ATUITraceViewerTimescaleView::ScrollDeltaPixels(double newStartTime, sint32 dx) {
	mStartTime = newStartTime;
	ScrollWindow(mhdlg, dx, 0, nullptr, nullptr);
}

void ATUITraceViewerTimescaleView::RescaleView(double newStartTime, double secondsPerPixel, double secondsPerDivision) {
	mStartTime = newStartTime;
	mSecondsPerPixel = secondsPerPixel;
	mSecondsPerDivision = secondsPerDivision;

	mDivisionDecimals = 0;
	if (mSecondsPerDivision < 0.000002f)
		mDivisionDecimals = 6;
	else if (mSecondsPerDivision < 0.00002f)
		mDivisionDecimals = 5;
	else if (mSecondsPerDivision < 0.0002f)
		mDivisionDecimals = 4;
	else if (mSecondsPerDivision < 0.002f)
		mDivisionDecimals = 3;
	else if (mSecondsPerDivision < 0.02f)
		mDivisionDecimals = 2;
	else if (mSecondsPerDivision < 0.2f)
		mDivisionDecimals = 1;

	InvalidateRect(mhdlg, nullptr, TRUE);
}

bool ATUITraceViewerTimescaleView::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhdlg, &ps);
	int savedDC = SaveDC(hdc);

	const vdsize32& sz = GetClientArea().size();
	const TraceViewerTheme& theme = GetTraceViewerTheme();

	SelectObject(hdc, mContext.mTimestampFont.get());
	SelectObject(hdc, GetStockObject(DC_PEN));
	SetDCPenColor(hdc, VDSwizzleU32(theme.mTimescaleText) >> 8);
	SetTextColor(hdc, VDSwizzleU32(theme.mTimescaleText) >> 8);
	MoveToEx(hdc, 0, sz.h - 1, nullptr);
	LineTo(hdc, sz.w, sz.h - 1);

	SetTextAlign(hdc, TA_TOP | TA_CENTER);
	SetBkColor(hdc, VDSwizzleU32(GetBackgroundColor()) >> 8);

	double div1f = mStartTime / mSecondsPerDivision;
	double div2f = (mStartTime + (double)sz.w * mSecondsPerPixel) / mSecondsPerDivision;
	sint64 div1 = (sint64)ceil(div1f);
	sint64 div2 = (sint64)ceil(div2f);

	VDStringW s;

	int majorDivHeight = (12 * mContext.mDpi + 48) / 96;
	int minorDivHeight = (6 * mContext.mDpi + 48) / 96;

	for(sint64 div = div1 - 1; div < div2 + 1; ++div) {
		sint32 x = (sint32)floor(((double)div * mSecondsPerDivision - mStartTime) / mSecondsPerPixel);
		const bool major = (div % 5) == 0;
		
		MoveToEx(hdc, x, major ? sz.h - majorDivHeight : sz.h - minorDivHeight, nullptr);
		LineTo(hdc, x, sz.h - 1);

		if (major) {
			s.sprintf(L"%.*f", mDivisionDecimals, (double)div * mSecondsPerDivision);
			ExtTextOutW(hdc, x, 0, ETO_OPAQUE, nullptr, s.c_str(), (UINT)s.size(), nullptr);
		}
	}

	RestoreDC(hdc, savedDC);
	EndPaint(mhdlg, &ps);
	return true;
}

void ATUITraceViewerTimescaleView::OnDpiChanged() {
	InvalidateRect(mhdlg, nullptr, TRUE);
}

///////////////////////////////////////////////////////////////////////////

class ATUITraceViewer;

class IATUITraceViewerHost {
public:
	virtual void SetTraceViewer(ATUITraceViewer *viewer) = 0;
	virtual void ClearLoadedTraceName() = 0;
	virtual void SetLoadedTraceNameCaptured() = 0;
};

class ATUITraceViewer final : public VDDialogFrameW32, public IATUITraceViewer {
public:
	ATUITraceViewer(ATTraceCollection *collection, IATUITraceViewerHost& host);
	~ATUITraceViewer();

	double GetViewEndTime() const;
	double GetViewCenterTime() const;

	void StartStopTracing();
	void StartNativeTrace();
	void ZoomIn();
	void ZoomOut();

	bool CanExport() const;
	void ExportToChromeTrace(const wchar_t *path) const;
	void ImportA800(const wchar_t *path);
	void Load(const wchar_t *path);
	void Save(const wchar_t *path) const;
	
	void ViewMemoryStatistics();

	void SetCollection(ATTraceCollection *collection);

private:
	void ScrollDeltaPixels(sint32 dx, sint32 dy) override;
	void ScrollToCenterTime(double centerTime);
	void ZoomDeltaSteps(double centerTime, sint32 steps) override;
	void SetFocusTime(double t) override;
	void SetSelection(double startTime, double endTime) override;
	void SetSelectionMode(bool selMode) override;
	void SetGroupHeightScale(int groupIndex, float scale) override;

private:
	bool OnLoaded() override;
	void OnDestroy() override;
	void OnSize() override;
	void OnDpiChanged() override;
	sint32 GetBackgroundColor() const override { return GetTraceViewerTheme().mPanelBackground; }
	bool PreNCDestroy() override { return true; }

	void RebuildViews();
	void RebuildToolbar();
	void UpdateToolbarSelectionState();
	void UpdateFonts();
	void ClearGroupViews();
	void UpdateChannelBaseHeights();
	void UpdateChannelPositions();
	void VScrollToPixel(sint32 pos);
	void UpdateHScroll(bool updateRange);
	void UpdateVScroll(bool updateRange);
	bool ShouldVScrollBeVisible() const;
	void UpdateTraceButton();

	void OnToolbarClicked(uint32 id);

	void OnLogTimestampModeChanged(ATUITraceViewerLogView::TimestampMode mode);
	void OnLogTimestampOriginChanged(double t);
	void OnHistoryTimestampModeChanged(ATUIHistoryTimestampMode mode);
	void OnHistoryTimestampOriginChanged(double t);

	void OpenHistory();
	void OpenProfile();
	void OpenLog();

	enum : uint32 {
		kToolbarId_StartStop = 1000,
		kToolbarId_ZoomIn,
		kToolbarId_ZoomOut,
		kToolbarId_Settings,
		kToolbarId_Select,
		kToolbarId_Move,
		kToolbarId_Profile,
		kToolbarId_History,
		kToolbarId_Log
	};

	sint32 mChannelSplitX = 200;
	sint32 mSplitterWidth = 5;
	sint32 mEventViewWidth = 0;
	sint32 mEventViewHeight = 0;
	sint32 mScrollY = 0;

	// Height of all channel groups, in pixels, not clipped by event viewport.
	sint32 mAllGroupHeight = 0;

	double mStartTime = 0;
	double mSecondsPerPixel = 1.0f / 10.0f;
	double mSecondsPerHScrollTick = 1.0f;
	sint32 mPixelsPerHScrollTick = 1;
	double mTraceDuration = 0;
	sint32 mZoomLevel = -5;

	uint32 mSimEventIdTraceLimited = 0;

	IATUITraceViewerHost& mHost;

	vdrefptr<ATTraceCollection> mpCollection;
	ATCPUTimestampDecoder mTimestampDecoder;
	bool mbRecording = false;
	ATTraceSettings mSettings {};

	ATUITraceViewerContext mContext;
	ATUITraceViewerTimescaleView mTimescaleView;
	ATUITraceViewerChannelView mChannelView;
	ATUITraceViewerEventView mEventView;
	ATUITraceViewerCPUHistoryView mCPUHistoryView;
	ATUITraceViewerCPUProfileView mCPUProfileView;
	ATUITraceViewerLogView mLogView;
	VDUIProxyToolbarControl mToolbar;
	VDUIProxyScrollBarControl mHScrollBar;
	VDUIProxyScrollBarControl mVScrollBar;

	vdfunction<void()> mThemeChangedFn;
};

ATUITraceViewer::ATUITraceViewer(ATTraceCollection *collection, IATUITraceViewerHost& host)
	: VDDialogFrameW32(IDD_TRACEVIEWER)
	, mHost(host)
	, mpCollection(collection)
	, mTimescaleView(mContext)
	, mChannelView(mContext)
	, mEventView(mContext)
	, mCPUHistoryView(mContext)
	, mCPUProfileView(mContext)
	, mLogView(mContext)
{
	mHost.SetTraceViewer(this);
	mContext.mpParent = this;

	ATTraceLoadDefaults(mSettings);

	mToolbar.SetDarkModeEnabled(true);
	mToolbar.SetOnClicked([this](uint32 id) { OnToolbarClicked(id); });

	mHScrollBar.SetOnValueChanged(
		[this](sint32 value, bool final) {
			ScrollToCenterTime((double)value * mSecondsPerHScrollTick);
		}
	);

	mVScrollBar.SetOnValueChanged(
		[this](sint32 value, bool final) {
			VScrollToPixel(value);
		}
	);

	mThemeChangedFn = [this] { Invalidate(); };

	mLogView.SetOnEntryActivated(
		[this](double t) {
			SetFocusTime(t);
		}
	);

	mLogView.SetOnTimestampModeChanged(std::bind_front(&ATUITraceViewer::OnLogTimestampModeChanged, this));
	mLogView.SetOnTimestampOriginChanged(std::bind_front(&ATUITraceViewer::OnLogTimestampOriginChanged, this));

	mCPUHistoryView.SetOnTimestampModeChanged(std::bind_front(&ATUITraceViewer::OnHistoryTimestampModeChanged, this));
	mCPUHistoryView.SetOnTimestampOriginChanged(std::bind_front(&ATUITraceViewer::OnHistoryTimestampOriginChanged, this));
}

ATUITraceViewer::~ATUITraceViewer() {
	mHost.SetTraceViewer(nullptr);
}

double ATUITraceViewer::GetViewEndTime() const {
	return mStartTime + mSecondsPerPixel * (double)mEventView.GetClientArea().width();
}

double ATUITraceViewer::GetViewCenterTime() const {
	return mStartTime + mSecondsPerPixel * (double)mEventView.GetClientArea().width() * 0.5;
}

void ATUITraceViewer::StartStopTracing() {
	if (!mbRecording) {
		mbRecording = true;

		SetCollection(nullptr);
		mHost.ClearLoadedTraceName();

		g_sim.StartTracing(mSettings);
		g_sim.Resume();
	} else {
		mbRecording = false;

		vdrefptr<ATTraceCollection> newCollection { g_sim.GetTraceCollection() };
		g_sim.StopTracing();
		g_sim.Pause();

		SetCollection(newCollection);
		mHost.SetLoadedTraceNameCaptured();
	}

	UpdateTraceButton();
}

void ATUITraceViewer::StartNativeTrace() {
	mbRecording = true;

	SetCollection(nullptr);
	mHost.ClearLoadedTraceName();

	ATNativeTraceSettings traceSettings;
	static_cast<ATBaseTraceSettings&>(traceSettings) = mSettings;
	g_sim.StartNativeTracing(traceSettings);
	g_sim.Resume();

	UpdateTraceButton();
}

void ATUITraceViewer::ZoomIn() {
	ZoomDeltaSteps(GetViewCenterTime(), 1);
}

void ATUITraceViewer::ZoomOut() {
	ZoomDeltaSteps(GetViewCenterTime(), -1);
}

bool ATUITraceViewer::CanExport() const {
	return mpCollection && mpCollection->GetGroupByType(kATTraceGroupType_CPUHistory) != nullptr;
}

void ATUITraceViewer::ExportToChromeTrace(const wchar_t *path) const {
	if (!CanExport())
		return;

	ATTraceGroup *cpuTraceGroup = mpCollection->GetGroupByType(kATTraceGroupType_CPUHistory);
	if (!cpuTraceGroup)
		return;

	VDFileStream fileOutput(path, nsVDFile::kWrite | nsVDFile::kCreateAlways | nsVDFile::kDenyRead | nsVDFile::kSequential);
	VDTextOutputStream textOutput(&fileOutput);

	textOutput.PutLine("{\n");

	ATTraceChannelCPUHistory& cpuTrace = *vdpoly_cast<ATTraceChannelCPUHistory *>(cpuTraceGroup->GetChannel(0));

	textOutput.PutLine("\"traceEvents\": [");

	// emit meta events for the CPU history trace (PID 1)
	static constexpr struct ThreadInfo {
		int mThreadId;
		const char *mpName;
	} kThreads[] = {
		{ kATProfileContext_Main, "Main" },
		{ kATProfileContext_IRQ, "IRQ" },
		{ kATProfileContext_VBI, "VBI" },
		{ kATProfileContext_DLI, "DLI" },
	};

	textOutput.PutLine(R"--({"pid":1,"tid":0,"ph":"M","name":"process_name","args":{"name":"CPU (calls)"}})--");
	textOutput.PutLine(R"--(,{"pid":1,"tid":0,"ph":"M","name":"process_sort_index","args":{"sort_index":1}})--");

	for(int i=0; i<std::ssize(kThreads); ++i) {
		const ThreadInfo& ti = kThreads[i];

		textOutput.FormatLine(R"--(,{"pid":1,"tid":%d,"ph":"M","name":"thread_name","args":{"name":"%s"}})--"
			, ti.mThreadId
			, ti.mpName
		);

		textOutput.FormatLine(R"--(,{"pid":1,"tid":%d,"ph":"M","name":"thread_sort_index","args":{"sort_index":%d}})--"
			, ti.mThreadId
			, i
		);
	}

	// if we have a frame channel, throw it into the counters under PID 0
	if (ATTraceGroup *frameTraceGroup = mpCollection->GetGroupByType(kATTraceGroupType_Frames)) {
		IATTraceChannel *frameTraceChannel = frameTraceGroup->GetChannel(0);

		textOutput.PutLine(R"--(,{"pid":0,"tid":10,"ph":"C","name":"Frames","ts":0,"args":{"value":0}})--");
		textOutput.PutLine(R"--(,{"pid":0,"tid":0,"ph":"M","name":"process_sort_index","args":{"sort_index":0}})--");

		int nextValue = 1;
		frameTraceChannel->StartIteration(0, frameTraceChannel->GetDuration(), 0);
		ATTraceEvent frameEvent;
		while(frameTraceChannel->GetNextEvent(frameEvent)) {
			textOutput.FormatLine(R"--(,{"pid":0,"tid":10,"ph":"C","name":"Frames","ts":%.2f,"args":{"value":%d}})--"
				, frameEvent.mEventStop * 1000000.0
				, nextValue
			);

			nextValue ^= 1;
		}
	}

	// iterate over all normal trace channels and throw them into the general pool under PID 2+ before we do the CPU history
	// trace
	unsigned pid = 2;
	for(size_t groupIdx = 0, groupCount = mpCollection->GetGroupCount(); groupIdx < groupCount; ++groupIdx) {
		ATTraceGroup *traceGroup = mpCollection->GetGroup(groupIdx);

		if (traceGroup->GetType() != kATTraceGroupType_Normal)
			continue;

		unsigned tid = 0;	
		for(size_t channelIdx = 0, channelCount = traceGroup->GetChannelCount(); channelIdx < channelCount; ++channelIdx) {
			IATTraceChannel *channel = traceGroup->GetChannel(channelIdx);

			channel->StartIteration(0, channel->GetDuration(), 0);

			ATTraceEvent ev;
			if (channel->GetNextEvent(ev)) {
				VDStringA prefix;
				prefix.sprintf(R"--(,{"pid":%d,"tid":%d,)--", pid, tid);

				if (tid == 0) {
					textOutput.FormatLine(R"--(,{"pid":%d,"tid":0,"ph":"M","name":"process_name","args":{"name":"%s"}})--", pid, VDTextWToU8(VDStringSpanW(traceGroup->GetName())).c_str());
					textOutput.FormatLine(R"--(,{"pid":%d,"tid":0,"ph":"M","name":"process_sort_index","args":{"sort_index":%d}})--", pid, pid);
				}

				textOutput.FormatLine(R"--(%s"ph":"M","name":"thread_name","args":{"name":"%s"}})--", prefix.c_str(), VDTextWToU8(VDStringSpanW(channel->GetName())).c_str());
				textOutput.FormatLine(R"--(%s"ph":"M","name":"thread_sort_index","args":{"sort_index":"%u"}})--", prefix.c_str(), tid);

				do {
					textOutput.FormatLine(R"--(%s"ph":"B","ts":%.2f,"name":"%s"})--", prefix.c_str(), ev.mEventStart * 1000000.0, VDTextWToU8(VDStringSpanW(ev.mpName)).c_str());
					textOutput.FormatLine(R"--(%s"ph":"E","ts":%.2f})--", prefix.c_str(), ev.mEventStop * 1000000.0);
				} while(channel->GetNextEvent(ev));

				++tid;
			}
		}

		if (tid)
			++pid;
	}

	uint32 startEventIdx = 0;
	uint32 endEventIdx = cpuTrace.GetEventCount();

	const ATCPUHistoryEntry *hents[256];
	uint32 pos = startEventIdx;
	uint32 baseCycle = cpuTrace.GetHistoryBaseCycle();

	bool adjustStack = false;
	bool init = true;
	uint8 lastS = 0;

	double cyclesToTimestamp = cpuTrace.GetSecondsPerTick() * 1000000.0;
	uint8 shadowStack[256] {};

	static constexpr unsigned kCallStackLimit = 32;
	uint32 callStack[kCallStackLimit];
	uint8 callStackTid[kCallStackLimit];
	unsigned callStackHeight = 0;

	struct CallFrameHash {
		size_t operator()(const std::pair<uint32, uint32>& v) const {
			return v.first + ((size_t)v.second << 16) + ((size_t)v.second >> 16);
		}
	};

	vdfastvector<std::pair<uint32, uint32>> callFrameTable;
	vdhashmap<std::pair<uint32, uint32>, uint32, CallFrameHash> callFrameLookup;

	auto cursor = cpuTrace.StartHistoryIteration(0, 0);

	int tid = kATProfileContext_Main;
	int nextTid = tid;
	bool firstEvent = true;
	while(pos < endEventIdx) {
		uint32 n = cpuTrace.ReadHistoryEvents(cursor, hents, pos, std::min<uint32>(endEventIdx - pos, (uint32)vdcountof(hents)));
		if (!n)
			break;

		if (init) {
			init = false;

			lastS = hents[0]->mS;
		}
			
		for(uint32 i=0; i<n; ++i) {
			const ATCPUHistoryEntry& he = *hents[i];

			if (he.mbIRQ != he.mbNMI) {
				adjustStack = true;

				if (he.mbNMI) {
					if (mTimestampDecoder.IsInterruptPositionVBI(he.mCycle))
						nextTid = kATProfileContext_VBI;
					else
						nextTid = kATProfileContext_DLI;
				} else {
					nextTid = kATProfileContext_IRQ;
				}
			}

			if (adjustStack) {
				sint8 sdir = (sint8)(he.mS - lastS);
				bool needOpen = false;

				unsigned popToHeight = callStackHeight;

				if (sdir > 0) {
					// stack pointer has gone up (pop), scan the shadow stack upward and arrange for closes as needed
					while(lastS != he.mS) {
						uint8 stackLevel = shadowStack[lastS];

						if (stackLevel) {
							shadowStack[lastS] = 0;

							if (popToHeight >= stackLevel)
								popToHeight = stackLevel - 1;
						}

						++lastS;
					}
				}
				else if (sdir < 0) {
					// stack pointer has gone down (push); clear the shadow stack downward and arrange for an open
					while(lastS != he.mS)
						shadowStack[--lastS] = 0;

					needOpen = true;
				}

				// Check if we are changing the thread ID, but aren't already scheduling an open. This can happen
				// with back-to-back interrupts.
				if (!needOpen && nextTid != tid) {
					// if there is a call frame at the current stack level, close it
					if (popToHeight && shadowStack[lastS]) {
						--popToHeight;
						shadowStack[lastS] = 0;
					}

					// request an open
					needOpen = true;
				}

				while(callStackHeight > popToHeight) {
					textOutput.FormatLine(R"--(,{"pid":1,"tid": %d,"ph":"E","ts":%.2f})--", tid, (double)(he.mCycle - baseCycle) * cyclesToTimestamp);

					--callStackHeight;
					tid = callStackTid[callStackHeight];
				}

				if (needOpen && callStackHeight < kCallStackLimit) {
					uint32 extpc = he.mPC + (he.mK << 16);
			
					const unsigned parentFrameId = callStackHeight ? callStack[callStackHeight - 1] : 0;

					auto r = callFrameLookup.insert(std::pair<uint32, uint32>(parentFrameId, extpc));

					if (r.second) {
						callFrameTable.emplace_back(parentFrameId, extpc);
						r.first->second = (uint32)callFrameLookup.size();
					}

					const unsigned frameId = r.first->second;
					callStack[callStackHeight] = frameId;
					callStackTid[callStackHeight] = tid;
					++callStackHeight;
					shadowStack[lastS] = callStackHeight;

					tid = nextTid;

					const double ts = (double)(he.mCycle - baseCycle) * cyclesToTimestamp;
					if (firstEvent) {
						firstEvent = false;

						// Chrome tries to be helpful by resetting the trace origin to the first event, but we really don't
						// want that to happen.

						if (ts > 0) {
							textOutput.FormatLine(R"--(,{"pid":1,"tid": %d,"ph":"B","name":"preroll","ts":0})--", tid);
							textOutput.FormatLine(R"--(,{"pid":1,"tid": %d,"ph":"E","name":"preroll","ts":%.2f})--", tid, ts);
						}
					}

					textOutput.FormatLine(R"--(,{"pid":1,"tid":%d,"ph":"B","name":"%s","sf":%u,"ts": %.2f})--", tid, ATGetDebugger()->GetAddressText(extpc, false, true).c_str(), frameId, ts);
				}

				adjustStack = false;
			}

			switch(he.mOpcode[0]) {
				case 0x20:		// JSR
				case 0x60:		// RTS
				case 0x40:		// RTI
				case 0x6C:		// JMP (abs)
					adjustStack = true;

					nextTid = tid;
					if (!(he.mP & AT6502::kFlagI))
						nextTid = kATProfileContext_Main;
					break;
			}
		}

		pos += n;
	}

	textOutput.PutLine("],");
	textOutput.PutLine("\"stackFrames\": {");

	const size_t numFrames = callFrameTable.size();
	for(size_t i=0; i<numFrames; ++i) {
		const auto [parentFrameId, extpc] = callFrameTable[i];

		if (parentFrameId)
			textOutput.FormatLine(R"--("%u":{"category":"pc","name":"%s","parent":%u}%s)--", i+1, ATGetDebugger()->GetAddressText(extpc, false, true).c_str(), parentFrameId, (i != numFrames-1) ? "," : "");
		else
			textOutput.FormatLine(R"--("%u":{"category":"pc","name":"%s"}%s)--", i+1, ATGetDebugger()->GetAddressText(extpc, false, true).c_str(), (i != numFrames-1) ? "," : "");
	}

	textOutput.PutLine("}");
	textOutput.PutLine("}");

	textOutput.Flush();
	fileOutput.close();
}

void ATUITraceViewer::ImportA800(const wchar_t *path) {
	auto traceCollection = ATLoadTraceFromAtari800(path);

	SetCollection(traceCollection);
}

void ATUITraceViewer::Load(const wchar_t *path) {
	struct ZipFile : public vdrefcounted<IVDRefCount> {
		VDFileStream mFile;
		VDZipArchive mArchive;

		ZipFile(const wchar_t *path) : mFile(path) {
			mArchive.Init(&mFile);
		}
	};
	
	vdrefptr zipFile(new ZipFile(path));

	SetCollection(nullptr);

	VDZipArchive& ziparch = zipFile->mArchive;
	sint32 n = ziparch.GetFileCount();

	for(sint32 i=0; i<n; ++i) {
		const VDZipArchive::FileInfo& info = ziparch.GetFileInfo(i);
		if (info.mDecodedFileName == L"trace.json") {
			vdautoptr ds(ATCreateSaveStateDeserializer(L"trace.json"));

			ATProgress progress;
			progress.Init(1000, nullptr, L"Loading trace");

			ds->SetProgressFn(
				[&](int i, int n) {
					if (n)
						progress.Update(i * 100 / n);
				}
			);

			vdrefptr<IATSerializable> rootObj;
			ds->Deserialize(ziparch, ~rootObj, zipFile);

			vdrefptr traceCollection = ATLoadTrace(*rootObj,
				[&](int i, int n) {
					if (n)
						progress.Update(100 + i * 900 / n);
				}
			);

			SetCollection(traceCollection);
			return;
		}
	}
}

void ATUITraceViewer::Save(const wchar_t *path) const {
	if (!mpCollection)
		return;

	ATProgress progress;
	progress.Init(1000, nullptr, L"Saving trace");

	VDFileStream fs(path, nsVDFile::kWrite | nsVDFile::kCreateAlways | nsVDFile::kSequential);
	VDBufferedWriteStream bs(&fs, 65536);
	vdautoptr<IVDZipArchiveWriter> zip(VDCreateZipArchiveWriter(bs));

	{
		vdautoptr<IATSaveStateSerializer> ser(ATCreateSaveStateSerializer(L"trace.json"));
		ser->SetCompressionLevel(VDDeflateCompressionLevel::Quick);

		ser->SetProgressFn(
			[&](int i, int n) {
				if (n)
					progress.Update(900 + i * 100 / n);
			}
		);

		ser->BeginSerialize(*zip);

		vdrefptr<IATSerializable> snapshot = ATSaveTrace(*mpCollection,
			*ser,
			[&](int i, int n) {
				if (n)
					progress.Update(i * 900 / n);
			}
		);

		ser->EndSerialize(*snapshot);
	}

	zip->Finalize();
	bs.Flush();
	fs.close();
}

void ATUITraceViewer::ViewMemoryStatistics() {
	if (!mpCollection)
		return;

	const size_t numGroups = mpCollection->GetGroupCount();

	VDStringW log;

	for(size_t i = 0; i < numGroups; ++i) {
		ATTraceGroup *group = mpCollection->GetGroup(i);
		const wchar_t *groupName = group->GetName();

		const size_t channelCount = group->GetChannelCount();
		for(size_t j = 0; j < channelCount; ++j) {
			IATTraceChannel *channel = group->GetChannel(j);
			const wchar_t *channelName = channel->GetName();

			if (channel->IsEmpty())
				continue;

			const uint64 traceSize = channel->GetTraceSize();
			log.append_sprintf(L"%ls:%ls: %.1fMB", groupName, channelName, (double)traceSize / 1048576.0);

			auto *cpuChannel = vdpoly_cast<ATTraceChannelCPUHistory *>(channel);
			if (cpuChannel) {
				log.append_sprintf(L" (%u insns @ %.2f bytes/insn)", cpuChannel->GetEventCount(), (double)traceSize / (double)cpuChannel->GetEventCount());
			}
			
			auto *videoChannel = vdpoly_cast<IATTraceChannelVideo *>(channel);
			if (videoChannel) {
				log.append_sprintf(L" (%u frame buffers / %u frames)", videoChannel->GetFrameBufferCount(), channel->GetEventCount());
			}

			log += '\n';
		}
	}

	if (!log.empty())
		log.pop_back();

	ShowInfo2(log.c_str(), L"Memory Statistics");
}

void ATUITraceViewer::SetCollection(ATTraceCollection *collection) {
	if (mpCollection != collection) {
		mpCollection = collection;

		mStartTime = 0;
		
		VScrollToPixel(0);
		RebuildViews();
	}
}

void ATUITraceViewer::ScrollDeltaPixels(sint32 dx, sint32 dy) {
	mStartTime -= mSecondsPerPixel*(double)dx;
	mEventView.ScrollDeltaPixels(mStartTime, dx, dy);
	mTimescaleView.ScrollDeltaPixels(mStartTime, dx);

	UpdateHScroll(false);
}

void ATUITraceViewer::ScrollToCenterTime(double t) {
	double viewWidthTime = (double)mEventViewWidth * mSecondsPerPixel;
	double pixelf = t / mSecondsPerPixel - (double)(mEventViewWidth >> 1);
	double oldStartTime = mStartTime;
	double newStartTime = floor(0.5 + pixelf) * mSecondsPerPixel;

	if (mStartTime == newStartTime)
		return;

	mStartTime = newStartTime;

	// Check if we're scrolling by more than a full view. This is more efficient and
	// also avoids overflow problems in computing the pixel delta.
	if (fabs(newStartTime - oldStartTime) >= viewWidthTime) {
		mEventView.ScrollToTime(mStartTime);
		mTimescaleView.ScrollToTime(mStartTime);
	} else {
		sint32 dx = (sint32)floor(0.5 + (oldStartTime - newStartTime) / mSecondsPerPixel);

		mEventView.ScrollDeltaPixels(mStartTime, dx, 0);
		mTimescaleView.ScrollDeltaPixels(mStartTime, dx);
	}

	UpdateHScroll(false);
}

void ATUITraceViewer::ZoomDeltaSteps(double centerTime, sint32 steps) {
	double pixelOffset = (centerTime - mStartTime) / mSecondsPerPixel;

	mZoomLevel -= steps;

	// Enforce some sane limits: 10ns - 1 second per 96dpi pixel
	mZoomLevel = std::max(std::min(mZoomLevel, 0), -40);

	mSecondsPerPixel = pow(10.0, (double)mZoomLevel / 5.0) * 96.0 / (double)mContext.mDpi;
	mStartTime = centerTime - mSecondsPerPixel * pixelOffset;
	
	int zl2 = (mZoomLevel < 0 ? mZoomLevel - 4 : mZoomLevel) / 5;

	double secondsPerDivision = pow(10.0, zl2 + 2);

	mEventView.RescaleView(mStartTime, mSecondsPerPixel);
	mTimescaleView.RescaleView(mStartTime, mSecondsPerPixel, secondsPerDivision);

	UpdateHScroll(true);
}

void ATUITraceViewer::SetFocusTime(double t) {
	mCPUHistoryView.SetCenterTime(t);

	const double viewWidthTime = mSecondsPerPixel * (double)mEventViewWidth;
	if (t < mStartTime || t > mStartTime + viewWidthTime) {
		double idealStartTime = t - mSecondsPerPixel * (double)(mEventViewWidth >> 1);

		if (fabs(idealStartTime - mStartTime) > viewWidthTime) {
			mStartTime = idealStartTime;
			mEventView.ScrollToTime(mStartTime);
			mTimescaleView.ScrollToTime(mStartTime);
		} else {
			double scrollDelta = idealStartTime - mStartTime;
			sint32 dx = (sint32)floor(0.5 + scrollDelta / mSecondsPerPixel);

			ScrollDeltaPixels(-dx, 0);
		}
	}

	mEventView.SetSelection(t, t);
	mLogView.SelectNearestEntryToTime(t);
	UpdateHScroll(false);
}

void ATUITraceViewer::SetSelection(double startTime, double endTime) {
	mCPUProfileView.SetSelectedRange(startTime, endTime);
}

void ATUITraceViewer::SetSelectionMode(bool selMode) {
	if (mContext.mbSelectionMode != selMode) {
		mContext.mbSelectionMode = selMode;
		mEventView.SetSelectionMode(selMode);
		UpdateToolbarSelectionState();
	}
}

void ATUITraceViewer::SetGroupHeightScale(int groupIndex, float scale) {
	if (groupIndex < 0 || (unsigned)groupIndex >= mContext.mGroups.size())
		return;

	ATUITraceViewerGroup *group = mContext.mGroups[groupIndex];

	if (group->mVariableHeightScale != scale) {
		group->mVariableHeightScale = scale;

		UpdateChannelPositions();
	}
}

bool ATUITraceViewer::OnLoaded() {
	mContext.mDpi = mCurrentDpi;

	HWND hwndToolbar = CreateWindow(TOOLBARCLASSNAME, _T(""), WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | WS_BORDER, 0, 0, 0, 0, mhdlg, (HMENU)100, VDGetLocalModuleHandleW32(), NULL);
	if (!hwndToolbar)
		return false;

	AddProxy(&mToolbar, hwndToolbar);
	AddProxy(&mHScrollBar, IDC_HSCROLLBAR);
	AddProxy(&mVScrollBar, IDC_VSCROLLBAR);

	SendMessage(mToolbar.GetHandle(), WM_SETFONT, (WPARAM)mhfont, TRUE);

	RebuildToolbar();
	UpdateFonts();

	mTimescaleView.Create(this);
	mChannelView.Create(this);
	mEventView.Create(this);

	OpenHistory();

	OnSize();

	RebuildViews();

	if (!mSimEventIdTraceLimited) {
		mSimEventIdTraceLimited = g_sim.GetEventManager()->AddEventCallback(kATSimEvent_TracingLimitReached, [this] { if (mbRecording) StartStopTracing(); });
	}

	ATUIRegisterThemeChangeNotification(&mThemeChangedFn);

	return VDDialogFrameW32::OnLoaded();
}

void ATUITraceViewer::OnDestroy() {
	ATUIUnregisterThemeChangeNotification(&mThemeChangedFn);

	mTimescaleView.Destroy();
	mChannelView.Destroy();
	mEventView.Destroy();
	mCPUHistoryView.Destroy();

	mContext.mChannelFont.reset();
	mContext.mTimestampFont.reset();

	ClearGroupViews();

	if (mSimEventIdTraceLimited) {
		g_sim.GetEventManager()->RemoveEventCallback(mSimEventIdTraceLimited);
		mSimEventIdTraceLimited = 0;
	}
}

void ATUITraceViewer::OnSize() {
	const vdrect32& r = GetClientArea();
	vdrect32 rToolbar = mToolbar.GetArea();

	mToolbar.SetArea(vdrect32(0, 0, r.width(), rToolbar.height()));

	const sint32 toolbarH = rToolbar.bottom - rToolbar.top;
	const sint32 timelineSplitY = toolbarH + mTimescaleView.GetIdealHeight();

	const sint32 hscrollH = GetDpiScaledMetric(SM_CYHSCROLL);
	const sint32 eventViewH = std::max<sint32>(0, r.bottom - hscrollH - toolbarH - mSplitterWidth);
	mEventViewHeight = eventViewH;

	// caution -- this uses mEventViewHeight
	const bool needVScrollBar = ShouldVScrollBeVisible();
	const sint32 vscrollW = needVScrollBar ? GetDpiScaledMetric(SM_CXVSCROLL) : 0;

	const sint32 eventViewX1 = mChannelSplitX + mSplitterWidth;
	mEventViewWidth = std::max<sint32>(0, r.right - eventViewX1 - vscrollW);

	const sint32 eventViewX2 = eventViewX1 + mEventViewWidth;
	const sint32 eventViewY2 = toolbarH + mSplitterWidth + eventViewH;

	mTimescaleView.SetArea(vdrect32(mChannelSplitX + mSplitterWidth, toolbarH, eventViewX2, timelineSplitY), false);
	mChannelView.SetArea(vdrect32(0, timelineSplitY + mSplitterWidth, mChannelSplitX, eventViewY2), false);
	mEventView.SetArea(vdrect32(eventViewX1, timelineSplitY + mSplitterWidth, eventViewX2, eventViewY2), false);

	mHScrollBar.SetArea(vdrect32(eventViewX1, eventViewY2, eventViewX1 + mEventViewWidth, eventViewY2 + hscrollH));

	if (needVScrollBar) {
		mVScrollBar.SetArea(vdrect32(eventViewX2, timelineSplitY, eventViewX2 + vscrollW, r.bottom));
		mVScrollBar.Show();

		UpdateVScroll(true);
	} else
		mVScrollBar.Hide();
}

void ATUITraceViewer::OnDpiChanged() {
	mContext.mDpi = mCurrentDpi;

	UpdateFonts();
	OnSize();
	UpdateChannelBaseHeights();
	RebuildToolbar();

	mTimescaleView.UpdateChildDpi();
	mChannelView.UpdateChildDpi();
	mEventView.UpdateChildDpi();

	ZoomDeltaSteps(GetViewCenterTime(), 0);
}

void ATUITraceViewer::RebuildViews() {
	vdrefptr<IATTraceChannel> frameChannel;
	vdrefptr<ATTraceChannelCPUHistory> cpuHistoryChannel;
	vdrefptr<ATTraceChannelTickBased> logChannel;

	ClearGroupViews();

	mTraceDuration = 0;

	if (mpCollection) {
		for(size_t i = 0, n = mpCollection->GetGroupCount(); i < n; ++i) {
			ATTraceGroup *group = mpCollection->GetGroup(i);

			if (group->GetChannelCount() == 0)
				continue;

			mTraceDuration = std::max(mTraceDuration, group->GetDuration());

			ATTraceGroupType type = group->GetType();
			if (type == kATTraceGroupType_Frames) {
				frameChannel = group->GetChannel(0);
				continue;
			} else if (type == kATTraceGroupType_CPUHistory) {
				cpuHistoryChannel = vdpoly_cast<ATTraceChannelCPUHistory *>(group->GetChannel(0));
				continue;
			} else if (type == kATTraceGroupType_Log) {
				logChannel = vdpoly_cast<ATTraceChannelTickBased *>(group->GetChannel(0));
			}

			vdautoptr<ATUITraceViewerGroup> viewerGroup;

			const size_t numChannels = group->GetChannelCount();
			for(size_t chIdx = 0; chIdx < numChannels; ++chIdx) {
				IATTraceChannel *channel = group->GetChannel(chIdx);
				if (channel->IsEmpty())
					continue;

				if (!viewerGroup) {
					viewerGroup = new ATUITraceViewerGroup;
					viewerGroup->mName = group->GetName();
					viewerGroup->mpGroup = group;

					if (type == kATTraceGroupType_Video)
						viewerGroup->mbVariableHeight = true;
				}

				vdautoptr<ATUITraceViewerChannel> viewerChannel { new ATUITraceViewerChannel };

				viewerChannel->mPosY = 0;
				viewerChannel->mHeight = 0;
				viewerChannel->mpChannel = channel;
				viewerChannel->mpGroup = viewerGroup;
				viewerChannel->mType
					= (type == kATTraceGroupType_Video) ? ATUITraceViewerChannel::kType_Video
					: (type == kATTraceGroupType_Tape) ? ATUITraceViewerChannel::kType_Tape
					: (type == kATTraceGroupType_Log) ? ATUITraceViewerChannel::kType_Log
					: ATUITraceViewerChannel::kType_Default;

				if (viewerChannel->mType == ATUITraceViewerChannel::kType_Video) {
					IATTraceChannelVideo *videoChannel = vdpoly_cast<IATTraceChannelVideo *>(channel);
					if (videoChannel) {
						uint32 n = channel->GetEventCount();

						if (n > 1) {
							viewerChannel->mFrameTime0 = videoChannel->GetTimeForFrame(0);
							viewerChannel->mAvgSecondsPerFrame = (videoChannel->GetTimeForFrame(n - 1) - videoChannel->GetTimeForFrame(0)) / (double)(n - 1);
						}
					}
				}

				viewerGroup->mChannels.push_back(viewerChannel);
				viewerChannel.release();
			}

			if (viewerGroup) {
				mContext.mGroups.push_back(viewerGroup);
				viewerGroup.release();
			}
		}
	}

	mContext.mpCPUHistoryChannel = cpuHistoryChannel;

	mEventView.SetFrameChannel(frameChannel);

	if (cpuHistoryChannel)
		mTimestampDecoder = cpuHistoryChannel->GetTimestampDecoder();
	else
		mTimestampDecoder = g_sim.GetTimestampDecoder();

	mCPUHistoryView.SetCPUTraceChannel(cpuHistoryChannel, mTimestampDecoder);
	mCPUProfileView.SetCPUTraceChannel(cpuHistoryChannel, mTimestampDecoder);
	mLogView.SetLogTraceChannel(logChannel, mTimestampDecoder);

	UpdateChannelBaseHeights();

	ZoomDeltaSteps(0, mZoomLevel + 15);
}

void ATUITraceViewer::RebuildToolbar() {
	mToolbar.Clear();

	{
		VDPixmapBuffer pximg;
		if (ATLoadImageResource(IDB_TOOLBAR_TRACEVIEWER, pximg)) {
			const sint32 iconSize = (GetDpiScaledMetric(SM_CXSMICON) + GetDpiScaledMetric(SM_CXICON)) / 2;

			RecolorToolstripImage(pximg, { 0, 4, 5, 6 });

			const uint32 n = pximg.w / pximg.h;
			mToolbar.InitImageList(n, iconSize, iconSize);
			mToolbar.AddImages(n, pximg);
		}
	}

	mToolbar.AddButton(kToolbarId_StartStop, 1, L"Start");
	mToolbar.AddButton(kToolbarId_ZoomIn, 3, L"Zoom In");
	mToolbar.AddButton(kToolbarId_ZoomOut, 2, L"Zoom Out");
	mToolbar.AddButton(kToolbarId_Settings, 4, L"Settings");
	mToolbar.AddSeparator();
	mToolbar.AddButton(kToolbarId_Select, 5, L"Select");
	mToolbar.AddButton(kToolbarId_Move, 6, L"Move");
	mToolbar.AddButton(kToolbarId_Profile, 7, L"Profile");
	mToolbar.AddButton(kToolbarId_History, 8, L"CPU History");
	mToolbar.AddButton(kToolbarId_Log, 9, L"Log");

	mToolbar.AutoSize();

	UpdateToolbarSelectionState();
}

void ATUITraceViewer::UpdateToolbarSelectionState() {
	mToolbar.SetItemPressed(kToolbarId_Select, mContext.mbSelectionMode);
	mToolbar.SetItemPressed(kToolbarId_Move, !mContext.mbSelectionMode);
}

void ATUITraceViewer::UpdateFonts() {
	mContext.mChannelFont.reset(CreateFontW((12 * mContext.mDpi + 48) / 96, 0, 0, 0, 0, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE | DEFAULT_PITCH, L"MS Shell Dlg 2"));
	mContext.mTimestampFont = mContext.mChannelFont;
	mContext.mEventFont = mContext.mChannelFont;

	mContext.mChannelFontMetrics = {};

	if (HDC hdc = GetDC(mhdlg)) {
		HGDIOBJ hOldFont = SelectObject(hdc, mContext.mChannelFont.get());
		if (hOldFont) {
			GetTextMetricsW(hdc, &mContext.mChannelFontMetrics);
			SelectObject(hdc, hOldFont);
		}
		ReleaseDC(mhdlg, hdc);
	}

	mContext.mTimestampFontMetrics = mContext.mChannelFontMetrics;
	mContext.mEventFontMetrics = mContext.mChannelFontMetrics;
}

void ATUITraceViewer::ClearGroupViews() {
	while(!mContext.mGroups.empty()) {
		auto *p = mContext.mGroups.back();
		mContext.mGroups.pop_back();

		for(auto *ch : p->mChannels)
			delete ch;

		delete p;
	}

	mContext.mpCPUHistoryChannel = nullptr;
}

void ATUITraceViewer::UpdateChannelBaseHeights() {
	const sint32 channelHeight = mContext.mEventFontMetrics.tmHeight + (8 * mContext.mDpi + 48) / 96;
	const sint32 tapeChannelHeight = (50 * mContext.mDpi + 48) / 96;

	for (auto *group : mContext.mGroups) {
		sint32 groupY = 0;

		for (auto *ch : group->mChannels) {
			ch->mPosY = groupY;

			switch(ch->mType) {
				case ATUITraceViewerChannel::kType_Default:
				default:
					ch->mBaseHeight = channelHeight;
					break;

				case ATUITraceViewerChannel::kType_Video:
					{
						sint32 videoHeight = 128;

						if (auto *videoCh = vdpoly_cast<IATTraceChannelVideo *>(ch->mpChannel)) {
							uint32 n = videoCh->GetFrameBufferCount();

							if (n > 0) {
								videoHeight = 16;

								for(uint32 i = 0; i < n; ++i) {
									auto sz = videoCh->GetFrameSizeByIndex(i);

									videoHeight = std::max<sint32>(videoHeight, sz.h);
								}
							}

							// clamp the assessed height in case we have some really huge frames
							if (videoHeight > 512)
								videoHeight = 512;
						}

						// Originally, the video channel was a fixed 100 pixels tall for a 128 pixel frame.
						const sint32 videoChannelHeight = VDRoundToInt32((float)videoHeight * (100.0f / 128.0f) * (float)mContext.mDpi / 96.0f);
						ch->mBaseHeight = videoChannelHeight;
					}
					break;

				case ATUITraceViewerChannel::kType_Tape:
					ch->mBaseHeight = tapeChannelHeight;
					break;
			}

			groupY += ch->mBaseHeight;
		}

		group->mBaseHeight = std::max<sint32>(groupY, 50);
	}

	UpdateChannelPositions();
}

void ATUITraceViewer::UpdateChannelPositions() {
	sint32 posY = 0;

	for (auto *group : mContext.mGroups) {
		group->mPosY = posY;

		if (group->mbVariableHeight) {
			group->mHeight = std::max<int>(50, VDRoundToInt32((float)group->mBaseHeight * group->mVariableHeightScale));

			sint32 groupY = 0;
			for (auto *ch : group->mChannels) {
				ch->mPosY = groupY;
				ch->mHeight = group->mHeight;
				groupY += ch->mHeight;
			}
		} else {
			group->mHeight = group->mBaseHeight;

			sint32 groupY = 0;
			for (auto *ch : group->mChannels) {
				ch->mPosY = groupY;
				ch->mHeight = ch->mBaseHeight;
				groupY += ch->mHeight;
			}
		}

		posY += group->mHeight;
	}

	// update the all group height, and update the vertical scroll bar if
	// needed
	if (mAllGroupHeight != posY) {
		mAllGroupHeight = posY;

		UpdateVScroll(true);
	}

	// repaint the channel and event views
	mChannelView.OnChannelsChanged();
	mEventView.Invalidate();
}

void ATUITraceViewer::VScrollToPixel(sint32 pos) {
	if (pos < 0)
		pos = 0;

	if (mScrollY != pos) {
		const sint32 origPos = mScrollY;
		const sint32 delta = pos - mScrollY;
		mScrollY = pos;

		mEventView.VScrollToPixel(pos);
		mChannelView.VScrollToPixel(pos);

		// if we scrolled up when scrolled past the normal bottom, recalc
		// the scroll bar range to shrink or hide it if necessary
		if (delta < 0 && origPos + mEventViewHeight > mAllGroupHeight)
			UpdateVScroll(true);
	}
}

void ATUITraceViewer::UpdateHScroll(bool updateRange) {
	if (!mHScrollBar.IsValid())
		return;

	if (updateRange) {
		double ticks = mTraceDuration / mSecondsPerPixel;

		mSecondsPerHScrollTick = mSecondsPerPixel;
		mPixelsPerHScrollTick = 1;

		while(ticks > (double)0x1FFFFFFF) {
			ticks *= 0.5;
			mSecondsPerHScrollTick += mSecondsPerHScrollTick;
			mPixelsPerHScrollTick += mPixelsPerHScrollTick;
		}
	}

	const sint32 newValue = (sint32)(0.5 + (mStartTime + (double)mEventViewWidth * 0.5 * mSecondsPerPixel) / mSecondsPerHScrollTick);

	if (updateRange) {
		sint32 newPageSize = std::max(1, (mEventViewWidth + mPixelsPerHScrollTick - 1) / mPixelsPerHScrollTick);
		auto newRange = std::make_pair<sint32, sint32>(
			0,
			(sint32)ceil(mTraceDuration / mSecondsPerHScrollTick) + newPageSize - 1
		);

		mHScrollBar.SetParams(
			newValue,
			newRange,
			newPageSize
		);
	} else {
		mHScrollBar.SetValue(newValue);
	}
}

void ATUITraceViewer::UpdateVScroll(bool updateRange) {
	if (!mVScrollBar.IsValid())
		return;

	const sint32 newValue = mScrollY;

	if (updateRange) {
		const bool shouldBeVisible = ShouldVScrollBeVisible();

		if (shouldBeVisible) {
			// We may be scrolled beyond the normal area, due to the channel stack being shrunk
			// or the window enlarged. Temporarily pretend the channel stack is taller to keep
			// the current scroll position valid; it will be shrunk if the view is scrolled back
			// up.
			const sint32 canvasHeight = std::max<sint32>(mAllGroupHeight, mEventViewHeight + mScrollY);

			if (mEventViewHeight && mEventViewHeight < canvasHeight) {
				mVScrollBar.SetParams(
					newValue,
					std::make_pair((sint32)0, canvasHeight),
					std::max<sint32>(1, mEventViewHeight)
				);
			}
		}

		if (mVScrollBar.IsVisible() != shouldBeVisible)
			OnSize();
	} else {
		mVScrollBar.SetValue(newValue);
	}
}

bool ATUITraceViewer::ShouldVScrollBeVisible() const {
	return mScrollY > 0 || mAllGroupHeight > mEventViewHeight;
}

void ATUITraceViewer::UpdateTraceButton() {
	mToolbar.SetItemText(kToolbarId_StartStop, mbRecording ? L"Stop" : L"Start");
	mToolbar.SetItemImage(kToolbarId_StartStop, mbRecording ? 0 : 1);
}

void ATUITraceViewer::OnToolbarClicked(uint32 id) {
	switch(id) {
		case kToolbarId_StartStop:
			StartStopTracing();
			break;

		case kToolbarId_ZoomIn:
			ZoomIn();
			break;

		case kToolbarId_ZoomOut:
			ZoomOut();
			break;

		case kToolbarId_Settings:
			{
				ATUIDialogTraceSettings dlg(mSettings);

				if (dlg.ShowDialog(this))
					ATTraceSaveDefaults(mSettings);
			}
			break;

		case kToolbarId_Select:
			SetSelectionMode(true);
			break;

		case kToolbarId_Move:
			SetSelectionMode(false);
			break;

		case kToolbarId_Profile:
			OpenProfile();
			break;

		case kToolbarId_History:
			OpenHistory();
			break;

		case kToolbarId_Log:
			OpenLog();
			break;
	}
}

void ATUITraceViewer::OnLogTimestampModeChanged(ATUITraceViewerLogView::TimestampMode mode) {
	using TimestampMode = ATUITraceViewerLogView::TimestampMode;

	switch(mode) {
		case TimestampMode::BeamPosition:
			mCPUHistoryView.SetTimestampMode(ATUIHistoryTimestampMode::Beam);
			break;

		case TimestampMode::Cycle:
			mCPUHistoryView.SetTimestampMode(ATUIHistoryTimestampMode::Cycles);
			break;

		case TimestampMode::Microseconds:
			mCPUHistoryView.SetTimestampMode(ATUIHistoryTimestampMode::Microseconds);
			break;
	}
}

void ATUITraceViewer::OnLogTimestampOriginChanged(double t) {
	mCPUHistoryView.SetTimestampOrigin(t);
}

void ATUITraceViewer::OnHistoryTimestampModeChanged(ATUIHistoryTimestampMode mode) {
	switch(mode) {
		case ATUIHistoryTimestampMode::Beam:
			mLogView.SetTimestampMode(ATUITraceViewerLogView::TimestampMode::BeamPosition, false);
			break;

		case ATUIHistoryTimestampMode::Microseconds:
			mLogView.SetTimestampMode(ATUITraceViewerLogView::TimestampMode::Microseconds, false);
			break;

		case ATUIHistoryTimestampMode::Cycles:
			mLogView.SetTimestampMode(ATUITraceViewerLogView::TimestampMode::Cycle, false);
			break;

		case ATUIHistoryTimestampMode::UnhaltedCycles:
		case ATUIHistoryTimestampMode::TapePositionSamples:
		case ATUIHistoryTimestampMode::TapePositionSeconds:
			break;
	}
}

void ATUITraceViewer::OnHistoryTimestampOriginChanged(double t) {
	mLogView.SetTimestampOriginTime(t, false);
}

void ATUITraceViewer::OpenHistory() {
	if (mCPUHistoryView.IsCreated()) {
		ATFrameWindow *frame = ATFrameWindow::GetFrameWindowFromContent(mCPUHistoryView.GetWindowHandle());

		if (frame)
			frame->ActivateFrame();
	} else {
		ATFrameWindow *frame = ATFrameWindow::GetFrameWindow(GetParent(mhdlg));
		if (frame) {
			ATContainerWindow *container = frame->GetContainer();
			vdrefptr<ATFrameWindow> frame2(new ATFrameWindow(container));

			frame2->Create(L"CPU History", CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, (VDGUIHandle)container->GetHandleW32());
			frame2->SetVisible(true);
			container->DockFrame(frame2, kATContainerDockRight);

			mCPUHistoryView.Create((VDGUIHandle)frame2->GetHandleW32());
		}
	}
}

void ATUITraceViewer::OpenProfile() {
	if (mCPUProfileView.IsCreated()) {
		ATFrameWindow *frame = ATFrameWindow::GetFrameWindowFromContent(mCPUProfileView.GetWindowHandle());

		if (frame)
			frame->ActivateFrame();
	} else {
		ATFrameWindow *frame = ATFrameWindow::GetFrameWindow(GetParent(mhdlg));
		if (frame) {
			ATContainerWindow *container = frame->GetContainer();
			vdrefptr<ATFrameWindow> frame2(new ATFrameWindow(container));

			frame2->Create(L"CPU Profile", CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, (VDGUIHandle)container->GetHandleW32());
			frame2->SetVisible(true);
			container->DockFrame(frame2, kATContainerDockBottom);

			mCPUProfileView.Create((VDGUIHandle)frame2->GetHandleW32());
		}
	}
}

void ATUITraceViewer::OpenLog() {
	if (mLogView.IsCreated()) {
		ATFrameWindow *frame = ATFrameWindow::GetFrameWindowFromContent(mLogView.GetWindowHandle());

		if (frame)
			frame->ActivateFrame();
	} else {
		ATFrameWindow *frame = ATFrameWindow::GetFrameWindow(GetParent(mhdlg));
		if (frame) {
			ATContainerWindow *container = frame->GetContainer();
			vdrefptr<ATFrameWindow> frame2(new ATFrameWindow(container));

			frame2->Create(L"Log", CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, (VDGUIHandle)container->GetHandleW32());
			frame2->SetVisible(true);
			container->DockFrame(frame2, kATContainerDockBottom);

			mLogView.Create((VDGUIHandle)frame2->GetHandleW32());
		}
	}
}

///////////////////////////////////////////////////////////////////////////

class ATUIPerformanceAnalyzerContainerWindow : public ATContainerWindow, public IATUITraceViewerHost {
public:
	void SetBaseCaption(const wchar_t *caption);

	void SetTraceViewer(ATUITraceViewer *viewer) override;
	void ClearLoadedTraceName() override;
	void SetLoadedTraceNameCaptured() override;

private:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam) override;

	bool OnCreate() override;
	void OnDestroy() override;
	void OnInitMenu(HMENU hmenu);
	bool OnCommand(UINT cmd);

private:
	void DoLoad();
	void DoSave();
	void DoStartNativeTrace();
	void DoImportAtari800();
	void UpdateCaption();

	ATUITraceViewer *mpTraceViewer = nullptr;
	HMENU mhmenu = nullptr;
	VDStringW mBaseCaption;
	VDStringW mLoadedTraceName;
};

void ATUIPerformanceAnalyzerContainerWindow::SetBaseCaption(const wchar_t *caption) {
	mBaseCaption = caption;
	UpdateCaption();
}

void ATUIPerformanceAnalyzerContainerWindow::ClearLoadedTraceName() {
	if (!mLoadedTraceName.empty()) {
		mLoadedTraceName.clear();
		UpdateCaption();
	}
}

void ATUIPerformanceAnalyzerContainerWindow::SetLoadedTraceNameCaptured() {
	static unsigned sTraceNumber = 0;

	mLoadedTraceName.sprintf(L"Captured Trace %u", ++sTraceNumber);
	UpdateCaption();
}

void ATUIPerformanceAnalyzerContainerWindow::SetTraceViewer(ATUITraceViewer *viewer) {
	mpTraceViewer = viewer;
}

LRESULT ATUIPerformanceAnalyzerContainerWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITMENU:
			OnInitMenu((HMENU)wParam);
			return 0;

		case WM_COMMAND:
			if (OnCommand((UINT)wParam))
				return 0;
			break;
	}

	return ATContainerWindow::WndProc(msg, wParam, lParam);
}

bool ATUIPerformanceAnalyzerContainerWindow::OnCreate() {
	ATUIRegisterModelessWindow(mhwnd);

	if (!mhmenu) {
		mhmenu = LoadMenu(VDGetLocalModuleHandleW32(), MAKEINTRESOURCE(IDR_PERFANALYZER_MENU));

		if (mhmenu)
			SetMenu(mhwnd, mhmenu);
	}

	return ATContainerWindow::OnCreate();
}
	
void ATUIPerformanceAnalyzerContainerWindow::OnDestroy() {
	if (mhmenu) {
		SetMenu(mhwnd, nullptr);
		DestroyMenu(mhmenu);
		mhmenu = nullptr;
	}

	ATUIUnregisterModelessWindow(mhwnd);

	ATContainerWindow::OnDestroy();
}

void ATUIPerformanceAnalyzerContainerWindow::OnInitMenu(HMENU hmenu) {
	VDEnableMenuItemByCommandW32(hmenu, ID_EXPORT_CHROMETRACE, mpTraceViewer && mpTraceViewer->CanExport());
}

bool ATUIPerformanceAnalyzerContainerWindow::OnCommand(UINT cmd) {
	try {
		switch(cmd) {
			case ID_FILE_LOAD:
				DoLoad();
				return true;

			case ID_FILE_SAVE:
				DoSave();
				return true;

			case ID_IMPORT_ATARI800:
				DoImportAtari800();
				return true;

			case ID_EXPORT_CHROMETRACE:
				if (mpTraceViewer) {
					const VDStringW& path = VDGetSaveFileName(VDMAKEFOURCC('c', 't', 'r', 'c'), (VDGUIHandle)mhwnd, L"Export Chrome Trace Format", L"Chrome Trace (*.json)\0*.json\0", L"json");

					if (!path.empty())
						mpTraceViewer->ExportToChromeTrace(path.c_str());
				}
				return true;

			case ID_TRACE_STARTNATIVETRACE:
				DoStartNativeTrace();
				return true;

			case ID_TOOLS_VIEWMEMORYSTATISTICS:
				if (mpTraceViewer)
					mpTraceViewer->ViewMemoryStatistics();
				return true;
		}
	} catch(const MyError& e) {
		ATUIShowError((VDGUIHandle)mhdlg, e);
	}

	return false;
}

void ATUIPerformanceAnalyzerContainerWindow::DoLoad() {
	if (!mpTraceViewer)
		return;

	const VDStringW& path = VDGetLoadFileName(VDMAKEFOURCC('t', 'r', 'c', 'e'), (VDGUIHandle)mhwnd, L"Load Trace", L"Altirra Trace (*.attrace)\0*.attrace\0", L"attrace");
	if (path.empty())
		return;

	mpTraceViewer->Load(path.c_str());
	mLoadedTraceName = VDFileSplitPath(path.c_str());
	UpdateCaption();
}

void ATUIPerformanceAnalyzerContainerWindow::DoSave() {
	if (!mpTraceViewer || !mpTraceViewer->CanExport())
		return;

	const VDStringW& path = VDGetSaveFileName(VDMAKEFOURCC('t', 'r', 'c', 'e'), (VDGUIHandle)mhwnd, L"Save Trace", L"Altirra Trace (*.attrace)\0*.attrace\0", L"attrace");
	if (path.empty())
		return;

	mpTraceViewer->Save(path.c_str());
	mLoadedTraceName = VDFileSplitPath(path.c_str());
	UpdateCaption();
}

void ATUIPerformanceAnalyzerContainerWindow::DoStartNativeTrace() {
	mpTraceViewer->StartNativeTrace();
}

void ATUIPerformanceAnalyzerContainerWindow::DoImportAtari800() {
	if (!mpTraceViewer)
		return;

	const VDStringW& path = VDGetLoadFileName(VDMAKEFOURCC('t', 'r', 'c', 'e'), (VDGUIHandle)mhwnd, L"Import Atari800WinPLus 4.0 / Atari800 5.x Monitor Trace", L"Atari800 Trace\0*.*\0", nullptr);
	if (path.empty())
		return;

	mpTraceViewer->ImportA800(path.c_str());
	mLoadedTraceName = VDFileSplitPath(path.c_str());
	UpdateCaption();
}

void ATUIPerformanceAnalyzerContainerWindow::UpdateCaption() {
	VDStringW caption;

	caption = mBaseCaption;

	if (!mLoadedTraceName.empty()) {
		caption += L" - ";
		caption += mLoadedTraceName;
	}

	SetCaption(caption.c_str());
}

///////////////////////////////////////////////////////////////////////////

void ATUIOpenTraceViewer(VDGUIHandle h, ATTraceCollection *collection) {
	vdrefptr<ATUIPerformanceAnalyzerContainerWindow> container { new ATUIPerformanceAnalyzerContainerWindow };

	if (container->Create(CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, true)) {
		container->SetBaseCaption(L"Altirra Performance Analyzer");

		vdrefptr<ATFrameWindow> frame(new ATFrameWindow(container));

		frame->Create(L"Trace", CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, (VDGUIHandle)container->GetHandleW32());

		container->DockFrame(frame, kATContainerDockCenter);

		ATUITraceViewer *p = new ATUITraceViewer(collection, *container);
		if (p->Create((VDGUIHandle)frame->GetHandleW32())) {
			RECT r;
			if (GetClientRect(frame->GetHandleW32(), &r)) {
				p->SetArea(vdrect32(0, 0, r.right, r.bottom), false);
			}
		} else {
			delete p;
		}
	}
}
