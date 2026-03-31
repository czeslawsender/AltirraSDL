//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2020 Avery Lee
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

#ifndef f_AT_DISKPROFILE_H
#define f_AT_DISKPROFILE_H

#include <vd2/system/vdtypes.h>

enum ATDiskEmulationMode : uint8;

enum ATDiskEmulationModeVariant : uint8 {
	Default,
	IndusGTSynchromesh,
	IndusGTSuperSynchromesh
};

// Emulation behavior and timing for each profile supported by the standard disk emulator. This
// is not used by the full drive emulator.
//
// All timings are in machine cycles (1.77/1.79MHz) unless stated otherwise.
//
struct ATDiskProfile {
	bool	mbSupportedNotReady = false;
	bool	mbSupportedCmdHighSpeedXF = false;			// Support for XF551-style high speed through command bit 7
	bool	mbSupportedCmdHighSpeedUSD = false;			// Support for US Doubler-style high speed through fast command frame
	bool	mbSupportedCmdPERCOM = false;				// Support for read/write PERCOM block commands
	bool	mbSupportedCmdFormatSkewed = false;			// Support for the US Doubler format skewed command ($66)
	bool	mbSupportedCmdFormatBoot = false;			// Support for the Indus GT format boot tracks command ($A3)
	bool	mbSupportedCmdGetHighSpeedIndex = false;	// Support for the get high speed index command ($3F)

	uint8	mHighSpeedIndexXF = 0;						// High-speed index (POKEY divisor) for high speed operations
	uint8	mHighSpeedIndexUSD = 0;						// High-speed index (POKEY divisor) for high speed operations

	uint8	mHighSpeedUSDCmdFrameRateLo = 0;			// Minimum POKEY divisor allowed for sending high-speed USD command frames
	uint8	mHighSpeedUSDCmdFrameRateHi = 0;			// Maximum POKEY divisor allowed for sending high-speed USD command frames
	uint32	mCyclesPerSIOByte = 1;
	uint32	mCyclesPerSIOBit = 1;
	uint32	mCyclesPerSIOByteHighSpeedUSD = 1;
	uint32	mCyclesPerSIOBitHighSpeedUSD = 1;
	uint32	mCyclesPerSIOByteHighSpeedXF = 1;
	uint32	mCyclesPerSIOBitHighSpeedXF = 1;
	uint32	mCyclesToACKSent = 1;
	uint32	mCyclesToNAKFromFrameEnd = 1;
	uint32	mCyclesToNAKFromCmdDeassert = 1;
	uint32	mCyclesToFDCCommand = 1;
	uint32	mCyclesPerDiskRotation = 1;
	uint32	mCyclesPerTrackStep = 1;
	uint32	mCyclesForHeadSettle = 1;
	uint32	mCyclesPostReadToCE = 0;					// Cycles from FDC operation complete to start of C/E byte
	uint32	mCyclesCEToDataFrame = 0;					// Cycles from C/E byte to data frame, constant factor
	uint32	mCyclesCEToDataFramePBDiv256 = 0;			// Cycles from C/E byte to data frame, per byte factor (24.8)
	uint32	mCyclesCEToDataFrameHighSpeedXF = 0;		// Cycles from C/E byte to data frame for high speed (XF551), constant factor
	uint32	mCyclesCEToDataFrameHighSpeedXFPBDiv256 = 0;// Cycles from C/E byte to data frame for high speed (XF551), per-byte factor (24.8)
	uint32	mCyclesCEToDataFrameHighSpeedUSD = 0;		// Cycles from C/E byte to data frame for high speed (USD), constant factor
	uint32	mCyclesCEToDataFrameHighSpeedUSDPBDiv256 = 0;// Cycles from C/E byte to data frame for high speed (USD), per-byte factor (24.8)

	uint32	mCyclesACKStopBitToStatusComplete = 0;
	uint32	mCyclesACKStopBitToReadPERCOMComplete = 0;

	uint32	mCyclesToMotorOff = 0;
	uint32	mCyclesToReceiveByteTimeout = 0;			// Cycles from byte receive to timeout (0 = none).
	uint32	mCyclesToReceiveFrameTimeout = 0;			// Cycles from frame start receive to timeout (0 = none).

	bool	mbSeekHalfTracks = false;
	bool	mbRetryMode1050 = false;
	bool	mbReverseOnForwardSeeks = false;
	bool	mbWaitForLongSectors = false;

	// True if the Write PERCOM Block command immediately changes the density
	// (XF551); false if it doesn't take effect until format (USDoubler).
	bool	mbWritePercomChangesDensity = false;

	bool	mbBufferTrackReads = false;
	bool	mbBufferTrackReadErrors = false;
	bool	mbBufferSector1 = false;

	// True if this disk drive requires SIO command to still be asserted at
	// the end of the command frame.
	bool	mbRequireCommandDeassertCheck = false;

	// True if this disk drive supports interrupting a command in progress.
	bool	mbSupportCommandTruncation = false;

	ATDiskProfile() = default;

	constexpr ATDiskProfile(ATDiskEmulationMode mode, ATDiskEmulationModeVariant variant = ATDiskEmulationModeVariant::Default);
};

const ATDiskProfile& ATGetDiskProfile(ATDiskEmulationMode mode);
const ATDiskProfile& ATGetDiskProfileIndusGTSynchromesh();
const ATDiskProfile& ATGetDiskProfileIndusGTSuperSynchromesh();

#endif
