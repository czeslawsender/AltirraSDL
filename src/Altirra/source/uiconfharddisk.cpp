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
#include <windows.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/math.h>
#include <vd2/system/strutil.h>
#include <vd2/system/w32assist.h>
#include <vd2/Dita/services.h>
#include <at/atnativeui/dialog.h>
#include "resource.h"
#include "idephysdisk.h"
#include "idevhdimage.h"
#include "oshelper.h"
#include <at/atnativeui/uiproxies.h>
#include <at/atcore/propertyset.h>

#ifndef BCM_SETSHIELD
#define BCM_SETSHIELD	0x160C
#endif

VDStringW ATUIShowDialogBrowsePhysicalDisks(VDGUIHandle hParent);
void ATCreateDeviceHardDisk(const ATPropertySet& pset, IATDevice **dev);

///////////////////////////////////////////////////////////////////////////

class ATUIDialogCreateVHDImage2 : public VDDialogFrameW32 {
public:
	ATUIDialogCreateVHDImage2();
	~ATUIDialogCreateVHDImage2();

	uint32 GetSectorCount() const { return mSectorCount; }
	const wchar_t *GetPath() const { return mPath.c_str(); }

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnOK();
	bool OnCommand(uint32 id, uint32 extcode);
	void UpdateGeometry();
	void UpdateEnables();

	VDStringW mPath;
	VDStringW mParentPath;
	uint32 mSectorCount = 8*1024*2;		// 8MB
	uint32 mSizeInMB = 8;
	uint32 mHeads = 15;
	uint32 mSPT = 63;
	bool mbAutoGeometry = true;
	bool mbDynamicDisk = true;
	bool mbDifferencingDisk = false;
	uint32 mInhibitUpdateLocks = 0;
};

ATUIDialogCreateVHDImage2::ATUIDialogCreateVHDImage2()
	: VDDialogFrameW32(IDD_CREATE_VHD)
{
}

ATUIDialogCreateVHDImage2::~ATUIDialogCreateVHDImage2() {
}

bool ATUIDialogCreateVHDImage2::OnLoaded() {
	UpdateGeometry();
	UpdateEnables();

	return VDDialogFrameW32::OnLoaded();
}

void ATUIDialogCreateVHDImage2::OnDataExchange(bool write) {
	ExchangeControlValueString(write, IDC_PATH, mPath);
	ExchangeControlValueString(write, IDC_PATH_PARENT, mParentPath);
	ExchangeControlValueUint32(write, IDC_SIZE_SECTORS, mSectorCount, 2048, 0xFFFFFFFEU);
	ExchangeControlValueUint32(write, IDC_SIZE_MB, mSizeInMB, 1, 4095);

	if (write) {
		mbAutoGeometry = IsButtonChecked(IDC_GEOMETRY_AUTO);
		mbDifferencingDisk = IsButtonChecked(IDC_TYPE_DIFFERENCING);
		mbDynamicDisk = mbDifferencingDisk || IsButtonChecked(IDC_TYPE_DYNAMIC);

		if (mbDifferencingDisk && mParentPath.empty())
			FailValidation(IDC_PATH_PARENT, L"Parent path is needed for a differencing disk image.", L"Invalid configuration");
	} else {
		CheckButton(IDC_GEOMETRY_AUTO, mbAutoGeometry);
		CheckButton(IDC_GEOMETRY_MANUAL, !mbAutoGeometry);

		CheckButton(IDC_TYPE_FIXED, !mbDynamicDisk);
		CheckButton(IDC_TYPE_DYNAMIC, mbDynamicDisk && !mbDifferencingDisk);
		CheckButton(IDC_TYPE_DIFFERENCING, mbDynamicDisk && mbDifferencingDisk);
	}

	if (!write || mbAutoGeometry) {
		ExchangeControlValueUint32(write, IDC_HEADS, mHeads, 1, 16);
		ExchangeControlValueUint32(write, IDC_SPT, mHeads, 1, 255);
	}
}

