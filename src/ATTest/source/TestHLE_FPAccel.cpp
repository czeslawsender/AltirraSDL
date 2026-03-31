//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2026 Avery Lee
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
#include <vd2/system/file.h>
#include <vd2/system/vdalloc.h>
#include <at/atcpu/breakpoints.h>
#include <at/atcpu/co6502.h>
#include <at/atcpu/memorymap.h>
#include <at/atcore/decmath.h>
#include <at/atcore/ksyms.h>
#include "decmath.h"
#include "test.h"

#pragma optimize("", off)

namespace {
	template<typename T>
	class TimeScope {
	public:
		TimeScope(const T& fn) : mFn(fn) {};
		~TimeScope() { mFn(); }

	private:
		const T& mFn;
	};
}

int ATTestHelper_HLE_FPAccel(bool bench, bool check) {
	using namespace ATKernelSymbols;
	using AccelFn = void(*)(IATFPAccelContext& ctx);

	int errors = 0;

	ATScheduler sch;
	vdautoptr cpu(new ATCoProc6502(false, true));
	cpu->SetBreakOnUnsupportedOpcode(true);

	const wchar_t *args = ATTestGetArguments();
	if (!*args)
		throw VDException("Missing test argument: reference OS ROM");

	vdautoarrayptr<uint8> osrom(new uint8[0x4000]);

	memset(osrom.get(), 0xFF, 0x4000);

	{
		VDFile f(args);
		auto actual = f.readData(osrom.get(), 0x4000);

		if (actual < 0)
			throw VDException("Failed to read OS ROM");

		if (actual < 0x4000) {
			memmove(osrom.get() + 0x4000 - actual, osrom.get(), actual);
			memset(osrom.get(), 0xFF, 0x4000 - actual);
		}

		f.close();
	}
	
	vdautoarrayptr<uint8> ram(new uint8[0x10000]);
	memset(ram.get(), 0xFF, 0x10000);

	ATCoProcMemoryMapView memView(cpu->GetReadMap(), cpu->GetWriteMap(), cpu->GetTraceMap());
	memView.SetMemory(0x00, 0x100, ram.get());
	memView.SetReadMemTraceable(0xC0, 0x10, osrom.get());
	memView.SetReadMemTraceable(0xD8, 0x28, osrom.get() + 0x1800);

	class TestAccelContext final : public IATFPAccelContext {
	public:
		TestAccelContext(uint8 *mem) : mpMem(mem) {}

		void SetFlagC() override {
			mbCarry = true;
		}

		void ClearFlagC() override {
			mbCarry = false;
		}

		uint8 ReadByte(uint16 addr) const override {
			return mpMem[addr];
		}

		void WriteByte(uint16 addr, uint8 value) override {
			mpMem[addr] = value;
		}

		ATDecFloat GetFR0() const {
			ATDecFloat v;
			memcpy(&v, mpMem + FR0, 6);

			return v;
		}

		ATDecFloatOpt GetResult() const {
			return mbCarry ? ATDecFloatOpt() : ATDecFloatOpt(GetFR0());
		}

		void SetFR0(const ATDecFloat& v) {
			memcpy(mpMem + FR0, &v, 6);
		}

		void SetFR1(const ATDecFloat& v) {
			memcpy(mpMem + FR1, &v, 6);
		}

		uint8 *const mpMem;
		bool mbCarry = false;
	} ctx { ram.get() };

	class BCDRandGen {
	public:
		ATDecFloat operator()() {
			ATDecFloat v;

			v.mSignExp = 0x30 + (RandByte() & 0x9F);

			do {
				v.mMantissa[0] = RandBCDByte();
			} while(v.mMantissa[0] == 0);

			for(int i = 1; i < 5; ++i)
				v.mMantissa[i] = RandBCDByte();

			return v;
		}

		ATDecFloat RandSignedUnit() {
			ATDecFloat v;

			v.mSignExp = 0x3F + (RandBCDByte() & 0x80);

			for(int i = 0; i < 5; ++i)
				v.mMantissa[i] = RandBCDByte();

			for(int i = 0; i < 5 && !v.mMantissa[0]; ++i) {
				for(int j = 0; j < 4; ++j)
					v.mMantissa[j] = v.mMantissa[j] + 1;

				v.mMantissa[4] = RandBCDByte();

				--v.mSignExp;
			}

			if (!v.mMantissa[0])
				v.mSignExp = 0;

			return v;
		}

		uint8 RandByte() {
			uint8 v = (uint8)mSeed;

			mSeed = (mSeed << 8) | (((mSeed >> 16) ^ (mSeed >> 17) ^ (mSeed >> 21) ^ (mSeed >> 23)) & 0xFF);

			return v;
		}

		uint8 RandBCDByte() {
			uint8 v;

			do {
				v = RandByte();
			} while((v & 0xF0) >= 0xA0 || (v & 0x0F) >= 0x0A);

			return v;
		}

	private:
		uint32 mSeed = 1;
	};

	uint32 startTime = 0;
	uint32 callCount = 0;

	const auto beginScope = [&] {
		callCount = 0;
		startTime = sch.GetTick();
	};

	const auto endScope = [&] {
		if (bench && callCount)
			printf("    %u calls (%.1f cycles/call average)\n", callCount, (double)(sch.GetTick() - startTime) / (double)callCount);
	};

	const auto makeTimeScope = [&] {
		beginScope();
		return TimeScope(endScope);
	};

	const auto invoke = [&](uint16 addr, bool trace = false) -> bool {
		cpu->Jump(addr);
		cpu->SetS(0xFD);
		cpu->SetP(0);

		ram[0x600] = 0x02;
		ram[0x1FF] = 0x05;
		ram[0x1FE] = 0xFF;

		uint32 baseTick = sch.GetTick();
		sch.SetStopTime(baseTick + 10000000);

		if (trace) {
			vdfastvector<bool> bptable(65536, true);

			class BPHandler final : public IATCPUBreakpointHandler {
			public:
				bool CheckBreakpoint(uint32 pc) override { return true; }
			} bphandler;

			cpu->SetBreakpointMap(bptable.data(), &bphandler);

			for(;;) {
				if (cpu->Run(sch))
					throw VDException("Max runtime exceeded while invoking after %u cycles: %04X | PC=%04X S=%02X", (unsigned)(sch.GetTick() - baseTick), addr, cpu->GetPC(), cpu->GetS());

				if (cpu->GetPC() == 0x600)
					break;

				const uint8 p = cpu->GetP();

				printf("PC=%04X A=%02X X=%02X Y=%02X S=%02X P=%02X (%c%c%c%c%c%c)\n"
					, cpu->GetPC()
					, cpu->GetA()
					, cpu->GetX()
					, cpu->GetY()
					, cpu->GetS()
					, p
					, p & 0x80 ? 'N' : '-'
					, p & 0x40 ? 'V' : '-'
					, p & 0x08 ? 'D' : '-'
					, p & 0x04 ? 'I' : '-'
					, p & 0x02 ? 'Z' : '-'
					, p & 0x01 ? 'C' : '-'
				);
			}

			cpu->SetBreakpointMap(nullptr, nullptr);
		} else {
			cpu->SetBreakpointMap(nullptr, nullptr);

			if (cpu->Run(sch))
				throw ATTestAssertionException("Max runtime exceeded while invoking after %u cycles: %04X | PC=%04X S=%02X", (unsigned)(sch.GetTick() - baseTick), addr, cpu->GetPC(), cpu->GetS());
		}

		if (cpu->GetPC() != 0x600)
			throw ATTestAssertionException("CPU crash while invoking: %04X", addr);

		if (cpu->GetP() & 8)
			throw ATTestAssertionException("Exit with decimal mode enabled while invoking: %04X", addr);

		const bool carry = (cpu->GetP() & 1) != 0;

		ctx.mbCarry = carry;

		++callCount;

		return carry;
	};

	//==========================================================================
	ATTestTrace("Testing IFP");

	const auto testIFP = [&](uint16 ival, const ATDecFloat& fvalref) {
		for(int i=0; i<2; ++i) {
			ram[FR0] = (uint8)ival;
			ram[FR0+1] = (uint8)(ival >> 8);

			if (i)
				ATAccelIFP(ctx);
			else
				invoke(IFP);

			ATDecFloat fval = ATDecFloat::FromBytes(&ram[FR0]);

			if (fval != fvalref)
				throw ATTestAssertionException("IPF test failed for $%04X: expected %s, got %s", ival, fvalref.ToString().c_str(), fval.ToString().c_str());
		}
	};

	try {
		auto timeScope = makeTimeScope();

		for(int i=0; i<256; ++i)
			testIFP(i, ATDecFloat::FromDouble((double)i));
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	try {
		auto timeScope = makeTimeScope();

		for(int i=256; i<65536; ++i)
			testIFP(i, ATDecFloat::FromDouble((double)i));
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}
	
	//==========================================================================
	ATTestTrace("Testing FPI");

	const auto testFPI = [&](double fval, int ivalref) {
		ATDecFloat fval2 = ATDecFloat::FromDouble(fval);

		for(int i=0; i<2; ++i) {
			memcpy(&ram[FR0], &fval2, 6);

			bool err;

			if (i) {
				ATAccelFPI(ctx);
				err = ctx.mbCarry;
			} else
				err = invoke(FPI);

			int ival = err ? -1 : VDReadUnalignedLEU16(&ram[FR0]);

			if (check && ival != ivalref)
				throw ATTestAssertionException("FPI accel=%d test failed for %s: expected %d, got %d", i, fval2.ToString().c_str(), ivalref, ival);
		}
	};

	try {
		auto timeScope = makeTimeScope();

		testFPI(-0.1, -1);

		for(int i=0; i<65536; ++i) {
			testFPI((double)i, i);
			testFPI((double)i + 0.49, i);

			// The math pack will "round" up [65535.5, 65536) to 0 with no error.
			testFPI((double)i + 0.50, (i + 1) & 0xFFFF);
			testFPI((double)i + 0.51, (i + 1) & 0xFFFF);
		}

		testFPI(65536.0, -1);
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	//==========================================================================
	ATTestTrace("Testing FASC");

	const auto testFASC = [&](const ATDecFloat& fval, int offset, const char *str) {
		for(int i=0; i<2; ++i) {
			memcpy(&ram[FR0], &fval, 6);
			VDWriteUnalignedLEU16(&ram[INBUFF], 0);

			if (i)
				ATAccelFASC(ctx);
			else
				invoke(FASC);

			const uint16 inbuff = VDReadUnalignedLEU16(&ram[INBUFF]);

			char buf[64];

			for(int i=0; i<64; ++i) {
				uint8 c = ram[(inbuff + i) & 0xFFFF];
				uint8 ch = c & 0x7F;

				if (ch < 0x20 || ch >= 0x7F)
					ch = '?';

				buf[i] = (char)ch;

				if ((c & 0x80) || i == 62) {
					buf[i + 1] = 0;
					break;
				}
			}

			if (check && strcmp(buf, str))
				throw ATTestAssertionException("FASC accel=%d failed for %s: expected \"%s\", was \"%s\"", i, fval.ToString().c_str(), buf, str);

			if (check && inbuff != offset + LBUFF)
				throw ATTestAssertionException("FASC accel=%d failed for %s \"%s\": INBUFF expected %04X, was %04X", i, fval.ToString().c_str(), str, offset + LBUFF, inbuff);
		}
	};

	try {
		auto timeScope = makeTimeScope();

		testFASC(ATDecFloat::FromDouble(0.0), 0, "0");
		testFASC(ATDecFloat::FromDouble(0.1), -1, "0.1");
		testFASC(ATDecFloat::FromDouble(0.11), -1, "0.11");
		testFASC(ATDecFloat::FromDouble(0.01), -1, "0.01");
		testFASC(ATDecFloat::FromDouble(1.0), 1, "1");
		testFASC(ATDecFloat::FromDouble(1.1), 1, "1.1");
		testFASC(ATDecFloat::FromDouble(1.23456789), 1, "1.23456789");
		testFASC(ATDecFloat::FromDouble(10.0), 0, "10");
		testFASC(ATDecFloat::FromDouble(12.34567891), 0, "12.34567891");
		testFASC(ATDecFloat::FromDouble(100.0), 1, "100");
		testFASC(ATDecFloat::FromDouble(1000.0), 0, "1000");
		testFASC(ATDecFloat::FromDouble(10000.0), 1, "10000");
		testFASC(ATDecFloat::FromDouble(100000.0), 0, "100000");
		testFASC(ATDecFloat::FromDouble(1000000.0), 1, "1000000");
		testFASC(ATDecFloat::FromDouble(10000000.0), 0, "10000000");
		testFASC(ATDecFloat::FromDouble(100000000.0), 1, "100000000");
		testFASC(ATDecFloat::FromDouble(1000000000.0), 0, "1000000000");
		testFASC(ATDecFloat::FromDouble(10000000000.0), 1, "1E+10");
		testFASC(ATDecFloat::FromDouble(12345678900.0), 1, "1.23456789E+10");
		testFASC(ATDecFloat::FromDouble(100000000000.0), 0, "1.0E+11");
		testFASC(ATDecFloat::FromDouble(110000000000.0), 0, "1.1E+11");
		testFASC(ATDecFloat::FromDouble(123000000000.0), 0, "1.23E+11");
		testFASC(ATDecFloat::FromDouble(123456789100.0), 0, "1.234567891E+11");
	
		testFASC(ATDecFloat::FromDouble(-0.1), -2, "-0.1");
		testFASC(ATDecFloat::FromDouble(-0.11), -2, "-0.11");
		testFASC(ATDecFloat::FromDouble(-0.01), -2, "-0.01");
		testFASC(ATDecFloat::FromDouble(-1.0), 0, "-1");
		testFASC(ATDecFloat::FromDouble(-10.0), -1, "-10");
		testFASC(ATDecFloat::FromDouble(-100.0), 0, "-100");
		testFASC(ATDecFloat::FromDouble(-1000.0), -1, "-1000");
		testFASC(ATDecFloat::FromDouble(-10000.0), 0, "-10000");
		testFASC(ATDecFloat::FromDouble(-100000.0), -1, "-100000");
		testFASC(ATDecFloat::FromDouble(-1000000.0), 0, "-1000000");
		testFASC(ATDecFloat::FromDouble(-10000000.0), -1, "-10000000");
		testFASC(ATDecFloat::FromDouble(-100000000.0), 0, "-100000000");
		testFASC(ATDecFloat::FromDouble(-1000000000.0), -1, "-1000000000");
		testFASC(ATDecFloat::FromDouble(-10000000000.0), 0, "-1E+10");
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}
	
	//==========================================================================
	ATTestTrace("Testing FADD");

	const auto testFADD = [&](const ATDecFloat& fvalx, const ATDecFloat& fvaly, const ATDecFloat& fvalr) {
		for(int i=0; i<2; ++i) {
			memcpy(&ram[FR0], &fvalx, 6);
			memcpy(&ram[FR1], &fvaly, 6);

			ram[FR0 + 6] = 0x55;
			ram[FR0 + 7] = 0x55;
			ram[FR1 + 6] = 0x55;
			ram[FR1 + 7] = 0x55;

			if (i)
				ATAccelFADD(ctx);
			else
				invoke(FADD);

			ATDecFloat fvalz;
			memcpy(&fvalz, &ram[FR0], sizeof fvalz);

			if (check && fvalz != fvalr)
				throw ATTestAssertionException("FADD accel=%d failed for %s + %s: expected %s, was %s", i, fvalx.ToString().c_str(), fvaly.ToString().c_str(), fvalr.ToString().c_str(), fvalz.ToString().c_str());
		}
	};

	try {
		auto timeScope = makeTimeScope();

		testFADD(ATDecFloat::FromDouble(0.0), ATDecFloat::FromDouble(0.0), ATDecFloat::FromDouble(0.0));
		testFADD(ATDecFloat::FromDouble(0.0), ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(1.0));
		testFADD(ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(0.0), ATDecFloat::FromDouble(1.0));
		testFADD(ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(2.0));
		testFADD(ATDecFloat::FromDouble(0.0), ATDecFloat::FromDouble(-1.0), ATDecFloat::FromDouble(-1.0));
		testFADD(ATDecFloat::FromDouble(-1.0), ATDecFloat::FromDouble(0.0), ATDecFloat::FromDouble(-1.0));
		testFADD(ATDecFloat::FromDouble(-1.0), ATDecFloat::FromDouble(-1.0), ATDecFloat::FromDouble(-2.0));
		testFADD(ATDecFloat::FromDouble(99.0), ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(100.0));
		testFADD(ATDecFloat::FromDouble(-99.0), ATDecFloat::FromDouble(-1.0), ATDecFloat::FromDouble(-100.0));
		testFADD(ATDecFloat::FromDouble(-1.0), ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(0.0));
		testFADD(ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(-1.0), ATDecFloat::FromDouble(0.0));

		// test denormalization
		testFADD(ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(-0.00000001), ATDecFloat::FromDouble(0.99999999));
		testFADD(ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(-0.99999999), ATDecFloat::FromDouble(0.00000001));

		// test normalization
		testFADD(ATDecFloat::FromDouble(9999999999.0), ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(10000000000.0));

		// test underflow to zero
		testFADD(ATDecFloat::FromDouble(1.0002e-98), ATDecFloat::FromDouble(-1.001e-98), ATDecFloat::FromDouble(0));
		testFADD(ATDecFloat::FromDouble(-1.0002e-98), ATDecFloat::FromDouble(1.001e-98), ATDecFloat::FromDouble(0));

		// test truncation behavior
		testFADD(ATDecFloat::FromDouble(1.0), ATDecFloat::FromDouble(0.000000009999999999), ATDecFloat::FromDouble(1.0));
		testFADD(ATDecFloat::FromDouble(-1.0), ATDecFloat::FromDouble(-0.000000009999999999), ATDecFloat::FromDouble(-1.0));

		{
			BCDRandGen rg;

			for(int i=0; i<100000; ++i) {
				ATDecFloat valx = rg();
				ATDecFloat valy = rg();

				memcpy(&ram[FR0], &valx, 6);
				memcpy(&ram[FR1], &valy, 6);

				invoke(FADD);

				ATDecFloat valr1;
				memcpy(&valr1, &ram[FR0], 6);

				memcpy(&ram[FR0], &valx, 6);
				memcpy(&ram[FR1], &valy, 6);

				ATAccelFADD(ctx);

				ATDecFloat valr2;
				memcpy(&valr2, &ram[FR0], 6);

				if (check && valr1 != valr2)
					throw ATTestAssertionException("FADD differ for %s + %s: unaccel %s, accel %s", valx.ToString().c_str(), valy.ToString().c_str(), valr1.ToString().c_str(), valr2.ToString().c_str());
			}
		}
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	//==========================================================================
	ATTestTrace("Testing FMUL");

	const auto testFMUL = [&](double valx, double valy, double valr) {
		const ATDecFloat fvalx = ATDecFloat::FromDouble(valx);
		const ATDecFloat fvaly = ATDecFloat::FromDouble(valy);
		const ATDecFloat fvalr = ATDecFloat::FromDouble(valr);

		for(int i=0; i<2; ++i) {
			memcpy(&ram[FR0], &fvalx, 6);
			memcpy(&ram[FR1], &fvaly, 6);

			if (i)
				ATAccelFMUL(ctx);
			else
				invoke(FMUL);

			ATDecFloat fvalz;
			memcpy(&fvalz, &ram[FR0], sizeof fvalz);

			if (check && fvalz != fvalr)
				throw ATTestAssertionException("FMUL accel=%d failed for %s * %s: expected %s, was %s", i, fvalx.ToString().c_str(), fvaly.ToString().c_str(), fvalr.ToString().c_str(), fvalz.ToString().c_str());
		}
	};

	try {
		auto timeScope = makeTimeScope();

		for(int i = -10; i <= 10; ++i) {
			for(int j = -10; j <= 10; ++j)
				testFMUL((double)i, (double)j, (double)(i * j));
		}

		testFMUL(1.11111111, 1.11111111, 1.23456789);
		testFMUL(3.14159265, 3.14159265, 9.86960437);

		{
			BCDRandGen rg;

			for(int i=0; i<10000; ++i) {
				ATDecFloat valx = rg();
				ATDecFloat valy = rg();

				if (!valx.mMantissa[0] || !valy.mMantissa[0])
					continue;

				memcpy(&ram[FR0], &valx, 6);
				memcpy(&ram[FR1], &valy, 6);

				invoke(FMUL);

				ATDecFloat valr1;
				memcpy(&valr1, &ram[FR0], 6);

				memcpy(&ram[FR0], &valx, 6);
				memcpy(&ram[FR1], &valy, 6);

				ATAccelFMUL(ctx);

				ATDecFloat valr2;
				memcpy(&valr2, &ram[FR0], 6);

				if (check && valr1 != valr2)
					throw ATTestAssertionException("FMUL differ for %s / %s: unaccel %s, accel %s", valx.ToString().c_str(), valy.ToString().c_str(), valr1.ToString().c_str(), valr2.ToString().c_str());
			}
		}
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	//==========================================================================
	ATTestTrace("Testing FDIV");

	const auto testFDIV = [&](double valx, double valy, double valr) {
		const ATDecFloat fvalx = ATDecFloat::FromDouble(valx);
		const ATDecFloat fvaly = ATDecFloat::FromDouble(valy);
		const ATDecFloat fvalr = ATDecFloat::FromDouble(valr);

		for(int i=0; i<2; ++i) {
			memcpy(&ram[FR0], &fvalx, 6);
			memcpy(&ram[FR1], &fvaly, 6);

			if (i)
				ATAccelFDIV(ctx);
			else
				invoke(FDIV);

			ATDecFloat fvalz;
			memcpy(&fvalz, &ram[FR0], sizeof fvalz);

			if (check && fvalz != fvalr)
				throw ATTestAssertionException("FDIV accel=%d failed for %s / %s: expected %s, was %s", i, fvalx.ToString().c_str(), fvaly.ToString().c_str(), fvalr.ToString().c_str(), fvalz.ToString().c_str());
		}
	};

	try {
		auto timeScope = makeTimeScope();

		testFDIV(1.11111111, 1.11111111, 1.0);
		testFDIV(3.14159265, 3.14159265, 1.0);
		testFDIV(2.0, 3.0, 0.6666666666);
		testFDIV(20.0, 30.0, 0.6666666666);
		testFDIV(20.0, 3.0, 6.66666666);
		testFDIV(2.0, 30.0, 0.0666666666);
		testFDIV(1.0, 3.14159265, 3.183098865E-01);
		testFDIV(1.0, 1.11111111, 9.000000009E-01);
		testFDIV(3.14159265, 2.79, 1.12601887);

		// test underflow behavior
		testFDIV( 1.0e-98,  1.0,  1.0e-98);
		testFDIV(-1.0e-98,  1.0, -1.0e-98);
		testFDIV( 1.0e-98, -1.0, -1.0e-98);
		testFDIV(-1.0e-98, -1.0,  1.0e-98);

		testFDIV( 1.0e-98,  1.01, 0);
		testFDIV(-1.0e-98,  1.01, 0);
		testFDIV( 1.0e-98, -1.01, 0);
		testFDIV(-1.0e-98, -1.01, 0);

		{
			BCDRandGen rg;

			for(int i=0; i<10000; ++i) {
				ATDecFloat valx = rg();
				ATDecFloat valy = rg();

				ctx.SetFR0(valx);
				ctx.SetFR1(valy);

				invoke(FDIV);

				ATDecFloat valr1 = ctx.GetFR0();

				ctx.SetFR0(valx);
				ctx.SetFR1(valy);

				ATAccelFDIV(ctx);

				ATDecFloat valr2 = ctx.GetFR0();

				if (check && valr1 != valr2)
					throw ATTestAssertionException("FDIV differs for %s / %s: unaccel %s, accel %s", valx.ToString().c_str(), valy.ToString().c_str(), valr1.ToString().c_str(), valr2.ToString().c_str());
			}
		}
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	//==========================================================================
	ATTestTrace("Testing EXP10");

	const auto testEXP10 = [&](double valx) {
		const ATDecFloat fvalx = ATDecFloat::FromDouble(valx);

		ctx.SetFR0(fvalx);
		invoke(EXP10);

		ATDecFloat fvalr1 = ctx.GetFR0();

		ctx.SetFR0(fvalx);
		ATAccelEXP10(ctx);

		ATDecFloat fvalr2 = ctx.GetFR0();

		if (check && fvalr1 != fvalr2)
			throw ATTestAssertionException("EXP10 differs for %s: unaccel %s, accel %s", fvalx.ToString().c_str(), fvalr1.ToString().c_str(), fvalr2.ToString().c_str());
	};

	try {
		auto timeScope = makeTimeScope();

		testEXP10(0.0);
		testEXP10(0.5);
		testEXP10(1.0);
		testEXP10(-0.5);
		testEXP10(-1.0);
		testEXP10(-98.0);
		testEXP10(-99.0);

		{
			BCDRandGen rg;
			const ATDecFloat scale = ATDecFloat::FromDouble(99.0);
			const ATDecFloat tiny = ATDecFloat::FromDouble(-98.0);

			for(int i=0; i<10000; ++i) {
				ATDecFloat valx = (rg() * scale).Value();

				if (valx >= scale)
					continue;

				ctx.SetFR0(valx);

				ctx.mbCarry = invoke(EXP10);
				ATDecFloatOpt valr1 = ctx.GetResult();

				ctx.SetFR0(valx);

				ATAccelEXP10(ctx);
				ATDecFloatOpt valr2 = ctx.GetResult();

				// The underflow case is rather tricky to match as the original
				// math pack is inconsistent, so we allow either an error or
				// zero.
				if (valx < tiny) {
					if (!valr1.IsValid())
						valr1 = ATDecFloat::Zero();

					if (!valr2.IsValid())
						valr2 = ATDecFloat::Zero();
				}

				if (check && valr1 != valr2) {
					throw ATTestAssertionException("EXP10[%d] differs for %s: unaccel %s, accel %s", i, valx.ToString().c_str(), valr1.ToString().c_str(), valr2.ToString().c_str());
				}
			}
		}
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	//==========================================================================
	ATTestTrace("Testing EXP");

	const auto testEXP = [&](double valx, bool trace = false) {
		const ATDecFloat fvalx = ATDecFloat::FromDouble(valx);

		ctx.SetFR0(fvalx);
		invoke(EXP, trace);

		auto fvalr1 = ctx.GetResult();

		ctx.SetFR0(fvalx);
		ATAccelEXP(ctx);

		auto fvalr2 = ctx.GetResult();

		if (check && fvalr1 != fvalr2)
			throw ATTestAssertionException("EXP differs for %s: unaccel %s, accel %s", fvalx.ToString().c_str(), fvalr1.ToString().c_str(), fvalr2.ToString().c_str());
	};

	try {
		auto timeScope = makeTimeScope();

		testEXP(0.0);
		testEXP(0.5);
		testEXP(1.0);
		testEXP(1.5);
		testEXP(2.0);
		testEXP(-0.5);
		testEXP(-1.0);
		testEXP(-1.5);
		testEXP(-2.0);
		testEXP(231.0);
		testEXP(-231.0);

		{
			BCDRandGen rg;
			const ATDecFloat scale = ATDecFloat::FromDouble(231.0);
			const ATDecFloat tiny = ATDecFloat::FromDouble(-231.0);

			for(int i=0; i<10000; ++i) {
				ATDecFloat valx = (rg.RandSignedUnit() * scale).Value();

				ctx.SetFR0(valx);

				ctx.mbCarry = invoke(EXP);
				ATDecFloatOpt valr1 = ctx.GetResult();

				ctx.SetFR0(valx);

				ATAccelEXP(ctx);
				ATDecFloatOpt valr2 = ctx.GetResult();

				// The underflow case is rather tricky to match as the original
				// math pack is inconsistent, so we allow either an error or
				// zero.
				if (valx < tiny) {
					if (!valr1.IsValid())
						valr1 = ATDecFloat::Zero();

					if (!valr2.IsValid())
						valr2 = ATDecFloat::Zero();
				}

				if (check && valr1 != valr2) {
					throw ATTestAssertionException("EXP[%d] differs for %s: unaccel %s, accel %s", i, valx.ToString().c_str(), valr1.ToString().c_str(), valr2.ToString().c_str());
				}
			}
		}
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	//==========================================================================
	ATTestTrace("Testing LOG10");

	try {
		auto timeScope = makeTimeScope();

		{
			BCDRandGen rg;
			const ATDecFloat scale = ATDecFloat::FromDouble(65535.0);
			const ATDecFloat tiny = ATDecFloat::FromDouble(-100.0);

			for(int i=0; i<10000; ++i) {
				ATDecFloat valx = rg().Abs();

				ctx.SetFR0(valx);

				ctx.mbCarry = invoke(LOG10);
				ATDecFloatOpt valr1 = ctx.GetResult();

				ctx.SetFR0(valx);

				ATAccelLOG10(ctx);
				ATDecFloatOpt valr2 = ctx.GetResult();

				if (check && valr1 != valr2) {
					throw ATTestAssertionException("LOG10[%d] differs for %s: unaccel %s, accel %s", i, valx.ToString().c_str(), valr1.ToString().c_str(), valr2.ToString().c_str());
				}
			}
		}
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	//==========================================================================
	ATTestTrace("Testing LOG");

	try {
		auto timeScope = makeTimeScope();

		{
			BCDRandGen rg;

			for(int i=0; i<10000; ++i) {
				ATDecFloat valx = rg().Abs();

				ctx.SetFR0(valx);

				ctx.mbCarry = invoke(LOG);
				ATDecFloatOpt valr1 = ctx.GetResult();

				ctx.SetFR0(valx);

				ATAccelLOG(ctx);
				ATDecFloatOpt valr2 = ctx.GetResult();

				if (check && valr1 != valr2) {
					throw ATTestAssertionException("LOG[%d] differs for %s: unaccel %s, accel %s", i, valx.ToString().c_str(), valr1.ToString().c_str(), valr2.ToString().c_str());
				}
			}
		}
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	//==========================================================================
	ATTestTrace("Testing NORMALIZE");

	try {
		auto timeScope = makeTimeScope();

		{
			BCDRandGen rg;

			for(int pass=0; pass<6; ++pass) {
				int n = pass == 5 ? 1 : 10000;

				for(int i=0; i<n; ++i) {
					ATDecFloat valx = rg();

					for(int j=0; j<pass; ++j) {
						valx.mMantissa[j] = 0;
					}

					ctx.SetFR0(valx);
					ram[FR0 + 6] = (ram[FR0 + 5] + 0x30) % 0xA0;
					ram[FR0 + 7] = (ram[FR0 + 5] + 0x60) % 0xA0;
					ram[FR0 + 8] = (ram[FR0 + 5] + 0x20) % 0xA0;
					ram[FR0 + 9] = (ram[FR0 + 5] + 0x70) % 0xA0;

					ctx.mbCarry = invoke(NORMALIZE);
					ATDecFloatOpt valr1 = ctx.GetResult();

					ctx.SetFR0(valx);

					ATAccelNORMALIZE(ctx);
					ATDecFloatOpt valr2 = ctx.GetResult();

					if (check && valr1 != valr2) {
						throw ATTestAssertionException("NORMALIZE[%d] differs for %s: unaccel %s, accel %s", i, valx.ToString().c_str(), valr1.ToString().c_str(), valr2.ToString().c_str());
					}
				}
			}
		}
	} catch(const ATTestAssertionException& e) {
		puts(e.c_str());
		++errors;
	}

	return errors;
}

AT_DEFINE_TEST_NONAUTO(HLE_FPAccel) {
	return ATTestHelper_HLE_FPAccel(g_ATTestTracingEnabled, true);
}

AT_DEFINE_TEST_NONAUTO(HLE_FPAccelBench) {
	return ATTestHelper_HLE_FPAccel(true, false);
}
