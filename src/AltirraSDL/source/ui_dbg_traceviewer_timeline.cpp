//	AltirraSDL - Dear ImGui Performance Analyzer (Trace Viewer)
//	Timeline visualization: channel labels, time ruler, event rendering,
//	zoom/pan, selection overlay.

#include <stdafx.h>
#include <cmath>
#include <imgui.h>
#include <imgui_internal.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include <vd2/system/text.h>
#include "ui_dbg_traceviewer.h"
#include "trace.h"

// =========================================================================
// Helpers
// =========================================================================

namespace {

inline ImU32 TraceColorToImCol32(uint32 c) {
	return IM_COL32((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

inline double PixelToTime(float px, const ATImGuiTraceViewerContext& ctx) {
	return ctx.mStartTime + (double)px * ctx.mSecondsPerPixel;
}

inline float TimeToPixel(double t, const ATImGuiTraceViewerContext& ctx) {
	return (float)((t - ctx.mStartTime) / ctx.mSecondsPerPixel);
}

void ComputeTimescaleDivisions(sint32 zoomLevel, double& outSecondsPerDiv, int& outDecimals) {
	// Match the Windows formula exactly:
	// zl2 = (zoomLevel < 0 ? zoomLevel - 4 : zoomLevel) / 5
	// secondsPerDivision = 10^(zl2 + 2)
	int zl2 = (zoomLevel < 0 ? zoomLevel - 4 : zoomLevel) / 5;
	outSecondsPerDiv = pow(10.0, zl2 + 2);

	// Compute decimal places for labels (matches Windows)
	outDecimals = 0;
	if (outSecondsPerDiv < 0.000002)
		outDecimals = 6;
	else if (outSecondsPerDiv < 0.00002)
		outDecimals = 5;
	else if (outSecondsPerDiv < 0.0002)
		outDecimals = 4;
	else if (outSecondsPerDiv < 0.002)
		outDecimals = 3;
	else if (outSecondsPerDiv < 0.02)
		outDecimals = 2;
	else if (outSecondsPerDiv < 0.2)
		outDecimals = 1;
}

} // anonymous namespace

// =========================================================================
// Timescale ruler
// =========================================================================

static void RenderTimescale(ImDrawList *drawList, ImVec2 origin, float width, float height, const ATImGuiTraceViewerContext& ctx) {
	double secsPerDiv = 0;
	int decimals = 0;
	ComputeTimescaleDivisions(ctx.mZoomLevel, secsPerDiv, decimals);

	ImU32 lineColor = IM_COL32(180, 180, 180, 255);
	ImU32 textColor = IM_COL32(220, 220, 220, 255);

	double div1f = ctx.mStartTime / secsPerDiv;
	double div2f = (ctx.mStartTime + (double)width * ctx.mSecondsPerPixel) / secsPerDiv;
	sint64 div1 = (sint64)ceil(div1f) - 1;
	sint64 div2 = (sint64)ceil(div2f) + 1;

	float majorTickH = height * 0.6f;
	float minorTickH = height * 0.3f;

	char label[64];

	for (sint64 div = div1; div <= div2; ++div) {
		float x = origin.x + TimeToPixel((double)div * secsPerDiv, ctx);
		if (x < origin.x - 50 || x > origin.x + width + 50)
			continue;

		bool major = (div % 5) == 0;
		float tickH = major ? majorTickH : minorTickH;

		drawList->AddLine(
			ImVec2(x, origin.y + height - tickH),
			ImVec2(x, origin.y + height),
			lineColor);

		if (major) {
			snprintf(label, sizeof(label), "%.*f", decimals, (double)div * secsPerDiv);
			ImVec2 textSize = ImGui::CalcTextSize(label);
			drawList->AddText(ImVec2(x - textSize.x * 0.5f, origin.y + 1), textColor, label);
		}
	}

	// Bottom border line
	drawList->AddLine(
		ImVec2(origin.x, origin.y + height - 1),
		ImVec2(origin.x + width, origin.y + height - 1),
		lineColor);
}

// =========================================================================
// Event area rendering
// =========================================================================

static void RenderEvents(ImDrawList *drawList, ImVec2 origin, float width, float eventAreaHeight,
	ATImGuiTraceViewerContext& ctx, float channelHeight, float groupHeaderHeight)
{
	double startTime = ctx.mStartTime;
	double endTime = ctx.mStartTime + (double)width * ctx.mSecondsPerPixel;
	double eventThreshold = ctx.mSecondsPerPixel * 2.0;

	// VBlank shading from frame channel
	if (ctx.mpFrameChannel) {
		ctx.mpFrameChannel->StartIteration(startTime, endTime, 0);
		ATTraceEvent fev;
		while (ctx.mpFrameChannel->GetNextEvent(fev)) {
			float x1 = origin.x + TimeToPixel(fev.mEventStart, ctx);
			float x2 = origin.x + TimeToPixel(fev.mEventStop, ctx);
			x1 = std::max(x1, origin.x);
			x2 = std::min(x2, origin.x + width);
			if (x2 > x1) {
				drawList->AddRectFilled(
					ImVec2(x1, origin.y),
					ImVec2(x2, origin.y + eventAreaHeight),
					IM_COL32(40, 40, 60, 80));
			}
		}
	}

	// Draw events for each channel
	float y = origin.y;
	for (const auto& group : ctx.mGroups) {
		y += groupHeaderHeight;

		for (const auto& ch : group.mChannels) {
			IATTraceChannel *channel = ch.mpChannel;
			if (!channel) {
				y += channelHeight;
				continue;
			}

			channel->StartIteration(startTime, endTime, eventThreshold);
			ATTraceEvent ev;
			while (channel->GetNextEvent(ev)) {
				float x1 = origin.x + TimeToPixel(ev.mEventStart, ctx);
				float x2 = origin.x + TimeToPixel(ev.mEventStop, ctx);

				// Clamp to visible area
				x1 = std::max(x1, origin.x);
				x2 = std::min(x2, origin.x + width);

				if (x2 <= x1)
					continue;

				// Ensure minimum 1-pixel width for visibility
				if (x2 - x1 < 1.0f)
					x2 = x1 + 1.0f;

				ImU32 bgCol = TraceColorToImCol32(ev.mBgColor);
				drawList->AddRectFilled(ImVec2(x1, y + 1), ImVec2(x2, y + channelHeight - 1), bgCol);

				// Draw label if rectangle is wide enough
				if (x2 - x1 > 40.0f && ev.mpName) {
					VDStringA name8 = VDTextWToU8(VDStringSpanW(ev.mpName));
					ImVec2 textSize = ImGui::CalcTextSize(name8.c_str());
					float textX = x1 + 2.0f;
					float textY = y + (channelHeight - textSize.y) * 0.5f;

					// Clip text to event rectangle
					drawList->PushClipRect(ImVec2(x1, y), ImVec2(x2, y + channelHeight), true);
					drawList->AddText(ImVec2(textX, textY), IM_COL32(0, 0, 0, 255), name8.c_str());
					drawList->PopClipRect();
				}
			}

			// Channel separator
			drawList->AddLine(
				ImVec2(origin.x, y + channelHeight),
				ImVec2(origin.x + width, y + channelHeight),
				IM_COL32(60, 60, 60, 255));

			y += channelHeight;
		}
	}

	// Selection overlay
	if (ctx.mbSelectionValid) {
		double selMin = std::min(ctx.mSelectStart, ctx.mSelectEnd);
		double selMax = std::max(ctx.mSelectStart, ctx.mSelectEnd);
		float sx1 = origin.x + TimeToPixel(selMin, ctx);
		float sx2 = origin.x + TimeToPixel(selMax, ctx);
		sx1 = std::max(sx1, origin.x);
		sx2 = std::min(sx2, origin.x + width);
		if (sx2 > sx1) {
			drawList->AddRectFilled(
				ImVec2(sx1, origin.y),
				ImVec2(sx2, origin.y + eventAreaHeight),
				IM_COL32(100, 150, 255, 60));
		}
	}

	// Focus time indicator
	if (ctx.mFocusTime >= 0) {
		float fx = origin.x + TimeToPixel(ctx.mFocusTime, ctx);
		if (fx >= origin.x && fx <= origin.x + width) {
			drawList->AddLine(
				ImVec2(fx, origin.y),
				ImVec2(fx, origin.y + eventAreaHeight),
				IM_COL32(255, 255, 0, 180), 1.5f);
		}
	}
}

// =========================================================================
// Main timeline renderer
// =========================================================================

void ATImGuiTraceViewer_RenderTimeline(ATImGuiTraceViewerContext& ctx) {
	if (!ctx.mpCollection) {
		ImGui::TextUnformatted("No trace loaded. Use Trace > Start Trace to record, or File > Load to open a trace file.");
		return;
	}

	const float channelHeight = 22.0f;
	const float groupHeaderHeight = ImGui::GetTextLineHeightWithSpacing() + 2.0f;
	const float timescaleHeight = 24.0f;
	const float labelWidth = 150.0f;
	const float splitterWidth = 4.0f;

	ImVec2 avail = ImGui::GetContentRegionAvail();
	float eventAreaWidth = avail.x - labelWidth - splitterWidth;
	if (eventAreaWidth < 50)
		eventAreaWidth = 50;

	// Compute total content height matching label rendering exactly
	float totalHeight = 0;
	for (const auto& group : ctx.mGroups) {
		totalHeight += groupHeaderHeight;
		totalHeight += channelHeight * (float)group.mChannels.size();
	}

	// Persistent vertical scroll offset (shared between labels and events)
	static float s_scrollY = 0;

	// ---- Timescale (non-scrolling, above events area) ----
	{
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		ImVec2 tsOrigin = ImVec2(cursorPos.x + labelWidth + splitterWidth, cursorPos.y);
		ImDrawList *drawList = ImGui::GetWindowDrawList();
		RenderTimescale(drawList, tsOrigin, eventAreaWidth, timescaleHeight, ctx);
		ImGui::Dummy(ImVec2(avail.x, timescaleHeight));
	}

	// ---- Main area: labels left, events right ----
	float viewHeight = avail.y - timescaleHeight - 2;
	if (viewHeight < 50)
		viewHeight = 50;

	ImGui::BeginChild("##TVMain", ImVec2(0, viewHeight), ImGuiChildFlags_None);
	{
		ImVec2 mainOrigin = ImGui::GetCursorScreenPos();

		// --- Channel labels (left column) ---
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 0));
		ImGui::BeginChild("##TVLabels", ImVec2(labelWidth, 0), ImGuiChildFlags_None);
		{
			ImGui::SetScrollY(s_scrollY);

			ImU32 groupColor = IM_COL32(200, 200, 255, 255);
			ImU32 channelColor = IM_COL32(200, 200, 200, 255);

			for (const auto& group : ctx.mGroups) {
				VDStringA groupName8 = VDTextWToU8(group.mName);
				float startY = ImGui::GetCursorPosY();
				ImGui::PushStyleColor(ImGuiCol_Text, groupColor);
				ImGui::SetCursorPosY(startY + (groupHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f);
				ImGui::TextUnformatted(groupName8.c_str());
				ImGui::PopStyleColor();
				ImGui::SetCursorPosY(startY + groupHeaderHeight);

				for (const auto& ch : group.mChannels) {
					VDStringA chName8 = VDTextWToU8(ch.mName);
					float curY = ImGui::GetCursorPosY();
					ImGui::Indent(10.0f);
					ImGui::PushStyleColor(ImGuiCol_Text, channelColor);
					ImGui::SetCursorPosY(curY + (channelHeight - ImGui::GetTextLineHeight()) * 0.5f);
					ImGui::TextUnformatted(chName8.c_str());
					ImGui::PopStyleColor();
					ImGui::Unindent(10.0f);
					ImGui::SetCursorPosY(curY + channelHeight);
				}
			}

			// Ensure content height matches totalHeight exactly
			float curY = ImGui::GetCursorPosY();
			if (totalHeight > curY)
				ImGui::Dummy(ImVec2(0, totalHeight - curY));
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();

		ImGui::SameLine(0, splitterWidth);

		// --- Events area (right column, custom drawing) ---
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::BeginChild("##TVEvents", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
		{
			ImGui::SetScrollY(s_scrollY);

			// Reserve content height for scroll range
			ImGui::Dummy(ImVec2(eventAreaWidth, totalHeight));

			ImVec2 eventOrigin = ImGui::GetCursorScreenPos();
			eventOrigin.y -= totalHeight;  // Dummy advanced cursor; back up

			ImDrawList *drawList = ImGui::GetWindowDrawList();

			// Clip to visible event area
			ImVec2 clipMin = ImVec2(eventOrigin.x, mainOrigin.y);
			ImVec2 clipMax = ImVec2(eventOrigin.x + eventAreaWidth, mainOrigin.y + viewHeight);
			drawList->PushClipRect(clipMin, clipMax, true);

			ImVec2 scrolledOrigin = ImVec2(eventOrigin.x, eventOrigin.y - s_scrollY);
			RenderEvents(drawList, scrolledOrigin, eventAreaWidth, totalHeight, ctx, channelHeight, groupHeaderHeight);

			drawList->PopClipRect();

			// --- Mouse interaction ---
			ImVec2 mousePos = ImGui::GetMousePos();
			bool hovered = (mousePos.x >= eventOrigin.x && mousePos.x < eventOrigin.x + eventAreaWidth &&
				mousePos.y >= mainOrigin.y && mousePos.y < mainOrigin.y + viewHeight);

			if (hovered) {
				ImGui::SetMouseCursor(ctx.mbSelectionMode ? ImGuiMouseCursor_TextInput : ImGuiMouseCursor_Arrow);

				float wheel = ImGui::GetIO().MouseWheel;
				if (wheel != 0) {
					if (ImGui::GetIO().KeyCtrl) {
						// Ctrl+Wheel = zoom centered on mouse
						double mouseTime = PixelToTime(mousePos.x - eventOrigin.x, ctx);
						ctx.ZoomDeltaSteps(mouseTime, (sint32)(wheel * 2), eventAreaWidth);
					} else {
						// Wheel without Ctrl = vertical scroll
						s_scrollY -= wheel * channelHeight * 3;
						s_scrollY = std::max(0.0f, std::min(s_scrollY, totalHeight - viewHeight));
					}
				}

				// Pan: middle-drag always, left-drag in move mode
				bool panButton = ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
				if (!ctx.mbSelectionMode && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
					panButton = true;

				if (panButton) {
					ImVec2 delta = ImGui::GetIO().MouseDelta;
					ctx.mStartTime -= (double)delta.x * ctx.mSecondsPerPixel;
					// Vertical pan via middle drag
					if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
						s_scrollY -= delta.y;
						s_scrollY = std::max(0.0f, std::min(s_scrollY, std::max(0.0f, totalHeight - viewHeight)));
					}
				}

				// Right-drag pans (Windows behavior)
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
					ImVec2 delta = ImGui::GetIO().MouseDelta;
					ctx.mStartTime -= (double)delta.x * ctx.mSecondsPerPixel;
				}

				// Selection (left-drag in selection mode)
				if (ctx.mbSelectionMode) {
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
						ctx.mSelectStart = PixelToTime(mousePos.x - eventOrigin.x, ctx);
						ctx.mSelectEnd = ctx.mSelectStart;
						ctx.mbSelectionValid = true;
					} else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ctx.mbSelectionValid) {
						ctx.mSelectEnd = PixelToTime(mousePos.x - eventOrigin.x, ctx);
					}
				}

				// Click sets focus time (move mode)
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ctx.mbSelectionMode) {
					ctx.mFocusTime = PixelToTime(mousePos.x - eventOrigin.x, ctx);
					ctx.mbFocusTimeChanged = true;
				}

				// Tooltip: find the event under mouse
				double mouseTime = PixelToTime(mousePos.x - eventOrigin.x, ctx);
				float mouseRelY = mousePos.y - scrolledOrigin.y;
				float checkY = 0;
				for (const auto& group : ctx.mGroups) {
					checkY += groupHeaderHeight;
					for (const auto& ch : group.mChannels) {
						if (mouseRelY >= checkY && mouseRelY < checkY + channelHeight && ch.mpChannel) {
							ch.mpChannel->StartIteration(mouseTime - ctx.mSecondsPerPixel * 2, mouseTime + ctx.mSecondsPerPixel * 2, 0);
							ATTraceEvent ev;
							while (ch.mpChannel->GetNextEvent(ev)) {
								if (mouseTime >= ev.mEventStart && mouseTime <= ev.mEventStop && ev.mpName) {
									VDStringA tip = VDTextWToU8(VDStringSpanW(ev.mpName));
									ImGui::SetTooltip("%s\n%.6fs - %.6fs", tip.c_str(), ev.mEventStart, ev.mEventStop);
									goto tooltip_done;
								}
							}
						}
						checkY += channelHeight;
					}
				}
				tooltip_done:;
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();

		// Sync scroll from labels child back (if user scrolled via labels scrollbar)
		// Read the labels child scroll after both children are done
	}
	ImGui::EndChild();
}