bool ATUIDialogCreateVHDImage2::OnOK() {
	if (VDDialogFrameW32::OnOK())
		return true;

	// Okay, let's actually try to create the VHD image!
	try {
		ATIDEVHDImage parentVhd;
		ATIDEVHDImage vhd;

		if (mbDifferencingDisk)
			parentVhd.Init(mParentPath.c_str(), false, true);

		vhd.InitNew(mPath.c_str(), mHeads, mSPT, mSectorCount, mbDynamicDisk, mbDifferencingDisk ? &parentVhd : nullptr);
		vhd.Flush();
	} catch(const MyUserAbortError&) {
		return true;
	} catch(const MyError& e) {
		ShowError2(e, L"Image creation failed");
		return true;
	}

	ShowInfo2(L"VHD creation was successful.", L"Image created");
	return false;
}

bool ATUIDialogCreateVHDImage2::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_BROWSE:
			{
				VDStringW s(VDGetSaveFileName('vhd ', (VDGUIHandle)mhdlg, L"Select location for new VHD image file", L"Virtual hard disk image\0*.vhd\0", L"vhd"));
				if (!s.empty())
					SetControlText(IDC_PATH, s.c_str());
			}
			return true;

		case IDC_BROWSE_PARENT:
			{
				VDStringW s(VDGetLoadFileName('vhd ', (VDGUIHandle)mhdlg, L"Select parent VHD image file", L"Virtual hard disk image\0*.vhd\0", L"vhd"));
				if (!s.empty())
					SetControlText(IDC_PATH_PARENT, s.c_str());
			}
			return true;

		case IDC_TYPE_FIXED:
		case IDC_TYPE_DYNAMIC:
		case IDC_TYPE_DIFFERENCING:
		case IDC_GEOMETRY_AUTO:
		case IDC_GEOMETRY_MANUAL:
			if (extcode == BN_CLICKED)
				UpdateEnables();
			return true;

		case IDC_SIZE_MB:
			if (extcode == EN_UPDATE && !mInhibitUpdateLocks) {
				uint32 mb = GetControlValueUint32(IDC_SIZE_MB);

				if (mb) {
					++mInhibitUpdateLocks;
					SetControlTextF(IDC_SIZE_SECTORS, L"%u", mb * 2048);
					--mInhibitUpdateLocks;
				}
			}
			return true;

		case IDC_SIZE_SECTORS:
			if (extcode == EN_UPDATE && !mInhibitUpdateLocks) {
				uint32 sectors = GetControlValueUint32(IDC_SIZE_SECTORS);

				if (sectors) {
					++mInhibitUpdateLocks;
					SetControlTextF(IDC_SIZE_MB, L"%u", sectors >> 11);
					--mInhibitUpdateLocks;
				}
			}
			return true;
	}

	return false;
}

void ATUIDialogCreateVHDImage2::UpdateGeometry() {
	// This calculation is from the VHD spec.
	uint32 secCount = std::min<uint32>(mSectorCount, 65535*16*255);

	if (secCount >= 65535*16*63) {
		mSPT = 255;
		mHeads = 16;
	} else {
		mSPT = 17;

		uint32 tracks = secCount / 17;
		uint32 heads = (tracks + 1023) >> 10;

		if (heads < 4) {
			heads = 4;
		}
		
		if (tracks >= (heads * 1024) || heads > 16) {
			mSPT = 31;
			heads = 16;
			tracks = secCount / 31;
		}

		if (tracks >= (heads * 1024)) {
			mSPT = 63;
			heads = 16;
		}

		mHeads = heads;
	}

	SetControlTextF(IDC_HEADS, L"%u", mHeads);
	SetControlTextF(IDC_SPT, L"%u", mSPT);
}

