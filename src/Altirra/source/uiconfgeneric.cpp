//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2009-2022 Avery Lee
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

#include "stdafx.h"
#include <vd2/Dita/services.h>
#include <at/atcore/enumparseimpl.h>
#include <at/atcore/propertyset.h>
#include <at/atnativeui/dialog.h>
#include "resource.h"
#include "uiaccessors.h"
#include "uiconfgeneric.h"

class ATUIConfDialogGenericPanel final : public VDDialogFrameW32, public IATUIConfigView {
public:
	ATUIConfDialogGenericPanel(IATUIConfigController& controller);
	~ATUIConfDialogGenericPanel();

	IATUIConfigCheckboxView& AddCheckbox() override;
	IATUIConfigPathView& AddPath() override;
	IATUIConfigStringEditView& AddStringEdit() override;
	IATUIConfigIntEditView& AddIntEdit() override;
	IATUIConfigColorView& AddColor() override;
	IATUIConfigIntDropDownView& AddIntDropDown() override;
	IATUIConfigDropDownView& AddDropDown(const ATEnumLookupTable& enumTable) override;

	void AddVerticalSpace() override;

	void Read(const ATPropertySet& pset) override;
	void Write(ATPropertySet& pset) const override;

private:
	class BaseView;
	class BoolView;
	class StringView;
	class IntView;
	class CheckboxView;
	class PathView;
	class StringEditView;
	class IntEditView;
	class ColorView;
	class IntDropDownView;
	class EnumDropDownView;

	bool OnLoaded() override;

	void OnValueChanged();
	void UpdateEnables();

	void AddView(BaseView *view);

	static constexpr uint32 kBaseId = 30000;

	IATUIConfigController& mController;
	vdfastvector<BaseView *> mViews;

	sint32 mYPosDLUs = 0;
};

class ATUIConfDialogGenericPanel::BaseView : public VDDialogFrameW32, public virtual IATUIConfigPropView {
public:
	using VDDialogFrameW32::VDDialogFrameW32;

	void SetViewIndex(ATUIConfDialogGenericPanel& parent, uint32 viewIndex) {
		mpParent = &parent;
		mViewIndex = viewIndex;
	}

	void SetTagImpl(const char *tag) override {
		mTag = tag;
	}

	void SetLabelImpl(const wchar_t *label) override {
		mLabel = label;
		UpdateLabel();
	}

	void SetHelpImpl(const wchar_t *text) override {
	}

	void SetHelpImpl(const wchar_t *caption, const wchar_t *text) override {
	}

	void SetEnableExprImpl(vdfunction<bool()> fn) override {
		mpEnableExpr = std::move(fn);
	}

	void UpdateEnable() {
		if (mpEnableExpr)
			SetEnabled(mpEnableExpr());
	}

	virtual void Read(const ATPropertySet& pset) = 0;
	virtual void Write(ATPropertySet& pset) const = 0;

protected:
	virtual void UpdateLabel() = 0;
	virtual void UpdateView() = 0;

	ATUIConfDialogGenericPanel *mpParent = nullptr;
	uint32 mViewIndex = 0;
	VDStringA mTag;
	VDStringW mLabel;
	vdfunction<bool()> mpEnableExpr;
};

class ATUIConfDialogGenericPanel::BoolView : public BaseView, public virtual IATUIConfigBoolView {
public:
	using BaseView::BaseView;
	using BaseView::SetTag;

	void SetDefaultImpl(bool val) override { mbDefault = val; }

	bool GetValue() const override {
		return mbValue;
	}

	void SetValueImpl(bool v) override {
		if (mbValue != v) {
			mbValue = v;
			UpdateView();
		}
	}

	void Read(const ATPropertySet& pset) override {
		if (!mTag.empty())
			SetValue(pset.GetBool(mTag.c_str(), mbDefault));
	}

	void Write(ATPropertySet& pset) const override {
		if (!mTag.empty() && mbValue != mbDefault)
			pset.SetBool(mTag.c_str(), mbValue);
	}

protected:
	bool mbDefault = false;
	bool mbValue = false;
};

