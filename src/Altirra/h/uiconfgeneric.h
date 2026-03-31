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

#ifndef f_AT_UICONFGENERIC_H
#define f_AT_UICONFGENERIC_H

#include <vd2/system/refcount.h>

class IATUIConfigPropView {
public:
	template<typename T>
	T& SetTag(this T& self, const char *tag) {
		self.SetTagImpl(tag);
		return self;
	}

	template<typename T>
	T& SetLabel(this T& self, const wchar_t *label) {
		self.SetLabelImpl(label);
		return self;
	}

	template<typename T>
	T& SetHelp(this T& self, const wchar_t *text) {
		self.SetHelpImpl(text);
		return self;
	}

	template<typename T>
	T& SetHelp(this T& self, const wchar_t *caption, const wchar_t *text) {
		self.SetTagImpl(caption, text);
		return self;
	}

	template<typename T>
	T& SetEnableExpr(this T& self, vdfunction<bool()> fn) {
		self.SetEnableExprImpl(fn);
		return self;
	}

	virtual void SetTagImpl(const char *tag) = 0;
	virtual void SetLabelImpl(const wchar_t *label) = 0;
	virtual void SetHelpImpl(const wchar_t *text) = 0;
	virtual void SetHelpImpl(const wchar_t *caption, const wchar_t *text) = 0;
	virtual void SetEnableExprImpl(vdfunction<bool()> fn) = 0;
};

///////////////////////////////////////////////////////////////////////////

class IATUIConfigBoolView : public virtual IATUIConfigPropView {
public:
	template<typename T>
	T& SetDefault(this T& self, bool val) {
		self.SetDefaultImpl(val);
		return self;
	}

	template<typename T>
	T& SetValue(this T& self, bool val) {
		self.SetValueImpl(val);
		return self;
	}

	virtual void SetDefaultImpl(bool val) = 0;
	virtual bool GetValue() const = 0;
	virtual void SetValueImpl(bool v) = 0;
};

class IATUIConfigIntView : public virtual IATUIConfigPropView  {
public:
	template<typename T>
	T& SetDefault(this T& self, sint32 val, bool writeDefault = false) {
		self.SetDefaultImpl(val, writeDefault);
		return self;
	}

	template<typename T>
	T& SetValue(this T& self, sint32 val) {
		self.SetValueImpl(val);
		return self;
	}

	virtual void SetDefaultImpl(sint32 val, bool writeDefault) = 0;

	virtual sint32 GetValue() const = 0;
	virtual void SetValueImpl(sint32 v) = 0;
};

class IATUIConfigStringView : public virtual IATUIConfigPropView  {
public:
	template<typename T>
	T& SetValue(this T& self, const wchar_t *s) {
		self.SetValueImpl(s);
		return self;
	}

	template<typename T>
	T& SetDefault(this T& self, const wchar_t *s, bool writeDefault = false) {
		self.SetDefaultImpl(s, writeDefault);
		return self;
	}

	virtual const wchar_t *GetValue() const = 0;
	virtual void SetValueImpl(const wchar_t *s) = 0;
	virtual void SetDefaultImpl(const wchar_t *s, bool writeDefault) = 0;
};

class IATUIConfigColorView : public virtual IATUIConfigPropView  {
public:
	template<typename T>
	T& SetValue(this T& self, sint32 color) {
		self.SetValueImpl(color);
		return self;
	}

	template<typename T>
	T& SetFixedPalette(this T& self, vdspan<const uint32> colors) {
		self.SetFixedPaletteImpl(colors);
		return self;
	}

	template<typename T>
	T& SetCustomPaletteKey(this T& self, const char *key) {
		self.SetCustomPaletteKeyImpl(key);
		return self;
	}

	virtual sint32 GetValue() const = 0;
	virtual void SetValueImpl(sint32 color) = 0;
	virtual void SetFixedPaletteImpl(vdspan<const uint32> colors) = 0;
	virtual void SetCustomPaletteKeyImpl(const char *key) = 0;
};

///////////////////////////////////////////////////////////////////////////

