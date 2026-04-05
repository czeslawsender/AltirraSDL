//	AltirraSDL - SDL3 command handler registration
//	Registers ATUICommand entries for all accelerator-table-driven commands.
//	This replaces the Windows cmd*.cpp files which depend on Win32 UI.

#include <stdafx.h>
#include <SDL3/SDL.h>
#include <at/atui/uicommandmanager.h>
#include <at/ataudio/pokey.h>
#include <at/atcore/constants.h>

#include "simulator.h"
#include "uiaccessors.h"
#include "inputmanager.h"
#include "inputdefs.h"
#include "antic.h"
#include "gtia.h"
#include "uikeyboard.h"
#include "ui_main.h"
#include "ui_debugger.h"
#include "ui_textselection.h"

extern ATSimulator g_sim;
extern SDL_Window *g_pWindow;
extern ATUIKeyboardOptions g_kbdOpts;
extern bool g_copyFrameRequested;

ATUICommandManager g_ATUICommandMgr;

ATUICommandManager& ATUIGetCommandManager() {
	return g_ATUICommandMgr;
}

// =========================================================================
// Display context commands
// =========================================================================

static void CmdPulseWarpOn() {
	ATUISetTurboPulse(true);
}

static void CmdPulseWarpOff() {
	ATUISetTurboPulse(false);
}

static void CmdCycleQuickMaps() {
	ATInputManager *pIM = g_sim.GetInputManager();
	if (pIM) pIM->CycleQuickMaps();
}

static void CmdHoldKeys() {
	ATUIToggleHoldKeys();
}

static void CmdWarmReset() {
	g_sim.WarmReset();
	g_sim.Resume();
}

static void CmdColdReset() {
	g_sim.ColdReset();
	g_sim.Resume();
	if (!g_kbdOpts.mbAllowShiftOnColdReset)
		g_sim.GetPokey().SetShiftKeyState(false, true);
}

static void CmdToggleNTSCPAL() {
	if (g_sim.GetVideoStandard() == kATVideoStandard_NTSC)
		g_sim.SetVideoStandard(kATVideoStandard_PAL);
	else
		g_sim.SetVideoStandard(kATVideoStandard_NTSC);
}

static void CmdNextANTICVisMode() {
	ATAnticEmulator& antic = g_sim.GetAntic();
	antic.SetAnalysisMode((ATAnticEmulator::AnalysisMode)
		(((int)antic.GetAnalysisMode() + 1) % ATAnticEmulator::kAnalyzeModeCount));
}

static void CmdNextGTIAVisMode() {
	ATGTIAEmulator& gtia = g_sim.GetGTIA();
	gtia.SetAnalysisMode((ATGTIAEmulator::AnalysisMode)
		(((int)gtia.GetAnalysisMode() + 1) % ATGTIAEmulator::kAnalyzeCount));
}

static void CmdTogglePause() {
	if (g_sim.IsPaused()) g_sim.Resume();
	else g_sim.Pause();
}

static void CmdCaptureMouse() {
	ATUICaptureMouse();
}

static void CmdToggleFullScreen() {
	extern void ATSetFullscreen(bool fs);
	bool fs = (SDL_GetWindowFlags(g_pWindow) & SDL_WINDOW_FULLSCREEN) != 0;
	ATSetFullscreen(!fs);
}

static void CmdToggleSlowMotion() {
	ATUISetSlowMotion(!ATUIGetSlowMotion());
}

static void CmdToggleChannel1() {
	ATPokeyEmulator& pokey = g_sim.GetPokey();
	pokey.SetChannelEnabled(0, !pokey.IsChannelEnabled(0));
}

static void CmdToggleChannel2() {
	ATPokeyEmulator& pokey = g_sim.GetPokey();
	pokey.SetChannelEnabled(1, !pokey.IsChannelEnabled(1));
}

static void CmdToggleChannel3() {
	ATPokeyEmulator& pokey = g_sim.GetPokey();
	pokey.SetChannelEnabled(2, !pokey.IsChannelEnabled(2));
}

static void CmdToggleChannel4() {
	ATPokeyEmulator& pokey = g_sim.GetPokey();
	pokey.SetChannelEnabled(3, !pokey.IsChannelEnabled(3));
}

static void CmdPasteText() {
	ATUIPasteText();
}

static void CmdSaveFrame() {
	ATUIShowSaveFrameDialog(g_pWindow);
}