class ATUIConfDialogGenericPanel::IntView : public BaseView, public virtual IATUIConfigIntView {
public:
	using BaseView::BaseView;
	using BaseView::SetTag;

	void SetDefaultImpl(sint32 val, bool writeDefault) override { mDefault = val; mbWriteDefault = writeDefault; }

	sint32 GetValue() const override {
		return mValue;
	}

	void SetValueImpl(sint32 v) override {
		if (mValue != v) {
			mValue = v;
			UpdateView();
		}
	}

	void Read(const ATPropertySet& pset) override {
		if (!mTag.empty())
			SetValue(pset.GetInt32(mTag.c_str(), mDefault));
	}

	void Write(ATPropertySet& pset) const override {
		if (!mTag.empty() && (mbWriteDefault || mValue != mDefault))
			pset.SetInt32(mTag.c_str(), mValue);
	}

protected:
	sint32 mDefault = 0;
	sint32 mValue = 0;
	bool mbWriteDefault = false;
};

class ATUIConfDialogGenericPanel::StringView : public BaseView, public virtual IATUIConfigStringView {
public:
	using BaseView::BaseView;
	using BaseView::SetTag;

	const wchar_t *GetValue() const override { return mValue.c_str(); }

	void SetValueImpl(const wchar_t *s) override {
		const VDStringSpanW sp(s);

		if (mValue != sp) {
			mValue = sp;

			UpdateView();
		}
	}

	void SetDefaultImpl(const wchar_t *s, bool writeDefault) override {
		mDefaultValue = s;
		mbWriteDefault = writeDefault;
	}

	void Read(const ATPropertySet& pset) override {
		if (!mTag.empty())
			SetValue(pset.GetString(mTag.c_str(), mDefaultValue.c_str()));
	}

	void Write(ATPropertySet& pset) const override {
		if (!mTag.empty() && (mbWriteDefault || mValue != mDefaultValue))
			pset.SetString(mTag.c_str(), mValue.c_str());
	}

protected:
	VDStringW mValue;
	VDStringW mDefaultValue;
	bool mbWriteDefault = false;
};

class ATUIConfDialogGenericPanel::CheckboxView final : public BoolView, public IATUIConfigCheckboxView {
public:
	CheckboxView() : BoolView(IDD_CFGPROP_CHECKBOX) {
		mCheckView.SetOnClicked(
			[this] {
				const bool v = mCheckView.GetChecked();
				if (mbValue != v) {
					mbValue = v;
					mpParent->OnValueChanged();
				}
			}
		);
	}

	IATUIConfigCheckboxView& SetText(const wchar_t *text) override {
		if (mText != text) {
			mText = text;

			mCheckView.SetCaption(mText.c_str());
		}

		return *this;
	}

private:
	VDUIProxyControl mLabelView;
	VDUIProxyButtonControl mCheckView;
	VDStringW mText;

	bool OnLoaded() override {
		AddProxy(&mLabelView, IDC_LABEL);
		AddProxy(&mCheckView, IDC_CHECK);
		mResizer.Add(mCheckView.GetWindowHandle(), mResizer.kTC);

		mCheckView.SetCaption(mText.c_str());
		UpdateLabel();
		return false;
	}

	void OnEnable(bool enable) override {
		mCheckView.SetEnabled(enable);
	}

	void UpdateView() override {
		mCheckView.SetChecked(mbValue);
	}

	void UpdateLabel() override {
		mLabelView.SetCaption(mLabel.c_str());
	}
};

class ATUIConfDialogGenericPanel::PathView final : public StringView, public IATUIConfigPathView {
public:
	PathView() : StringView(IDD_CFGPROP_PATH) {
		mPathView.SetOnTextChanged(
			[this](VDUIProxyEditControl *c) {
				VDStringW s = c->GetText();
				if (mValue != s) {
					mValue = s;

					mpParent->OnValueChanged();
				}
			}
		);

		mBrowseView.SetOnClicked(
			[this] {
				OnBrowse();
			}
		);
	}

	IATUIConfigPathView& SetBrowseCaption(const wchar_t *caption) override {
		mBrowseCaption = caption;
		return *this;
	}
	
	IATUIConfigPathView& SetBrowseKey(uint32 key) override {
		mBrowseKey = key;
		return *this;
	}

