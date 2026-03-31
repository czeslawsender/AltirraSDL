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

#include <stdafx.h>
#include <vd2/system/binary.h>
#include <vd2/system/constexpr.h>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/zip.h>
#include <at/ataudio/pokey.h>
#include "memorymanager.h"
#include "vgmplayer.h"


void ATCreateDeviceVGMPlayer(const ATPropertySet& pset, IATDevice **dev) {
	vdrefptr<ATDeviceVGMPlayer> p(new ATDeviceVGMPlayer);

	*dev = p.release();
}

extern const ATDeviceDefinition g_ATDeviceDefVGMPlayer = { "vgmplayer", nullptr, L"VGM Player", ATCreateDeviceVGMPlayer, kATDeviceDefFlag_Internal };

ATDeviceVGMPlayer::ATDeviceVGMPlayer() {
}

ATDeviceVGMPlayer::~ATDeviceVGMPlayer() {
}

void ATDeviceVGMPlayer::Load(ATPokeyEmulator& pokey, double cyclesPerSecond, IVDStream& stream) {
	mpPokey = &pokey;

	VDBufferedStream bs(&stream, 4096);
	uint8 header[256] {};

	const auto throwNotVgm = [] { throw VDException(L"File is not a VGM format file."); };

	// Read minimal VGM 1.00 size and check signature
	if (bs.ReadData(header, 0x40) != 0x40 || VDReadUnalignedLEU32(header) != 0x206d6756)
		throwNotVgm();

	// get version
	const uint32 version = VDReadUnalignedLEU32(header + 0x08);

	// fail if version < 1.0
	if (version < 0x0100)
		throwNotVgm();

	// For v1.50+, offset 0x34 gives the relative offset to the VGM data. It must be at
	// least 0xC since the minimum size for v1.50+ is 64 bytes. For versions prior to
	// v1.50, it must be 0.
	const uint32_t vgmOffset = VDReadUnalignedLEU32(header + 0x34);

	const auto throwInvalidVgm = [] { throw VDException(L"Unable to parse VGM file as the header is invalid."); };

	if (version < 0x0150) {
		if (vgmOffset)
			throwInvalidVgm();
	} else {
		if (vgmOffset < 0xC)
			throwInvalidVgm();

		if (vgmOffset > 0xC) {
			bs.Read(header + 0x40, std::min<uint32>(vgmOffset - 0xC, 0xC0));

			// skip any remaining header that we don't support
			if (vgmOffset > 0xCC)
				bs.Skip(vgmOffset - 0xCC);
		}
	}

	// Check if the VGM contains a valid POKEY clock.
	const uint32 pokeyInfo = VDReadUnalignedLEU32(header + 0xB0);
	const uint32 pokeyClock = pokeyInfo & 0x3FFFFFFF;

	if (!pokeyClock)
		throw VDException(L"The VGM file does not contain POKEY commands.");

	// check for NTSC or PAL clocks
	const bool isNTSC = (pokeyClock > 1789772 - 50 && pokeyClock < 1789773 + 50);
	const bool isPAL = (pokeyClock > 1773447 - 50 && pokeyClock < 1773448 + 50);

	if (!isNTSC && !isPAL)
		throw VDException(L"The VGM file contains POKEY commands, but the clock rate is too far out of range (%u Hz).", pokeyClock);

	mbPAL = isPAL;
	mbStereo = (pokeyInfo & 0x40000000) != 0;

	// fetch eof position
	sint64 eofPos = VDReadUnalignedLEU32(header + 0x04) + 4;

	// process commands
	static constexpr auto kBaseCommandArgLenTable = []() -> VDCxArray<uint8, 256> {
		VDCxArray<uint8, 256> table {};

		// 00..2F not defined in spec
		for(uint32 i = 0x30; i <= 0x3F; ++i) table.v[i] = 1;
		for(uint32 i = 0x40; i <= 0x4E; ++i) table.v[i] = 2;
		table.v[0x4F] = 1;
		table.v[0x50] = 1;
		for(uint32 i = 0x51; i <= 0x5F; ++i) table.v[i] = 2;
		table.v[0x61] = 2;
		// 62..66 is single byte
		table.v[0x67] = 6;	// data block
		table.v[0x68] = 11; // PCM RAM write
		// 69..6F not defined in spec
		// 70..7F is single byte
		// 80..8F is single byte
		table.v[0x90] = 4;
		table.v[0x91] = 4;
		table.v[0x92] = 5;
		table.v[0x93] = 10;
		table.v[0x94] = 1;
		table.v[0x95] = 4;
		table.v[0xA0] = 2;
		// A1..AF not defined in spec
		for(uint32 i = 0xB0; i <= 0xBF; ++i) table.v[i] = 2;
		for(uint32 i = 0xC0; i <= 0xCF; ++i) table.v[i] = 3;
		for(uint32 i = 0xD0; i <= 0xDF; ++i) table.v[i] = 3;
		for(uint32 i = 0xE0; i <= 0xFF; ++i) table.v[i] = 4;

		return table;
	}();

	uint8 simpleCmd[16] {};
	uint32 sampleCounter = 0;
	double cyclesPerSample = cyclesPerSecond / 44100.0;

	for(;;) {
		sint64 pos = bs.Pos();

		if (pos >= eofPos)
			break;

		// read first byte
		bs.Read(simpleCmd, 1);
		++pos;

		// handle EOS
		if (simpleCmd[0] == 0x66)
			break;

		// look up number of argument bytes for the command
		const uint32 argBytes = kBaseCommandArgLenTable.v[simpleCmd[0]];

		if (argBytes) {
			// check if we have enough data
			if (eofPos - pos < argBytes)
				break;

			// read the argument bytes
			bs.Read(simpleCmd + 1, argBytes);

			pos += argBytes;
		}

		// handle commands that we support or need to skip extra data for
		if (simpleCmd[0] == 0x61) {
			// wait 0-65535 samples
			sampleCounter += VDReadUnalignedLEU16(simpleCmd + 1);
		} else if (simpleCmd[0] == 0x62) {
			// wait 735 samples
			sampleCounter += 735;
		} else if (simpleCmd[0] == 0x63) {
			// wait 882 samples
			sampleCounter += 882;
		} else if (simpleCmd[0] == 0x67) {
			// data block
			const uint32 dataLen = VDReadUnalignedLEU32(simpleCmd + 3);

			if (dataLen) {
				if (eofPos - pos < dataLen)
					break;

				bs.Skip(dataLen);
			}
		} else if (simpleCmd[0] == 0x68) {
			// PCM RAM write
			const uint32 dataLen = VDReadUnalignedLEU32(simpleCmd + 8) & 0xFFFFFF;

			if (dataLen) {
				if (eofPos - pos < dataLen)
					break;

				bs.Skip(dataLen);
			}
		} else if (simpleCmd[0] >= 0x70 && simpleCmd[0] <= 0x7F) {
			// wait 1-16 samples
			sampleCounter += simpleCmd[0] - 0x6F;
		} else if (simpleCmd[0] == 0xBB) {
			// POKEY write -- skip anything but audio and SKCTL
			switch(simpleCmd[1] & 0xF) {
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7:
				case 8:
				case 15:
					// Stereo doesn't sound that great if the two POKEYs are updated with the same
					// timing, as they can stay too synced. Therefore, we displace writes to the second
					// POKEY by half a sample.
					//
					// The player also attempts to displace the second POKEY's init, but that only
					// applies if the VGM itself doesn't reset POKEY.
					{
						const bool secondaryWrite = mbStereo && (simpleCmd[1] & 0x80);

						mEvents.emplace_back(
							Event {
								(uint64)(0.5 + ((double)sampleCounter + (secondaryWrite ? 0.5 : 0.0)) * cyclesPerSample),
								(uint8)((simpleCmd[1] & 0x0F) + (secondaryWrite ? 0x10 : 0x00)),
								simpleCmd[2]
							}
						);
					}
					break;
			}
		}
	}

	// We need to re-sort the events since we may have exchanged some out of order due to
	// the secondary POKEY offset.
	std::sort(
		mEvents.begin(),
		mEvents.end(),
		[](const Event& x, const Event& y) {
			return x.mCycleOffset < y.mCycleOffset;
		}
	);
}