void ATUIDialogCreateVHDImage2::UpdateEnables() {
	bool enableParent = IsButtonChecked(IDC_TYPE_DIFFERENCING);
	bool enableGeometry = !enableParent;
	bool enableManualControls = IsButtonChecked(IDC_GEOMETRY_MANUAL) && enableGeometry;

	EnableControl(IDC_STATIC_PARENT, enableParent);
	EnableControl(IDC_PATH_PARENT, enableParent);
	EnableControl(IDC_BROWSE_PARENT, enableParent);
	EnableControl(IDC_STATIC_SIZEMB, enableGeometry);
	EnableControl(IDC_STATIC_SIZESECTORS, enableGeometry);
	EnableControl(IDC_SIZE_MB, enableGeometry);
	EnableControl(IDC_SIZE_SECTORS, enableGeometry);
	EnableControl(IDC_STATIC_GEOMETRY, enableGeometry);
	EnableControl(IDC_GEOMETRY_AUTO, enableGeometry);
	EnableControl(IDC_GEOMETRY_MANUAL, enableGeometry);

	EnableControl(IDC_STATIC_HEADS, enableManualControls);
	EnableControl(IDC_STATIC_SPT, enableManualControls);
	EnableControl(IDC_HEADS, enableManualControls);
	EnableControl(IDC_SPT, enableManualControls);
}

///////////////////////////////////////////////////////////////////////////

class ATUIDialogDeviceHardDisk final : public VDDialogFrameW32 {
public:
	ATUIDialogDeviceHardDisk(ATPropertySet& props);
	~ATUIDialogDeviceHardDisk();

	bool OnLoaded() override;
	void OnDataExchange(bool write) override;

protected:
	void UpdateEnables();
	void UpdateGeometry();
	void UpdateCapacityByCHS();
	void UpdateCapacityBySizeMB();
	void UpdateCapacityBySizeSectors();
	void UpdateCapacityBySectorCount(uint64 sectors, bool updateMb = true, bool updateSectors = true);

	void ShowCHSGeometry(uint32 cylinders, uint32 heads, uint32 spt);
	void ShowSizeInMB(uint32 sizemb);
	void ShowSizeInSectors(uint32 sectors);

	void OnBrowseImage();
	void OnBrowseDisk();
	void OnCreateVHD();

	static bool ParseUint32(const wchar_t *s, uint32& dst);

	uint32 mInhibitUpdateLocks;
	ATPropertySet& mProps;

	VDUIProxyButtonControl mBrowseImageView;
	VDUIProxyButtonControl mBrowseDiskView;
	VDUIProxyButtonControl mCreateVHDView;
	VDUIProxyEditControl mCylindersView;
	VDUIProxyEditControl mHeadsView;
	VDUIProxyEditControl mSectorsPerTrackView;
	VDUIProxyEditControl mSizeMBView;
	VDUIProxyEditControl mSizeSectorsView;
};

ATUIDialogDeviceHardDisk::ATUIDialogDeviceHardDisk(ATPropertySet& props)
	: VDDialogFrameW32(IDD_DEVICE_HARDDISK)
	, mInhibitUpdateLocks(0)
	, mProps(props)
{
	mBrowseImageView.SetOnClicked(
		[this] { OnBrowseImage(); }
	);

	mBrowseDiskView.SetOnClicked(
		[this] { OnBrowseDisk(); }
	);

	mCreateVHDView.SetOnClicked(
		[this] { OnCreateVHD(); }
	);

	mCylindersView.SetOnTextChanged(
		[this](VDUIProxyEditControl*) {
			if (!mInhibitUpdateLocks)
				UpdateCapacityByCHS();
		}
	);

	mHeadsView.SetOnTextChanged(
		[this](VDUIProxyEditControl*) {
			if (!mInhibitUpdateLocks)
				UpdateCapacityByCHS();
		}
	);

	mSectorsPerTrackView.SetOnTextChanged(
		[this](VDUIProxyEditControl*) {
			if (!mInhibitUpdateLocks)
				UpdateCapacityByCHS();
		}
	);

	mSizeMBView.SetOnTextChanged(
		[this](VDUIProxyEditControl*) {
			if (!mInhibitUpdateLocks)
				UpdateCapacityBySizeMB();
		}
	);

	mSizeSectorsView.SetOnTextChanged(
		[this](VDUIProxyEditControl*) {
			if (!mInhibitUpdateLocks)
				UpdateCapacityBySizeSectors();
		}
	);
}

ATUIDialogDeviceHardDisk::~ATUIDialogDeviceHardDisk() {
}

