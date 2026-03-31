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

#ifndef f_AT_ATAUDIO_AUDIOSAMPLEPLAYER_H
#define f_AT_ATAUDIO_AUDIOSAMPLEPLAYER_H

#include <vd2/system/vdalloc.h>
#include <at/atcore/audiomixer.h>
#include <at/atcore/audiosource.h>

class ATAudioSampleBuffer;
class ATScheduler;
class ATAudioSound;
class ATAudioSoundGroup;
class ATAudioConvolutionPlayer;
class ATAudioConvolutionOutput;
class ATAudioSamplePool;
class ATAudioSamplePlayer;

class ATAudioSoundGroup final : public IATAudioSoundGroup, public vdlist_node {
public:
	ATAudioSoundGroup(ATAudioSamplePlayer& parent) : mpParent(&parent) {}

	int AddRef() override;
	int Release() override;

	bool IsAnySoundQueued() const override;
	void StopAllSounds() override;

	int mRefCount = 0;
	ATAudioSamplePlayer *mpParent;

	ATAudioGroupDesc mDesc;

	// List of active sounds in the group. This is an unsorted list unless supercede
	// mode is enabled, in which case that policy results in this being sorted.
	vdlist<ATAudioSound> mSounds;
};

struct ATAudioSoundInfo {
	ATSoundId mId {};
	float mVolumeL = 0;
	float mVolumeR = 0;
	uint64 mNextTime = 0;
	uint64 mEndTime = 0;
	uint32 mOffset = 0;
	uint32 mSubOffset = 0;
	uint32 mLength = 0;
	uint64 mSampleStepF32 = 0;
	ATAudioMix mMix {};

	bool mbLooping = false;
	bool mbEndValid = false;
	bool mbPanned = false;

	// True if the sound is being sampled at mixing rate with no offset,
	// and thus needs no resampling.
	bool mbMatchedRate = false;

	IATAudioSampleSource *mpSource = nullptr;
	vdrefptr<ATAudioSampleBuffer> mpSampleBuffer;
	vdrefptr<IVDRefCount> mpOwner;

	// This needs to be a weak pointer; all sounds in a group are implicitly
	// soft-stopped when the group is released. It will be null between when
	// the group is released and the soft-stop completes.
	ATAudioSoundGroup *mpGroup = nullptr;
};

class ATAudioSound final : public vdlist_node, public ATAudioSoundInfo {
public:
	void Reset();
};

class ATAudioSamplePlayer final : public IATSyncAudioSource, public IATAudioAsyncSource, public IATSyncAudioSamplePlayer {
	ATAudioSamplePlayer(const ATAudioSamplePlayer&) = delete;
	ATAudioSamplePlayer& operator=(const ATAudioSamplePlayer&) = delete;
public:
	ATAudioSamplePlayer(ATAudioSamplePool& pool, ATScheduler& sch);
	~ATAudioSamplePlayer();

	void Init();
	void Shutdown();

	void SetRates(float mixingRate, float pokeyMixingRateDivMixingRate, double outputSamplesPerTick);

public:
	IATSyncAudioSource& AsSource() override { return *this; }

	vdrefptr<IATAudioSampleHandle> RegisterSample(vdspan<const sint16> soundData, const ATAudioSoundSamplingRate& samplingRate, float volume) override;

	ATSoundId AddSound(IATAudioSoundGroup& soundGroup, uint32 delay, ATAudioSampleId sampleId, float volume) override;
	ATSoundId AddLoopingSound(IATAudioSoundGroup& soundGroup, uint32 delay, ATAudioSampleId sampleId, float volume) override;

	ATSoundId AddSound(IATAudioSoundGroup& soundGroup, uint32 delay, IATAudioSampleHandle& sample, const ATSoundParams& params) override;

	ATSoundId AddSound(IATAudioSoundGroup& soundGroup, uint32 delay, IATAudioSampleSource *src, IVDRefCount *owner, uint32 len, float volume) override;
	ATSoundId AddLoopingSound(IATAudioSoundGroup& soundGroup, uint32 delay, IATAudioSampleSource *src, IVDRefCount *owner, float volume) override;

	vdrefptr<IATAudioSoundGroup> CreateGroup(const ATAudioGroupDesc& desc) override;

	void ForceStopSound(ATSoundId id) override;
	void StopSound(ATSoundId id) override;
	void StopSound(ATSoundId id, uint64 time) override;

	vdrefptr<IATSyncAudioConvolutionPlayer> CreateConvolutionPlayer(ATAudioSampleId sampleId) override;
	vdrefptr<IATSyncAudioConvolutionPlayer> CreateConvolutionPlayer(const sint16 *sample, uint32 len) override;

public:
	bool RequiresStereoMixingNow() const override;
	void WriteAudio(const ATSyncAudioMixInfo& mixInfo) override;
	bool WriteAsyncAudio(const ATAudioAsyncMixInfo& mixInfo) override;

private:
	friend ATAudioSoundGroup;
	friend ATAudioConvolutionPlayer;

	using SoundGroup = ATAudioSoundGroup;
	using Sound = ATAudioSound;
	using Sounds = vdfastvector<Sound *>;

	uint32 TickDeltaToSampleDelta(uint32 ticks) const;

	void MixMono(float *dst, const sint16 *src, uint32 n, float vol);
	void MixStereo(float *dstL, float *dstR, const sint16 *src, uint32 n, float volumeL, float volumeR);

	virtual uint64 MixMonoResample(float *dst, const sint16 *src, uint32 n, float vol, uint64 accum, uint64 step);
	virtual uint64 MixStereoResample(float *dstL, float *dstR, const sint16 *src, uint32 n, float volumeL, float volumeR, uint64 accum, uint64 step);

	ATSoundId StartSound(Sound *s, IATAudioSoundGroup& soundGroup, uint64 startTime);
	void FreeSound(Sound *s);

	static Sounds::iterator FindSoundById(Sounds& sounds, ATSoundId id);
	void CleanupGroup(SoundGroup& group);
	void StopGroupSounds(SoundGroup& group);

	void RemoveConvolutionPlayer(ATAudioConvolutionPlayer& cplayer);

	ATScheduler& mScheduler;
	uint32 mNextSoundId = 1;
	uint64 mLastMixTime = 0;
	uint32 mPannedSoundCount = 0;

	Sounds mReadySounds;
	Sounds mPlayingSounds;
	Sounds mFreeSounds;

	vdlist<SoundGroup> mGroups;

	vdautoptr<ATAudioConvolutionOutput> mpConvoOutput;
	vdfastvector<ATAudioConvolutionPlayer *> mConvoPlayers;

	// These are placeholder values until the sound player is configured.
	float mMixingRate = 20000.0f;
	float mPokeyMixingRateDivMixingRate = 63920.8f / 20000.0f;
	double mOutputSamplesPerTick = 1.0;

	ATAudioSamplePool& mPool;
};

#endif
