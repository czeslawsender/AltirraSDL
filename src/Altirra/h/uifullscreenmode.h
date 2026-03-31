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

#ifndef f_AT_UIFULLSCREENMODE_H
#define f_AT_UIFULLSCREENMODE_H

#include <at/atnativeui/dialog.h>
#include <at/atnativeui/uiproxies.h>

class ATUIDialogFullScreenMode : public VDDialogFrameW32 {
public:
	struct ModeInfo {
		uint32 mWidth;
		uint32 mHeight;
		uint32 mRefresh;

		bool operator==(const ModeInfo&) const = default;
	};

	ATUIDialogFullScreenMode();

	const ModeInfo& GetSelectedItem() const { return mSelectedMode; }
	void SetSelectedItem(const ModeInfo& modeInfo) { mSelectedMode = modeInfo; }

protected:
	struct ModeItem;
	struct ModeInfoLess;
	struct ModeInfoEqual;
	struct ModeInfoMatch;

	bool OnLoaded();
	void OnDestroy();
	void OnDataExchange(bool write);
	void OnSelectedItemChanged(VDUIProxyListView *sender, int index);

	ModeInfo	mSelectedMode;

	VDUIProxyListView mList;
	VDDelegate mDelSelItemChanged;
};

#endif