bool ATUIDialogDeviceHardDisk::OnLoaded() {
	AddProxy(&mBrowseImageView, IDC_IDE_IMAGEBROWSE);
	AddProxy(&mBrowseDiskView, IDC_IDE_DISKBROWSE);
	AddProxy(&mCreateVHDView, IDC_CREATE_VHD);

	AddProxy(&mCylindersView, IDC_IDE_CYLINDERS);
	AddProxy(&mHeadsView, IDC_IDE_HEADS);
	AddProxy(&mSectorsPerTrackView, IDC_IDE_SPT);

	AddProxy(&mSizeMBView, IDC_IDE_SIZE);
	AddProxy(&mSizeSectorsView, IDC_SECTOR_COUNT);

	mBrowseDiskView.ShowElevationNeeded();

	ATUIEnableEditControlAutoComplete(GetControl(IDC_IDE_IMAGEPATH));

	return VDDialogFrameW32::OnLoaded();
}

void ATUIDialogDeviceHardDisk::OnDataExchange(bool write) {
	if (!write) {
		++mInhibitUpdateLocks;

		SetControlText(IDC_IDE_IMAGEPATH, mProps.GetString("path"));
		CheckButton(IDC_IDEREADONLY, !mProps.GetBool("write_enabled"));

		uint32 cylinders = mProps.GetUint32("cylinders", 0);
		uint32 heads = mProps.GetUint32("heads", 0);
		uint32 spt = mProps.GetUint32("sectors_per_track", 0);

		if (!cylinders || !heads || !spt) {
			heads = 0;
			spt = 0;
			cylinders = 0;
		} else {
			VDStringW s;

			s.sprintf(L"%u", cylinders);
			mCylindersView.SetText(s.c_str());

			s.sprintf(L"%u", heads);
			mHeadsView.SetText(s.c_str());

			s.sprintf(L"%u", spt);
			mSectorsPerTrackView.SetText(s.c_str());
		}

		--mInhibitUpdateLocks;

		bool fast = mProps.GetBool("solid_state");
		CheckButton(IDC_SPEED_FAST, fast);
		CheckButton(IDC_SPEED_SLOW, !fast);

		UpdateCapacityByCHS();

		if (!cylinders || !heads || !spt) {
			uint32 totalSectors = mProps.GetUint32("sectors");
			if (totalSectors)
				UpdateCapacityBySectorCount(totalSectors);
		}

		UpdateEnables();
	} else {
		const bool write = !IsButtonChecked(IDC_IDEREADONLY);
		const bool fast = IsButtonChecked(IDC_SPEED_FAST);

		VDStringW path;
		GetControlText(IDC_IDE_IMAGEPATH, path);

		if (path.empty()) {
			FailValidation(IDC_IDE_IMAGEPATH);
			return;
		}

		uint32 cylinders = 0;
		uint32 heads = 0;
		uint32 sectors = 0;

		if (!path.empty()) {
			VDStringW s;

			s = mCylindersView.GetText();
			if (!s.empty()) {
				cylinders = GetControlValueUint32(IDC_IDE_CYLINDERS);
				if (cylinders > 16777216)
					FailValidation(mCylindersView.GetWindowId());
			}

			s = mHeadsView.GetText();
			if (!s.empty()) {
				if (!ParseUint32(s.c_str(), heads) || heads > 16)
					FailValidation(mHeadsView.GetWindowId());
			}

			s = mSectorsPerTrackView.GetText();
			if (!s.empty()) {
				if (!ParseUint32(s.c_str(), sectors) || sectors > 255)
					FailValidation(mSectorsPerTrackView.GetWindowId());
			}
		}

		if (!mbValidationFailed) {
			mProps.Clear();
			mProps.SetString("path", path.c_str());

			if (cylinders && heads && sectors) {
				mProps.SetUint32("cylinders", cylinders);
				mProps.SetUint32("heads", heads);
				mProps.SetUint32("sectors_per_track", sectors);
				mProps.SetUint32("sectors", cylinders * heads * sectors);
			}

			mProps.SetBool("write_enabled", write);
			mProps.SetBool("solid_state", fast);
		}
	}
}

void ATUIDialogDeviceHardDisk::UpdateEnables() {
}

