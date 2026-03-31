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
//
//	As a special exception, this library can also be redistributed and/or
//	modified under an alternate license. See COPYING.RMT in the same source
//	archive for details.

#include <stdafx.h>
#include <at/ataudio/audioconvolutionplayer.h>
#include <at/ataudio/audiosampleplayer.h>

void ATAudioConvolutionOutput::PreTransformSample(float *sample) {
	mFFT.Forward(sample);

	constexpr float scale = 1.0f / (32767.0f * kFFTScale);
	
	for(int i=0; i<kConvSize; ++i)
		sample[i] *= scale;
}

void ATAudioConvolutionOutput::AccumulateImpulses(const float *impulseFrame, const float *sampleXform) {
	// convert impulse train to frequency domain
	mFFT.Forward(mXformBuffer, impulseFrame);

	// multiply spectra of impulse train and sound sample (convolving the impulse
	// train by the sound sample in the time domain)
	mFFT.MultiplyAdd(mAccumBuffer, mXformBuffer, sampleXform);

	// mark that we have output to accumulate in the overlap buffer
	mbHasOutput = true;
}

bool ATAudioConvolutionOutput::Commit(float *dstL, float *dstR, uint32 len) {
	const auto accumAndZero = [](float *VDRESTRICT dst, float *VDRESTRICT src, size_t n) {
		for(size_t i=0; i<n; ++i) {
			dst[i] += src[i];
			src[i] = 0;
		}
	};

	const auto accum2AndZero = [](float *VDRESTRICT dst1, float *VDRESTRICT dst2, float *VDRESTRICT src, size_t n) {
		for(size_t i=0; i<n; ++i) {
			float v = src[i];
			src[i] = 0;
			dst1[i] += v;
			dst2[i] += v;
		}
	};

	// If we had any impulses to product output this frame, do inverse FFT to convert the
	// output back to time domain and accumulate into overlap buffer, then reset the
	// accumulation buffer back to zeroed in frequency domain.
	if (mbHasOutput) {
		mbHasOutput = false;

		mFFT.Inverse(mAccumBuffer);

		accumAndZero(&mOverlapBuffer[mBaseOffset], mAccumBuffer, kConvSize - mBaseOffset);
		accumAndZero(&mOverlapBuffer[0], &mAccumBuffer[kConvSize - mBaseOffset], mBaseOffset);

		// Reset number of output samples to accumulate to full.
		mOverlapSamples = kConvSize;
	}
	
	// If we ran out of output samples because we have no more impulses and used up all the
	// generated output, we're done.
	if (!mOverlapSamples)
		return false;

	// Compute how many output samples we have to mix -- up to this audio frame's
	// worth or however many we have left, whichever is smaller.
	const uint32 alen = std::min(len, mOverlapSamples);

	// Compute split for wrapping around the overlap (source) buffer for the mix.
	const uint32 alen1 = std::min(alen, kConvSize - mBaseOffset);
	const uint32 alen2 = alen - alen1;

	// Accumulate and zero the audio frame's worth of samples.
	if (dstR) {
		accum2AndZero(dstL, dstR, &mOverlapBuffer[mBaseOffset], alen1);
		accum2AndZero(dstL + alen1, dstR + alen1, mOverlapBuffer, alen2);
	} else {
		accumAndZero(dstL, &mOverlapBuffer[mBaseOffset], alen1);
		accumAndZero(dstL + alen1, mOverlapBuffer, alen2);
	}

	// Rotate out the used (and now zeroed) samples.
	mOverlapSamples -= alen;
	mBaseOffset += alen;
	mBaseOffset &= kConvSize - 1;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void ATAudioConvolutionPlayer::Init(ATAudioSamplePlayer& parent, ATAudioConvolutionOutput& output, const sint16 *sample, uint32 len, uint32 baseTime) {
	mpParent = &parent;
	mpOutput = &output;
	mBaseTime = baseTime;

	len = std::min<uint32>(len, ATAudioConvolutionOutput::kMaxSampleSize);

	for(uint32 i=0; i<len; ++i)
		mSampleBuffer[i] = (float)sample[i];

	mpOutput->PreTransformSample(mSampleBuffer);
}

void ATAudioConvolutionPlayer::Shutdown() {
	mpParent = nullptr;
	mpOutput = nullptr;
}

void ATAudioConvolutionPlayer::CommitFrame(uint32 nextTime) {
	if (mbHasImpulse) {
		mbHasImpulse = false;

		if (mpOutput)
			mpOutput->AccumulateImpulses(mImpulseBuffer, mSampleBuffer);

		memset(mImpulseBuffer, 0, sizeof(float) * ATAudioConvolutionOutput::kMaxFrameSize);
	}

	mBaseTime = nextTime;
}

int ATAudioConvolutionPlayer::AddRef() {
	return ++mRefCount;
}

int ATAudioConvolutionPlayer::Release() {
	int rc = --mRefCount;
	if (rc == 1) {
		if (mpParent)
			mpParent->RemoveConvolutionPlayer(*this);
	} else if (rc == 0)
		delete this;

	return rc;
}

void ATAudioConvolutionPlayer::Play(uint32 t, float volume) {
	uint32 tickOffset = t - mBaseTime;

	if (tickOffset >= (ATAudioConvolutionOutput::kMaxFrameSize - 1) * 28)
		return;

	uint32 sampleOffset = tickOffset / 28;
	float subOffset = (float)(tickOffset % 28) / 28.0f;

	mImpulseBuffer[sampleOffset] += volume - volume * subOffset;
	mImpulseBuffer[sampleOffset + 1] += volume * subOffset;
	mbHasImpulse = true;
}
