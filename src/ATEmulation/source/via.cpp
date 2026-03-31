//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2014 Avery Lee
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
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stdafx.h>
#include <at/atcore/consoleoutput.h>
#include <at/atcore/snapshotimpl.h>
#include <at/atcore/scheduler.h>
#include <at/atemulation/via.h>

ATVIA6522Emulator::ATVIA6522Emulator()
	: mIRB(0)
	, mIRA(0)
	, mORB(0)
	, mORA(0)
	, mDDRB(0)
	, mDDRA(0)
	, mT1C(0)
	, mT1L(0)
	, mT2C(0)
	, mT2L(0)
	, mSR(0)
	, mACR(0)
	, mPCR(0)
	, mIFR(0)
	, mIER(0)
	, mTimerPB7(0xFF)
	, mTimerPB7Mask(0x00)
	, mbTimer1UnderflowInProgress(false)
	, mCA1Input(true)
	, mCA2Input(true)
	, mCB1Input(true)
	, mCB2Input(true)
	, mCA2(true)
	, mCB2(true)
	, mbIrqState(false)
	, mpScheduler(nullptr)
	, mpEventCA2Update(nullptr)
	, mpEventCB2Update(nullptr)
{
	mpOutputFn = nullptr;
}

ATVIA6522Emulator::~ATVIA6522Emulator() {
	Shutdown();
}

void ATVIA6522Emulator::Init(ATScheduler *sch) {
	mpScheduler = sch;

	Reset();
}

void ATVIA6522Emulator::Shutdown() {
	if (mpScheduler) {
		mpScheduler->UnsetEvent(mpEventCA2Update);
		mpScheduler->UnsetEvent(mpEventCB2Update);
		mpScheduler->UnsetEvent(mpEventT1Update);
		mpScheduler = nullptr;
	}
}

void ATVIA6522Emulator::DumpStatus(ATConsoleOutput& out) {
	const uint32 output = ComputeOutput();

	out("Port A:  [ORA $%02X] & [DDRA $%02X] <+> input $%02X => read $%02X, output $%02X", mORA, mDDRA, mPortAInput, DebugReadByte(1), output & 0xFF);
	out("Port B:  [ORB $%02X] & [DDRB $%02X] <+> input $%02X => read $%02X, output $%02X", mORB, mDDRB, mPortBInput, DebugReadByte(0), (output >> 8) & 0xFF);
	out("CA1/CB1: CA1%c, CB1%c", mCA1Input ? '+' : '-', mCB1Input ? '+' : '-');

	static constexpr const char *kShiftModes[8] {
		"Shift off",
		"Shift in T2",
		"Shift in sysclk",
		"Shift in xclk",
		"Shift free T2",
		"Shift out T2",
		"Shift out sysclk",
		"Shift out xclk",
	};

	out("ACR:     $%02X | %s | %s | %s | %s | %s"
		, mACR
		, mACR & 0x80 ? "T1 -> PB7" : "No PB7"
		, mACR & 0x40 ? "T1 free-run": "T1 one-shot"
		, mACR & 0x20 ? "T2 count" : "T2 one-shot"
		, kShiftModes[(mACR >> 2) & 7]
		, mACR & 0x02 ? "PB latched" : "PB no latch"
		, mACR & 0x02 ? "PA latched" : "PA no latch"
	);

	static constexpr const char *kC2Modes[8] {
		"-in auto",
		"-in manual",
		"+in auto",
		"+in manual",
		"out handshake",
		"out pulse",
		"-manual",
		"+manual",
	};

	out("PCR:     $%02X | CB2 %s | CB1 %c | CA2 %s | CA1 %c"
		, mPCR
		, kC2Modes[(mPCR >> 5) & 7]
		, mPCR & 0x10 ? '+' : '-'
		, kC2Modes[(mPCR >> 1) & 7]
		, mPCR & 0x01 ? '+' : '-'
	);

	out("IFR:     $%02X", mIFR);
	out("IER:     $%02X", mIER);

	double t1period = (double)mT1L * 1000.0 * mpScheduler->GetRate().AsInverseDouble();
	if (mpEventT1Update)
		out("T1L:     $%04X (%.2f ms) - %u cycles to next active update", mT1L, t1period, mpScheduler->GetTicksToEvent(mpEventT1Update));
	else
		out("T1L:     $%04X (%.2f ms)", mT1L, t1period);
}

