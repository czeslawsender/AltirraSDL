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
#include <vd2/system/vdstl_algorithm.h>
#include <at/ataudio/audiosamplebuffer.h>

ATAudioSampleBuffer::ATAudioSampleBuffer(vdspan<const sint16> soundData, const ATAudioSoundSamplingRate& samplingRate, float volume)
	: mSamplingRate(samplingRate)
	, mVolume(volume * (1.0f / 32767.0f))
{
	size_t len = soundData.size();
	size_t allocLen = (len + kSampleHeader + kSampleFooter + 3) & ~(size_t)3;

	mBuffer.resize(allocLen);
	mSampleCount = (uint32)len;
	mSamplingRate = samplingRate;

	// copy in main sample buffer
	vdcopy_checked_r(vdspan(mBuffer).subspan(kSampleHeader, len), soundData);

	// wrap header (used for looping)
	for(size_t i = kSampleHeader; i > 0; --i)
		mBuffer[i - 1] = mBuffer[i - 1 + len];

	// zero footer (used for one-shot)
	for(size_t i = len + kSampleHeader; i < allocLen; ++i)
		mBuffer[i] = 0;
}