void ATUIDialogDeviceHardDisk::UpdateGeometry() {
	uint32 imageSizeMB = GetControlValueUint32(IDC_IDE_SIZE);

	if (imageSizeMB) {
		uint32 heads;
		uint32 sectors;
		uint32 cylinders;

		if (imageSizeMB <= 64) {
			heads = 4;
			sectors = 32;
			cylinders = imageSizeMB << 4;
		} else {
			heads = 16;
			sectors = 63;
			cylinders = (imageSizeMB * 128 + 31) / 63;
		}

		if (cylinders > 16777216)
			cylinders = 16777216;

		++mInhibitUpdateLocks;
		SetControlTextF(IDC_IDE_CYLINDERS, L"%u", cylinders);
		SetControlTextF(IDC_IDE_HEADS, L"%u", heads);
		SetControlTextF(IDC_IDE_SPT, L"%u", sectors);
		--mInhibitUpdateLocks;
	}
}

void ATUIDialogDeviceHardDisk::UpdateCapacityByCHS() {
	VDStringW s;
	uint32 cyls = 0;
	uint32 heads = 0;
	uint32 spt = 0;

	s = mCylindersView.GetText();
	ParseUint32(s.c_str(), cyls);

	s = mHeadsView.GetText();
	ParseUint32(s.c_str(), heads);

	if (heads > 16)
		heads = 0;

	s = mSectorsPerTrackView.GetText();
	ParseUint32(s.c_str(), spt);

	if (spt > 255)
		spt = 0;

	uint64 sizeSectors = 0;

	if (cyls && heads && spt) {
		sizeSectors = cyls;
		
		sizeSectors *= heads;
		if (sizeSectors > UINT32_MAX)
			sizeSectors = 0;

		sizeSectors *= spt;
		if (sizeSectors > UINT32_MAX)
			sizeSectors = 0;
	}

	++mInhibitUpdateLocks;

	if (sizeSectors) {
		s.sprintf(L"%u", (uint32)(sizeSectors >> 11));
		mSizeMBView.SetText(s.c_str());

		s.sprintf(L"%u", (uint32)sizeSectors);
		mSizeSectorsView.SetText(s.c_str());
	} else {
		mSizeMBView.SetText(L"--");
		mSizeSectorsView.SetText(L"--");
	}

	--mInhibitUpdateLocks;
}

void ATUIDialogDeviceHardDisk::UpdateCapacityBySizeMB() {
	VDStringW s = mSizeMBView.GetText();
	uint32 sizemb = 0;

	ParseUint32(s.c_str(), sizemb);

	UpdateCapacityBySectorCount((uint64)sizemb << 11, false, true);
}

void ATUIDialogDeviceHardDisk::UpdateCapacityBySizeSectors() {
	VDStringW s = mSizeSectorsView.GetText();
	uint32 sizeSectors = 0;

	ParseUint32(s.c_str(), sizeSectors);

	UpdateCapacityBySectorCount(sizeSectors, true, false);
}

void ATUIDialogDeviceHardDisk::UpdateCapacityBySectorCount(uint64 sectors, bool updateMb, bool updateSectors) {
	uint32 spt = 63;
	uint32 heads = 15;
	uint32 cylinders = 1;

	if (sectors > UINT32_MAX)
		sectors = UINT32_MAX;

	if (sectors) {
		// We need to truncate as otherwise the disk code will raise the sector size to
		// accommodate the CHS geometry.
		cylinders = VDClampToUint32(sectors / (heads * spt));
	}

	ShowCHSGeometry(cylinders, heads, spt);

	if (updateMb)
		ShowSizeInMB((uint32)(sectors >> 11));

	if (updateSectors)
		ShowSizeInSectors(sectors);
}

void ATUIDialogDeviceHardDisk::ShowCHSGeometry(uint32 cylinders, uint32 heads, uint32 spt) {
	VDStringW s;

	++mInhibitUpdateLocks;

	s.sprintf(L"%u", cylinders);
	mCylindersView.SetText(s.c_str());

	s.sprintf(L"%u", heads);
	mHeadsView.SetText(s.c_str());

	s.sprintf(L"%u", spt);
	mSectorsPerTrackView.SetText(s.c_str());

	--mInhibitUpdateLocks;
}