class IATUIConfigCheckboxView : public virtual IATUIConfigBoolView {
public:
	virtual IATUIConfigCheckboxView& SetText(const wchar_t *text) = 0;
};

class IATUIConfigSliderView : public virtual IATUIConfigIntView {
public:
	virtual IATUIConfigSliderView& SetRange(sint32 minVal, sint32 maxVal) = 0;
	virtual IATUIConfigSliderView& SetPage(sint32 pageSize) = 0;
};

class IATUIConfigPathView : public virtual IATUIConfigStringView {
public:
	virtual IATUIConfigPathView& SetBrowseCaption(const wchar_t *caption) = 0;
	virtual IATUIConfigPathView& SetBrowseKey(uint32 key) = 0;
	virtual IATUIConfigPathView& SetSave() = 0;
	virtual IATUIConfigPathView& SetType(const wchar_t *filter, const wchar_t *ext) = 0;
	virtual IATUIConfigPathView& SetTypeImage() = 0;
};

class IATUIConfigStringEditView : public virtual IATUIConfigStringView {
public:
};

class IATUIConfigIntEditView : public virtual IATUIConfigIntView {
public:
	virtual IATUIConfigIntEditView& SetRange(sint32 minVal, sint32 maxVal) = 0;
};

class IATUIConfigIntDropDownView : public virtual IATUIConfigPropView {
public:
	virtual IATUIConfigIntDropDownView& AddChoice(sint32 value, const wchar_t *text) = 0;

	virtual sint32 GetValue() const = 0;
	virtual void SetValue(sint32 value) = 0;
	virtual void SetDefaultValueImpl(sint32 value) = 0;

	template<typename T>
	T& SetDefaultValue(this T& self, sint32 value) {
		self.SetDefaultValueImpl(value);
		return self;
	}
};

class IATUIConfigDropDownView : public virtual IATUIConfigPropView {
public:
	virtual IATUIConfigDropDownView& AddRawChoice(uint32 value, const wchar_t *text) = 0;

	template<typename T> requires std::is_enum_v<T>
	IATUIConfigDropDownView& AddChoice(T value, const wchar_t *text) {
		return AddRawChoice((uint32)value, text);
	}
	
	template<typename T> requires std::is_enum_v<T>
	void SetDefaultValue(T value) {
		SetRawDefaultValue((uint32)value);
	}

	virtual uint32 GetRawValue() const = 0;
	virtual void SetRawValue(uint32 value) = 0;
	virtual void SetRawDefaultValue(uint32 value) = 0;

	template<typename T> requires std::is_enum_v<T>
	T GetValue() {
		return (T)GetRawValue();
	}

	template<typename T> requires std::is_enum_v<T>
	void SetValue(T value) {
		SetRawValue((uint32)value);
	}
};

///////////////////////////////////////////////////////////////////////////

class IATUIConfigView {
public:
	virtual IATUIConfigCheckboxView& AddCheckbox() = 0;
	virtual IATUIConfigPathView& AddPath() = 0;
	virtual IATUIConfigStringEditView& AddStringEdit() = 0;
	virtual IATUIConfigIntEditView& AddIntEdit() = 0;

	virtual IATUIConfigColorView& AddColor() = 0;
	virtual IATUIConfigIntDropDownView& AddIntDropDown() = 0;
	virtual IATUIConfigDropDownView& AddDropDown(const ATEnumLookupTable& enumTable) = 0;

	virtual void AddVerticalSpace() = 0;

	template<typename T>
	IATUIConfigDropDownView& AddDropDown() {
		return AddDropDown(ATGetEnumLookupTable<T>());
	}

	virtual void Read(const ATPropertySet& pset) = 0;
	virtual void Write(ATPropertySet& pset) const = 0;
};

class IATUIConfigController {
public:
	virtual void BuildDialog(IATUIConfigView& view) = 0;
};

bool ATUIShowDialogGenericConfig(VDGUIHandle h, IATUIConfigController& controller);
bool ATUIShowDialogGenericConfig(VDGUIHandle h, ATPropertySet& pset, const wchar_t *name, vdfunction<void(IATUIConfigView&)> fn);

#endif