	IATUIConfigPathView& SetSave() override {
		mbSave = true;
		return *this;
	}

	IATUIConfigPathView& SetType(const wchar_t *filter, const wchar_t *ext) override {
		const wchar_t *filterEnd = filter;
		while(*filterEnd) {
			filterEnd += wcslen(filterEnd) + 1;
		}

		mBrowseFilter.assign(filter, filterEnd);
		mBrowseExt = ext ? ext : L"";
		return *this;
	}

	IATUIConfigPathView& SetTypeImage() override {
		SetBrowseKey('img ');
		return SetType(L"Supported image files\0*.png;*.jpg;*.jpeg\0All files\0*.*\0", nullptr);
	}

private:
	VDUIProxyControl mLabelView;
	VDUIProxyEditControl mPathView;
	VDUIProxyButtonControl mBrowseView;
	bool mbSave = false;
	VDStringW mBrowseFilter;
	VDStringW mBrowseExt;
	VDStringW mBrowseCaption;
	uint32 mBrowseKey = 'path';

	bool OnLoaded() override {
		AddProxy(&mLabelView, IDC_LABEL);
		AddProxy(&mPathView, IDC_PATH);
		AddProxy(&mBrowseView, IDC_BROWSE);
		mResizer.Add(mPathView.GetWindowHandle(), mResizer.kTC);
		mResizer.Add(mBrowseView.GetWindowHandle(), mResizer.kTR);
		UpdateLabel();
		return false;
	}

	void OnEnable(bool enable) override {
		mPathView.SetEnabled(enable);
		mBrowseView.SetEnabled(enable);
	}

	void UpdateView() override {
		mPathView.SetText(mValue.c_str());
	}

	void UpdateLabel() override {
		mLabelView.SetCaption(mLabel.c_str());
	}

	void OnBrowse() {
		const wchar_t *filter = mBrowseFilter.empty() ? L"All files\0*.*\0" : mBrowseFilter.c_str();
		const wchar_t *ext = mBrowseExt.empty() ? nullptr : mBrowseExt.c_str();

		if (mbSave)
			VDGetSaveFileName(mBrowseKey, (VDGUIHandle)mhdlg, L"Select file", filter, ext);
		else
			VDGetLoadFileName(mBrowseKey, (VDGUIHandle)mhdlg, L"Select file", filter, ext);
	}
};

////////////////////////////////////////////////////////////////////////////

class ATUIConfDialogGenericPanel::StringEditView final : public StringView, public IATUIConfigStringEditView {
public:
	StringEditView() : StringView(IDD_CFGPROP_EDIT) {
		mEditView.SetOnTextChanged(
			[this](VDUIProxyEditControl *c) {
				VDStringW s = c->GetText();
				if (mValue != s) {
					mValue = s;

					mpParent->OnValueChanged();
				}
			}
		);
	}

private:
	VDUIProxyControl mLabelView;
	VDUIProxyEditControl mEditView;

	bool OnLoaded() override {
		AddProxy(&mLabelView, IDC_LABEL);
		AddProxy(&mEditView, IDC_EDIT);
		mResizer.Add(mEditView.GetWindowHandle(), mResizer.kTC);
		UpdateLabel();
		return false;
	}

	void OnEnable(bool enable) override {
		mEditView.SetEnabled(enable);
	}

	void UpdateView() override {
		mEditView.SetText(mValue.c_str());
	}

	void UpdateLabel() override {
		mLabelView.SetCaption(mLabel.c_str());
	}
};

////////////////////////////////////////////////////////////////////////////

class ATUIConfDialogGenericPanel::IntEditView final : public IntView, public IATUIConfigIntEditView {
public:
	IntEditView() : IntView(IDD_CFGPROP_EDIT) {
		mEditView.SetOnTextChanged(
			[this](VDUIProxyEditControl *c) {
				VDStringW s = c->GetText();
				long long v = 0;
				wchar_t ch = 0;

				if (1 != swscanf(s.c_str(), L"%llu %lc", &v, &ch) || v != (sint32)v) {
					s.sprintf(L"%u", mValue);
					c->SetText(s.c_str());
					return;
				}

				sint32 val = (sint32)v;
				if (mValue != val) {
					mValue = val;

					mpParent->OnValueChanged();
				}
			}
		);
	}
	
