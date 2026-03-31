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
#include <at/ataudio/audiosamplebuffer.h>
#include <at/ataudio/audiosamplepool.h>
#include <at/ataudio/audiosampleplayer.h>

ATAudioSamplePool::ATAudioSamplePool() {
}

ATAudioSamplePool::~ATAudioSamplePool() {
	Shutdown();
}

void ATAudioSamplePool::Shutdown() {
	for(ATAudioSampleBuffer *& p : mStockSamples)
		vdsaferelease <<= p;

	mStockSamples.clear();

	mFreeSounds.clear();
	mAllocator.Clear();
}

ATAudioSampleBuffer *ATAudioSamplePool::GetStockSample(ATAudioSampleId sampleId) const {
	const size_t index = (size_t)sampleId;;

	if (!index || index > mStockSamples.size())
		return nullptr;

	return mStockSamples[index - 1];
}

void ATAudioSamplePool::RegisterStockSample(ATAudioSampleId sampleId, vdspan<const sint16> soundData, float samplingRate, float volume) {
	size_t index = (size_t)sampleId;

	if (mStockSamples.size() <= index)
		mStockSamples.resize(index, nullptr);

	ATAudioSampleBuffer *buffer = new ATAudioSampleBuffer(soundData, samplingRate, volume);
	buffer->AddRef();

	mStockSamples[index - 1] = buffer;

}

ATAudioSound *ATAudioSamplePool::AllocateSound() {
	if (mFreeSounds.empty())
		mFreeSounds.push_back(mAllocator.Allocate<ATAudioSound>());

	ATAudioSound *s = mFreeSounds.back();
	mFreeSounds.pop_back();

	s->Reset();
	return s;
}

void ATAudioSamplePool::FreeSound(ATAudioSound *s) {
	if (s->mpGroup) {
		s->mpGroup->mSounds.erase(s);
		s->mpGroup = nullptr;
	}

	s->Reset();

	mFreeSounds.push_back(s);
}
