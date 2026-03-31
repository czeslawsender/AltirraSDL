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
#include <vd2/system/math.h>
#include <at/ataudio/audioconvolutionplayer.h>
#include <at/ataudio/audiosamplebuffer.h>
#include <at/ataudio/audiosampleplayer.h>
#include <at/ataudio/audiosamplepool.h>
#include <at/atcore/scheduler.h>

////////////////////////////////////////////////////////////////////////////////

int ATAudioSoundGroup::AddRef() {
	VDASSERT(mRefCount >= 0);

	return ++mRefCount;
}

int ATAudioSoundGroup::Release() {
	int rc = --mRefCount;
	VDASSERT(rc >= 0);

	if (!rc) {
		if (mpParent)
			mpParent->CleanupGroup(*this);

		delete this;
	}

	return rc;
}

bool ATAudioSoundGroup::IsAnySoundQueued() const {
	return !mSounds.empty();
}

void ATAudioSoundGroup::StopAllSounds() {
	if (mpParent)
		mpParent->StopGroupSounds(*this);
}

///////////////////////////////////////////////////////////////////////////

void ATAudioSound::Reset() {
	static_cast<ATAudioSoundInfo&>(*this) = {};
}

///////////////////////////////////////////////////////////////////////////

ATAudioSamplePlayer::ATAudioSamplePlayer(ATAudioSamplePool& pool, ATScheduler& sch)
	: mScheduler(sch)
	, mPool(pool)
{
}

ATAudioSamplePlayer::~ATAudioSamplePlayer() {
	Shutdown();
}

void ATAudioSamplePlayer::Init() {
	mLastMixTime = mScheduler.GetTick();
}

void ATAudioSamplePlayer::Shutdown() {
	for(SoundGroup *group : mGroups) {
		group->mpParent = nullptr;
	}

	const auto clearSounds = [this](Sounds& sounds) {
		while(!sounds.empty()) {
			FreeSound(sounds.back());
			sounds.pop_back();
		}
	};

	clearSounds(mReadySounds);
	clearSounds(mPlayingSounds);

	mGroups.clear();

	while(!mConvoPlayers.empty()) {
		mConvoPlayers.back()->Shutdown();
		mConvoPlayers.back()->Release();
		mConvoPlayers.pop_back();
	}
}

void ATAudioSamplePlayer::SetRates(float mixingRate, float pokeyMixingRateDivMixingRate, double outputSamplesPerTick) {
	mMixingRate = mixingRate;
	mPokeyMixingRateDivMixingRate = pokeyMixingRateDivMixingRate;
	mOutputSamplesPerTick = outputSamplesPerTick;
}

vdrefptr<IATAudioSampleHandle> ATAudioSamplePlayer::RegisterSample(vdspan<const sint16> soundData, const ATAudioSoundSamplingRate& samplingRate, float volume) {
	return vdrefptr<IATAudioSampleHandle>(new ATAudioSampleBuffer(soundData, samplingRate, volume));
}

ATSoundId ATAudioSamplePlayer::AddSound(IATAudioSoundGroup& soundGroup, uint32 delay, ATAudioSampleId sampleId, float volume) {
	ATAudioSampleBuffer *sample = mPool.GetStockSample(sampleId);
	if (!sample)
		return ATSoundId::Invalid;

	return AddSound(soundGroup, delay, *sample, ATSoundParams().Volume(volume));
}

ATSoundId ATAudioSamplePlayer::AddLoopingSound(IATAudioSoundGroup& soundGroup, uint32 delay, ATAudioSampleId sampleId, float volume) {
	ATAudioSampleBuffer *sample = mPool.GetStockSample(sampleId);
	if (!sample)
		return ATSoundId::Invalid;

	return AddSound(soundGroup, delay, *sample, ATSoundParams().Volume(volume).Loop());
}

