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

#ifndef f_AT_VGMPLAYER_H
#define f_AT_VGMPLAYER_H

#include <at/atcore/deviceimpl.h>
#include <at/atcore/scheduler.h>

class ATMemoryLayer;
class ATPokeyEmulator;

class ATDeviceVGMPlayer final
	: public ATDevice
	, public IATSchedulerCallback
{
public:
	ATDeviceVGMPlayer();
	~ATDeviceVGMPlayer();

	void Load(ATPokeyEmulator& pokey, double cyclesPerSecond, IVDStream& stream);

	bool IsStereo() const { return mbStereo; }
	bool IsPAL() const { return mbPAL; }

public:
	void GetDeviceInfo(ATDeviceInfo& info) override;
	void Init() override;
	void Shutdown() override;
	void WarmReset() override;
	void ColdReset() override;

public:
	void OnScheduledEvent(uint32 id) override;

private:
	struct Event {
		uint64 mCycleOffset;
		uint8 mRegister;
		uint8 mValue;
	};

	sint32 DebugReadByte(uint32 addr) const;
	sint32 ReadByte(uint32 addr);
	bool WriteByte(uint32 addr, uint8 value);
	void ReplyWithDurationCyclesAsTimestamp(double cycles);

	ATPokeyEmulator *mpPokey = nullptr;

	ATMemoryManager *mpMemMgr = nullptr;
	ATMemoryLayer *mpMemLayerControl = nullptr;

	ATScheduler *mpScheduler = nullptr;
	ATEvent *mpPlayEvent = nullptr;
	uint64 mPlayStartCycle = 0;
	size_t mEventIndex = 0;
	uint32 mReadIndex = 0;
	uint32 mReadLength = 0;
	bool mbStereo = false;
	bool mbPAL = false;

	uint8 mReadBuffer[8] {};

	vdfastvector<Event> mEvents;
};

#endif
