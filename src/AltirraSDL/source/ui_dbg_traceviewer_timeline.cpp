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
#include "tracetape.h"
#include "ui_main.h"

ATTapeEditorRequest g_tapeEditorRequest;

// Context menu state for tape channel right-click
static uint32 s_tapeContextSample = 0;
static float s_tapeContextPixelsPerSample = 0;

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

	// Frame channel: alternating shading + vertical boundary lines
	if (ctx.mpFrameChannel) {
		ctx.mpFrameChannel->StartIteration(startTime, endTime, 0);
		ATTraceEvent fev;
		bool alternate = false;
		while (ctx.mpFrameChannel->GetNextEvent(fev)) {
			float x1 = origin.x + TimeToPixel(fev.mEventStart, ctx);
			float x2 = origin.x + TimeToPixel(fev.mEventStop, ctx);

			// Vertical frame boundary line
			if (x1 >= origin.x && x1 <= origin.x + width) {
				drawList->AddLine(
					ImVec2(x1, origin.y),
					ImVec2(x1, origin.y + eventAreaHeight),
					IM_COL32(80, 80, 120, 120));
			}

			// Alternating VBLANK shading
			x1 = std::max(x1, origin.x);
			x2 = std::min(x2, origin.x + width);
			if (x2 > x1 && alternate) {
				drawList->AddRectFilled(
					ImVec2(x1, origin.y),
					ImVec2(x2, origin.y + eventAreaHeight),
					IM_COL32(40, 40, 60, 50));
			}
			alternate = !alternate;
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

			if (ch.mType == ATImGuiTraceViewerContext::kChannelType_Log) {
				// Log channels: draw colored dots (circles), matching Windows Ellipse rendering
				float dotRadius = (channelHeight - 4.0f) * 0.5f;
				float dotCenterY = y + channelHeight * 0.5f;

				channel->StartIteration(startTime - ctx.mSecondsPerPixel * dotRadius, endTime + ctx.mSecondsPerPixel * dotRadius, 0);
				ATTraceEvent ev;
				sint32 lastDotX = -9999;
				while (channel->GetNextEvent(ev)) {
					float cx = origin.x + TimeToPixel(ev.mEventStart, ctx);
					sint32 dotX = (sint32)cx;
					if (dotX == lastDotX)
						continue;  // deduplicate at same pixel
					lastDotX = dotX;

					if (cx >= origin.x - dotRadius && cx <= origin.x + width + dotRadius) {
						ImU32 bgCol = TraceColorToImCol32(ev.mBgColor);
						drawList->AddCircleFilled(ImVec2(cx, dotCenterY), dotRadius, bgCol);
						drawList->AddCircle(ImVec2(cx, dotCenterY), dotRadius, IM_COL32(0, 0, 0, 180));
					}
				}
			} else {
				// Default and other channels: colored rectangles with labels
				channel->StartIteration(startTime, endTime, eventThreshold);
				ATTraceEvent ev;
				while (channel->GetNextEvent(ev)) {
					float x1 = origin.x + TimeToPixel(ev.mEventStart, ctx);
					float x2 = origin.x + TimeToPixel(ev.mEventStop, ctx);

					x1 = std::max(x1, origin.x);
					x2 = std::min(x2, origin.x + width);

					if (x2 <= x1)
						continue;

					if (x2 - x1 < 1.0f)
						x2 = x1 + 1.0f;

					ImU32 bgCol = TraceColorToImCol32(ev.mBgColor);
					drawList->AddRectFilled(ImVec2(x1, y + 1), ImVec2(x2, y + channelHeight - 1), bgCol);

					if (x2 - x1 > 40.0f && ev.mpName) {
						VDStringA name8 = VDTextWToU8(VDStringSpanW(ev.mpName));
						ImVec2 textSize = ImGui::CalcTextSize(name8.c_str());
						float textX = x1 + 2.0f;
						float textY = y + (channelHeight - textSize.y) * 0.5f;

						drawList->PushClipRect(ImVec2(x1, y), ImVec2(x2, y + channelHeight), true);
						drawList->AddText(ImVec2(textX, textY), IM_COL32(0, 0, 0, 255), name8.c_str());
						drawList->PopClipRect();
					}
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
	const float splitterWidth = 5.0f;

	// Use context label width (draggable splitter)
	float& labelWidth = ctx.mLabelWidth;

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

	// Vertical scroll offset (stored in context, reset on collection change)
	float& scrollY = ctx.mScrollY;

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
		ImGui::BeginChild("##TVLabels", ImVec2(labelWidth, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
		{
			ImGui::SetScrollY(scrollY);

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

		// --- Draggable splitter between labels and events ---
		ImGui::SameLine(0, 0);
		{
			ImVec2 splitterPos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##TVSplitter", ImVec2(splitterWidth, viewHeight));
			if (ImGui::IsItemHovered())
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
				labelWidth += ImGui::GetIO().MouseDelta.x;
				labelWidth = std::max(50.0f, std::min(labelWidth, avail.x * 0.5f));
			}
			// Draw splitter line
			ImDrawList *dl = ImGui::GetWindowDrawList();
			float splMidX = splitterPos.x + splitterWidth * 0.5f;
			dl->AddLine(ImVec2(splMidX, splitterPos.y), ImVec2(splMidX, splitterPos.y + viewHeight), IM_COL32(80, 80, 80, 200));
		}

		ImGui::SameLine(0, 0);

		// --- Events area (right column, custom drawing) ---
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::BeginChild("##TVEvents", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
		{
			ImGui::SetScrollY(scrollY);

			// Reserve content height for scroll range
			ImGui::Dummy(ImVec2(eventAreaWidth, totalHeight));

			ImVec2 eventOrigin = ImGui::GetCursorScreenPos();
			eventOrigin.y -= totalHeight;  // Dummy advanced cursor; back up

			// Place an InvisibleButton over the visible area to consume mouse
			// input and prevent drag events from propagating to the parent
			// window (which would cause the entire pane to move).
			ImGui::SetCursorScreenPos(ImVec2(eventOrigin.x, mainOrigin.y));
			ImGui::InvisibleButton("##TVEventsInput", ImVec2(eventAreaWidth, viewHeight));
			bool eventsHovered = ImGui::IsItemHovered();
			bool eventsActive = ImGui::IsItemActive();

			ImDrawList *drawList = ImGui::GetWindowDrawList();

			// Clip to visible event area
			ImVec2 clipMin = ImVec2(eventOrigin.x, mainOrigin.y);
			ImVec2 clipMax = ImVec2(eventOrigin.x + eventAreaWidth, mainOrigin.y + viewHeight);
			drawList->PushClipRect(clipMin, clipMax, true);

			// eventOrigin already incorporates the child window scroll
			// (SetScrollY shifts GetCursorScreenPos), so use it directly.
			ImVec2 scrolledOrigin = ImVec2(eventOrigin.x, eventOrigin.y);
			RenderEvents(drawList, scrolledOrigin, eventAreaWidth, totalHeight, ctx, channelHeight, groupHeaderHeight);

			drawList->PopClipRect();

			// --- Mouse interaction ---
			ImVec2 mousePos = ImGui::GetMousePos();
			bool hovered = eventsHovered || eventsActive;

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
						scrollY -= wheel * channelHeight * 3;
						scrollY = std::max(0.0f, std::min(scrollY, totalHeight - viewHeight));
					}
				}

				// Pan: middle-drag always, left-drag in move mode
				bool panButton = ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
				if (!ctx.mbSelectionMode && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
					panButton = true;

				if (panButton) {
					ImVec2 delta = ImGui::GetIO().MouseDelta;
					ctx.mStartTime -= (double)delta.x * ctx.mSecondsPerPixel;
					if (ctx.mStartTime < 0) ctx.mStartTime = 0;
					if (ctx.mTraceDuration > 0 && ctx.mStartTime > ctx.mTraceDuration)
						ctx.mStartTime = ctx.mTraceDuration;
					// Vertical pan via middle drag
					if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
						scrollY -= delta.y;
						scrollY = std::max(0.0f, std::min(scrollY, std::max(0.0f, totalHeight - viewHeight)));
					}
				}

				// Right-drag pans (Windows behavior)
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
					ImVec2 delta = ImGui::GetIO().MouseDelta;
					ctx.mStartTime -= (double)delta.x * ctx.mSecondsPerPixel;
					if (ctx.mStartTime < 0) ctx.mStartTime = 0;
					if (ctx.mTraceDuration > 0 && ctx.mStartTime > ctx.mTraceDuration)
						ctx.mStartTime = ctx.mTraceDuration;
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
							ch.mpChannel->StartIteration(mouseTime - ctx.mSecondsPerPixel * 4, mouseTime + ctx.mSecondsPerPixel * 4, 0);
							ATTraceEvent ev;
							while (ch.mpChannel->GetNextEvent(ev)) {
								// For point events (log entries), use a pixel-based
								// proximity check instead of strict time range.
								double tolerance = ctx.mSecondsPerPixel * 4;
								bool hit = (ev.mEventStart == ev.mEventStop || ev.mEventStop - ev.mEventStart < tolerance)
									? (fabs(mouseTime - ev.mEventStart) < tolerance)
									: (mouseTime >= ev.mEventStart && mouseTime <= ev.mEventStop);
								if (hit && ev.mpName) {
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

				// Right-click context menu on tape channels.
				// Use IsMouseReleased (not IsMouseClicked) so right-drag panning
				// still works — matching Windows WM_CONTEXTMENU which fires on
				// button release.  GetMouseDragDelta checks that the user didn't
				// drag before releasing.
				ImVec2 rDragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
				bool rightClicked = ImGui::IsMouseReleased(ImGuiMouseButton_Right)
					&& (fabsf(rDragDelta.x) < 3.0f && fabsf(rDragDelta.y) < 3.0f);
				if (rightClicked) {
					float rcCheckY = 0;
					for (const auto& group : ctx.mGroups) {
						rcCheckY += groupHeaderHeight;
						for (const auto& ch : group.mChannels) {
							if (mouseRelY >= rcCheckY && mouseRelY < rcCheckY + channelHeight
								&& ch.mType == ATImGuiTraceViewerContext::kChannelType_Tape
								&& ch.mpChannel) {
								// Compute tape sample position under mouse (matching Windows OnContextMenuTape)
								ATTraceChannelTape *tapeChannel = static_cast<ATTraceChannelTape *>(&*ch.mpChannel);
								tapeChannel->StartIteration(mouseTime, tapeChannel->GetDuration(), 0);
								ATTraceEvent tev;
								if (tapeChannel->GetNextEvent(tev)) {
									const auto& tapeEvent = tapeChannel->GetLastEvent();
									const double samplesPerSec = tapeChannel->GetSamplesPerSec();
									uint32 pos = VDClampToUint32((sint64)(0.5 + (mouseTime - tev.mEventStart) * samplesPerSec) + (sint64)tapeEvent.mPosition);
									float pixelsPerTapeSample = (float)(1.0 / (ctx.mSecondsPerPixel * samplesPerSec));

									// Store for popup handler (file-scope statics below)
									s_tapeContextSample = pos;
									s_tapeContextPixelsPerSample = pixelsPerTapeSample;

									ImGui::OpenPopup("##TapeContextMenu");
								}
							}
							rcCheckY += channelHeight;
						}
					}
				}
			}

			// Tape channel context popup (must be outside hovered block for ImGui popup handling)
			if (ImGui::BeginPopup("##TapeContextMenu")) {
				if (ImGui::MenuItem("Go To Tape Editor...")) {
					g_tapeEditorRequest.pending = true;
					g_tapeEditorRequest.hasLocation = true;
					g_tapeEditorRequest.sample = s_tapeContextSample;
					g_tapeEditorRequest.pixelsPerSample = s_tapeContextPixelsPerSample;
				}
				ImGui::EndPopup();
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();

	}
	ImGui::EndChild();

	// --- Horizontal scrollbar for timeline panning ---
	if (ctx.mTraceDuration > 0) {
		double viewWidthTime = ctx.mSecondsPerPixel * (double)eventAreaWidth;
		if (viewWidthTime < ctx.mTraceDuration) {
			float scrollFrac = (float)(ctx.mStartTime / (ctx.mTraceDuration - viewWidthTime));
			scrollFrac = std::max(0.0f, std::min(1.0f, scrollFrac));

			float scrollBarWidth = avail.x;
			float thumbWidth = std::max(20.0f, scrollBarWidth * (float)(viewWidthTime / ctx.mTraceDuration));
			float scrollRange = scrollBarWidth - thumbWidth;

			ImVec2 barPos = ImGui::GetCursorScreenPos();
			float barHeight = 12.0f;
			ImGui::InvisibleButton("##TVHScroll", ImVec2(scrollBarWidth, barHeight));

			ImDrawList *dl = ImGui::GetWindowDrawList();
			// Track background
			dl->AddRectFilled(barPos, ImVec2(barPos.x + scrollBarWidth, barPos.y + barHeight), IM_COL32(30, 30, 30, 255));
			// Thumb
			float thumbX = barPos.x + scrollFrac * scrollRange;
			dl->AddRectFilled(ImVec2(thumbX, barPos.y + 1), ImVec2(thumbX + thumbWidth, barPos.y + barHeight - 1), IM_COL32(100, 100, 120, 255));

			if (scrollRange > 1.0f) {
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					float newFrac = (ImGui::GetMousePos().x - barPos.x - thumbWidth * 0.5f) / scrollRange;
					newFrac = std::max(0.0f, std::min(1.0f, newFrac));
					ctx.mStartTime = newFrac * (ctx.mTraceDuration - viewWidthTime);
				} else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					float newFrac = (ImGui::GetMousePos().x - barPos.x - thumbWidth * 0.5f) / scrollRange;
					newFrac = std::max(0.0f, std::min(1.0f, newFrac));
					ctx.mStartTime = newFrac * (ctx.mTraceDuration - viewWidthTime);
				}
			}
		}
	}
}
