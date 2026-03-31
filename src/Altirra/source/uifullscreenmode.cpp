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
#include <windows.h>
#include "uifullscreenmode.h"
#include "resource.h"

struct ATUIDialogFullScreenMode::ModeItem : public vdrefcounted<IVDUIListViewVirtualItem>, public ModeInfo {
	ModeItem(const ModeInfo& modeInfo) : ModeInfo(modeInfo) {}

	void GetText(int subItem, VDStringW& s) const;
};

struct ATUIDialogFullScreenMode::ModeInfoLess {
	bool operator()(const ModeInfo& x, const ModeInfo& y) const {
		if (x.mWidth != y.mWidth)
			return x.mWidth < y.mWidth;

		if (x.mHeight != y.mHeight)
			return x.mHeight < y.mHeight;

		return x.mRefresh < y.mRefresh;
	}
};

struct ATUIDialogFullScreenMode::ModeInfoMatch {
	ModeInfoMatch(const ModeInfo& mode) : mMode(mode) {}

	bool operator()(const ModeInfo& mode) const {
		return mMode == mode;
	}

	const ModeInfo mMode;
};

void ATUIDialogFullScreenMode::ModeItem::GetText(int subItem, VDStringW& s) const {
	switch(subItem) {
		case 0:
			s.sprintf(L"%ux%u", mWidth, mHeight);
			break;

		case 1:
			if (!mRefresh)
				s = L"Default";
			else
				s.sprintf(L"%uHz", mRefresh);
			break;
	}
}

ATUIDialogFullScreenMode::ATUIDialogFullScreenMode()
	: VDDialogFrameW32(IDD_OPTIONS_DISPLAY_MODE)
{
	memset(&mSelectedMode, 0, sizeof mSelectedMode);

	mList.OnItemSelectionChanged() += mDelSelItemChanged.Bind(this, &ATUIDialogFullScreenMode::OnSelectedItemChanged);
}

bool ATUIDialogFullScreenMode::OnLoaded() {
	AddProxy(&mList, IDC_MODE_LIST);

	mResizer.Add(mList.GetHandle(), mResizer.kMC);
	mResizer.Add(IDOK, mResizer.kBR);
	mResizer.Add(IDCANCEL, mResizer.kBR);

	mList.InsertColumn(0, L"Resolution", 0);
	mList.InsertColumn(1, L"Refresh rate", 0);
	mList.SetFullRowSelectEnabled(true);

	VDDialogFrameW32::OnLoaded();

	SetFocusToControl(IDC_LIST);
	return true;
}

void ATUIDialogFullScreenMode::OnDestroy() {
	mList.Clear();
}

void ATUIDialogFullScreenMode::OnDataExchange(bool write) {
	if (write) {
		ModeItem *p = static_cast<ModeItem *>(mList.GetSelectedVirtualItem());

		if (!p) {
			FailValidation(IDC_LIST);
			return;
		}

		mSelectedMode = *p;
	} else {
		struct {
			DEVMODE dm;
			char buf[1024];
		} devMode;

		vdfastvector<ModeInfo> modes;

		int modeIndex = 0;
		for(;; ++modeIndex) {
			devMode.dm.dmSize = sizeof(DEVMODE);
			devMode.dm.dmDriverExtra = sizeof devMode.buf;

			if (!EnumDisplaySettingsEx(NULL, modeIndex, &devMode.dm, EDS_RAWMODE))
				break;

			// throw out paletted modes
			if (devMode.dm.dmBitsPerPel < 15)
				continue;

			// throw out interlaced modes
			if (devMode.dm.dmDisplayFlags & DM_INTERLACED)
				continue;

			ModeInfo mode = { devMode.dm.dmPelsWidth, devMode.dm.dmPelsHeight, devMode.dm.dmDisplayFrequency };

			if (mode.mRefresh == 1)
				mode.mRefresh = 0;

			modes.push_back(mode);
		}

		std::sort(modes.begin(), modes.end(), ModeInfoLess());
		modes.erase(std::unique(modes.begin(), modes.end()), modes.end());

		int selectedIndex = (int)(std::find_if(modes.begin(), modes.end(), ModeInfoMatch(mSelectedMode)) - modes.begin());
		if (selectedIndex >= (int)modes.size())
			selectedIndex = -1;

		for(vdfastvector<ModeInfo>::const_iterator it(modes.begin()), itEnd(modes.end());
			it != itEnd;
			++it)
		{
			const ModeInfo& modeInfo = *it;

			ModeItem *modeItem = new ModeItem(modeInfo);

			if (modeItem) {
				modeItem->AddRef();
				mList.InsertVirtualItem(-1, modeItem);
				modeItem->Release();
			}
		}

		mList.SetSelectedIndex(selectedIndex);
		mList.EnsureItemVisible(selectedIndex);
		mList.AutoSizeColumns();
		EnableControl(IDOK, selectedIndex >= 0);
	}
}

void ATUIDialogFullScreenMode::OnSelectedItemChanged(VDUIProxyListView *sender, int index) {
	EnableControl(IDOK, index >= 0);
}