	IATUIConfigIntEditView& SetRange(sint32 minVal, sint32 maxVal) override {
		return *this;
	}

private:
	VDUIProxyControl mLabelView;
	VDUIProxyEditControl mEditView;

	bool OnLoaded() override {
		AddProxy(&mLabelView, IDC_LABEL);
		AddProxy(&mEditView, IDC_EDIT);
		mResizer.Add(mEditView.GetWindowHandle(), mResizer.kTC);
		UpdateLabel();
		UpdateView();
		return false;
	}

	void OnEnable(bool enable) override {
		mEditView.SetEnabled(enable);
	}

	void UpdateView() override {
		VDStringW s;
		s.sprintf(L"%u", mValue);
		mEditView.SetText(s.c_str());
	}

	void UpdateLabel() override {
		mLabelView.SetCaption(mLabel.c_str());
	}
};

////////////////////////////////////////////////////////////////////////////

class ATUIConfDialogGenericPanel::ColorView final : public BaseView, public IATUIConfigColorView {
public:	
	using BaseView::BaseView;
	using BaseView::SetTag;

	ColorView() : BaseView(IDD_CFGPROP_COLOR) {
		mEnableView.SetOnClicked(
			[this] {
				if (mEnableView.GetChecked()) {
					if (mValue < 0)
						SetValue(0);
				} else {
					SetValue(-1);
				}
			}
		);

		mBrowseView.SetOnClicked(
			[this] {
				OnBrowse();
			}
		);
	}

	sint32 GetValue() const override { return mValue; }
	void SetValueImpl(sint32 c) override {
		if (c < 0)
			c = -1;
		else
			c &= 0xFFFFFF;

		if (mValue != c) {
			mValue = c;

			UpdateView();
		}
	}
	
	void SetFixedPaletteImpl(vdspan<const uint32> colors) override {
		mFixedPalette.assign(colors.begin(), colors.end());
	}

	void SetCustomPaletteKeyImpl(const char *key) override {
		mCustomPaletteKey = key ? key : "";
	}


	void Read(const ATPropertySet& pset) override {
		if (!mTag.empty()) {
			uint32 c = 0;

			if (pset.TryGetUint32(mTag.c_str(), c))
				SetValue(c & 0xFFFFFF);
			else
				SetValue(-1);
		}
	}

	void Write(ATPropertySet& pset) const override {
		if (!mTag.empty() && mValue >= 0)
			pset.SetUint32(mTag.c_str(), mValue);
	}

private:
	VDUIProxyControl mLabelView;
	VDUIProxyButtonControl mEnableView;
	VDUIProxyStaticControl mColorView;
	VDUIProxyButtonControl mBrowseView;
	sint32 mValue = -1;
	vdfastvector<uint32> mFixedPalette;
	VDStringA mCustomPaletteKey;

	bool OnLoaded() override {
		AddProxy(&mLabelView, IDC_LABEL);
		AddProxy(&mEnableView, IDC_ENABLE);
		AddProxy(&mColorView, IDC_STATIC_COLOR);
		AddProxy(&mBrowseView, IDC_BROWSE);
		mResizer.Add(mColorView.GetWindowHandle(), mResizer.kMC);
		mResizer.Add(mBrowseView.GetWindowHandle(), mResizer.kTR);
		UpdateLabel();
		UpdateView();
		return false;
	}

	void OnEnable(bool enable) override {
		mEnableView.SetEnabled(enable);
		mColorView.SetEnabled(enable);
		mBrowseView.SetEnabled(enable);
	}

	void UpdateView() override {
		mEnableView.SetChecked(mValue >= 0);

		if (mValue >= 0) {
			mColorView.SetBgOverrideColor(mValue);
			mColorView.SetVisible(true);
			mBrowseView.SetVisible(true);
		} else {
			mColorView.SetVisible(false);
			mBrowseView.SetVisible(false);
		}
	}

	void UpdateLabel() override {
		mLabelView.SetCaption(mLabel.c_str());
	}

