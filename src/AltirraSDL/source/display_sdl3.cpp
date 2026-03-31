//	Altirra SDL3 frontend - IVDVideoDisplay implementation

#include <stdafx.h>
#include <vd2/system/atomic.h>
#include <vd2/VDDisplay/display.h>
#include "display_sdl3_impl.h"

// ============================================================
// VDVideoDisplayFrame implementation (from VDDisplay library)
// ============================================================

VDVideoDisplayFrame::VDVideoDisplayFrame() : mRefCount(0) {}
VDVideoDisplayFrame::~VDVideoDisplayFrame() {}

int VDVideoDisplayFrame::AddRef() {
	return mRefCount.inc();
}

int VDVideoDisplayFrame::Release() {
	int n = mRefCount.dec();
	if (!n)
		delete this;
	return n;
}

// VDDisplay global settings stubs
void VDDSetBloomV2Settings(const VDDBloomV2Settings&) {}

VDVideoDisplaySDL3::VDVideoDisplaySDL3(SDL_Renderer *renderer, int w, int h)
	: mpRenderer(renderer)
	, mWidth(w)
	, mHeight(h)
{
}

VDVideoDisplaySDL3::~VDVideoDisplaySDL3() {
	FlushBuffers();
	if (mpTexture)
		SDL_DestroyTexture(mpTexture);
}

void VDVideoDisplaySDL3::Destroy() {
	delete this;
}

void VDVideoDisplaySDL3::Reset() {
	FlushBuffers();
}

void VDVideoDisplaySDL3::PostBuffer(VDVideoDisplayFrame *frame) {
	if (!frame) return;

	if (mPendingFrame) {
		if (mPrevFrame)
			mPrevFrame->Release();
		mPrevFrame = mPendingFrame;
	}

	frame->AddRef();
	mPendingFrame = frame;
}

bool VDVideoDisplaySDL3::RevokeBuffer(bool allowFrameSkip, VDVideoDisplayFrame **ppFrame) {
	if (mPrevFrame) {
		*ppFrame = mPrevFrame;
		mPrevFrame = nullptr;
		return true;
	}
	if (allowFrameSkip && mPendingFrame) {
		*ppFrame = mPendingFrame;
		mPendingFrame = nullptr;
		return true;
	}
	return false;
}

void VDVideoDisplaySDL3::FlushBuffers() {
	if (mPendingFrame) { mPendingFrame->Release(); mPendingFrame = nullptr; }
	if (mPrevFrame)    { mPrevFrame->Release();    mPrevFrame    = nullptr; }
}

void VDVideoDisplaySDL3::Present() {
	if (!mPendingFrame) return;

	const VDPixmap& px = mPendingFrame->mPixmap;
	if (!px.data || !px.w || !px.h) return;

	if (!mpTexture || mTextureW != px.w || mTextureH != px.h) {
		if (mpTexture)
			SDL_DestroyTexture(mpTexture);
		mpTexture = SDL_CreateTexture(mpRenderer,
			SDL_PIXELFORMAT_XRGB8888,
			SDL_TEXTUREACCESS_STREAMING,
			px.w, px.h);
		mTextureW = px.w;
		mTextureH = px.h;

		// Resize conversion buffer for palettized frames
		mConvertBuffer.resize((size_t)px.w * px.h);
	}

	if (!mpTexture) return;

	const void *srcData = px.data;
	int srcPitch = (int)px.pitch;

	// GTIA outputs Pal8 (palettized 8-bit) — convert to XRGB8888
	if (px.format == nsVDPixmap::kPixFormat_Pal8 && px.palette) {
		const uint32 *pal = px.palette;
		uint32 *dst = mConvertBuffer.data();

		for (int y = 0; y < px.h; y++) {
			const uint8 *src = (const uint8 *)px.data + y * px.pitch;
			uint32 *dstRow = dst + y * px.w;
			for (int x = 0; x < px.w; x++)
				dstRow[x] = pal[src[x]] | 0xFF000000u;  // Force alpha to opaque
		}

		srcData = dst;
		srcPitch = px.w * 4;
	}

	SDL_UpdateTexture(mpTexture, nullptr, srcData, srcPitch);
	SDL_RenderClear(mpRenderer);
	SDL_RenderTexture(mpRenderer, mpTexture, nullptr, nullptr);
	SDL_RenderPresent(mpRenderer);

	mPendingFrame->Release();
	mPendingFrame = nullptr;
}