ATSoundId ATAudioSamplePlayer::AddSound(IATAudioSoundGroup& soundGroup, uint32 delay, IATAudioSampleHandle& sample, const ATSoundParams& params) {
	ATAudioSampleBuffer& buffer = static_cast<ATAudioSampleBuffer&>(sample);
	const float playVolume = params.mVolume * buffer.mVolume;

	if (fabsf(playVolume) < 1e-10f)
		return ATSoundId {};

	const uint64 t = mScheduler.GetTick64() + delay;
	Sound *s = mPool.AllocateSound();

	if (params.mbLooping) {
		s->mbLooping = true;
		s->mOffset = ATAudioSampleBuffer::kSampleHeader;
	} else {
		s->mbLooping = false;
		s->mOffset = 0;
	}

	s->mEndTime = 0;
	s->mbEndValid = false;
	s->mLength = buffer.mSampleCount;
	s->mpSampleBuffer = &buffer;

	// Compute panning volumes using equal power law (3dB). Center is 0dB for
	// compatibility with original full-volume mono playback. This means that
	// panned sounds should be manually attenuated.
	float volumeL = cosf((params.mPan + 1.0f) * (nsVDMath::kfPi / 4.0f));
	float volumeR = sqrtf(1.0f - volumeL * volumeL);
	s->mVolumeL = playVolume * volumeL * nsVDMath::kfSqrt2;
	s->mVolumeR = playVolume * volumeR * nsVDMath::kfSqrt2;

	double step = 1.0;

	switch(buffer.mSamplingRate.mUnit) {
		case ATAudioSamplingRateUnit::Hz:
			step = (double)buffer.mSamplingRate.mValue / mMixingRate;
			break;

		case ATAudioSamplingRateUnit::PokeyMixingRate:
			step = (double)buffer.mSamplingRate.mValue * mPokeyMixingRateDivMixingRate;
			break;
	}

	step *= params.mRateScale;

	if (fabs(step - 1.0) < 1e-5) {
		s->mbMatchedRate = true;
		s->mSampleStepF32 = UINT64_C(0x1'0000'0000);
	} else {
		s->mbMatchedRate = false;
		s->mSampleStepF32 = (uint64)(0.5 + step * 0x1p32);

		// Check if the rate is invalid so we don't have overflow problems in
		// the sond mixer. The sound mixer is only able to accommodate
		// up to one loop per sample, and we're waaay beyond useful rates at that
		// point anyway.
		if (s->mSampleStepF32 < 0x100000 || s->mSampleStepF32 >= ((uint64)s->mLength << 32)) {
			FreeSound(s);
			return ATSoundId {};
		}
	}

	return StartSound(s, soundGroup, t);
}

ATSoundId ATAudioSamplePlayer::AddSound(IATAudioSoundGroup& soundGroup, uint32 delay, IATAudioSampleSource *src, IVDRefCount *owner, uint32 len, float volume) {
	const uint64 t = mScheduler.GetTick64() + delay;

	Sound *s = mPool.AllocateSound();
	s->mEndTime = t + kATCyclesPerSyncSample * len;
	s->mLength = len;
	s->mVolumeL = volume;
	s->mVolumeR = volume;
	s->mpSource = src;
	s->mpOwner = owner;
	s->mbMatchedRate = true;
	s->mSampleStepF32 = UINT64_C(0x1'0000'0000);

	s->mbLooping = false;
	s->mbEndValid = true;

	return StartSound(s, soundGroup, t);
}

ATSoundId ATAudioSamplePlayer::AddLoopingSound(IATAudioSoundGroup& soundGroup, uint32 delay, IATAudioSampleSource *src, IVDRefCount *owner, float volume) {
	const uint64 t = mScheduler.GetTick64() + delay;

	Sound *s = mPool.AllocateSound();
	s->mEndTime = t;
	s->mLength = 0;
	s->mVolumeL = volume;
	s->mVolumeR = volume;
	s->mpSource = src;
	s->mpOwner = owner;
	s->mbMatchedRate = true;
	s->mSampleStepF32 = UINT64_C(0x1'0000'0000);

	s->mbLooping = false;
	s->mbEndValid = false;

	return StartSound(s, soundGroup, t);
}