	void OnBrowse() {
		sint32 v = VDUIShowColorPicker((VDGUIHandle)mhwnd, mValue, mFixedPalette, mCustomPaletteKey.c_str());

		if (v >= 0) {
			SetValue((uint32)v);
		}
	}
};

////////////////////////////////////////////////////////////////////////////

class ATUIConfDialogGenericPanel::IntDropDownView final : public BaseView, public IATUIConfigIntDropDownView {
public:
	using BaseView::BaseView;
	using BaseView::SetTag;

	IntDropDownView()
		: BaseView(IDD_CFGPROP_DROPDOWN)
	{
		mComboView.SetOnSelectionChanged(
			[this](int idx) {
				if ((unsigned)idx < mChoices.size()) {
					mValue = mChoices[idx].mValue;
					mpParent->OnValueChanged();
				}
			}
		);
	}

	sint32 GetValue() const override { return mValue; }
	void SetValue(sint32 value) override {
		if (mValue != value) {
			mValue = value;

			UpdateView();
		}
	}

	void SetDefaultValueImpl(sint32 value) override {
		mDefaultValue = value;
		mbDefaultValueSet = true;
	}

	IATUIConfigIntDropDownView& AddChoice(sint32 value, const wchar_t *name) override {
		auto& choice = mChoices.emplace_back();
		choice.mValue = value;
		choice.mName = name;

		if (IsCreated())
			mComboView.AddItem(name);

		return *this;
	}

	void Read(const ATPropertySet& pset) override {
		if (!mTag.empty()) {
			sint32 v = pset.GetInt32(mTag.c_str(), mbDefaultValueSet ? mDefaultValue : mChoices.empty() ? 0 : mChoices[0].mValue);
			mValue = ~v;

			SetValue(v);
		}
	}

	void Write(ATPropertySet& pset) const override {
		if (!mTag.empty())
			pset.SetInt32(mTag.c_str(), mValue);
	}

private:
	VDUIProxyControl mLabelView;
	VDUIProxyComboBoxControl mComboView;
	sint32 mValue = 0;
	sint32 mDefaultValue = 0;
	bool mbDefaultValueSet = false;

	struct Choice {
		sint32 mValue;
		VDStringW mName;
	};

	vdvector<Choice> mChoices;

	bool OnLoaded() override {
		AddProxy(&mLabelView, IDC_LABEL);
		AddProxy(&mComboView, IDC_COMBO);
		mResizer.Add(mComboView.GetWindowHandle(), mResizer.kTC);

		for(const Choice& choice : mChoices) {
			mComboView.AddItem(choice.mName.c_str());
		}

		UpdateLabel();
		return false;
	}

	void OnEnable(bool enable) override {
		mComboView.SetEnabled(enable);
	}

	void UpdateView() override {
		int index = -1;

		if (!mChoices.empty()) {
			index = 0;

			auto it = std::find_if(mChoices.begin(), mChoices.end(), [value = mValue](const Choice& choice) { return choice.mValue == value; });

			if (it != mChoices.end())
				index = (int)(it - mChoices.begin());
		}

		mComboView.SetSelection(index);
	}

	void UpdateLabel() override {
		mLabelView.SetCaption(mLabel.c_str());
	}
};

////////////////////////////////////////////////////////////////////////////

class ATUIConfDialogGenericPanel::EnumDropDownView final : public BaseView, public IATUIConfigDropDownView {
public:
	using BaseView::BaseView;
	using BaseView::SetTag;

	EnumDropDownView(const ATEnumLookupTable& enumTable)
		: BaseView(IDD_CFGPROP_DROPDOWN)
		, mEnumTable(enumTable)
		, mDefaultValue(enumTable.mDefaultValue)
	{
		mComboView.SetOnSelectionChanged(
			[this](int idx) {
				if ((unsigned)idx < mChoices.size())
					mValue = mChoices[idx].mValue;
			}
		);
	}

	uint32 GetRawValue() const override { return mValue; }
	void SetRawValue(uint32 value) override {
		if (mValue != value) {
			mValue = value;

			UpdateView();
		}
	}

	void SetRawDefaultValue(uint32 value) override {
		mDefaultValue = value;
	}

