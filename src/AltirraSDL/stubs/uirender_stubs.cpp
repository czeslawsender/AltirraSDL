//	Altirra SDL3 frontend - uirender.h stub implementation
//	Provides a null IATUIRenderer for the Linux build.

#include <stdafx.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include "uirender.h"

class ATUIRendererNull final : public IATUIRenderer {
	int mRefCount = 1;
public:
	int AddRef() override { return ++mRefCount; }
	int Release() override { int n = --mRefCount; if (!n) delete this; return n; }

	// IATDeviceIndicatorManager
	void SetStatusFlags(uint32) override {}
	void ResetStatusFlags(uint32, uint32) override {}
	void PulseStatusFlags(uint32) override {}
	void SetStatusCounter(uint32, uint32) override {}
	void SetDiskLEDState(uint32, sint32) override {}
	void SetDiskMotorActivity(uint32, bool) override {}
	void SetDiskErrorState(uint32, bool) override {}
	void SetHActivity(bool) override {}
	void SetIDEActivity(bool, uint32) override {}
	void SetPCLinkActivity(bool) override {}
	void SetFlashWriteActivity() override {}
	void SetCartridgeActivity(sint32, sint32) override {}
	void SetCassetteIndicatorVisible(bool) override {}
	void SetCassettePosition(float, float, bool, bool) override {}
	void SetRecordingPosition() override {}
	void SetRecordingPositionPaused() override {}
	void SetRecordingPosition(float, sint64, bool) override {}
	void SetModemConnection(const char*) override {}
	void SetStatusMessage(const wchar_t*) override {}
	uint32 AllocateErrorSourceId() override { return 0; }
	void ClearErrors(uint32) override {}
	void ReportError(uint32, const wchar_t*) override {}

	// IATUIRenderer
	bool IsVisible() const override { return false; }
	void SetVisible(bool) override {}
	void SetCyclesPerSecond(double) override {}
	void SetLedStatus(uint8) override {}
	void SetHeldButtonStatus(uint8) override {}
	void SetPendingHoldMode(bool) override {}
	void SetPendingHeldKey(int) override {}
	void SetPendingHeldButtons(uint8) override {}
	void ClearWatchedValue(int) override {}
	void SetWatchedValue(int, uint32, WatchFormat) override {}
	void SetTracingSize(sint64) override {}
	void SetAudioStatus(const ATUIAudioStatus*) override {}
	void SetAudioMonitor(bool, ATAudioMonitor*) override {}
	void SetAudioDisplayEnabled(bool, bool) override {}
	void SetAudioScopeEnabled(bool) override {}
	void SetSlightSID(ATSlightSIDEmulator*) override {}
	vdrect32 GetPadArea() const override { return {}; }
	void SetPadInputEnabled(bool) override {}
	void SetFpsIndicator(float) override {}
	void SetMessage(StatusPriority, const wchar_t*) override {}
	void ClearMessage(StatusPriority) override {}
	void SetHoverTip(int, int, const wchar_t*) override {}
	void SetPaused(bool) override {}
	void SetUIManager(ATUIManager*) override {}
	void Relayout(int, int) override {}
	void Update() override {}
	sint32 GetIndicatorSafeHeight() const override { return 0; }
	void AddIndicatorSafeHeightChangedHandler(const vdfunction<void()>*) override {}
	void RemoveIndicatorSafeHeightChangedHandler(const vdfunction<void()>*) override {}
	void BeginCustomization() override {}
};

void ATCreateUIRenderer(IATUIRenderer **r) {
	*r = new ATUIRendererNull();
}
