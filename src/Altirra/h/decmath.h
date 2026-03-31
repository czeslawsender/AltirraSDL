//	Altirra - Atari 800/800XL emulator
//	Copyright (C) 2008 Avery Lee
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

#ifndef AT_DECMATH_H
#define AT_DECMATH_H

class ATCPUEmulator;
class ATCPUEmulatorMemory;

class IATFPAccelContext {
public:
	virtual void SetFlagC() = 0;
	virtual void ClearFlagC() = 0;
	virtual uint8 ReadByte(uint16 addr) const = 0;
	virtual void WriteByte(uint16 addr, uint8 value) = 0;
};

class ATFPAccelContext final : public IATFPAccelContext {
public:
	ATFPAccelContext(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem)
		: mCpu(cpu), mMem(mem) {}

	void SetFlagC() override;
	void ClearFlagC() override;
	uint8 ReadByte(uint16 addr) const override;
	void WriteByte(uint16 addr, uint8 value) override;

private:
	ATCPUEmulator& mCpu;
	ATCPUEmulatorMemory& mMem;
};

double ATDebugReadDecFloatAsBinary(ATCPUEmulatorMemory& mem, uint16 addr);
double ATReadDecFloatAsBinary(ATCPUEmulatorMemory& mem, uint16 addr);
double ATReadDecFloatAsBinary(const uint8 bytes[6]);

void ATAccelAFP(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelFASC(IATFPAccelContext& ctx);
void ATAccelIFP(IATFPAccelContext& ctx);
void ATAccelFPI(IATFPAccelContext& ctx);
void ATAccelFADD(IATFPAccelContext& ctx);
void ATAccelFSUB(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelFMUL(IATFPAccelContext& ctx);
void ATAccelFDIV(IATFPAccelContext& ctx);
void ATAccelLOG(IATFPAccelContext& ctx);
void ATAccelLOG10(IATFPAccelContext& ctx);
void ATAccelEXP(IATFPAccelContext& ctx);
void ATAccelEXP10(IATFPAccelContext& ctx);
void ATAccelSKPSPC(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelISDIGT(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelNORMALIZE(IATFPAccelContext& ctx);
void ATAccelPLYEVL(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelZFR0(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelZF1(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelZFL(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelLDBUFA(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelFLD0R(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelFLD0P(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelFLD1R(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelFLD1P(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelFST0R(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelFST0P(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelFMOVE(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);
void ATAccelREDRNG(ATCPUEmulator& cpu, ATCPUEmulatorMemory& mem);

#endif
