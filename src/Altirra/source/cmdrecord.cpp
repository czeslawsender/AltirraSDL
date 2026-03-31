//	Altirra - Atari 800/800XL/5200 emulator
//	Copyright (C) 2026 Avery Lee
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
#include <vd2/Dita/services.h>
#include "cmdhelpers.h"
#include "sapconverter.h"
#include "simulator.h"
#include "uiaccessors.h"
#include "uicommondialogs.h"
#include "uifilefilters.h"
#include "uifrontend.h"
#include "uiqueue.h"
#include "videowriter.h"

extern ATSimulator g_sim;

std::shared_ptr<ATVideoRecordingSettings> ATUIShowDialogVideoEncoding(VDGUIHandle parent, bool hz50);

////////////////////////////////////////////////////////////////////////////////

ATUIRecordingStatus ATUIGetRecordingStatus() {
	return ATUIGetFrontEnd().GetRecordingStatus();
}

void OnCommandRecordPause() {
	ATUIGetFrontEnd().PauseRecording();
}

void OnCommandRecordResume() {
	ATUIGetFrontEnd().ResumeRecording();
}

void OnCommandRecordPauseResume() {
	ATUIGetFrontEnd().TogglePauseResumeRecording();
}

void OnCommandRecordStop() {
	ATUIGetFrontEnd().CheckRecordingExceptions();
	ATUIGetFrontEnd().StopRecording();
}

void OnCommandRecordRawAudio(ATUICommandContext& ctx) {
	if (!ATUIGetFrontEnd().IsRecording()) {
		VDStringW s;
		
		if (!ctx.GetArg(0, s))
			s = VDGetSaveFileName('raud', ATUIGetNewPopupOwner(), L"Record raw audio", L"Raw 32-bit float data\0*.pcm\0", L"pcm");

		if (!s.empty()) {
			ctx.SetArg(0, s.c_str());
			ctx.MarkCompleted(true);

			ATUIGetFrontEnd().RecordRawAudio(s.c_str());
		} else
			ctx.MarkCompleted(false);
	}
}

void OnCommandRecordAudio(ATUICommandContext& ctx) {
	if (!ATUIGetFrontEnd().IsRecording()) {
		VDStringW s;
		
		if (!ctx.GetArg(0, s))
			s = VDGetSaveFileName('raud', ATUIGetNewPopupOwner(), L"Record audio", L"Wave audio (*.wav)\0*.wav\0", L"wav");

		if (!s.empty()) {
			ctx.SetArg(0, s.c_str());
			ctx.MarkCompleted(true);

			ATUIGetFrontEnd().RecordAudio(s.c_str());
		} else
			ctx.MarkCompleted(false);
	}
}

void OnCommandRecordVideo() {
	if (ATUIGetFrontEnd().IsRecording())
		return;

	const bool hz50 = g_sim.GetVideoStandard() != kATVideoStandard_NTSC && g_sim.GetVideoStandard() != kATVideoStandard_PAL60;

	const auto settings = ATUIShowDialogVideoEncoding(ATUIGetNewPopupOwner(), hz50);

	if (!settings)
		return;

	VDStringW s;
	
	switch(settings->mEncoding) {
		case kATVideoEncoding_Raw:
		case kATVideoEncoding_RLE:
		case kATVideoEncoding_ZMBV:
			s = VDGetSaveFileName('rvid', ATUIGetNewPopupOwner(), L"Record raw video", L"Audio/Visual Interleaved (*.avi)\0*.avi\0", L"avi");
			break;

		case kATVideoEncoding_WMV7:
		case kATVideoEncoding_WMV9:
			s = VDGetSaveFileName('rvid', ATUIGetNewPopupOwner(), L"Record raw video", L"Windows Media Video (*.wmv)\0*.wmv\0", L"wmv");
			break;

		case kATVideoEncoding_H264_AAC:
		case kATVideoEncoding_H264_MP3:
			s = VDGetSaveFileName('rvid', ATUIGetNewPopupOwner(), L"Record raw video", L"MPEG-4/AVC (*.mp4)\0*.mp4\0", L"mp4");
			break;
	}

	if (s.empty())
		return;

	ATUIGetFrontEnd().RecordVideo(s.c_str(), *settings);
}

