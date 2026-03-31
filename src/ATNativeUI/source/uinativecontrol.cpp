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
#include <windowsx.h>
#include <at/atnativeui/nativeevents.h>
#include <at/atnativeui/uiframe.h>
#include <at/atnativeui/uinativecontrol.h>
#include <at/atnativeui/uinativedraw.h>

////////////////////////////////////////////////////////////////////////////////

ATUINativeControlDrawHandler::ATUINativeControlDrawHandler(ATUINativeWindowProxy& parent) {}

ATUINativeControlDrawHandler::~ATUINativeControlDrawHandler() = default;

vdfloat2 ATUINativeControlDrawHandler::ClientToViewport(const vdpoint32& pt) const {
	const float dpi = mpNativeDraw->GetDpi();

	return vdfloat2 { (float)pt.x, (float)pt.y } * (96.0f / dpi);
}

vdpoint32 ATUINativeControlDrawHandler::ViewportToClient(const vdfloat2& pt) const {
	const float dpi = mpNativeDraw->GetDpi();
	const vdfloat2 cpt = pt * (dpi / 96.0f);

	return vdpoint32(
		VDRoundToInt32(cpt.x),
		VDRoundToInt32(cpt.y)
	);
}

bool ATUINativeControlDrawHandler::HandleMessage(ATUINativeControlMessage& msg) {
	if (msg.mMsg == WM_CREATE) {
		mpNativeDraw.reset(new ATUINativeDraw);
		mpNativeDraw->Init(msg.mParent);
		return false;
	} else if (msg.mMsg == WM_DESTROY) {
		mpNativeDraw = nullptr;
		return false;
	} else if (msg.mMsg == WM_ERASEBKGND) {
		return true;
	} else if (msg.mMsg == WM_PAINT) {
		if (PAINTSTRUCT ps; BeginPaint(msg.mhwnd, &ps)) {
			if (mbRTResizePending) {
				vdsize32 sz = msg.mParent.GetClientSize();
				mpNativeDraw->Resize(sz.w, sz.h);

				mbRTResizePending = false;
				mRTWidth = sz.w;
				mRTHeight = sz.h;
			}

			if (mpNativeDraw->Begin()) {
				OnPaint(*mpNativeDraw);

				mpNativeDraw->End();
			}

			EndPaint(msg.mhwnd, &ps);
		}

		return true;
	} else if (msg.mMsg == WM_SIZE) {
		mpNativeDraw->SetDpi(ATUIGetWindowDpiW32(msg.mhwnd));

		if (ShouldRedrawOnResize())
			msg.mParent.Invalidate();

		if (!mbRTResizePending && (LOWORD(msg.mLParam) != mRTWidth || HIWORD(msg.mLParam) != mRTHeight)) {
			mbRTResizePending = true;
		}

		return false;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

sint32 ATUINativeControlScrollHandler::GetScrollPos(bool vert) const {
	SCROLLINFO si {};
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_POS;

	GetScrollInfo(mParent.GetWindowHandle(), vert ? SB_VERT : SB_HORZ, &si);
	return si.nPos;
}

void ATUINativeControlScrollHandler::SetScrollVisible(bool vert, bool visible) {
	ShowScrollBar(mParent.GetWindowHandle(), vert ? SB_VERT : SB_HORZ, visible);
}

void ATUINativeControlScrollHandler::SetScrollParams(bool vert, std::optional<sint32> pos, std::optional<std::pair<sint32, sint32>> range, std::optional<sint32> pageSize) {
	SCROLLINFO si {};
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_DISABLENOSCROLL;

	if (pos.has_value()) {
		si.fMask |= SIF_POS;
		si.nPos = pos.value();
	}

	if (range.has_value()) {
		si.fMask |= SIF_RANGE;
		si.nMin = range.value().first;
		si.nMax = range.value().second;
	}

	if (pageSize.has_value()) {
		si.fMask |= SIF_PAGE;
		si.nPage = pageSize.value();
	}

	SetScrollInfo(mParent.GetWindowHandle(), vert ? SB_VERT : SB_HORZ, &si, true);
}

bool ATUINativeControlScrollHandler::HandleMessage(ATUINativeControlMessage& msg) {
	switch(msg.mMsg) {
		case WM_HSCROLL:
			if (!msg.mLParam) {
				HandleScrollMessage(false, LOWORD(msg.mWParam));
				return true;
			}
			break;

		case WM_VSCROLL:
			if (!msg.mLParam) {
				HandleScrollMessage(true, LOWORD(msg.mWParam));
				return true;
			}
			break;
	}

	return false;
}

sint32 ATUINativeControlScrollHandler::GetScrollLineDelta(bool vert) const {
	return 1;
}

void ATUINativeControlScrollHandler::OnHScroll(sint32 hpos) {
}

void ATUINativeControlScrollHandler::OnVScroll(sint32 vpos) {
}

void ATUINativeControlScrollHandler::HandleScrollMessage(bool vert, uint32 code) {
	SCROLLINFO si {};
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_TRACKPOS | SIF_RANGE | SIF_PAGE | SIF_POS;

	const int sbCode = vert ? SB_VERT : SB_HORZ;
	GetScrollInfo(mParent.GetWindowHandle(), sbCode, &si);

	sint32 newPos = si.nPos;
	sint32 newPosDelta = 0;

	switch(code) {
		case SB_LEFT:
			newPos = si.nMin;
			break;

		case SB_RIGHT:
			newPos = si.nMax;
			break;

		case SB_LINELEFT:
			newPosDelta = -GetScrollLineDelta(vert);
			break;

		case SB_LINERIGHT:
			newPosDelta = GetScrollLineDelta(vert);
			break;

		case SB_PAGELEFT:
			newPosDelta = -(sint32)si.nPage;
			break;

		case SB_PAGERIGHT:
			newPosDelta = (sint32)si.nPage;
			break;

		case SB_THUMBTRACK:
			newPos = si.nTrackPos;
			break;
	}

	if (newPos > si.nMax)
		newPos = si.nMax;

	if (newPos < si.nMin)
		newPos = si.nMin;

	if (newPosDelta != 0) {
		if (newPosDelta < 0) {
			if (newPos > si.nMin && (uint32)newPos - (uint32)si.nMin > (uint32)-newPosDelta)
				newPos += newPosDelta;
			else
				newPos = si.nMin;
		} else {
			if (newPos < si.nMax && (uint32)si.nMax - (uint32)newPos > (uint32)newPosDelta)
				newPos += newPosDelta;
			else
				newPos = si.nMax;
		}
	}

	if (si.nPos != newPos) {
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_POS;
		si.nPos = newPos;

		SetScrollInfo(mParent.GetWindowHandle(), sbCode, &si, TRUE);
	}

	if (vert)
		OnVScroll(newPos);
	else
		OnHScroll(newPos);
}

////////////////////////////////////////////////////////////////////////////////

bool ATUINativeControlMouseInputHandler::HandleMessage(ATUINativeControlMessage& msg) {
	switch(msg.mMsg) {
		case WM_LBUTTONDOWN:
			RegisterForMouseLeave(msg);
			return OnMouseDown(vdpoint32(GET_X_LPARAM(msg.mLParam), GET_Y_LPARAM(msg.mLParam)), 0);

		case WM_RBUTTONDOWN:
			RegisterForMouseLeave(msg);
			return OnMouseDown(vdpoint32(GET_X_LPARAM(msg.mLParam), GET_Y_LPARAM(msg.mLParam)), 1);

		case WM_MBUTTONDOWN:
			RegisterForMouseLeave(msg);
			return OnMouseDown(vdpoint32(GET_X_LPARAM(msg.mLParam), GET_Y_LPARAM(msg.mLParam)), 2);

		case WM_XBUTTONDOWN:
			RegisterForMouseLeave(msg);
			return OnMouseDown(vdpoint32(GET_X_LPARAM(msg.mLParam), GET_Y_LPARAM(msg.mLParam)), 3);

		case WM_LBUTTONUP:
			RegisterForMouseLeave(msg);
			return OnMouseUp(vdpoint32(GET_X_LPARAM(msg.mLParam), GET_Y_LPARAM(msg.mLParam)), 0);

		case WM_RBUTTONUP:
			RegisterForMouseLeave(msg);
			return OnMouseUp(vdpoint32(GET_X_LPARAM(msg.mLParam), GET_Y_LPARAM(msg.mLParam)), 1);

		case WM_MBUTTONUP:
			RegisterForMouseLeave(msg);
			return OnMouseUp(vdpoint32(GET_X_LPARAM(msg.mLParam), GET_Y_LPARAM(msg.mLParam)), 2);

		case WM_XBUTTONUP:
			RegisterForMouseLeave(msg);
			return OnMouseUp(vdpoint32(GET_X_LPARAM(msg.mLParam), GET_Y_LPARAM(msg.mLParam)), 3);

		case WM_MOUSEMOVE:
			RegisterForMouseLeave(msg);
			OnMouseMove(vdpoint32(GET_X_LPARAM(msg.mLParam), GET_Y_LPARAM(msg.mLParam)));
			return true;

		case WM_MOUSELEAVE:
			mbLeaveRegistered = false;
			OnMouseLeave();
			return true;

		case WM_CAPTURECHANGED:
			OnCaptureLost();
			return true;

		default:
			return false;
	}
}

bool ATUINativeControlMouseInputHandler::OnMouseDown(const vdpoint32& pt, int button) {
	return false;
}

bool ATUINativeControlMouseInputHandler::OnMouseUp(const vdpoint32& pt, int button) {
	return false;
}

void ATUINativeControlMouseInputHandler::OnMouseMove(const vdpoint32& pt) {
}

void ATUINativeControlMouseInputHandler::OnMouseLeave() {
	mbLeaveRegistered = true;
}

void ATUINativeControlMouseInputHandler::OnCaptureLost() {
}

void ATUINativeControlMouseInputHandler::RegisterForMouseLeave(const ATUINativeControlMessage& msg) {
	if (!mbLeaveRegistered) {
		mbLeaveRegistered = true;

		TRACKMOUSEEVENT tme {};
		tme.cbSize = sizeof(TRACKMOUSEEVENT);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = msg.mhwnd;

		TrackMouseEvent(&tme);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool ATUINativeControlKeyboardInputHandler::HandleMessage(ATUINativeControlMessage& msg) {
	switch(msg.mMsg) {
		case WM_KEYDOWN:
			return OnKeyDown(ATUINativeDecodeKeyEvent(msg.mWParam, msg.mLParam));

		case WM_KEYUP:
			return OnKeyUp(ATUINativeDecodeKeyEvent(msg.mWParam, msg.mLParam));

		case WM_CHAR:
			return OnChar(ATUINativeDecodeCharEvent(msg.mWParam, msg.mLParam));
	}

	return false;
}

bool ATUINativeControlKeyboardInputHandler::OnKeyDown(const ATUIKeyEvent& event) {
	return false;
}

bool ATUINativeControlKeyboardInputHandler::OnKeyUp(const ATUIKeyEvent& event) {
	return false;
}

bool ATUINativeControlKeyboardInputHandler::OnChar(const ATUICharEvent& event) {
	return false;
}

////////////////////////////////////////////////////////////////////////////////

VDZLRESULT ATUINativeControlBase::WndProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	ATUINativeControlMessage cmsg { *this };

	cmsg.mhwnd = mhwnd;
	cmsg.mMsg = msg;
	cmsg.mWParam = wParam;
	cmsg.mLParam = lParam;
	cmsg.mResult = 0;

	if (OnMessage(cmsg))
		return cmsg.mResult;

	switch(msg) {
		case WM_CREATE:
			OnCreate();
			break;

		case WM_DESTROY:
			OnDestroy();
			break;

		case WM_SIZE:
			OnSize();
			break;

		case WM_GETDLGCODE:
			if (mbWantEnter && wParam == VK_RETURN)
				return DLGC_WANTMESSAGE;

			return mDlgCode;
	}

	return ATUINativeWindow::WndProc(msg, wParam, lParam);
}

bool ATUINativeControlBase::OnMessage(ATUINativeControlMessage& msg) {
	return false;
}

void ATUINativeControlBase::OnCreate() {
}

void ATUINativeControlBase::OnDestroy() {
}

void ATUINativeControlBase::OnSize() {
}

void ATUINativeControlBase::SetWantArrowKeys() {
	mDlgCode |= DLGC_WANTARROWS;
}

void ATUINativeControlBase::SetWantChars() {
	mDlgCode |= DLGC_WANTCHARS;
}

void ATUINativeControlBase::SetWantTab() {
	mDlgCode |= DLGC_WANTTAB;
}

void ATUINativeControlBase::SetWantEnter() {
	mbWantEnter = true;
}
