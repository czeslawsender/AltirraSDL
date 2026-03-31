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

#ifndef f_AT_ATAUDIO_AUDIOSAMPLEBUFFER_H
#define f_AT_ATAUDIO_AUDIOSAMPLEBUFFER_H

#include <at/atcore/audiomixer.h>

////////////////////////////////////////////////////////////////////////////////
// Audio sample buffer
//
// Buffer object for sound samples. This ensures a specific buffer layout
// suitable for efficient sound mixing.
//
class ATAudioSampleBuffer final : public vdrefcounted<IATAudioSampleHandle> {
public:
	// Header before the start of the sample. This is wrapped from the end of
	// the same, and used for looping.
	static constexpr int kSampleHeader = 8;

	// Footer after the end of the sample. This is zeroed to allow for
	// read-beyond for one-shots.
	static constexpr int kSampleFooter = 8;

	uint32 mSampleCount = 0;
	ATAudioSoundSamplingRate mSamplingRate {};
	float mVolume = 0;

	ATAudioSampleBuffer(vdspan<const sint16> soundData, const ATAudioSoundSamplingRate& samplingRate, float volume);

	// Return the start of the sample for one-shot playback. This points to the
	// beginning of the sample.
	const sint16 *GetOneShotSampleStart() const {
		return mBuffer.data() + kSampleHeader;
	}

	// Return the start of the sample for looped playback. This starts
	// kSampleHeader samples before the start/end of the looped sample; this
	// offset must be taken into account when setting the starting playback
	// position.
	const sint16 *GetLoopingSampleStart() const {
		return mBuffer.data();
	}

private:
	vdblock<sint16> mBuffer;
};

#endif