void ATVIA6522Emulator::SetPortAInput(uint8 val, uint8 mask) {
	val = mPortAInput ^ ((mPortAInput ^ val) & mask);

	if (mPortAInput == val)
		return;

	mPortAInput = val;

	if (!(mACR & 0x01))
		mIRA = val;
}

void ATVIA6522Emulator::SetPortBInput(uint8 val, uint8 mask) {
	val = mPortBInput ^ ((mPortBInput ^ val) & mask);

	if (mPortBInput == val)
		return;

	mPortBInput = val;

	if (!(mACR & 0x02))
		mIRB = val;
}

void ATVIA6522Emulator::SetCA1Input(bool state) {
	if (mCA1Input == state)
		return;

	mCA1Input = state;

	// check if we got the required transition to assert IRQ
	if ((mPCR & 0x01) == (state ? 1 : 0))
		SetIF(kIF_CA1);

	// check for handshake mode on CA2 -- we need to deassert CA2 on CA1
	// transition
	if ((mPCR & 0x0E) == 0x08)
		mpScheduler->SetEvent(1, this, kEventId_CA2Deassert, mpEventCA2Update);
}

void ATVIA6522Emulator::SetCA2Input(bool state) {
	if (mCA2Input != state) {
		mCA2Input = state;

		//	000 = Input, IRQ on negative edge, clear IRQ on read/write
		//	001 = Input, IRQ on negative edge
		//	010 = Input, IRQ on positive edge, clear IRQ on read/write
		//	011 = Input, IRQ on positive edge
		//	100 = Output, set low on read/write, reset on CA1 edge
		//	101 = Output, set low for one cycle on read/write
		//	110 = Output, low
		//	111 = Output, high
		switch(mPCR & 0x0E) {
			case 0x00:
			case 0x02:
			case 0x04:
			case 0x06:
				break;
			case 0x08:

				break;
			case 0x0A:
			case 0x0C:
			case 0x0E:
				break;
			default:
				VDNEVERHERE;
		}
	}
}

void ATVIA6522Emulator::SetCB1Input(bool state) {
	if (mCB1Input == state)
		return;

	mCB1Input = state;

	// check if we got the required transition to assert IRQ
	if ((mPCR & 0x01) == (state ? 1 : 0))
		SetIF(kIF_CB1);

	// check for handshake mode on CB2 -- we need to deassert CB2 on CB1
	// transition
	if ((mPCR & 0xE0) == 0x80)
		mpScheduler->SetEvent(1, this, kEventId_CB2Deassert, mpEventCB2Update);
}

void ATVIA6522Emulator::SetCB2Input(bool state) {
	if (mCB2Input != state) {
		mCB2Input = state;

		//	000 = Input, IRQ on negative edge, clear IRQ on read/write
		//	001 = Input, IRQ on negative edge
		//	010 = Input, IRQ on positive edge, clear IRQ on read/write
		//	011 = Input, IRQ on positive edge
		//	100 = Output, set low on read/write, reset on CA1 edge
		//	101 = Output, set low for one cycle on read/write
		//	110 = Output, low
		//	111 = Output, high
		switch(mPCR & 0xE0) {
			case 0x00:
			case 0x20:
			case 0x40:
			case 0x60:
				break;
			case 0x80:

				break;
			case 0xA0:
			case 0xC0:
			case 0xE0:
				break;
			default:
				VDNEVERHERE;
		}
	}
}

