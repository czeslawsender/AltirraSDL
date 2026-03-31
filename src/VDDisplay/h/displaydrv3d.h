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

#ifndef f_VD2_VDDISPLAY_DISPLAYDRV3D_H
#define f_VD2_VDDISPLAY_DISPLAYDRV3D_H

#include <vd2/system/refcount.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/vdstl.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/VDDisplay/displaydrv.h>
#include <vd2/VDDisplay/renderer.h>
#include <vd2/Tessa/Context.h>
#include "displaynode3d.h"
#include "renderer3d.h"

struct VDPixmap;
class IVDTContext;
class IVDTTexture2D;
class IVDDisplayCustomEffectD3D11;

///////////////////////////////////////////////////////////////////////////

class VDDisplayDriver3D final : public VDVideoDisplayMinidriver, public IVDDisplayCompositionEngine, public IVDTAsyncPresent {
	VDDisplayDriver3D(const VDDisplayDriver3D&) = delete;
	VDDisplayDriver3D& operator=(const VDDisplayDriver3D&) = delete;
public:
	VDDisplayDriver3D();
	~VDDisplayDriver3D();

	virtual bool Init(HWND hwnd, HMONITOR hmonitor, const VDVideoDisplaySourceInfo& info) override;
	virtual void Shutdown() override;

	virtual bool ModifySource(const VDVideoDisplaySourceInfo& info) override;

	virtual void SetFilterMode(FilterMode mode) override;
	virtual void SetFullScreen(bool fullscreen, uint32 w, uint32 h, uint32 refresh, bool use16bit) override;
	virtual void SetDesiredCustomRefreshRate(float hz, float hzmin, float hzmax) override;
	virtual void SetDestRectF(const vdrect32f *r, uint32 color) override;
	virtual void SetPixelSharpness(float xfactor, float yfactor) override;
	virtual bool SetScreenFX(const VDVideoDisplayScreenFXInfo *screenFX) override;

	virtual bool IsValid() override;
	virtual bool IsFramePending() override;
	virtual bool IsScreenFXSupported() const override;
	virtual VDDHighColorAvailability GetHDRAvailability() const override;
	virtual VDDHighColorAvailability GetWCGAvailability() const override;

	virtual bool Resize(int w, int h) override;
	virtual bool Update(UpdateMode) override;
	virtual void Refresh(UpdateMode) override;
	virtual bool Paint(HDC hdc, const RECT& rClient, UpdateMode lastUpdateMode) override;
	virtual void PresentQueued() override;

	virtual VDDVSyncStatus GetVSyncStatus() const override { return mVSyncStatus; }
	virtual bool AreVSyncTicksNeeded() const override { return false; }

	IVDDisplayCompositionEngine *GetDisplayCompositionEngine() override { return this; }

public:
	void LoadCustomEffect(const wchar_t *path) override;

public:
	virtual void QueuePresent(bool restarted) override;
	virtual void OnPresentCompleted(const VDTAsyncPresentStatus& status) override;

private:
	bool CreateSwapChain();
	bool CreateImageNode();
	void DestroyImageNode();
	bool BufferQueuedNode(VDDisplayNode3D *srcNode, uint32 w, uint32 h, bool hdr, VDDisplayQueuedSourceNode3D **ppNode);
	bool BufferQueuedNode(VDDisplayNode3D *srcNode, float outx, float outy, float outw, float outh, uint32 w, uint32 h, bool hdr, VDDisplayQueuedSourceNode3D **ppNode);
	bool BufferNode(VDDisplayNode3D *srcNode, uint32 w, uint32 h, bool hdr, VDDisplaySourceNode3D **ppNode);
	bool BufferNode(VDDisplayNode3D *srcNode, float outx, float outy, float outw, float outh, uint32 w, uint32 h, bool hdr, VDDisplaySourceNode3D **ppNode);
	bool RebuildTree();
	void AdvanceTimingLog();
	void ApplyCustomRefreshRate();

