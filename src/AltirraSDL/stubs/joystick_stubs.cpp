//	Altirra SDL3 frontend - joystick.h stub implementation
//	Provides a null joystick manager for the initial Linux build.
//	SDL3 joystick support will be added in Phase 6.

#include <stdafx.h>
#include <vd2/system/vdtypes.h>
#include "joystick.h"

class ATJoystickManagerStub final : public IATJoystickManager {
public:
	bool Init(void*, ATInputManager*) override { return true; }
	void Shutdown() override {}

	ATJoystickTransforms GetTransforms() const override { return {}; }
	void SetTransforms(const ATJoystickTransforms&) override {}
	void SetCaptureMode(bool) override {}
	void SetOnActivity(const vdfunction<void()>&) override {}
	void RescanForDevices() override {}

	PollResult Poll() override { return kPollResult_NoActivity; }
	bool PollForCapture(int&, uint32&, uint32&) override { return false; }
	const ATJoystickState *PollForCapture(uint32&) override { return nullptr; }
	uint32 GetJoystickPortStates() const override { return 0; }
};

IATJoystickManager *ATCreateJoystickManager() {
	return new ATJoystickManagerStub();
}