void ATVIA6522Emulator::SetInterruptFn(const vdfunction<void(bool)>& fn) {
	mInterruptFn = fn;
}

void ATVIA6522Emulator::Reset() {
	mIRA = mPortAInput;
	mIRB = mPortBInput;
	mORB = 0;
	mORA = 0;
	mDDRB = 0;
	mDDRA = 0;
	mT1C = 0;
	mT1L = 0;
	mT2C = 0;
	mT2L = 0;
	mSR = 0;
	mACR = 0;
	mPCR = 0;
	mIFR = 0;
	mIER = 0;
	mTimerPB7 = 0xFF;
	mTimerPB7Mask = 0x00;
	mCA2 = true;
	mCB2 = true;
	mbIrqState = false;
	mbTimer1UnderflowInProgress = false;

	if (mInterruptFn)
		mInterruptFn(false);
	
	mpScheduler->UnsetEvent(mpEventCA2Update);
	mpScheduler->UnsetEvent(mpEventCB2Update);
	mpScheduler->UnsetEvent(mpEventT1Update);

	mT1LastUpdate = mpScheduler->GetTick64();

	UpdateOutput();
}

uint8 ATVIA6522Emulator::DebugReadByte(uint8 address) const {
	switch(address & 15) {
		case 0:
			return (mIRB & ~mDDRB) + (mORB & mDDRB);

		case 1:
			return mIRA;

		case 2:
			return mDDRB;

		case 3:
			return mDDRA;

		case 4:
			return (uint8)mT1C;

		case 5:
			return (uint8)(mT1C >> 8);

		case 6:
			return (uint8)mT1L;

		case 7:
			return (uint8)(mT1L >> 8);

		case 8:
			return (uint8)mT2L;

		case 9:
			return (uint8)(mT2C >> 8);

		case 10:
			return mSR;

		case 11:
			return mACR;

		case 12:
			return mPCR;

		case 13:
			{
				uint8 value = mIER & mIFR;

				if (value)
					value |= 0x80;

				return value;
			}
			break;

		case 14:
			return mIER;

		case 15:
			return mIRA;

		default:
			VDNEVERHERE;
	}
}

uint8 ATVIA6522Emulator::ReadByte(uint8 address) {
	switch(address & 15) {
		case 0:
			// check for read-sensitive modes on CB2
			switch(mPCR & 0xE0) {
				case 0x00:	// input mode, negative transition - clear IFR0
				case 0x40:	// input mode, positive transition - clear IFR0
					ClearIF(kIF_CA2);
					break;

				case 0x80:	// handshake mode - assert CB2
					mpScheduler->SetEvent(1, this, kEventId_CB2Assert, mpEventCB2Update);
					break;
			}

			// clear CB1/CB2 interrupts
			ClearIF(kIF_CB1 | kIF_CB2);

			return (mIRB & ~mDDRB) + (mORB & mDDRB);

		case 1:
			// check for read-sensitive modes on CA2
			switch(mPCR & 0x0E) {
				case 0x00:	// input mode, negative transition - clear IFR0
				case 0x04:	// input mode, positive transition - clear IFR0
					ClearIF(kIF_CA2);
					break;

				case 0x08:	// handshake mode - assert CA2
					mpScheduler->SetEvent(1, this, kEventId_CA2Assert, mpEventCA2Update);
					break;
			}

			// clear CA1/CA2 interrupts
			ClearIF(kIF_CA1 | kIF_CA2);
			return mIRA;

		case 2:
			return mDDRB;

		case 3:
			return mDDRA;

		case 4:
			ClearIF(kIF_T1);
			return (uint8)mT1C;

		case 5:
			return (uint8)(mT1C >> 8);

		case 6:
			return (uint8)mT1L;

		case 7:
			return (uint8)(mT1L >> 8);

		case 8:
			ClearIF(kIF_T2);
			return (uint8)mT2L;

		case 9:
			return (uint8)(mT2C >> 8);

		case 10:
			return mSR;

		case 11:
			return mACR;

		case 12:
			return mPCR;

		case 13:
			return mIFR + ((mIER & mIFR) ? 0x80 : 0x00);

		case 14:
			return mIER;

		case 15:
			return mIRA;

		default:
			VDNEVERHERE;
	}
}