void OnCommandRecordVgm(ATUICommandContext& ctx) {
	if (!ATUIGetFrontEnd().IsRecording()) {
		VDStringW s;
		
		if (!ctx.GetArg(0, s))
			s = VDGetSaveFileName("VGMAudio"_vdtypeid, ATUIGetNewPopupOwner(), L"Record VGM audio file", L"VGM POKEY recording (*.vgm)\0*.vgm\0", L"vgm");

		if (!s.empty()) {
			ctx.SetArg(0, s.c_str());
			ctx.MarkCompleted(true);

			ATUIGetFrontEnd().RecordVgm(s.c_str());
		} else
			ctx.MarkCompleted(false);
	}
}

void OnCommandRecordSapTypeR(ATUICommandContext& ctx) {
	if (!ATUIGetFrontEnd().IsRecording()) {
		VDStringW s;
		
		if (!ctx.GetArg(0, s))
			s = VDGetSaveFileName('rsap', ATUIGetNewPopupOwner(), L"Record SAP type R music file", L"SAP Type R\0*.sap\0", L"sap");

		if (!s.empty()) {
			ctx.SetArg(0, s.c_str());
			ctx.MarkCompleted(true);

			ATUIGetFrontEnd().RecordSap(s.c_str());
		} else
			ctx.MarkCompleted(false);
	}
}

class ATUIFutureConvertSapToExe : public ATUIFuture {
public:
	virtual void RunInner() {
		switch(mStage) {
			case 0:
				mpSrcFileDialogResult = ATUIShowOpenFileDialog('lsap', L"Select source SAP file",
					g_ATUIFileFilter_LoadSAP);

				Wait(mpSrcFileDialogResult);
				++mStage;
				break;

			case 1:
				if (!mpSrcFileDialogResult->mbAccepted) {
					MarkCompleted();
					break;
				}

				mpDstFileDialogResult = ATUIShowSaveFileDialog('sxex', L"Select output executable name",
					g_ATUIFileFilter_SaveXEX);

				Wait(mpDstFileDialogResult);
				++mStage;
				break;

			case 2:
				if (!mpDstFileDialogResult->mbAccepted) {
					MarkCompleted();
					break;
				}

				ATConvertSAPToPlayer(mpDstFileDialogResult->mPath.c_str(), mpSrcFileDialogResult->mPath.c_str());

				MarkCompleted();
				break;
		}
	}

	vdrefptr<ATUIFileDialogResult> mpSrcFileDialogResult;
	vdrefptr<ATUIFileDialogResult> mpDstFileDialogResult;
};

void OnCommandToolsConvertSapToExe() {
	vdrefptr<ATUIFutureConvertSapToExe> stage(new ATUIFutureConvertSapToExe);

	ATUIPushStep(stage->GetStep());
}

namespace {
	bool IsRecording() {
		return ATUIGetRecordingStatus() != kATUIRecordingStatus_None;
	}

	template<ATUIRecordingStatus T_Status>
	bool RecordingStatusIs() {
		return ATUIGetRecordingStatus() == T_Status;
	}
}

void ATUIInitCommandMappingsRecord(ATUICommandManager& cmdMgr) {
	using namespace ATCommands;

	static constexpr struct ATUICommand kCommands[]={
		{ "Record.Stop", OnCommandRecordStop, IsRecording },
		{ "Record.RawAudio", OnCommandRecordRawAudio, Not<IsRecording>, CheckedIf<RecordingStatusIs<kATUIRecordingStatus_RawAudio>> },
		{ "Record.Audio", OnCommandRecordAudio, Not<IsRecording>, CheckedIf<RecordingStatusIs<kATUIRecordingStatus_Audio>> },
		{ "Record.Video", OnCommandRecordVideo, Not<IsRecording>, CheckedIf<RecordingStatusIs<kATUIRecordingStatus_Video>> },
		{ "Record.Pause", OnCommandRecordPause, RecordingStatusIs<kATUIRecordingStatus_Video> },
		{ "Record.Resume", OnCommandRecordResume, RecordingStatusIs<kATUIRecordingStatus_Video> },
		{ "Record.PauseResume", OnCommandRecordPauseResume, RecordingStatusIs<kATUIRecordingStatus_Video> },
		{ "Record.SAPTypeR", OnCommandRecordSapTypeR, Not<IsRecording>, CheckedIf<RecordingStatusIs<kATUIRecordingStatus_Sap>> },
		{ "Record.VGM", OnCommandRecordVgm, Not<IsRecording>, CheckedIf<RecordingStatusIs<kATUIRecordingStatus_Vgm>> },

	};

	cmdMgr.RegisterCommands(kCommands, vdcountof(kCommands));
}
