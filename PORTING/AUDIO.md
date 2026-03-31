# Audio Output

## Current Architecture (Windows)

```
POKEY Emulator (synthesis at ~64 kHz)
    |
    v
IATSyncAudioSource       -- clean interface (WriteAudio callback)
    |
    v
IATAudioMixer            -- mixes multiple sources, resamples to output rate
    |
    v
IATAudioOutput           -- CLEAN high-level interface
    |  (manages mixer, volume, latency, API selection)
    v
IVDAudioOutput           -- CONTAMINATED (Init takes tWAVEFORMATEX)
    |
    v
[WaveOut | DirectSound | XAudio2 | WASAPI]
```

Two interfaces exist at different levels:

- **`IATAudioOutput`** (in `at/ataudio/audiooutput.h`): Clean. No Win32
  types. Methods: `Init(ATScheduler&)`, `InitNativeAudio()`, `SetApi()`,
  `WriteAudio()`, `AsMixer()`, `SetVolume()`, `SetLatency()`, etc.

- **`IVDAudioOutput`** (in `vd2/Riza/audioout.h`): Contaminated. `Init()`
  takes `tWAVEFORMATEX*`. Used internally by `IATAudioOutput` implementations.

The SDL3 build works at the `IATAudioOutput` level, completely replacing the
lower layers.

## SDL3 Architecture

```
POKEY Emulator (synthesis at ~64 kHz)
    |
    v
IATSyncAudioSource
    |
    v
IATAudioMixer (unchanged)
    |
    v
ATAudioOutputSDL3 : IATAudioOutput
    |
    v
SDL_AudioStream → SDL_AudioDevice
    |
    v
OS Audio (PulseAudio/ALSA/CoreAudio/WASAPI)
```

### ATAudioOutputSDL3 Class

```cpp
class ATAudioOutputSDL3 : public IATAudioOutput {
public:
    // IATAudioOutput (full interface -- 23 pure virtual methods)
    void Init(ATScheduler& scheduler) override;
    void InitNativeAudio() override;
    ATAudioApi GetApi() override;
    void SetApi(ATAudioApi api) override;  // ignored on SDL3 (SDL picks best)
    void SetAudioTap(IATAudioTap *tap) override;
    ATUIAudioStatus GetAudioStatus() const override;
    IATAudioMixer& AsMixer() override;
    ATAudioSamplePool& GetPool() override;
    void SetCyclesPerSecond(double cps, double repeatfactor) override;
    bool GetMute() override;
    void SetMute(bool mute) override;
    float GetVolume() override;
    void SetVolume(float vol) override;
    float GetMixLevel(ATAudioMix mix) const override;
    void SetMixLevel(ATAudioMix mix, float level) override;
    int GetLatency() override;
    void SetLatency(int ms) override;
    int GetExtraBuffer() override;
    void SetExtraBuffer(int ms) override;
    void SetFiltersEnabled(bool enable) override;
    void Pause() override;
    void Resume() override;
    void WriteAudio(const float *left, const float *right,
                    uint32 count, bool pushAudio,
                    bool pushStereoAsAudio, uint64 timestamp) override;

private:
    SDL_AudioStream *mpStream = nullptr;
    SDL_AudioDeviceID mDeviceID = 0;
    ATAudioMixer mMixer;
    ATAudioSamplePool mPool;
    float mVolume = 1.0f;
    int mLatencyMs = 40;
    bool mMuted = false;
};
```

### Initialization

```cpp
void ATAudioOutputSDL3::InitNativeAudio() {
    SDL_AudioSpec spec;
    spec.freq = 44100;         // standard output rate
    spec.format = SDL_AUDIO_F32;  // float samples (matches WriteAudio)
    spec.channels = 2;         // stereo

    mpStream = SDL_CreateAudioStream(&spec, &spec);
    mDeviceID = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    SDL_BindAudioStream(mDeviceID, mpStream);
}
```

### Audio Flow

1. The scheduler ticks the POKEY emulator, which generates samples at ~64 kHz
   (machine clock / 28 cycles per sample).

2. The mixer collects samples from all audio sources
   (`IATSyncAudioSource::WriteAudio`), resamples from ~64 kHz to 44100 Hz,
   and calls `IATAudioOutput::WriteAudio()` with interleaved float buffers.