void ATVIA6522Emulator::WriteByte(uint8 address, uint8 value) {
	switch(address & 15) {
		case 0:
			if (mORB != value) {
				uint8 delta = (mORB ^ value) & mDDRB;

				mORB = value;

				if (delta)
					UpdateOutput();
			}

			// check for write-sensitive modes on CB2
			switch(mPCR & 0xE0) {
				case 0x00:	// input mode, negative transition
				case 0x40:	// input mode, positive transition
					ClearIF(kIF_CB2);
					break;

				case 0x80:	// handshake mode
					mpScheduler->SetEvent(1, this, kEventId_CB2Assert, mpEventCB2Update);
					break;
			}

			// clear CB1/CB2 interrupts
			ClearIF(kIF_CB1 | kIF_CB2);
			break;

		case 1:
			if (mORA != value) {
				uint8 delta = (mORA ^ value) & mDDRA;

				mORA = value;

				if (delta)
					UpdateOutput();
			}

			// check for write-sensitive modes on CA2
			switch(mPCR & 0x0E) {
				case 0x00:	// input mode, negative transition
				case 0x04:	// input mode, positive transition
					ClearIF(kIF_CA2);
					break;

				case 0x08:	// handshake mode
					mpScheduler->SetEvent(1, this, kEventId_CA2Assert, mpEventCA2Update);
					break;
			}

			// clear CA1/CA2 interrupts
			ClearIF(kIF_CA1 | kIF_CA2);
			break;

		case 2:
			if (mDDRB != value) {
				uint8 delta = ~mORB & (mDDRB ^ value);

				mDDRB = value;

				if (delta)
					UpdateOutput();
			}
			break;

		case 3:
			if (mDDRA != value) {
				uint8 delta = ~mORA & (mDDRA ^ value);

				mDDRA = value;

				if (delta)
					UpdateOutput();
			}
			break;

		case 4:
		case 6:
			mT1L = (uint16)((mT1L & 0xff00) + value);
			break;

		case 5:
			mT1L = (uint16)((mT1L & 0x00ff) + ((uint32)value << 8));
			mT1C = mT1L;
			ClearIF(kIF_T1);
			break;

		case 7:
			mT1L = (uint16)((mT1L & 0x00ff) + ((uint32)value << 8));
			ClearIF(kIF_T1);
			break;

		case 8:
			mT2L = (uint16)((mT2L & 0xff00) + value);
			break;

		case 9:
			mT2C = (uint16)(mT2L + ((uint32)value << 8));
			ClearIF(kIF_T2);
			break;

		case 10:
			mSR = value;
			break;

		case 11:
			// |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
			// | T1control |T2ctl|  Shift control  |PBLtc|PALtc|
			//
			if (uint8 delta = mACR ^ value) {
				mACR = value;

				// check if PA latch has been enabled
				if (delta & value & 0x01) {
					// latch current port A value
					mIRA = mPortAInput;
				}

				// check if PB latch has been enabled
				if (delta & value & 0x02) {
					// latch current port B value
					mIRB = mPortBInput;
				}

				// if timer 1 PB7 output is being toggled, re-evaluate state
				if (delta & 0x80) {
					UpdateT1Event();
				}

				// check if timer 1 PB7 output has been changed
				if (delta & 0x80) {
					mTimerPB7Mask = value & 0x80;

					UpdateOutput();
				}
			}
			break;

		case 12:
			// |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
			// |       CB2       | CB1 |       CA2       | CA1 |
			//
			// CA2/CB2:
			//	000 = Input, IRQ on negative edge, clear IRQ on read/write
			//	001 = Input, IRQ on negative edge
			//	010 = Input, IRQ on positive edge, clear IRQ on read/write
			//	011 = Input, IRQ on positive edge
			//	100 = Output, set low on read/write, reset on CA1 edge
			//	101 = Output, set low for one cycle on read/write
			//	110 = Output, low
			//	111 = Output, high
			//
			if (uint8 delta = mPCR ^ value) {
				mPCR = value;

				// CB2
				if (delta & 0xE0) {
					switch(value >> 5) {
						case 0:
						case 1:
						case 2:
						case 3:
						case 7:
							mpScheduler->UnsetEvent(mpEventCB2Update);
							mCB2 = true;
							break;

						case 6:
							mpScheduler->UnsetEvent(mpEventCA2Update);
							mCB2 = false;
							break;

						case 4:
						case 5:
						default:
							break;
					}
				}

				// CA2
				if (delta & 0x0E) {
					switch((value >> 1) & 7) {
						case 0:
						case 1:
						case 2:
						case 3:
						case 7:
							mpScheduler->UnsetEvent(mpEventCA2Update);
							mCA2 = true;
							break;

						case 6:
							mpScheduler->UnsetEvent(mpEventCA2Update);
							mCA2 = false;
							break;

						case 4:
						case 5:
						default:
							break;
					}
				}

				UpdateOutput();
			}
			break;

		case 13:
			ClearIF(value);
			break;

		case 14:
			{
				const uint8 mask = (value & 0x7f);

				if (value & 0x80) {
					if (~mIER & mask) {
						mIER |= mask;

						if (!mbIrqState && (mIER & mIFR)) {
							mbIrqState = true;

							if (mInterruptFn)
								mInterruptFn(true);
						}
					}
				} else {
					if (mIER & mask) {
						mIER &= ~mask;

						if (mbIrqState && !(mIER & mIFR)) {
							mbIrqState = false;

							if (mInterruptFn)
								mInterruptFn(false);
						}
					}
				}
			}
			break;

		case 15:
			break;
	}
}

