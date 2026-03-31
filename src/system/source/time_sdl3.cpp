//	Altirra - Atari 800/800XL/5200 emulator
//	System library - timing functions for non-Windows (SDL3)

#include <stdafx.h>
#include <chrono>
#include <thread>

#include <SDL3/SDL.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/time.h>
#include <vd2/system/thread.h>

// -------------------------------------------------------------------------
// Tick / precision timer
// -------------------------------------------------------------------------

uint32 VDGetCurrentTick() {
	return (uint32)SDL_GetTicks();
}

uint64 VDGetCurrentTick64() {
	return SDL_GetTicks();
}

uint64 VDGetPreciseTick() {
	return SDL_GetPerformanceCounter();
}

static uint64 sInitPreciseFreq() {
	return SDL_GetPerformanceFrequency();
}

uint64 VDGetPreciseTicksPerSecondI() {
	static uint64 freq = sInitPreciseFreq();
	return freq;
}

double VDGetPreciseTicksPerSecond() {
	static double freq = (double)sInitPreciseFreq();
	return freq;
}

double VDGetPreciseSecondsPerTick() {
	static double spt = 1.0 / (double)sInitPreciseFreq();
	return spt;
}

uint32 VDGetAccurateTick() {
	return VDGetCurrentTick();
}

// -------------------------------------------------------------------------
// VDCallbackTimer
// -------------------------------------------------------------------------

VDCallbackTimer::VDCallbackTimer()
	: mTimerAccuracy(0)
	, mpCB(nullptr)
	, mTimerPeriod(0)
	, mbExit(false)
	, mbPrecise(true)
{
}

VDCallbackTimer::~VDCallbackTimer() {
	Shutdown();
}

bool VDCallbackTimer::Init(IVDTimerCallback *pCB, uint32 period_ms) {
	return Init2(pCB, period_ms * 10000);
}

bool VDCallbackTimer::Init2(IVDTimerCallback *pCB, uint32 period_100ns) {
	return Init3(pCB, period_100ns, period_100ns >> 1, true);
}

bool VDCallbackTimer::Init3(IVDTimerCallback *pCB, uint32 period_100ns, uint32 /*accuracy_100ns*/, bool precise) {
	Shutdown();

	mpCB     = pCB;
	mbExit   = false;
	mbPrecise = precise;
	mTimerAccuracy  = 1;
	mTimerPeriod    = period_100ns;
	mTimerPeriodAdjustment = 0;
	mTimerPeriodDelta      = 0;

	if (ThreadStart())
		return true;

	Shutdown();
	return false;
}

void VDCallbackTimer::Shutdown() {
	if (isThreadAttached()) {
		mbExit = true;
		msigExit.signal();
		ThreadWait();
	}
	mTimerAccuracy = 0;
}

void VDCallbackTimer::SetRateDelta(int delta_100ns) {
	mTimerPeriodDelta = delta_100ns;
}

void VDCallbackTimer::AdjustRate(int adjustment_100ns) {
	mTimerPeriodAdjustment += adjustment_100ns;
}

bool VDCallbackTimer::IsTimerRunning() const {
	return mTimerAccuracy != 0;
}

void VDCallbackTimer::ThreadRun() {
	using namespace std::chrono;

	auto period_ns = nanoseconds((uint64)mTimerPeriod * 100);
	auto next = steady_clock::now() + period_ns;
	const auto maxDelay = period_ns * 2;

	while (!mbExit) {
		auto now = steady_clock::now();
		auto remaining = next - now;

		if (remaining > nanoseconds(0)) {
			uint32 ms = (uint32)(duration_cast<milliseconds>(remaining).count() + 1);
			msigExit.tryWait(ms);
		}

		if (mbExit)
			break;

		mpCB->TimerCallback();

		int adjust   = mTimerPeriodAdjustment.xchg(0);
		int perdelta = mTimerPeriodDelta;
		uint64 ep = (uint64)mTimerPeriod + adjust + perdelta;
		period_ns = nanoseconds(ep * 100);
		next += period_ns;

		// drift guard
		auto late = steady_clock::now() - next;
		if (late > maxDelay)
			next = steady_clock::now() + period_ns;
	}
}

// -------------------------------------------------------------------------
// VDLazyTimer
// -------------------------------------------------------------------------

VDLazyTimer::VDLazyTimer() {}

VDLazyTimer::~VDLazyTimer() {
	Stop();
}

void VDLazyTimer::SetOneShot(IVDTimerCallback *pCB, uint32 delay) {
	SetOneShotFn([=]() { pCB->TimerCallback(); }, delay);
}

void VDLazyTimer::SetOneShotFn(const vdfunction<void()>& fn, uint32 delay) {
	Stop();
	mpFn = fn;
	mbPeriodic = false;
	mTimerId = 1;

	vdfunction<void()> f = fn;
	std::thread([f, delay]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(delay));
		f();
	}).detach();
}

void VDLazyTimer::SetPeriodic(IVDTimerCallback *pCB, uint32 delay) {
	SetPeriodicFn([=]() { pCB->TimerCallback(); }, delay);
}

void VDLazyTimer::SetPeriodicFn(const vdfunction<void()>& fn, uint32 delay) {
	Stop();
	mpFn = fn;
	mbPeriodic = true;
	mTimerId = 1;
	mbTimerRunning = true;

	uint32 ms = delay;
	mTimerThread = std::thread([this, ms]() {
		while (mbTimerRunning.load(std::memory_order_relaxed)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
			if (!mbTimerRunning.load(std::memory_order_relaxed))
				break;
			if (mpFn)
				mpFn();
		}
	});
}

void VDLazyTimer::Stop() {
	mbTimerRunning = false;
	if (mTimerThread.joinable())
		mTimerThread.join();
	mTimerId = 0;
}

// StaticTimeCallback is unused on non-Windows
void VDLazyTimer::StaticTimeCallback(VDZHWND, VDZUINT, VDZUINT_PTR, VDZDWORD) {}