void ATUIDialogDeviceHardDisk::ShowSizeInMB(uint32 sizemb) {
	VDStringW s;

	++mInhibitUpdateLocks;

	s.sprintf(L"%u", sizemb);
	mSizeMBView.SetText(s.c_str());

	--mInhibitUpdateLocks;
}

void ATUIDialogDeviceHardDisk::ShowSizeInSectors(uint32 sectors) {
	VDStringW s;

	++mInhibitUpdateLocks;

	s.sprintf(L"%u", sectors);
	mSizeSectorsView.SetText(s.c_str());

	--mInhibitUpdateLocks;
}

void ATUIDialogDeviceHardDisk::OnBrowseImage() {
	int optvals[1]={false};

	static constexpr VDFileDialogOption kOpts[]={
		{ VDFileDialogOption::kConfirmFile, 0 },
		{0}
	};

	VDStringW s(VDGetSaveFileName('ide ', (VDGUIHandle)mhdlg, L"Select IDE image file", L"All files\0*.*\0", NULL, kOpts, optvals));
	if (!s.empty()) {
		if (s.size() >= 4 && !vdwcsicmp(s.c_str() + s.size() - 4, L".vhd")) {
			try {
				vdrefptr<ATIDEVHDImage> vhdImage(new ATIDEVHDImage);

				vhdImage->Init(s.c_str(), false, false);

				UpdateCapacityBySectorCount(vhdImage->GetSectorCount());
			} catch(const MyError& e) {
				ShowError2(e, L"VHD image mount failed");
				return;
			}
		} else {
			VDDirectoryIterator it(s.c_str());

			if (it.Next()) {
				UpdateCapacityBySectorCount(it.GetSize() >> 9);
			}
		}

		SetControlText(IDC_IDE_IMAGEPATH, s.c_str());

		uint32 attr = VDFileGetAttributes(s.c_str());
		if (attr != kVDFileAttr_Invalid)
			CheckButton(IDC_IDEREADONLY, (attr & kVDFileAttr_ReadOnly) != 0);
	}
}

void ATUIDialogDeviceHardDisk::OnBrowseDisk() {
	if (!ATIsUserAdministrator()) {
		ShowError(L"You must run Altirra with local administrator access in order to mount a physical disk for emulation.", L"Altirra Error");
		return;
	}

	ShowWarning(
		L"This option uses a physical disk for IDE/SCSI/SD emulation. You can either map the entire disk or a partition within the disk. However, only read only access is supported.\n"
		L"\n"
		L"You can use a partition that is currently mounted by Windows. However, changes to the file system in Windows may not be reflected consistently in the emulator.",
		L"Altirra Warning");

	const VDStringW& path = ATUIShowDialogBrowsePhysicalDisks((VDGUIHandle)mhdlg);

	if (!path.empty()) {
		SetControlText(IDC_IDE_IMAGEPATH, path.c_str());
		CheckButton(IDC_IDEREADONLY, true);

		sint64 size = ATIDEGetPhysicalDiskSize(path.c_str());
		uint64 sectors = (uint64)size >> 9;

		UpdateCapacityBySectorCount(sectors);
	}
}

void ATUIDialogDeviceHardDisk::OnCreateVHD() {
	ATUIDialogCreateVHDImage2 createVHDDlg;

	if (createVHDDlg.ShowDialog((VDGUIHandle)mhdlg)) {
		UpdateCapacityBySectorCount(createVHDDlg.GetSectorCount());
		SetControlText(IDC_IDE_IMAGEPATH, createVHDDlg.GetPath());
	}
}

bool ATUIDialogDeviceHardDisk::ParseUint32(const wchar_t *s, uint32& dst) {
	unsigned val;
	wchar_t tmp;

	if (1 != swscanf(s, L" %u %c", &val, &tmp)) {
		dst = 0;
		return false;
	}

	dst = val;
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool ATUIConfDevHardDisk(VDGUIHandle hParent, ATPropertySet& props) {
	ATUIDialogDeviceHardDisk dlg(props);

	return dlg.ShowDialog(hParent) != 0;
}