struct ATSaveStateVIA6522 final : public ATSnapExchangeObject<ATSaveStateVIA6522, "ATSaveStateVIA6522"> {
public:
	template<ATExchanger T>
	void Exchange(T& ex);

	uint8 mIRA = 0;
	uint8 mIRB = 0;
	uint8 mORA = 0;
	uint8 mORB = 0;
	uint8 mDDRA = 0;
	uint8 mDDRB = 0;
	uint16 mT1L = 0;
	uint16 mT1C = 0;
	uint16 mT2L = 0;
	uint16 mT2C = 0;
	uint8 mSR = 0;
	uint8 mACR = 0;
	uint8 mPCR = 0;
	uint8 mIFR = 0;
	uint8 mIER = 0;
};

template<ATExchanger T>
void ATSaveStateVIA6522::Exchange(T& ex) {
	ex.Transfer("arch_ira", &mIRA);
	ex.Transfer("arch_irb", &mIRB);

	ex.Transfer("arch_ora", &mORA);
	ex.Transfer("arch_orb", &mORB);
	ex.Transfer("arch_ddra", &mDDRA);
	ex.Transfer("arch_ddrb", &mDDRB);

	ex.Transfer("arch_t1l", &mT1L);
	ex.Transfer("arch_t1c", &mT1C);
	ex.Transfer("arch_t2l", &mT2L);
	ex.Transfer("arch_t2c", &mT2C);

	ex.Transfer("arch_sr", &mSR);
	ex.Transfer("arch_acr", &mACR);
	ex.Transfer("arch_pcr", &mPCR);

	ex.Transfer("arch_ifr", &mIFR);
	ex.Transfer("arch_ier", &mIER);
}