vdrefptr<IATAudioSoundGroup> ATAudioSamplePlayer::CreateGroup(const ATAudioGroupDesc& desc) {
	vdrefptr<IATAudioSoundGroup> group(new SoundGroup(*this));

	static_cast<SoundGroup *>(group.get())->mDesc = desc;
	mGroups.push_back(static_cast<SoundGroup *>(group.get()));

	return group;
}

void ATAudioSamplePlayer::ForceStopSound(ATSoundId id) {
	auto it = FindSoundById(mPlayingSounds, id);
	if (it != mPlayingSounds.end()) {
		FreeSound(*it);

		if (&*it != &mPlayingSounds.back())
			*it = mPlayingSounds.back();

		mPlayingSounds.pop_back();
		return;
	}

	it = FindSoundById(mReadySounds, id);
	if (it != mReadySounds.end()) {
		FreeSound(*it);

		if (&*it != &mReadySounds.back())
			*it = mReadySounds.back();

		mReadySounds.pop_back();
		return;
	}
}

void ATAudioSamplePlayer::StopSound(ATSoundId id) {
	StopSound(id, mScheduler.GetTick64());
}

void ATAudioSamplePlayer::StopSound(ATSoundId id, uint64 time) {
	const auto stopSoundInList = [=, this](Sounds& soundList) -> bool {
		auto it = FindSoundById(soundList, id);

		if (it == soundList.end())
			return false;

		Sound *const s = *it;

		// check if we're killing the sound before it starts
		if (time <= s->mNextTime) {
			*it = soundList.back();
			soundList.pop_back();

			FreeSound(s);
			return true;
		}

		// check if we're trying to kill a one-shot after it would already end
		if (s->mbEndValid && time >= s->mEndTime)
			return true;

		// mark new end time and exit
		s->mEndTime = time;
		s->mbEndValid = true;

		return true;
	};

	if (!stopSoundInList(mReadySounds))
		stopSoundInList(mPlayingSounds);
}

vdrefptr<IATSyncAudioConvolutionPlayer> ATAudioSamplePlayer::CreateConvolutionPlayer(ATAudioSampleId sampleId) {
	const ATAudioSampleBuffer *buffer = mPool.GetStockSample(sampleId);

	if (!buffer)
		return nullptr;

	for(ATAudioConvolutionPlayer *cplayer : mConvoPlayers) {
		if (cplayer->GetSampleId() == sampleId)
			return vdrefptr(cplayer);
	}

	if (!mpConvoOutput)
		mpConvoOutput = new ATAudioConvolutionOutput;

	vdrefptr<ATAudioConvolutionPlayer> cp(new ATAudioConvolutionPlayer(sampleId));

	mConvoPlayers.push_back(cp);
	cp->AddRef();

	cp->Init(*this, *mpConvoOutput, buffer->GetOneShotSampleStart(), buffer->mSampleCount, (uint32)mLastMixTime);

	return cp;
}

vdrefptr<IATSyncAudioConvolutionPlayer> ATAudioSamplePlayer::CreateConvolutionPlayer(const sint16 *sample, uint32 len) {
	if (!mpConvoOutput)
		mpConvoOutput = new ATAudioConvolutionOutput;

	vdrefptr<ATAudioConvolutionPlayer> cp(new ATAudioConvolutionPlayer(kATAudioSampleId_None));

	mConvoPlayers.push_back(cp);
	cp->AddRef();

	cp->Init(*this, *mpConvoOutput, sample, len, (uint32)mLastMixTime);

	return cp;
}

bool ATAudioSamplePlayer::RequiresStereoMixingNow() const {
	return mPannedSoundCount != 0;
}

void ATAudioSamplePlayer::WriteAudio(const ATSyncAudioMixInfo& mixInfo) {
	WriteAsyncAudio(mixInfo);
}