3. `ATAudioOutputSDL3::WriteAudio()` pushes samples to the SDL3 audio stream:

```cpp
void ATAudioOutputSDL3::WriteAudio(const float *left, const float *right,
    uint32 count, bool pushAudio, bool pushStereoAsAudio, uint64 timestamp) {
    // Interleave left/right into stereo buffer
    float interleaved[2048];
    for (uint32 i = 0; i < count && i < 1024; ++i) {
        interleaved[i*2]   = left[i] * mVolume;
        interleaved[i*2+1] = right[i] * mVolume;
    }

    SDL_PutAudioStreamData(mpStream, interleaved, count * 2 * sizeof(float));
}
```

4. SDL3 pulls from the audio stream on its audio thread and delivers to the
   OS audio subsystem.

### Buffer Management

SDL3 manages its own audio buffering internally. The `SetLatency()` method
controls how much data we keep queued:

```cpp
void ATAudioOutputSDL3::SetLatency(int ms) {
    mLatencyMs = ms;
    // Adjust stream buffer: SDL3 handles this via internal buffering
    // We control latency by monitoring SDL_GetAudioStreamQueued()
    // and throttling WriteAudio if too far ahead
}
```

Monitor `SDL_GetAudioStreamQueued()` to detect underflow (queue empty) or
excessive latency (queue too full). The `GetAudioStatus()` method reports
this to the emulator for adaptive timing.

### SetApi

On Windows, the emulator lets users choose between WaveOut, DirectSound,
XAudio2, and WASAPI. On SDL3, the audio backend is chosen automatically by
SDL3 based on the platform. The `SetApi()` method becomes a no-op (or could
hint via `SDL_SetHint` if needed).

### Sample Rate

The POKEY synthesizes at the machine clock rate (~1.79 MHz / 28 = ~63,920
Hz for NTSC). The mixer resamples to the output rate. SDL3's
`SDL_CreateAudioStream` can also perform resampling if input and output specs
differ, but since the mixer already handles this, pass matched specs to
SDL3.

### Factory Function

`ATCreateAudioOutput()` is called inside `ATSimulator::Init()` (in
`simulator.cpp` line ~920), not by the frontend. The factory lives in
`ATAudio/source/audiooutput.cpp` and currently returns `new ATAudioOutput`
(the Win32 implementation).

For the SDL3 build, provide an alternative `audiooutput_sdl3.cpp` that
implements the same factory:

```cpp
// ATAudio/source/audiooutput_sdl3.cpp
IATAudioOutput *ATCreateAudioOutput() {
    return new ATAudioOutputSDL3();
}
```

The build system compiles either `audiooutput.cpp` (Windows) or
`audiooutput_sdl3.cpp` (SDL3), never both. This keeps the existing
`ATAudioOutput` class completely untouched and avoids `#ifdef` blocks in
the original code.

Note: `audiooutput.cpp` also contains `ATAudioOutput` (the Win32
implementation class, ~1100 lines). The SDL3 file contains
`ATAudioOutputSDL3` plus the factory function. Both files define the same
external symbol (`ATCreateAudioOutput`) so they are mutually exclusive at
link time.

### ATAudioApi Enum

The existing enum defines five values:
- `kATAudioApi_WaveOut`, `kATAudioApi_DirectSound`, `kATAudioApi_XAudio2`,
  `kATAudioApi_WASAPI`, `kATAudioApi_Auto`

On SDL3, `SetApi()` is a no-op (SDL3 chooses the best backend automatically).
`GetApi()` returns `kATAudioApi_Auto`.

## Summary of New Files

| File | Purpose |
|------|---------|
| `src/ATAudio/source/audiooutput_sdl3.cpp` | `ATAudioOutputSDL3` implementing `IATAudioOutput` |

## Interface Dependency

Depends only on:

- `IATAudioOutput` (clean)
- `IATSyncAudioSource` / `IATAudioMixer` (clean)
- SDL3 audio headers

Does **not** depend on `Riza/audioout.h`, `tWAVEFORMATEX`, DirectSound,
WASAPI, or any Win32 headers.