void ATVIA6522Emulator::LoadState(const IATObjectState *state) {
	Reset();

	if (!state)
		return;

	const auto& viastate = atser_cast<const ATSaveStateVIA6522&>(*state);

	mORA = viastate.mORA;
	mORB = viastate.mORB;
	mDDRA = viastate.mDDRA;
	mDDRB = viastate.mDDRB;

	mT1L = viastate.mT1L;
	mT1C = viastate.mT1C;
	mT2L = viastate.mT2L;
	mT2C = viastate.mT2C;

	mSR = viastate.mSR;
	mACR = viastate.mACR;

	if (mACR & 0x01)
		mIRA = viastate.mIRA;
	else
		mIRA = mPortAInput;

	if (mACR & 0x02)
		mIRB = viastate.mIRB;
	else
		mIRB = mPortBInput;

	// Invoke write path to ensure invariants are upheld for PCR and IER
	WriteByte(12, viastate.mPCR);

	mIFR = viastate.mIFR & 0x7F;
	WriteByte(14, viastate.mIER | 0x80);
	WriteByte(14, ~viastate.mIER & 0x7F);

	UpdateOutput();
}

vdrefptr<IATObjectState> ATVIA6522Emulator::SaveState() {
	vdrefptr viastate { new ATSaveStateVIA6522 };

	viastate->mIRA = mIRA;
	viastate->mIRB = mIRB;

	viastate->mORA = mORA;
	viastate->mORB = mORB;
	viastate->mDDRA = mDDRA;
	viastate->mDDRB = mDDRB;

	viastate->mT1L = mT1L;
	viastate->mT1C = mT1C;
	viastate->mT2L = mT2L;
	viastate->mT2C = mT2C;
	viastate->mSR = mSR;
	viastate->mACR = mACR;
	viastate->mPCR = mPCR;

	// encode derived IFR bit 7 for consistency with spec even though we don't use it
	viastate->mIFR = DebugReadByte(13);

	viastate->mIER = mIER;

	return viastate;
}

void ATVIA6522Emulator::OnScheduledEvent(uint32 id) {
	switch(id) {
		case kEventId_CA2Assert:
			mpEventCA2Update = nullptr;
			if (mCA2) {
				mCA2 = false;
				UpdateOutput();
			}
			break;

		case kEventId_CA2Deassert:
			mpEventCA2Update = nullptr;
			if (!mCA2) {
				mCA2 = true;
				UpdateOutput();
			}
			break;

		case kEventId_CB2Assert:
			mpEventCB2Update = nullptr;
			if (mCB2) {
				mCB2 = false;
				UpdateOutput();
			}
			break;

		case kEventId_CB2Deassert:
			mpEventCB2Update = nullptr;
			if (!mCB2) {
				mCB2 = true;
				UpdateOutput();
			}
			break;

		case kEventId_T1Update:
			mpEventT1Update = nullptr;

			UpdateT1Event();
			break;
	}
}

void ATVIA6522Emulator::SetIF(uint8 mask) {
	if (~mIFR & mask) {
		mIFR |= mask;

		// if CA1 or CB1 is being set and latching is enabled, update the latch
		if (mask & 0x02) {
			if (mACR & 0x01)
				mIRA = mPortAInput;
		}

		if (mask & 0x10) {
			if (mACR & 0x02)
				mIRB = mPortBInput;
		}

		if (!mbIrqState && (mIFR & mIER)) {
			mbIrqState = true;

			if (mInterruptFn)
				mInterruptFn(true);
		}
	}
}

void ATVIA6522Emulator::ClearIF(uint8 mask) {
	if (mIFR & mask) {
		mIFR &= ~mask;

		if (mbIrqState && !(mIFR & mIER)) {
			mbIrqState = false;

			if (mInterruptFn)
				mInterruptFn(false);
		}

		// if T1 flag is being disabled, we have to re-enable active T1
		if (mask & kIF_T1)
			UpdateT1Event();
	}
}