bool ATAudioSamplePlayer::WriteAsyncAudio(const ATAudioAsyncMixInfo& mixInfo) {
	float *dstL = mixInfo.mpLeft;
	float *dstR = mixInfo.mpRight;		// normally null, but the edge player may be asked to do stereo
	uint32 n = mixInfo.mCount;

	const uint64 mixStartTime = mixInfo.mStartTime;
	const uint64 mixEndTime = mixInfo.mStartTime + mixInfo.mNumCycles;
	bool wroteAudio = false;

	// safety check: panned sound count should be zero if sound lists are empty
	if (mPannedSoundCount != 0 && mReadySounds.empty() && mPlayingSounds.empty()) {
		VDFAIL("Panned sound count is incorrect.");

		mPannedSoundCount = 0;
	}

	// process ready sounds
	auto it = mReadySounds.begin(), itEnd = mReadySounds.end();
	while(it != itEnd) {
		Sound *s = *it;

		// check if start time passed
		if (s->mNextTime >= mixEndTime) {
			// start time not passed yet -- skip this sound
			++it;
			continue;
		}

		// check if we're already past end time
		if (!s->mbEndValid || s->mEndTime > mixStartTime) {
			// passed sound start time but not end time -- move to playing list
			mPlayingSounds.push_back(s);
			s = nullptr;
		}

		// remove sound from ready list
		--itEnd;
		if (it != itEnd)
			*it = *itEnd;

		mReadySounds.pop_back();

		if (s)
			FreeSound(s);
	}

	// process currently playing sounds
	it = mPlayingSounds.begin();
	itEnd = mPlayingSounds.end();
	while(it != itEnd) {
		Sound *VDRESTRICT const s = *it;
		bool soundExpired = false;
		
		do {
			// drop sounds that we've already passed
			if (s->mEndTime <= mixStartTime && s->mbEndValid) {
				soundExpired = true;
				break;
			}
				
			// skip if not time to continue sound yet (only happens in rare circumstances)
			if (s->mNextTime >= mixEndTime)
				break;

			// check for the sample starting behind the current window
			const uint32 srcLen = s->mLength;
			uint32 dstOffset = 0;

			VDASSERT(!srcLen || s->mOffset < srcLen);
		
			if (s->mNextTime < mixStartTime) {
				// count the number of samples we need to skip between the sound start and the
				// window start
				const uint32 tickDelay = mixStartTime - s->mNextTime;
				const uint32 dstSampleDelay = TickDeltaToSampleDelta(tickDelay);
				uint64 newSrcOffset = 0;
				uint32 newSrcSubOffset = 0;

				if (s->mbMatchedRate) {
					newSrcOffset = dstSampleDelay + s->mOffset;
					newSrcSubOffset = s->mSubOffset;
				} else {
					uint64 srcSampleDelayLo = dstSampleDelay * (uint64)(uint32)s->mSampleStepF32;
					uint64 srcSampleDelayHi = dstSampleDelay * (uint64)(uint32)(s->mSampleStepF32 >> 32);

					uint64 srcSampleDelay = srcSampleDelayHi + (srcSampleDelayLo >> 32);
					srcSampleDelayLo = (uint32)srcSampleDelayLo;
					srcSampleDelayLo += s->mSubOffset;

					newSrcOffset = srcSampleDelay + (srcSampleDelayLo >> 32) + s->mOffset;
					newSrcSubOffset = (uint32)srcSampleDelayLo;
				}

				VDASSERT(newSrcOffset < srcLen);

				if (newSrcOffset) {
					// if looping is enabled, wrap offset within loop
					if (s->mbLooping)
						newSrcOffset %= s->mLength;

					// if we skipped everything, skip the sound -- this can happen if it only has
					// a fractional sample left
					if (srcLen && newSrcOffset >= srcLen) {
						soundExpired = true;
						continue;
					}
				}

				s->mOffset = (uint32)newSrcOffset;
				s->mSubOffset = (uint32)newSrcSubOffset;
			} else if (s->mNextTime > mixStartTime) {
				// sound starts within window -- set destination offset if it starts after the window
				dstOffset = TickDeltaToSampleDelta(s->mNextTime - mixStartTime);

				if (dstOffset > n)
					dstOffset = n;
			}

			// convert source samples available to output samples
			uint32 len = srcLen;

			if (!len) {
				len = n;
			} else {
				VDASSERT(len > s->mOffset);

				if (!s->mbLooping) {
					len -= s->mOffset;

					if (!s->mbMatchedRate) {
						// compute available source samples in 32.32
						const uint64 limitF32 = (uint64)len << 32;

						// compute how many output samples that can fit within the available source
						// samples
						uint64 len64 = (limitF32 - s->mSubOffset - 1) / s->mSampleStepF32 + 1;

						// at low rates, we may have more than 2^32 source samples available, so make
						// sure not to overflow
						len = len64 > n ? n : (uint64)len64;

						// validate optimality
						VDASSERT(s->mSubOffset + s->mSampleStepF32 * (len - 1) < limitF32);
						VDASSERT(len >= n || s->mSubOffset + s->mSampleStepF32 * len >= limitF32);
					}
				}
			}

			// check if the sound will be truncated due to ending before the window -- note that
			// this may not match the end of a one-shot if the sound has been stopped
			uint32 mixEnd = n;

			if (s->mbEndValid && s->mEndTime < mixEndTime) {
				mixEnd = TickDeltaToSampleDelta(s->mEndTime - mixStartTime);

				if (mixEnd <= dstOffset) {
					VDASSERT(mixEnd >= dstOffset);
					break;
				}
			}

			// clip sound to end of current mixing window
			if (len > mixEnd - dstOffset)
				len = mixEnd - dstOffset;

			if (!len)
				break;

			VDASSERT(dstOffset <= n && n - dstOffset >= len);

			wroteAudio = true;

			// mix samples
			const float volumeL = s->mVolumeL * mixInfo.mpMixLevels[s->mMix];
			const float volumeR = s->mVolumeR * mixInfo.mpMixLevels[s->mMix];
			float *dstL2 = dstL + dstOffset;
			float *dstR2 = dstR ? dstR + dstOffset : nullptr;

			if (s->mpSource) {
				// mix source
				s->mpSource->MixAudio(dstL2, len, volumeL, s->mOffset, mixInfo.mMixingRate);

				if (dstR2)
					s->mpSource->MixAudio(dstR2, len, volumeL, s->mOffset, mixInfo.mMixingRate);

				s->mOffset += len;
			} else {
				// direct sample -- mix it ourselves
				uint32 srcOffset = s->mOffset;
				uint32 srcSubOffset = s->mSubOffset;
				const sint16 *src0 = s->mbLooping ? s->mpSampleBuffer->GetLoopingSampleStart() : s->mpSampleBuffer->GetOneShotSampleStart();
				const sint16 *src = src0 + srcOffset;

				while(len) {
					// compute the length of the block to mix
					uint32 blockLen = len;

					// if looping is enabled, make sure this block does not
					// cross a loop boundary
					if (s->mbLooping) {
						const uint32 srcSamplesLeft = srcLen - srcOffset;
						uint32 dstSamplesLeft = srcSamplesLeft;
						
						if (!s->mbMatchedRate) {
							uint64 dstSamplesLeft64 = (((uint64)srcSamplesLeft << 32) - s->mSubOffset - 1) / s->mSampleStepF32 + 1;

							if (blockLen > dstSamplesLeft64) {
								blockLen = (uint32)dstSamplesLeft64;
							}
						} else {
							if (blockLen > dstSamplesLeft)
								blockLen = dstSamplesLeft;
						}
					}

					len -= blockLen;

					// mix this block
					if (s->mbMatchedRate) {
						VDASSERT(!s->mLength || (srcOffset < s->mLength && s->mLength - srcOffset >= blockLen));

						if (dstR2)
							MixStereo(dstL2, dstR2, src, blockLen, volumeL, volumeR);
						else
							MixMono(dstL2, src, blockLen, volumeL);

						srcOffset += blockLen;
					} else {
						uint64 accum = ((uint64)srcOffset << 32) + srcSubOffset;

						VDASSERT(!s->mLength || (accum < ((uint64)s->mLength << 32) && accum + s->mSampleStepF32 * (blockLen - 1) < ((uint64)s->mLength << 32)));

						if (dstR2)
							accum = MixStereoResample(dstL2, dstR2, src0, blockLen, volumeL, volumeR, accum, s->mSampleStepF32);
						else
							accum = MixMonoResample(dstL2, src0, blockLen, volumeL, accum, s->mSampleStepF32);

						srcOffset = (uint32)(accum >> 32);
						srcSubOffset = (uint32)accum;
					}

					dstL2 += blockLen;
					if (dstR2)
						dstR2 += blockLen;

					src = src0;

					if (srcOffset >= srcLen) {
						if (s->mbLooping) {
							srcOffset -= srcLen;

							VDASSERT(!s->mLength || srcOffset < s->mLength);
						} else {
							soundExpired = true;
							break;
						}
					}
				}

				if (soundExpired)
					break;

				s->mOffset = srcOffset;

				VDASSERT(!srcLen || s->mOffset < srcLen);
			}

			s->mNextTime = mixEndTime;

			if (s->mbEndValid && s->mNextTime >= s->mEndTime)
				soundExpired = true;
		} while(false);

		if (soundExpired) {
			--itEnd;

			if (it != itEnd)
				*it = *itEnd;

			mPlayingSounds.pop_back();
			
			FreeSound(s);
		} else {
			++it;
		}
	}

	// process convolution sounds
	if (mpConvoOutput) {
		for(ATAudioConvolutionPlayer *player : mConvoPlayers) {
			player->CommitFrame((uint32)mixEndTime);
		}

		if (mpConvoOutput->Commit(mixInfo.mpLeft, mixInfo.mpRight, mixInfo.mCount))
			wroteAudio = true;
	}

	mLastMixTime = mixEndTime;

	return wroteAudio;
}