	IATUIConfigDropDownView& AddRawChoice(uint32 value, const wchar_t *name) override {
		auto& choice = mChoices.emplace_back();
		choice.mValue = value;
		choice.mName = name;

		if (IsCreated())
			mComboView.AddItem(name);

		return *this;
	}

	void Read(const ATPropertySet& pset) override {
		if (!mTag.empty()) {
			uint32 v = mDefaultValue;

			pset.TryGetEnum(mEnumTable, mTag.c_str(), v);
			mValue = ~v;
			SetRawValue(v);
		}
	}

	void Write(ATPropertySet& pset) const override {
		if (!mTag.empty())
			pset.SetEnum(mEnumTable, mTag.c_str(), mValue);
	}

private:
	const ATEnumLookupTable& mEnumTable;
	VDUIProxyControl mLabelView;
	VDUIProxyComboBoxControl mComboView;
	uint32 mValue = 0;
	uint32 mDefaultValue = 0;

	struct Choice {
		uint32 mValue;
		VDStringW mName;
	};

	vdvector<Choice> mChoices;

	bool OnLoaded() override {
		AddProxy(&mLabelView, IDC_LABEL);
		AddProxy(&mComboView, IDC_COMBO);
		mResizer.Add(mComboView.GetWindowHandle(), mResizer.kTC);

		for(const Choice& choice : mChoices) {
			mComboView.AddItem(choice.mName.c_str());
		}

		UpdateLabel();
		return false;
	}

	void OnEnable(bool enable) override {
		mComboView.SetEnabled(enable);
	}

	void UpdateView() override {
		int index = -1;

		if (!mChoices.empty()) {
			index = 0;

			auto it = std::find_if(mChoices.begin(), mChoices.end(), [value = mValue](const Choice& choice) { return choice.mValue == value; });

			if (it != mChoices.end())
				index = (int)(it - mChoices.begin());
		}

		mComboView.SetSelection(index);
	}

	void UpdateLabel() override {
		mLabelView.SetCaption(mLabel.c_str());
	}
};

////////////////////////////////////////////////////////////////////////////

ATUIConfDialogGenericPanel::ATUIConfDialogGenericPanel(IATUIConfigController& controller)
	: VDDialogFrameW32(IDD_CFGPROP_GENERIC)
	, mController(controller)
{
}

ATUIConfDialogGenericPanel::~ATUIConfDialogGenericPanel() {
	while(!mViews.empty()) {
		delete mViews.back();
		mViews.pop_back();
	}
}

IATUIConfigCheckboxView& ATUIConfDialogGenericPanel::AddCheckbox() {
	CheckboxView *view = new CheckboxView;
	AddView(view);

	return *view;
}

IATUIConfigPathView& ATUIConfDialogGenericPanel::AddPath() {
	PathView *view = new PathView;
	AddView(view);

	return *view;
}

IATUIConfigStringEditView& ATUIConfDialogGenericPanel::AddStringEdit() {
	StringEditView *view = new StringEditView;
	AddView(view);

	return *view;
}

IATUIConfigIntEditView& ATUIConfDialogGenericPanel::AddIntEdit() {
	IntEditView *view = new IntEditView;
	AddView(view);

	return *view;
}

IATUIConfigColorView& ATUIConfDialogGenericPanel::AddColor() {
	ColorView *view = new ColorView;
	AddView(view);

	return *view;
}

IATUIConfigIntDropDownView& ATUIConfDialogGenericPanel::AddIntDropDown() {
	IntDropDownView *view = new IntDropDownView;
	AddView(view);

	return *view;
}

IATUIConfigDropDownView& ATUIConfDialogGenericPanel::AddDropDown(const ATEnumLookupTable& enumTable) {
	EnumDropDownView *view = new EnumDropDownView(enumTable);
	AddView(view);

	return *view;
}

void ATUIConfDialogGenericPanel::AddVerticalSpace() {
	mYPosDLUs += 4;
}

void ATUIConfDialogGenericPanel::Read(const ATPropertySet& pset) {
	for(BaseView *view : mViews)
		view->Read(pset);

	UpdateEnables();
}

