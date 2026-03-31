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

#ifndef f_AT_ATNATIVEUI_UINATIVECONTROL_H
#define f_AT_ATNATIVEUI_UINATIVECONTROL_H

#include <vd2/system/vdalloc.h>
#include <vd2/system/win32/miniwindows.h>
#include <at/atnativeui/uinativewindow.h>
#include <at/atui/events.h>

class ATUINativeControlBase;
class ATUINativeDraw;

struct ATUINativeControlMessage {
	ATUINativeControlBase& mParent;

	VDZHWND mhwnd;
	VDZUINT mMsg;
	VDZWPARAM mWParam;
	VDZLPARAM mLParam;

	VDZLRESULT mResult;
};

class ATUINativeControlDrawHandler {
public:
	ATUINativeControlDrawHandler(ATUINativeWindowProxy& parent);
	~ATUINativeControlDrawHandler();

	ATUINativeDraw *GetNativeDraw() const { return mpNativeDraw.get(); }

	vdfloat2 ClientToViewport(const vdpoint32& pt) const;
	vdpoint32 ViewportToClient(const vdfloat2& pt) const;

	bool HandleMessage(ATUINativeControlMessage& msg);
	virtual bool ShouldRedrawOnResize() const { return false; }
	virtual void OnPaint(ATUINativeDraw& draw) {}

private:
	vduniqueptr<ATUINativeDraw> mpNativeDraw;
	sint32 mRTWidth = 0;
	sint32 mRTHeight = 0;
	bool mbRTResizePending = true;
};

class ATUINativeControlScrollHandler {
public:
	ATUINativeControlScrollHandler(ATUINativeWindowProxy& parent) : mParent(parent) {}

	sint32 GetScrollPos(bool vert) const;
	void SetScrollVisible(bool vert, bool visible);
	void SetScrollParams(bool vert, std::optional<sint32> pos, std::optional<std::pair<sint32, sint32>> range, std::optional<sint32> pageSize);

	bool HandleMessage(ATUINativeControlMessage& msg);

	virtual sint32 GetScrollLineDelta(bool vert) const;
	virtual void OnHScroll(sint32 hpos);
	virtual void OnVScroll(sint32 vpos);

private:
	void HandleScrollMessage(bool vert, uint32 code);

	ATUINativeWindowProxy& mParent;
};

class ATUINativeControlMouseInputHandler {
public:
	ATUINativeControlMouseInputHandler(ATUINativeWindowProxy& parent) {}

	bool HandleMessage(ATUINativeControlMessage& msg);

	virtual bool OnMouseDown(const vdpoint32& pt, int button);
	virtual bool OnMouseUp(const vdpoint32& pt, int button);
	virtual void OnMouseMove(const vdpoint32& pt);
	virtual void OnMouseLeave();
	virtual void OnCaptureLost();

private:
	void RegisterForMouseLeave(const ATUINativeControlMessage& msg);

	bool mbLeaveRegistered = false;
};

class ATUINativeControlKeyboardInputHandler {
public:
	ATUINativeControlKeyboardInputHandler(ATUINativeWindowProxy& parent) {}

	bool HandleMessage(ATUINativeControlMessage& msg);

	virtual bool OnKeyDown(const ATUIKeyEvent& event);
	virtual bool OnKeyUp(const ATUIKeyEvent& event);
	virtual bool OnChar(const ATUICharEvent& event);
};

class ATUINativeControlBase : public ATUINativeWindow {
public:
	VDZLRESULT WndProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) override;

protected:
	virtual bool OnMessage(ATUINativeControlMessage& msg);
	virtual void OnCreate();
	virtual void OnDestroy();
	virtual void OnSize();

	void SetWantArrowKeys();
	void SetWantChars();
	void SetWantTab();
	void SetWantEnter();

	sint32 mWidth = 0;
	sint32 mHeight = 0;
	uint32 mDlgCode = 0;
	bool mbWantEnter : 1 = false;
};

template<typename... Handlers>
class ATUINativeControl : public ATUINativeControlBase, public Handlers... {
public:
	ATUINativeControl() : Handlers(static_cast<ATUINativeWindowProxy&>(*this))... {}

	bool OnMessage(ATUINativeControlMessage& msg) override {
		return (... || static_cast<Handlers&>(*this).HandleMessage(msg));
	}
};

#endif