static void CmdCopyText() {
	ATUITextCopy(ATTextCopyMode::ASCII);
}

static void CmdCopyFrame() {
	g_copyFrameRequested = true;
}

static void CmdSelectAll() {
	ATUITextSelectAll();
}

static void CmdDeselect() {
	ATUITextDeselect();
}

// =========================================================================
// Global context commands
// =========================================================================

static void CmdBootImage() {
	ATUIShowBootImageDialog(g_pWindow);
}

static void CmdOpenImage() {
	ATUIShowOpenImageDialog(g_pWindow);
}

static void CmdOpenSourceFile() {
	ATUIShowOpenSourceFileDialog(g_pWindow);
}

// These access g_uiState which is in main_sdl3.cpp — use extern accessors
extern void ATUISetShowDiskManager(bool v);
extern void ATUISetShowSystemConfig(bool v);
extern void ATUISetShowCheater(bool v);

static void CmdDrivesDialog() {
	ATUISetShowDiskManager(true);
}

static void CmdConfigure() {
	ATUISetShowSystemConfig(true);
}

static void CmdCheatDialog() {
	ATUISetShowCheater(true);
}

static void CmdDebugRunStop() {
	ATUIDebuggerRunStop();
}

static void CmdDebugStepInto() {
	ATUIDebuggerStepInto();
}

static void CmdDebugStepOver() {
	ATUIDebuggerStepOver();
}

static void CmdDebugStepOut() {
	ATUIDebuggerStepOut();
}

static void CmdDebugBreak() {
	ATUIDebuggerBreak();
}

// Pane commands — only active when debugger is open.
// mpTestFn returns false when debugger is closed, so the key is NOT consumed
// and falls through to the emulator (matching old if-chain behavior).
static bool TestDebuggerOpen() {
	return ATUIDebuggerIsOpen();
}

static void CmdPaneDisplay() {
	ATUIDebuggerFocusDisplay();
}

static void CmdPaneConsole() {
	ATActivateUIPane(kATUIPaneId_Console, true, true);
}

static void CmdPaneRegisters() {
	ATActivateUIPane(kATUIPaneId_Registers, true, true);
}

static void CmdPaneDisassembly() {
	ATActivateUIPane(kATUIPaneId_Disassembly, true, true);
}

static void CmdPaneCallStack() {
	ATActivateUIPane(kATUIPaneId_CallStack, true, true);
}

static void CmdPaneHistory() {
	ATActivateUIPane(kATUIPaneId_History, true, true);
}

static void CmdPaneMemory1() {
	ATActivateUIPane(kATUIPaneId_MemoryN, true, true);
}

static void CmdPanePrinterOutput() {
	ATActivateUIPane(kATUIPaneId_PrinterOutput, true, true);
}

static void CmdPaneProfileView() {
	ATActivateUIPane(kATUIPaneId_Profiler, true, true);
}

// =========================================================================
// Debugger context commands
// =========================================================================

static void CmdDebugRun() {
	ATUIDebuggerRunStop();
}

static void CmdDebugToggleBreakpoint() {
	ATUIDebuggerToggleBreakpoint();
}

static void CmdDebugNewBreakpoint() {
	ATActivateUIPane(kATUIPaneId_Breakpoints, true, true);
	ATUIDebuggerShowBreakpointDialog(-1);
}

// =========================================================================
// Command table
// =========================================================================