	HWND mhwnd;
	HMONITOR mhMonitor;
	IVDTContext *mpContext;
	IVDTSwapChain *mpSwapChain;
	VDDisplayImageNode3D *mpImageNode;
	VDDisplayImageSourceNode3D *mpImageSourceNode;
	vdrefptr<VDDisplayNode3D> mpRootNode;
	vdrefptr<VDDisplayCustomEffectPipelineNode3D> mpCustomEffectPipelineNode;

	FilterMode mFilterMode;
	bool mbCompositionTreeDirty = false;
	bool mbFramePending = false;
	bool mbFrameVSync = false;
	bool mbFrameVSyncAdaptive = false;

	bool mbRenderLinear = false;
	bool mbHDR = false;
	bool mbUseScreenFX = false;
	VDVideoDisplayScreenFXInfo mScreenFXInfo {};

	bool mbFullScreen;
	uint32 mFullScreenWidth;
	uint32 mFullScreenHeight;
	uint32 mFullScreenRefreshRate;

	float mCustomRefreshRate = 0;
	float mCustomRefreshRateMin = 0;
	float mCustomRefreshRateMax = 0;

	VDVideoDisplaySourceInfo mSource;

	VDTDisplayColorInfo mDisplayColorInfo {};
	bool mbDisplayColorInfoValid = false;

	VDDisplayNodeContext3D mDisplayNodeContext;
	VDDisplayRenderer3D mRenderer;
	IVDDisplayFont *mpDebugFont = nullptr;
	bool mbTraceRendering = false;

	vdrefptr<IVDDisplayCustomEffectD3D11> mpCustomEffect;
	VDStringW mErrorString;
	VDStringW mDebugString;

	VDDVSyncStatus mVSyncStatus;

	struct TimingEntry {
		float mVSyncOffset = 0;
		float mPresentWaitTime = 0;
		float mLastPresentDelay = 0;
		float mLastPresentCallTime = 0;
		float mLastPresentFramesQueued = 0;
		float mEstimatedRefreshPeriod = 0;
		uint64 mSyncTick = 0;
		uint32 mSyncCount = 0;

		TimingEntry& operator+=(const TimingEntry& other) {
			mVSyncOffset += other.mVSyncOffset;
			mPresentWaitTime += other.mPresentWaitTime;
			mLastPresentDelay += other.mLastPresentDelay;
			mLastPresentCallTime += other.mLastPresentCallTime;
			mLastPresentFramesQueued += other.mLastPresentFramesQueued;
			return *this;
		}

		TimingEntry& AccumulateMin(const TimingEntry& other) {
			mVSyncOffset = std::min(mVSyncOffset, other.mVSyncOffset);
			mPresentWaitTime = std::min(mPresentWaitTime, other.mPresentWaitTime);
			mLastPresentDelay = std::min(mLastPresentDelay, other.mLastPresentDelay);
			mLastPresentCallTime = std::min(mLastPresentCallTime, other.mLastPresentCallTime);
			mLastPresentFramesQueued = std::min(mLastPresentFramesQueued, other.mLastPresentFramesQueued);
			return *this;
		}

		TimingEntry& AccumulateMax(const TimingEntry& other) {
			mVSyncOffset = std::max(mVSyncOffset, other.mVSyncOffset);
			mPresentWaitTime = std::max(mPresentWaitTime, other.mPresentWaitTime);
			mLastPresentDelay = std::max(mLastPresentDelay, other.mLastPresentDelay);
			mLastPresentCallTime = std::max(mLastPresentCallTime, other.mLastPresentCallTime);
			mLastPresentFramesQueued = std::max(mLastPresentFramesQueued, other.mLastPresentFramesQueued);
			return *this;
		}

		TimingEntry& operator*=(float scale) {
			mVSyncOffset *= scale;
			mPresentWaitTime *= scale;
			mLastPresentDelay *= scale;
			mLastPresentCallTime *= scale;
			mLastPresentFramesQueued *= scale;
			return *this;
		}
	};

	TimingEntry mTimingLog[15] {};
	TimingEntry mTimingAverage {};
	TimingEntry mTimingMin {};
	TimingEntry mTimingMax {};
	uint32 mTimingIndex = 0;
	uint32 mTimingLogLength = 0;
};

#endif