void ATDeviceVGMPlayer::GetDeviceInfo(ATDeviceInfo& info) {
	info.mpDef = &g_ATDeviceDefVGMPlayer;
}

void ATDeviceVGMPlayer::Init() {
	mpScheduler = GetService<IATDeviceSchedulingService>()->GetMachineScheduler();

	mpMemMgr = GetService<ATMemoryManager>();

	ATMemoryHandlerTable handlers {};
	handlers.mpThis = this;
	handlers.mbPassAnticReads = true;
	handlers.mbPassReads = true;
	handlers.mbPassWrites = true;
	handlers.BindDebugReadHandler<&ATDeviceVGMPlayer::DebugReadByte>();
	handlers.BindReadHandler<&ATDeviceVGMPlayer::ReadByte>();
	handlers.BindWriteHandler<&ATDeviceVGMPlayer::WriteByte>();
	mpMemLayerControl = mpMemMgr->CreateLayer(kATMemoryPri_HardwareOverlay, handlers, 0xD2, 0x01);

	mpMemMgr->SetLayerName(mpMemLayerControl, "VGM player");
	mpMemMgr->EnableLayer(mpMemLayerControl, true);
}

void ATDeviceVGMPlayer::Shutdown() {
	if (mpMemMgr) {
		mpMemMgr->DeleteLayerPtr(&mpMemLayerControl);
		mpMemMgr = nullptr;
	}

	if (mpScheduler) {
		mpScheduler->UnsetEvent(mpPlayEvent);
		mpScheduler = nullptr;
	}
}

