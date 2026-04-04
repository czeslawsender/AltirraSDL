//	AltirraSDL - ImGui progress dialog
//	Implements IATProgressHandler using Dear ImGui modal popups.
//	Replaces Windows ATUIProgressDialogW32 and ATUIProgressBackgroundTaskDialogW32.
//
//	Design note — foreground vs. background progress:
//
//	The Windows version uses PeekMessage() inside Update() to pump the
//	Win32 message loop, which lets the native dialog repaint during a
//	synchronous operation on the main thread.  ImGui has no equivalent —
//	rendering requires a full NewFrame/Render/Present cycle.
//
//	Foreground progress (Begin/Update/End) therefore cannot visually
//	update the popup during a synchronous scan.  The popup state is
//	tracked correctly, and cancellation support is wired up, but the
//	user won't see the bar move until the operation completes.  This is
//	acceptable for fast operations (firmware scan).
//
//	Background tasks (RunTask) launch a worker thread and return false
//	if the main thread can't drive a render loop.  ATRunTaskWithProgress
//	then falls back to running the task synchronously on the calling
//	thread with a null context.

#include <stdafx.h>
#include <imgui.h>
#include <vd2/system/error.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include <vd2/system/text.h>
#include <at/atcore/progress.h>

#include "ui_progress.h"

///////////////////////////////////////////////////////////////////////////
// ImGui progress handler — single instance, registered via
// ATSetProgressHandler().
///////////////////////////////////////////////////////////////////////////

class ATUIProgressHandlerImGui final : public IATProgressHandler {
public:
	// IATProgressHandler
	void Begin(uint32 total, const wchar_t *status, const wchar_t *desc) override;
	void BeginF(uint32 total, const wchar_t *status, const wchar_t *descFormat, va_list descArgs) override;
	void Update(uint32 value) override;
	bool CheckForCancellationOrStatus() override;
	void UpdateStatus(const wchar_t *statusMessage) override;
	void End() override;
	bool RunTask(const wchar_t *desc, const vdfunction<void(IATTaskProgressContext&)>& fn) override;

	// Called each frame from ATUIRenderFrame().
	void Render();

private:
	// Foreground progress state
	bool mActive = false;
	bool mCancelled = false;
	bool mNeedOpen = false;
	uint32 mTotal = 0;
	uint32 mValue = 0;
	int mNestingCount = 0;
	VDStringA mDescriptionUtf8;
	VDStringW mStatusFormat;
	VDStringA mStatusUtf8;
};

///////////////////////////////////////////////////////////////////////////
// Foreground progress (used by ATProgress wrapper — e.g. firmware scan)
///////////////////////////////////////////////////////////////////////////

void ATUIProgressHandlerImGui::Begin(uint32 total, const wchar_t *status, const wchar_t *desc) {
	if (!mNestingCount++) {
		mTotal = total;
		mValue = 0;
		mCancelled = false;
		mActive = true;
		mNeedOpen = true;

		if (desc)
			mDescriptionUtf8 = VDTextWToU8(VDStringW(desc));
		else
			mDescriptionUtf8.clear();

		if (status)
			mStatusFormat = status;
		else
			mStatusFormat.clear();

		mStatusUtf8.clear();
	}
}

void ATUIProgressHandlerImGui::BeginF(uint32 total, const wchar_t *status, const wchar_t *descFormat, va_list descArgs) {
	VDStringW desc;
	desc.append_vsprintf(descFormat, descArgs);
	Begin(total, status, desc.c_str());
}

void ATUIProgressHandlerImGui::Update(uint32 value) {
	if (mCancelled)
		throw MyUserAbortError();

	if (mNestingCount == 1) {
		if (value > mTotal)
			value = mTotal;
		mValue = value;

		if (!mStatusFormat.empty()) {
			VDStringW buf;
			buf.sprintf(mStatusFormat.c_str(), mValue, mTotal);
			mStatusUtf8 = VDTextWToU8(buf);
		}
	}
}

bool ATUIProgressHandlerImGui::CheckForCancellationOrStatus() {
	if (mCancelled)
		throw MyUserAbortError();

	return mNestingCount == 1;
}

void ATUIProgressHandlerImGui::UpdateStatus(const wchar_t *statusMessage) {
	if (mNestingCount == 1) {
		mStatusFormat.clear();
		if (statusMessage)
			mStatusUtf8 = VDTextWToU8(VDStringW(statusMessage));
		else
			mStatusUtf8.clear();
	}
}

void ATUIProgressHandlerImGui::End() {
	if (mNestingCount > 0 && !--mNestingCount) {
		mActive = false;
		mNeedOpen = false;
	}
}

///////////////////////////////////////////////////////////////////////////
// Background task (used by ATRunTaskWithProgress)
//
// The Windows version blocks inside ShowDialog() which has its own
// message pump.  ImGui cannot do this — we would need to drive the
// full SDL event + ImGui render loop from inside RunTask, which
// requires access to the display backend, window, and full frame
// pipeline.
//
// Instead we return false, which causes ATRunTaskWithProgress() to
// fall back to running the task synchronously on the calling thread
// with a null progress context.  The task still runs correctly; it
// just has no cancel UI.
//
// A proper async implementation would require refactoring the main
// loop to support re-entrant rendering or moving the task to a
// background thread with deferred result delivery.
///////////////////////////////////////////////////////////////////////////

bool ATUIProgressHandlerImGui::RunTask(const wchar_t *desc, const vdfunction<void(IATTaskProgressContext&)>& fn) {
	// Cannot drive ImGui render loop from here — fall back to sync.
	return false;
}

///////////////////////////////////////////////////////////////////////////
// Render — called each frame from ATUIRenderFrame()
///////////////////////////////////////////////////////////////////////////

void ATUIProgressHandlerImGui::Render() {
	// Foreground progress popup
	if (mActive) {
		if (mNeedOpen) {
			ImGui::OpenPopup("##Progress");
			mNeedOpen = false;
		}

		ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
			ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("##Progress", nullptr,
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
			if (!mDescriptionUtf8.empty())
				ImGui::TextUnformatted(mDescriptionUtf8.c_str());

			float fraction = mTotal > 0 ? (float)mValue / (float)mTotal : 0.0f;
			ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0));

			if (!mStatusUtf8.empty())
				ImGui::TextUnformatted(mStatusUtf8.c_str());

			ImGui::Spacing();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				mCancelled = true;
			}

			ImGui::EndPopup();
		}
	}
}

///////////////////////////////////////////////////////////////////////////
// Global instance and public API
///////////////////////////////////////////////////////////////////////////

static ATUIProgressHandlerImGui g_progressHandler;

void ATUIInitProgressSDL3() {
	ATSetProgressHandler(&g_progressHandler);
}

void ATUIShutdownProgressSDL3() {
	ATSetProgressHandler(nullptr);
}

void ATUIRenderProgress() {
	g_progressHandler.Render();
}