uint32 ATAudioSamplePlayer::TickDeltaToSampleDelta(uint32 ticks) const {
	return VDRoundToInt((double)ticks * mOutputSamplesPerTick);
}

// mono (expected for normal mixing)
void ATAudioSamplePlayer::MixMono(float *dst, const sint16 *src, uint32 n, float vol) {
	float *VDRESTRICT dstL2 = dst;

	for(uint32 i=0; i<n; ++i) {
		const float sample = (float)*src++ * vol;
		*dstL2++ += sample;
	}
}

// mono-to-stereo (expected for edge mixing)
void ATAudioSamplePlayer::MixStereo(float *dstL, float *dstR, const sint16 *src, uint32 n, float volumeL, float volumeR) {
	float *VDRESTRICT dstL2 = dstL;
	float *VDRESTRICT dstR2 = dstR;

	if (volumeL == volumeR) {
		for(uint32 i=0; i<n; ++i) {
			const float sample = (float)*src++ * volumeL;
			*dstL2++ += sample;
			*dstR2++ += sample;
		}
	} else {
		for(uint32 i=0; i<n; ++i) {
			const float sample = (float)*src++;
			*dstL2++ += sample * volumeL;
			*dstR2++ += sample * volumeR;
		}
	}
}

uint64 ATAudioSamplePlayer::MixMonoResample(float *dst, const sint16 *src, uint32 n, float vol, uint64 accum, uint64 step) {
	float *VDRESTRICT dstL2 = dst;

	for(uint32 i=0; i<n; ++i) {
		size_t offset = (size_t)(accum >> 32);
		const float x1 = (float)src[offset];
		const float x2 = (float)src[offset+1];
		const float sample = (x1 + (x2 - x1) * ((float)(sint32)((uint32)accum >> 1) * 0x1p-31f)) * vol;
		accum += step;

		*dstL2++ += sample;
	}

	return accum;
}

