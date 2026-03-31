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

#ifndef f_AT_ATAUDIO_AUDIOCONVOLUTIONPLAYER_H
#define f_AT_ATAUDIO_AUDIOCONVOLUTIONPLAYER_H

#include <at/atcore/audiomixer.h>
#include <at/atcore/fft.h>

class ATAudioSamplePlayer;

///////////////////////////////////////////////////////////////////////////
// Audio convolution output
//
// Common output for all convolution players on a mix bus. The individual
// players have a precomputed forward FFT for the sound sample and a
// runtime computed forward FFT of the playback impulses for that sample;
// the dot products of the per-sample FFTs are added together for a final
// shared inverse FFT for the output sent to the mixer.
//
class ATAudioConvolutionOutput {
public:
	static constexpr int kConvSize = 4096;
	static constexpr int kMaxFrameSize = 1536;
	static constexpr int kMaxSampleSize = kConvSize - kMaxFrameSize;
	static constexpr float kFFTScale = (float)kConvSize / 2;

	void PreTransformSample(float *sample);
	void AccumulateImpulses(const float *impulseFrame, const float *sampleXform);
	bool Commit(float *dstL, float *dstR, uint32 len);

	uint32 mBaseOffset = 0;
	uint32 mOverlapSamples = 0;
	bool mbHasOutput = false;

	// we optimize for efficiency, since we'll only be executing for a tiny
	// fraction of the frame and don't want to take the AVX clocking hit
	ATFFT<kConvSize> mFFT { false };

	alignas(16) float mXformBuffer[kConvSize] {};
	alignas(16) float mAccumBuffer[kConvSize] {};
	alignas(16) float mOverlapBuffer[kConvSize] {};
};

///////////////////////////////////////////////////////////////////////////

class ATAudioConvolutionPlayer final : public IATSyncAudioConvolutionPlayer {
public:
	ATAudioConvolutionPlayer(ATAudioSampleId sampleId) : mSampleId(sampleId) {}

	void Init(ATAudioSamplePlayer& parent, ATAudioConvolutionOutput& output, const sint16 *sample, uint32 len, uint32 baseTime);
	void Shutdown();

	ATAudioSampleId GetSampleId() const { return mSampleId; }
	void CommitFrame(uint32 nextTime);
	
	int AddRef() override;
	int Release() override;

	void Play(uint32 t, float volume) override;

private:
	const ATAudioSampleId mSampleId;
	int mRefCount = 0;
	uint32 mBaseTime = 0;
	bool mbHasImpulse = false;
	ATAudioSamplePlayer *mpParent = nullptr;
	ATAudioConvolutionOutput *mpOutput = nullptr;

	alignas(16) float mSampleBuffer[ATAudioConvolutionOutput::kConvSize] {};
	alignas(16) float mImpulseBuffer[ATAudioConvolutionOutput::kConvSize] {};
};

#endif
