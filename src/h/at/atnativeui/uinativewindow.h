//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2008-2014 Avery Lee
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

#ifndef f_AT_UINATIVEWINDOW_H
#define f_AT_UINATIVEWINDOW_H

#include <vd2/system/atomic.h>
#include <vd2/system/unknown.h>
#include <vd2/system/vectors.h>
#include <vd2/system/win32/miniwindows.h>
#include <at/atnativeui/nativewindowproxy.h>
#include <at/atui/constants.h>

template<typename T> inline VDZATOM g_ATUINativeWindowClass;

class ATUINativeWindow : public IVDUnknown, public ATUINativeWindowProxy {
public:
	ATUINativeWindow();
	virtual ~ATUINativeWindow();

	static VDZATOM Register();
	static VDZATOM RegisterCustom(const VDZWNDCLASS& wc);

	template<typename T>
	static VDZATOM RegisterForClass(const wchar_t *className) {
		if (!g_ATUINativeWindowClass<T>)
			g_ATUINativeWindowClass<T> = RegisterForClass(className, StaticClassSpecificWndProc<T>);

		return g_ATUINativeWindowClass<T>;
	}

	template<typename T>
	static T *FromHandle(VDZHWND hwnd) {
		return static_cast<T *>(BaseFromHandle(hwnd));
	}

	static ATUINativeWindow *BaseFromHandle(VDZHWND hwnd);

	static void Unregister();

	int AddRef();
	int Release();
	void *AsInterface(uint32 iid) override;

	bool Create(int x, int y, int w, int h, bool visible, const wchar_t *text = nullptr);
	bool CreateChild(ATUINativeWindowProxy& parent, uint32 id, int x, int y, int w, int h, bool visible, const wchar_t *text = nullptr);
	bool CreateChild(VDZHWND hwndParent, VDZUINT id, int x, int y, int w, int h, VDZDWORD styles, VDZDWORD exstyles = 0, const wchar_t *text = nullptr);

	// Enables immediate response to touch taps, disabling the touch delay for
	// right-clicks. Useful on windows that have no right-click actions or need
	// for gestures.
	void SetTouchMode(ATUITouchMode touchMode);

	static VDZLRESULT VDZCALLBACK StaticWndProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);
	virtual VDZLRESULT WndProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) = 0;

protected:
	virtual ATUITouchMode GetTouchModeAtPoint(const vdpoint32& pt) const;

	void UpdateTouchMode(ATUITouchMode touchMode);

	VDAtomicInt mRefCount;

	ATUITouchMode mTouchMode;

	static VDZATOM sWndClass;
	static VDZATOM sWndClassMain;

private:
	using ClassFactoryFn = ATUINativeWindow *(*)();

	template<typename T>
	static VDZLRESULT VDZCALLBACK StaticClassSpecificWndProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
		if (msg == 0x0081 /*WM_NCCREATE*/)
			return StaticWndProcClassSpecificCreate(hwnd, ClassFactory<T>, wParam, lParam);
		else
			return StaticWndProc(hwnd, msg, wParam, lParam);
	}

	static VDZLRESULT VDZCALLBACK StaticWndProcClassSpecificCreate(VDZHWND hwnd, ClassFactoryFn factory, VDZWPARAM wParam, VDZLPARAM lParam);

	template<typename T>
	static ATUINativeWindow *ClassFactory() {
		return new(std::nothrow) T;
	}

	static VDZATOM RegisterForClass(const wchar_t *className, VDZWNDPROC classSpecificWndProc);
};

#endif