uint64 ATAudioSamplePlayer::MixStereoResample(float *dstL, float *dstR, const sint16 *src, uint32 n, float volumeL, float volumeR, uint64 accum, uint64 step) {
	float *VDRESTRICT dstL2 = dstL;
	float *VDRESTRICT dstR2 = dstR;

	for(uint32 i=0; i<n; ++i) {
		size_t offset = (size_t)(accum >> 32);
		const float x1 = (float)src[offset];
		const float x2 = (float)src[offset+1];
		const float sample = (x1 + (x2 - x1) * ((float)(sint32)((uint32)accum >> 1) * 0x1p-31f));
		accum += step;

		*dstL2++ += sample * volumeL;
		*dstR2++ += sample * volumeR;
	}

	return accum;
}

ATSoundId ATAudioSamplePlayer::StartSound(Sound *s, IATAudioSoundGroup& soundGroup, uint64 startTime) {
	s->mId = (ATSoundId)mNextSoundId;
	mNextSoundId += 2;

	SoundGroup& soundGroupImpl = static_cast<SoundGroup&>(soundGroup);
	auto& sounds = soundGroupImpl.mSounds;

	// If the remove-superceded-sounds option is enabled on the group, stop any sounds that would start
	// on or after this sound's start time.
	if (soundGroupImpl.mDesc.mbRemoveSupercededSounds) {
		while(!sounds.empty()) {
			Sound& lastSound = *sounds.back();
			if (lastSound.mNextTime < startTime)
				break;

			// Force stop is fine here as we're guaranteed that the conflicting sound hasn't started
			// yet (the start time for the new sound can't be in the past).
			ForceStopSound(lastSound.mId);
		}
	}

	soundGroupImpl.mSounds.push_back(s);

	s->mpGroup = &soundGroupImpl;
	s->mMix = soundGroupImpl.mDesc.mAudioMix;
	s->mNextTime = startTime;

	try {
		mReadySounds.push_back(s);
	} catch(...) {
		FreeSound(s);
		throw;
	}

	if (s->mVolumeL != s->mVolumeR) {
		s->mbPanned = true;
		++mPannedSoundCount;
	}

	return s->mId;
}

