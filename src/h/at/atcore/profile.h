//	Altirra - Atari 800/800XL/5200 emulator
//	Core library - process Profile interface
//	Copyright (C) 2009-2015 Avery Lee
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

#ifndef f_AT_ATCORE_PROFILE_H
#define f_AT_ATCORE_PROFILE_H

enum ATProfileEvent {
	kATProfileEvent_BeginFrame,
	kATProfileEvent_DisplayVSync,
};

enum ATProfileRegion {
	kATProfileRegion_Idle,
	kATProfileRegion_IdleFrameDelay,
	kATProfileRegion_Simulation,
	kATProfileRegion_NativeEvents,
	kATProfileRegion_NativeMessage,
	kATProfileRegion_DisplayPost,
	kATProfileRegion_DisplayTick,
	kATProfileRegion_DisplayPresent,
	kATProfileRegionCount
};

class IATProfiler {
public:
	virtual void OnEvent(ATProfileEvent event) = 0;

	virtual void OnEventWithArg(ATProfileEvent event, uintptr arg) {
		OnEvent(event);
	}

	virtual void BeginRegion(ATProfileRegion region) = 0;

	virtual void BeginRegionWithArg(ATProfileRegion region, uintptr arg) {
		BeginRegion(region);
	}

	virtual void EndRegion(ATProfileRegion region) = 0;
};

extern IATProfiler *g_pATProfiler;

inline void ATProfileMarkEvent(ATProfileEvent event) {
	if (g_pATProfiler)
		g_pATProfiler->OnEvent(event);
}

inline void ATProfileMarkEventWithArg(ATProfileEvent event, uintptr arg) {
	if (g_pATProfiler)
		g_pATProfiler->OnEventWithArg(event, arg);
}

inline void ATProfileBeginRegion(ATProfileRegion region) {
	if (g_pATProfiler)
		g_pATProfiler->BeginRegion(region);
}

inline void ATProfileBeginRegionWithArg(ATProfileRegion region, uintptr arg) {
	if (g_pATProfiler)
		g_pATProfiler->BeginRegionWithArg(region, arg);
}

inline void ATProfileEndRegion(ATProfileRegion region) {
	if (g_pATProfiler)
		g_pATProfiler->EndRegion(region);
}

#endif
