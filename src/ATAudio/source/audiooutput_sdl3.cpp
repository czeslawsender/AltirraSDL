//	Altirra - Atari 800/800XL/5200 emulator
//	SDL3 audio output implementation
//
//	Implements IATAudioOutput for SDL3. For Phase 5, this is a stub that
//	keeps the emulator running without audio. Real SDL3 audio mixing will
//	be added in Phase 6.

#include <stdafx.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/refcount.h>
#include <at/atcore/audiosource.h>
#include <at/atcore/audiomixer.h>
#include <at/ataudio/audiooutput.h>
#include <at/ataudio/audiosamplepool.h>

// -------------------------------------------------------------------------
// Stub implementations of required sub-interfaces
// -------------------------------------------------------------------------

class ATStubSoundGroup final : public IATAudioSoundGroup {
	int mRefCount = 1;
public:
	int AddRef() override { return ++mRefCount; }
	int Release() override { int n = --mRefCount; if (!n) delete this; return n; }
	bool IsAnySoundQueued() const override { return false; }
	void StopAllSounds() override {}
};

class ATStubSyncSamplePlayer final : public IATSyncAudioSamplePlayer, public IATSyncAudioSource {
public:
	// IATSyncAudioSource
	bool RequiresStereoMixingNow() const override { return false; }
	void WriteAudio(const ATSyncAudioMixInfo& mixInfo) override {}

	// IATSyncAudioSamplePlayer
	IATSyncAudioSource& AsSource() override { return *this; }
	vdrefptr<IATAudioSampleHandle> RegisterSample(vdspan<const sint16>, const ATAudioSoundSamplingRate&, float) override { return {}; }
	ATSoundId AddSound(IATAudioSoundGroup&, uint32, ATAudioSampleId, float) override { return ATSoundId::Invalid; }
	ATSoundId AddLoopingSound(IATAudioSoundGroup&, uint32, ATAudioSampleId, float) override { return ATSoundId::Invalid; }
	ATSoundId AddSound(IATAudioSoundGroup&, uint32, IATAudioSampleSource*, IVDRefCount*, uint32, float) override { return ATSoundId::Invalid; }
	ATSoundId AddLoopingSound(IATAudioSoundGroup&, uint32, IATAudioSampleSource*, IVDRefCount*, float) override { return ATSoundId::Invalid; }
	ATSoundId AddSound(IATAudioSoundGroup&, uint32, IATAudioSampleHandle&, const ATSoundParams&) override { return ATSoundId::Invalid; }
	vdrefptr<IATAudioSoundGroup> CreateGroup(const ATAudioGroupDesc&) override { return vdrefptr<IATAudioSoundGroup>(new ATStubSoundGroup); }
	void ForceStopSound(ATSoundId) override {}
	void StopSound(ATSoundId) override {}
	void StopSound(ATSoundId, uint64) override {}
	vdrefptr<IATSyncAudioConvolutionPlayer> CreateConvolutionPlayer(ATAudioSampleId) override { return {}; }
	vdrefptr<IATSyncAudioConvolutionPlayer> CreateConvolutionPlayer(const sint16*, uint32) override { return {}; }
};

class ATStubEdgePlayer final : public IATSyncAudioEdgePlayer {
public:
	void AddEdges(const ATSyncAudioEdge*, size_t, float) override {}
	void AddEdgeBuffer(ATSyncAudioEdgeBuffer*) override {}
};

// -------------------------------------------------------------------------
// Stub audio mixer
// -------------------------------------------------------------------------

class ATAudioMixerStub final : public IATAudioMixer {
public:
	void AddSyncAudioSource(IATSyncAudioSource*) override {}
	void RemoveSyncAudioSource(IATSyncAudioSource*) override {}
	void AddAsyncAudioSource(IATAudioAsyncSource&) override {}
	void RemoveAsyncAudioSource(IATAudioAsyncSource&) override {}

	IATSyncAudioSamplePlayer& GetSamplePlayer() override { return mSamplePlayer; }
	IATSyncAudioSamplePlayer& GetEdgeSamplePlayer() override { return mSamplePlayer; }
	IATSyncAudioEdgePlayer& GetEdgePlayer() override { return mEdgePlayer; }
	IATSyncAudioSamplePlayer& GetAsyncSamplePlayer() override { return mSamplePlayer; }

	void AddInternalAudioTap(IATInternalAudioTap*) override {}
	void RemoveInternalAudioTap(IATInternalAudioTap*) override {}
	void BlockInternalAudio() override {}
	void UnblockInternalAudio() override {}

private:
	ATStubSyncSamplePlayer mSamplePlayer;
	ATStubEdgePlayer mEdgePlayer;
};

// -------------------------------------------------------------------------
// ATAudioOutputSDL3 — Phase 5 stub (silent, but functional)
// -------------------------------------------------------------------------

class ATAudioOutputSDL3 final : public IATAudioOutput {
public:
	void Init(ATScheduler&) override {}
	void InitNativeAudio() override {}

	ATAudioApi GetApi() override { return kATAudioApi_Auto; }
	void SetApi(ATAudioApi) override {}

	void SetAudioTap(IATAudioTap*) override {}
	ATUIAudioStatus GetAudioStatus() const override { return {}; }

	IATAudioMixer& AsMixer() override { return mMixer; }
	ATAudioSamplePool& GetPool() override { return mPool; }

	void SetCyclesPerSecond(double, double) override {}

	bool GetMute() override { return mMuted; }
	void SetMute(bool mute) override { mMuted = mute; }
	float GetVolume() override { return mVolume; }
	void SetVolume(float v) override { mVolume = v; }

	float GetMixLevel(ATAudioMix) const override { return 1.0f; }
	void SetMixLevel(ATAudioMix, float) override {}

	int GetLatency() override { return 40; }
	void SetLatency(int) override {}
	int GetExtraBuffer() override { return 0; }
	void SetExtraBuffer(int) override {}
	void SetFiltersEnabled(bool) override {}

	void Pause() override {}
	void Resume() override {}

	void WriteAudio(const float*, const float*, uint32, bool, bool, uint64) override {}

private:
	ATAudioMixerStub mMixer;
	ATAudioSamplePool mPool;
	float mVolume = 1.0f;
	bool mMuted = false;
};

// -------------------------------------------------------------------------
// Factory function — replaces the Windows ATCreateAudioOutput()
// -------------------------------------------------------------------------

IATAudioOutput *ATCreateAudioOutput() {
	return new ATAudioOutputSDL3();
}