void ATDeviceVGMPlayer::WarmReset() {
	mpScheduler->UnsetEvent(mpPlayEvent);

	mReadIndex = 0;
	mReadLength = 0;
}

void ATDeviceVGMPlayer::ColdReset() {
	WarmReset();
}

void ATDeviceVGMPlayer::OnScheduledEvent(uint32 id) {
	const uint64 offset = mpScheduler->GetTick64() - mPlayStartCycle;

	mpPlayEvent = nullptr;

	if (mEventIndex < mEvents.size()) {
		const auto& ev = mEvents[mEventIndex];

		if (offset < ev.mCycleOffset) {
			mpPlayEvent = mpScheduler->AddEvent((uint32)std::clamp<uint64>(ev.mCycleOffset - offset, 1, 1000000), this, 1);
			return;
		}

		mpPokey->WriteByte(ev.mRegister, ev.mValue);
		++mEventIndex;

		mpPlayEvent = mpScheduler->AddEvent(1, this, 1);
	}
}

sint32 ATDeviceVGMPlayer::DebugReadByte(uint32 addr) const {
	if (addr < 0xD240 || addr >= 0xD280)
		return -1;

	if (addr == 0xD240) {
		// status port
		//
		// D7 = 1: Playing
		// D6 = 1: Read pending

		uint8 v = 0x3F;

		if (mpPlayEvent)
			v |= 0x80;

		if (mReadIndex < mReadLength)
			v |= 0x40;

		return v;
	} else if (addr == 0xD241) {
		// data read port

		if (mReadIndex < mReadLength)
			return mReadBuffer[mReadIndex];
	}

	return 0xFF;
}

sint32 ATDeviceVGMPlayer::ReadByte(uint32 addr) {
	if (addr == 0xD241) {
		// data read port

		if (mReadIndex < mReadLength)
			return mReadBuffer[mReadIndex++];
	}

	return DebugReadByte(addr);
}

bool ATDeviceVGMPlayer::WriteByte(uint32 addr, uint8 value) {
	if (addr < 0xD240 || addr >= 0xD280)
		return false;

	if (addr == 0xD240) {
		// control write port
		//
		// $A0: Identify
		// $A1: Stop
		// $A2: Play
		// $A3: Read current time
		// $A4: Read total duration

		mReadIndex = 0;
		mReadLength = 0;

		if (value == 0xA0) {
			mReadLength = 4;
			mReadBuffer[0] = 'V';
			mReadBuffer[1] = 'G';
			mReadBuffer[2] = 'M';
			mReadBuffer[3] = ' ';
		} else if (value == 0xA1) {
			mpScheduler->UnsetEvent(mpPlayEvent);
		} else if (value == 0xA2) {
			mpScheduler->SetEvent(1, this, 1, mpPlayEvent);
			mPlayStartCycle = mpScheduler->GetTick64();
			mEventIndex = 0;
		} else if (value == 0xA3) {
			double cycles = 0;

			if (mpPlayEvent)
				cycles = (double)(mpScheduler->GetTick64() - mPlayStartCycle);

			ReplyWithDurationCyclesAsTimestamp(cycles);
		} else if (value == 0xA4) {
			double cycles = 0;

			if (!mEvents.empty())
				cycles = mEvents.back().mCycleOffset;

			ReplyWithDurationCyclesAsTimestamp(cycles);
		}
	} else if (addr == 0xD241) {
		// data write port
	}

	return true;
}

void ATDeviceVGMPlayer::ReplyWithDurationCyclesAsTimestamp(double cycles) {
	mReadLength = 4;

	int t = (int)(0.5 + 100.0 * cycles * mpScheduler->GetRate().AsInverseDouble());
	int csecs = t % 100;
	t /= 100;
	int secs = t % 60;
	t /= 60;
	int mins = t % 60;
	t /= 60;
	int hrs = (t / 24) % 100;

	mReadBuffer[0] = (hrs / 10)*16 + (hrs % 10);
	mReadBuffer[1] = (mins / 10)*16 + (mins % 10);
	mReadBuffer[2] = (secs / 10)*16 + (secs % 10);
	mReadBuffer[3] = (csecs / 10)*16 + (csecs % 10);
}