uint32 ATVIA6522Emulator::ComputeOutput() const {
	uint8 porta = mORA | ~mDDRA;
	uint8 portb = mORB | ~mDDRB;

	portb ^= (portb ^ mTimerPB7) & mTimerPB7Mask;

	uint32 val = ((uint32)portb << 8) + porta;

	if (mCA2)
		val |= kATVIAOutputBit_CA2;

	if (mCB2)
		val |= kATVIAOutputBit_CB2;

	return val;
}

void ATVIA6522Emulator::UpdateOutput() {
	const uint32 val = ComputeOutput();

	if (mCurrentOutput != val) {
		mCurrentOutput = val;

		if (mpOutputFn)
			mpOutputFn(mpOutputFnData, val);
	}
}

void ATVIA6522Emulator::UpdateT1Event() {
	// If the T1 IRQ flag is already set and PB7 output is disabled, we don't
	// need the timer.
	if (!(mACR & 0x80) && (mIFR & kIF_T1)) {
		mpScheduler->UnsetEvent(mpEventT1Update);
		return;
	}

	UpdateT1State();
	
	// If an underflow is in progress, the counter is 0xFFFF and in the process
	// of being reloaded from the latch. We have to distinguish this from an
	// actual counter value of 0xFFFF loaded from a latch value of 0xFFFF.
	mpScheduler->SetEvent(mbTimer1UnderflowInProgress ? mT1L + 2 : mT1C + 1, this, kEventId_T1Update, mpEventT1Update);
}

void ATVIA6522Emulator::UpdateT1State() {
	const uint64 t = mpScheduler->GetTick64();
	uint64 dt = t - mT1LastUpdate;
	if (!dt)
		return;

	mT1LastUpdate = t;

	// if not enough cycles have passed to underflow, just decrement the counter
	// and return
	if (mT1C >= dt) {
		mT1C -= dt;
		return;
	}

	// consume cycles to drop the counter to 0
	dt -= mT1C;

	// decrement 1 cycle for underflow
	--dt;

	// Decrement 1 cycle for underflow, then split the remaining cycles into
	// whole loops and fractional loops. The behavior emulated here is that on
	// an underflow, the counter steps one additional time to $FFFF before
	// being reloaded from the latch. This means that for T1L = N, the period
	// is N+2 cycles.
	//
	// Note that it is assumed that the counter is reloaded from the latch
	// even in one-shot mode. This contradicts the MOS, Rockwell, and WDC
	// datasheets which say it should begin counting down from $FFFF, but
	// actual testing on Rockwell 6522s has confirmed reload from latch for
	// both modes:
	//
	// http://forum.6502.org/viewtopic.php?f=4&t=2901

	uint32 cycles = (uint32)(dt % (mT1C + 1));
	uint32 loops = (uint32)(dt / (mT1C + 1));

	// update counter
	if (cycles) {
		mbTimer1UnderflowInProgress = false;
		mT1C = (uint16)(mT1L - (cycles - 1));
	} else {
		mbTimer1UnderflowInProgress = true;
		mT1C = 0xFFFF;
	}

	// check if timer 1 is in one-shot or free-running mode
	if (mACR & 0x40) {
		// timer 1 is in free-run mode -- toggle PB7 according to number of
		// underflows, which is cycles+1
		if (!(loops & 1)) {
			mTimerPB7 ^= 0x80;

			if (mTimerPB7Mask)
				UpdateOutput();
		}

		// assert timer /IRQ, since we're guaranteed that at least one underflow
		// has occurred
		SetIF(kIF_T1);
	} else {
		// timer 1 is in one-shot mode -- raise PB7
		if (!(mTimerPB7 & 0x80)) {
			mTimerPB7 |= 0x80;

			if (mTimerPB7Mask)
				UpdateOutput();

			// assert timer /IRQ only if PB7 has changed, to mimic one-shot
			// behavior
			SetIF(kIF_T1);
		}
	}
}