void ATUIConfDialogGenericPanel::Write(ATPropertySet& pset) const {
	pset.Clear();

	for(BaseView *view : mViews)
		view->Write(pset);
}

bool ATUIConfDialogGenericPanel::OnLoaded() {
	mController.BuildDialog(*this);

	UpdateEnables();

	vdsize32 sz = GetSize();
	sz.h = DLUsToPixelSize(vdsize32(0, mYPosDLUs)).h;
	SetSize(sz);

	return false;
}

void ATUIConfDialogGenericPanel::OnValueChanged() {
	UpdateEnables();
}

void ATUIConfDialogGenericPanel::UpdateEnables() {
	for(BaseView *view : mViews)
		view->UpdateEnable();
}

void ATUIConfDialogGenericPanel::AddView(BaseView *view) {
	vdautoptr view2(view);

	view->SetViewIndex(*this, (uint32)mViews.size());

	mViews.push_back(nullptr);
	mViews.back() = view2.release();

	if (view->Create(this)) {
		const uint16 id = (uint16)(kBaseId + (mViews.size() - 1));
		view->SetWindowId(id);

		sint32 htDLUs = view->GetTemplateSizeDLUs().h + 1;

		mResizer.AddWithOffsets(view->GetWindowHandle(), 0, mYPosDLUs, 0, mYPosDLUs + htDLUs, mResizer.kTC, true, true);

		mYPosDLUs += htDLUs;
	}
}

////////////////////////////////////////////////////////////////////////////

class ATUIConfDialogGeneric final : public VDDialogFrameW32, private IATUIConfigController {
public:
	ATUIConfDialogGeneric(ATPropertySet& pset, const wchar_t *caption, vdfunction<void(IATUIConfigView&)> fn);

private:
	bool OnLoaded() override;
	void OnDataExchange(bool write) override;

	void BuildDialog(IATUIConfigView& view) override;

	ATPropertySet& mPropSet;
	vdfunction<void(IATUIConfigView&)> mpConfigurator;
	VDStringW mCaption;

	ATUIConfDialogGenericPanel mPropPanel;
};

ATUIConfDialogGeneric::ATUIConfDialogGeneric(ATPropertySet& pset, const wchar_t *caption, vdfunction<void(IATUIConfigView&)> fn)
	: VDDialogFrameW32(IDD_DEVICE_GENERIC)
	, mPropSet(pset)
	, mpConfigurator(std::move(fn))
	, mCaption(caption)
	, mPropPanel(*this)
{
}

bool ATUIConfDialogGeneric::OnLoaded() {
	mResizer.Add(IDOK, mResizer.kBR);
	mResizer.Add(IDCANCEL, mResizer.kBR);

	mPropPanel.Create(this);

	vdrect32 r = GetControlPos(IDC_STATIC_LAYOUTRECT);

	mPropPanel.SetPosition(r.top_left());
	vdsize32 baseSize = GetSize();
	vdsize32 panelSize = mPropPanel.GetSize();
	SetSize(vdsize32(baseSize.w + panelSize.w - r.width(), baseSize.h + panelSize.h - r.height()));
	SetCurrentSizeAsMinSize();

	mResizer.Add(mPropPanel.GetHandleW32(), mResizer.kMC);

	SetCaption(mCaption.c_str());

	OnDataExchange(false);
	mPropPanel.Focus();
	return true;
}

void ATUIConfDialogGeneric::OnDataExchange(bool write) {
	if (write)
		mPropPanel.Write(mPropSet);
	else
		mPropPanel.Read(mPropSet);
}

void ATUIConfDialogGeneric::BuildDialog(IATUIConfigView& view) {
	mpConfigurator(view);
}

////////////////////////////////////////////////////////////////////////////

bool ATUIShowDialogGenericConfig(VDGUIHandle h, IATUIConfigController& controller) {
	ATUIConfDialogGenericPanel dlg(controller);

	return dlg.ShowDialog(h);
}

bool ATUIShowDialogGenericConfig(VDGUIHandle h, ATPropertySet& pset, const wchar_t *name, vdfunction<void(IATUIConfigView&)> fn) {
	ATUIConfDialogGeneric dlg(pset, name, fn);

	return dlg.ShowDialog(h);
}