static const ATUICommand kSDL3Commands[] = {
	// Display context
	{ "System.PulseWarpOn",            CmdPulseWarpOn,          nullptr, nullptr, nullptr },
	{ "System.PulseWarpOff",           CmdPulseWarpOff,         nullptr, nullptr, nullptr },
	{ "Input.CycleQuickMaps",          CmdCycleQuickMaps,       nullptr, nullptr, nullptr },
	{ "Console.HoldKeys",              CmdHoldKeys,             nullptr, nullptr, nullptr },
	{ "System.WarmReset",              CmdWarmReset,             nullptr, nullptr, nullptr },
	{ "System.ColdReset",              CmdColdReset,             nullptr, nullptr, nullptr },
	{ "Video.ToggleStandardNTSCPAL",   CmdToggleNTSCPAL,       nullptr, nullptr, nullptr },
	{ "View.NextANTICVisMode",         CmdNextANTICVisMode,     nullptr, nullptr, nullptr },
	{ "View.NextGTIAVisMode",          CmdNextGTIAVisMode,      nullptr, nullptr, nullptr },
	{ "System.TogglePause",            CmdTogglePause,          nullptr, nullptr, nullptr },
	{ "Input.CaptureMouse",            CmdCaptureMouse,         nullptr, nullptr, nullptr },
	{ "View.ToggleFullScreen",         CmdToggleFullScreen,     nullptr, nullptr, nullptr },
	{ "System.ToggleSlowMotion",       CmdToggleSlowMotion,     nullptr, nullptr, nullptr },
	{ "Audio.ToggleChannel1",          CmdToggleChannel1,       nullptr, nullptr, nullptr },
	{ "Audio.ToggleChannel2",          CmdToggleChannel2,       nullptr, nullptr, nullptr },
	{ "Audio.ToggleChannel3",          CmdToggleChannel3,       nullptr, nullptr, nullptr },
	{ "Audio.ToggleChannel4",          CmdToggleChannel4,       nullptr, nullptr, nullptr },
	{ "Edit.PasteText",                CmdPasteText,            nullptr, nullptr, nullptr },
	{ "Edit.SaveFrame",                CmdSaveFrame,            nullptr, nullptr, nullptr },
	{ "Edit.CopyText",                 CmdCopyText,             nullptr, nullptr, nullptr },
	{ "Edit.CopyFrame",                CmdCopyFrame,            nullptr, nullptr, nullptr },
	{ "Edit.SelectAll",                CmdSelectAll,            nullptr, nullptr, nullptr },
	{ "Edit.Deselect",                 CmdDeselect,             nullptr, nullptr, nullptr },

	// Global context
	{ "File.BootImage",                CmdBootImage,            nullptr, nullptr, nullptr },
	{ "File.OpenImage",                CmdOpenImage,            nullptr, nullptr, nullptr },
	{ "Debug.OpenSourceFile",          CmdOpenSourceFile,       nullptr, nullptr, nullptr },
	{ "Disk.DrivesDialog",             CmdDrivesDialog,         nullptr, nullptr, nullptr },
	{ "System.Configure",              CmdConfigure,            nullptr, nullptr, nullptr },
	{ "Cheat.CheatDialog",             CmdCheatDialog,          nullptr, nullptr, nullptr },
	{ "Debug.RunStop",                 CmdDebugRunStop,         nullptr, nullptr, nullptr },
	{ "Debug.StepInto",                CmdDebugStepInto,        nullptr, nullptr, nullptr },
	{ "Debug.StepOver",                CmdDebugStepOver,        nullptr, nullptr, nullptr },
	{ "Debug.StepOut",                 CmdDebugStepOut,         nullptr, nullptr, nullptr },
	{ "Debug.Break",                   CmdDebugBreak,           nullptr, nullptr, nullptr },
	{ "Pane.Display",                  CmdPaneDisplay,          TestDebuggerOpen, nullptr, nullptr },
	{ "Pane.Console",                  CmdPaneConsole,          TestDebuggerOpen, nullptr, nullptr },
	{ "Pane.Registers",                CmdPaneRegisters,        TestDebuggerOpen, nullptr, nullptr },
	{ "Pane.Disassembly",              CmdPaneDisassembly,      TestDebuggerOpen, nullptr, nullptr },
	{ "Pane.CallStack",                CmdPaneCallStack,        TestDebuggerOpen, nullptr, nullptr },
	{ "Pane.History",                  CmdPaneHistory,          TestDebuggerOpen, nullptr, nullptr },
	{ "Pane.Memory1",                  CmdPaneMemory1,          TestDebuggerOpen, nullptr, nullptr },
	{ "Pane.PrinterOutput",            CmdPanePrinterOutput,    TestDebuggerOpen, nullptr, nullptr },
	{ "Pane.ProfileView",              CmdPaneProfileView,      TestDebuggerOpen, nullptr, nullptr },

	// Debugger context
	{ "Debug.Run",                     CmdDebugRun,             nullptr, nullptr, nullptr },
	{ "Debug.ToggleBreakpoint",        CmdDebugToggleBreakpoint, nullptr, nullptr, nullptr },
	{ "Debug.NewBreakpoint",           CmdDebugNewBreakpoint,   nullptr, nullptr, nullptr },
};

void ATUIInitSDL3Commands() {
	g_ATUICommandMgr.RegisterCommands(kSDL3Commands, vdcountof(kSDL3Commands));
}