void ATAudioSamplePlayer::FreeSound(Sound *s) {
	if (s) {
		if (s->mbPanned) {
			VDASSERT(mPannedSoundCount > 0);
			--mPannedSoundCount;
		}

		mPool.FreeSound(s);
	}
}

ATAudioSamplePlayer::Sounds::iterator ATAudioSamplePlayer::FindSoundById(Sounds& sounds, ATSoundId id) {
	auto it = sounds.begin(), itEnd = sounds.end();
	for(; it != itEnd; ++it) {
		if ((*it)->mId == id)
			return it;
	}

	return itEnd;
}

void ATAudioSamplePlayer::CleanupGroup(SoundGroup& group) {
	group.mpParent = nullptr;
	mGroups.erase(&group);

	StopGroupSounds(group);
}

void ATAudioSamplePlayer::StopGroupSounds(SoundGroup& group) {
	for(Sound *sound : group.mSounds) {
		// must remove the sound from the group before we try to soft-stop it, so that
		// StopSound() doesn't invalidate our iterators
		sound->mpGroup = nullptr;

		StopSound(sound->mId);
	}

	group.mSounds.clear();
}

void ATAudioSamplePlayer::RemoveConvolutionPlayer(ATAudioConvolutionPlayer& cplayer) {
	cplayer.Shutdown();

	auto it = std::find(mConvoPlayers.begin(), mConvoPlayers.end(), &cplayer);
	if (it != mConvoPlayers.end()) {
		*it = mConvoPlayers.back();
		mConvoPlayers.pop_back();

		cplayer.Release();
	}
}
