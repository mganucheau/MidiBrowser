#include "ArrangementView.h"
#include "PluginProcessor.h"
#include "UndoActions.h"
#include "MidiFileData.h"
#include "CompingModel.h"
#include <algorithm>
#include <vector>

namespace pflow {

namespace {

double loopMinSeparationBeats(const PatternFlowProcessor& proc)
{
    const int gs = proc.gridSnap.load();
    const double div = (gs == (int)PatternFlowProcessor::GridSize::Off)
        ? 0.25
        : PatternFlowProcessor::getGridDivision((PatternFlowProcessor::GridSize)gs);
    return juce::jmax(1.0 / 64.0, div);
}

/** Visual grid step in the arrangement (matches beats dropdown; Off → beat lines). */
double arrangementGridStepBeats(const PatternFlowProcessor& proc)
{
    const int gs = proc.gridSnap.load();
    if (gs == (int)PatternFlowProcessor::GridSize::Off)
        return 1.0;
    return PatternFlowProcessor::getGridDivision((PatternFlowProcessor::GridSize)gs);
}

void commitTakeCompUndoIfChanged(PatternFlowProcessor& proc,
                                 const std::vector<TakeCompSegment>& before,
                                 const std::vector<TakeCompSegment>& after)
{
    if (takeCompSegmentsEquivalent(before, after)) return;
    {
        juce::ScopedLock sl(proc.laneLock);
        proc.ensureDefaultTakeComp();
        proc.takeComps[0].segments = before;
    }
    proc.rebuildCombinedClip();
    proc.undoManager.beginNewTransaction();
    proc.undoManager.perform(new SetTakeCompSegmentsAction(proc, before, after));
}

} // namespace

ArrangementView::ArrangementView(PatternFlowProcessor& proc) : processor(proc)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);

}

void ArrangementView::refresh()
{
    processor.removeEmptyLanesExceptFirst();
    processor.rebuildCombinedClip();
    rebuildClipBlocks();
    repaint();
}

void ArrangementView::zoomToFitSession()
{
    int totalBeats = processor.arrangementBars.load() * 4;
    float availableW = (float)(getWidth() - metrics::laneHeaderW);
    if (availableW <= 0) availableW = 400.0f;

    beatsPerPixel = (float)totalBeats / availableW;
    scrollBeatOffset = 0.0f;
    refresh();
}

void ArrangementView::refreshComponentColours()
{
}

double ArrangementView::xToBeat(float x) const
{
    return (double)(x - metrics::laneHeaderW) * beatsPerPixel + scrollBeatOffset;
}

float ArrangementView::beatToX(double beat) const
{
    float bpp = (beatsPerPixel > 1e-6f) ? beatsPerPixel : 0.1f;
    return (float)((beat - scrollBeatOffset) / (double)bpp) + metrics::laneHeaderW;
}

int ArrangementView::yToLane(float y) const
{
    return std::max(0, (int)((y + verticalScrollOffset - rulerH - effectiveLaneHeight_) / effectiveLaneHeight_));
}

float ArrangementView::laneToY(int lane) const
{
    // Lane 0 starts below the combined lane
    return (float)((lane + 1) * effectiveLaneHeight_) + rulerH - verticalScrollOffset;
}

float ArrangementView::combinedLaneY() const
{
    return (float)rulerH - verticalScrollOffset;
}

float ArrangementView::arrangementContentBottomY() const
{
    juce::ScopedLock sl(processor.laneLock);
    int numLanes = (int)processor.lanes.size();
    if (numLanes <= 0)
        return combinedLaneY() + (float)effectiveLaneHeight_;
    return laneToY(numLanes - 1) + (float)effectiveLaneHeight_;
}

void ArrangementView::rebuildClipBlocks()
{
    clipBlocks.clear();
    juce::ScopedLock sl(processor.laneLock);
    int numLanes = (int)processor.lanes.size();
    int totalRows = numLanes + 1;  // combined + lanes
    int availableH = getHeight() - rulerH;
    int defaultTotalH = totalRows * metrics::laneHeight;
    // Only shrink when lanes would go past the bottom; otherwise use default height
    effectiveLaneHeight_ = (totalRows > 0 && defaultTotalH > availableH)
        ? juce::jmax(metrics::laneHeightMin, availableH / totalRows)
        : metrics::laneHeight;

    for (int li = 0; li < numLanes; ++li)
    {
        auto& lane = processor.lanes[li];
        for (int ri = 0; ri < (int)lane.regions.size(); ++ri)
        {
            auto& r = lane.regions[ri];
            float x1 = beatToX(r.startBeat);
            float x2 = beatToX(r.endBeat);
            float y  = laneToY(li);

            ClipBlock cb;
            cb.bounds      = { x1, y + 2.0f, x2 - x1, (float)effectiveLaneHeight_ - 4.0f };
            cb.laneIndex   = li;
            cb.regionIndex = ri;
            clipBlocks.push_back(cb);
        }
    }

    // During cross-lane drag, move the dragged clip's visual position to the hovered lane
    if (draggingClip && selLane >= 0 && selRegion >= 0 && hoveredLane >= 0 && hoveredLane != selLane)
    {
        for (auto& cb : clipBlocks)
        {
            if (cb.laneIndex == selLane && cb.regionIndex == selRegion)
            {
                float newY = laneToY(hoveredLane);
                cb.bounds.setY(newY + 2.0f);
                break;
            }
        }
    }
}

void ArrangementView::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg());
    rebuildClipBlocks();
    paintBeatGrid(g);
    paintRuler(g);
    paintTimeSelection(g);
    paintLoopMarkers(g);
    paintCombinedLane(g);
    paintLaneHeaders(g);
    paintClipBlocks(g);
    paintPlayhead(g);
    if (draggingOver) paintDropIndicator(g);
    if (selectionBoxDragging)
    {
        g.setColour(colours::accent().withAlpha(0.3f));
        g.fillRect(juce::Rectangle<float>::leftTopRightBottom(
            std::min(selectionBoxStart.x, selectionBoxCurrent.x),
            std::min(selectionBoxStart.y, selectionBoxCurrent.y),
            std::max(selectionBoxStart.x, selectionBoxCurrent.x),
            std::max(selectionBoxStart.y, selectionBoxCurrent.y)));
        g.setColour(colours::accent());
        g.drawRect(juce::Rectangle<float>::leftTopRightBottom(
            std::min(selectionBoxStart.x, selectionBoxCurrent.x),
            std::min(selectionBoxStart.y, selectionBoxCurrent.y),
            std::max(selectionBoxStart.x, selectionBoxCurrent.x),
            std::max(selectionBoxStart.y, selectionBoxCurrent.y)), 1.0f);
    }

    // Empty state guidance
    if (clipBlocks.empty())
    {
        juce::ScopedLock sl(processor.laneLock);
        if (processor.lanes.empty())
        {
            g.setColour(colours::textDim());
            g.setFont(14.0f);
            auto area = getLocalBounds().withTrimmedTop(rulerH).withTrimmedLeft(metrics::laneHeaderW);
            g.drawText("Drag MIDI files here or click + to add a lane",
                       area, juce::Justification::centred);
        }
    }
}

void ArrangementView::paintRuler(juce::Graphics& g)
{
    // Ruler background (#252525 tertiary per DAW style)
    g.setColour(colours::bgLighter());
    g.fillRect(metrics::laneHeaderW, 0, getWidth() - metrics::laneHeaderW, rulerH);

    // Corner block (top-left, matching header area)
    g.setColour(colours::bgLighter());
    g.fillRect(0, 0, metrics::laneHeaderW, rulerH);
    g.setColour(colours::textDim().withAlpha(0.6f));
    g.setFont(11.0f);
    g.drawText(juce::String::charToString(0x23F1), 0, 0, metrics::laneHeaderW, rulerH,
               juce::Justification::centred);

    // Bottom separator (#2d2d2d)
    g.setColour(colours::panelBorder());
    g.drawHorizontalLine(rulerH - 1, 0.0f, (float)getWidth());

    int totalBeats = processor.arrangementBars.load() * 4;
    const double gridDiv = arrangementGridStepBeats(processor);

    for (double beat = 0; beat <= totalBeats; beat += gridDiv)
    {
        float x = beatToX(beat);
        if (x < metrics::laneHeaderW || x > getWidth()) continue;
        bool isBar = (std::fmod(beat, 4.0) < 0.001);
        bool isBeat = (std::fmod(beat, 1.0) < 0.001);

        if (isBar)
        {
            // Bar number (text-[10px] per DAW style)
            g.setColour(colours::text());
            g.setFont(10.0f);
            int barNum = (int)(beat / 4.0) + 1;
            g.drawText(juce::String(barNum), (int)x + 3, 1, 24, rulerH - 4,
                       juce::Justification::centredLeft);
            g.setColour(colours::textDim().withAlpha(0.4f));
            g.drawVerticalLine((int)x, (float)(rulerH - 6), (float)(rulerH - 1));
        }
        else if (isBeat)
        {
            g.setColour(colours::textDim().withAlpha(0.2f));
            g.drawVerticalLine((int)x, (float)(rulerH - 4), (float)(rulerH - 1));
        }
        else
        {
            g.setColour(colours::textDim().withAlpha(0.12f));
            g.drawVerticalLine((int)x, (float)(rulerH - 3), (float)(rulerH - 1));
        }
    }
}

void ArrangementView::paintBeatGrid(juce::Graphics& g)
{
    float startX = (float)metrics::laneHeaderW;
    int totalBeats = processor.arrangementBars.load() * 4;
    const float gridBottom = std::min((float)getHeight(), arrangementContentBottomY());

    float endX = beatToX(totalBeats);
    if (endX < getWidth())
    {
        g.setColour(colours::bg());
        g.fillRect(endX, (float)rulerH, (float)getWidth() - endX, (float)(getHeight() - rulerH));
    }

    const double gridDiv = arrangementGridStepBeats(processor);

    for (double beat = 0; beat <= totalBeats; beat += gridDiv)
    {
        float x = beatToX(beat);
        if (x < startX) continue;

        bool isBar  = (std::fmod(beat, 4.0) < 0.001);
        bool isBeat = (std::fmod(beat, 1.0) < 0.001);

        if (isBar)        g.setColour(colours::panelBorder().withAlpha(0.5f));
        else if (isBeat)  g.setColour(colours::panelBorder().withAlpha(0.2f));
        else              g.setColour(colours::panelBorder().withAlpha(0.08f));

        g.drawVerticalLine((int)x, (float)rulerH, gridBottom);
    }
}

void ArrangementView::paintLoopMarkers(juce::Graphics& g)
{
    float lx = beatToX(processor.loopStartBeat.load());
    float rx = beatToX(processor.loopEndBeat.load());
    float rH = (float)rulerH;
    const bool loopOn = processor.loopEnabled.load();
    const juce::Colour accentCol = loopOn ? colours::accent() : colours::textDim();

    // ── Loop region highlight in ruler (Ableton-style solid brace) ──
    g.setColour(accentCol.withAlpha(0.35f));
    g.fillRect(lx, 0.0f, rx - lx, rH);

    // Top bar connecting the brackets
    g.setColour(accentCol);
    g.fillRect(lx, 0.0f, rx - lx, 3.0f);

    // ── Loop region highlight in arrangement ──
    g.setColour(accentCol.withAlpha(0.06f));
    g.fillRect(lx, rH, rx - lx, (float)(getHeight() - rulerH));

    // ── Left bracket handle (Ableton-style L-bracket) ──
    g.setColour(accentCol);
    // Vertical bar
    g.fillRect(lx - 1.0f, 0.0f, 4.0f, rH);
    // Horizontal tab at top
    g.fillRect(lx - 1.0f, 0.0f, 12.0f, 3.0f);
    // Triangle flag
    juce::Path leftFlag;
    leftFlag.addTriangle(lx, 3.0f, lx + 10.0f, 3.0f, lx, rH * 0.7f);
    g.setColour(accentCol.withAlpha(0.6f));
    g.fillPath(leftFlag);

    // ── Right bracket handle (Ableton-style reversed L-bracket) ──
    g.setColour(accentCol);
    // Vertical bar
    g.fillRect(rx - 2.0f, 0.0f, 4.0f, rH);
    // Horizontal tab at top
    g.fillRect(rx - 11.0f, 0.0f, 12.0f, 3.0f);
    // Triangle flag
    juce::Path rightFlag;
    rightFlag.addTriangle(rx, 3.0f, rx - 10.0f, 3.0f, rx, rH * 0.7f);
    g.setColour(accentCol.withAlpha(0.6f));
    g.fillPath(rightFlag);

    // ── Vertical boundary lines through arrangement area ──
    g.setColour(accentCol.withAlpha(0.5f));
    g.drawVerticalLine((int)lx, rH, (float)getHeight());
    g.drawVerticalLine((int)rx, rH, (float)getHeight());

    // ── Dashed pattern inside ruler body for drag affordance ──
    g.setColour(accentCol.withAlpha(0.15f));
    float midY = rH * 0.6f;
    for (float dx = lx + 14.0f; dx < rx - 14.0f; dx += 8.0f)
        g.fillRect(dx, midY - 1.0f, 4.0f, 2.0f);
}

void ArrangementView::paintTimeSelection(juce::Graphics& g)
{
    if (!hasTimeSelection) return;
    // Don't draw selection when loop is active and covers the same range
    if (processor.loopEnabled.load())
    {
        double ls = processor.loopStartBeat.load();
        double le = processor.loopEndBeat.load();
        if (std::abs(timeSelStartBeat - ls) < 0.01 && std::abs(timeSelEndBeat - le) < 0.01)
            return;
    }

    float lx = beatToX(timeSelStartBeat);
    float rx = beatToX(timeSelEndBeat);
    float rH = (float)rulerH;

    // Semi-transparent highlight in ruler
    g.setColour(colours::accent().withAlpha(0.2f));
    g.fillRect(lx, 0.0f, rx - lx, rH);

    // Lighter highlight through arrangement area
    g.setColour(colours::accent().withAlpha(0.04f));
    g.fillRect(lx, rH, rx - lx, (float)(getHeight() - rulerH));

    // Selection boundary lines
    g.setColour(colours::accent().withAlpha(0.4f));
    g.drawVerticalLine((int)lx, 0.0f, (float)getHeight());
    g.drawVerticalLine((int)rx, 0.0f, (float)getHeight());
}

void ArrangementView::paintLaneHeaders(juce::Graphics& g)
{
    juce::ScopedLock sl(processor.laneLock);

    bool anySolo = false;
    for (const auto& l : processor.lanes) if (l.solo) { anySolo = true; break; }

    for (int i = 0; i < (int)processor.lanes.size(); ++i)
    {
        float y = laneToY(i);
        auto& lane = processor.lanes[i];
        bool dimLane = lane.muted || (anySolo && !lane.solo);

        // Lane header background (DAW Professional: bgLighter, panelBorder when selected)
        bool isSelected = (i == selLane && selRegion < 0);
        g.setColour(isSelected ? colours::panelBorder() : colours::bgLighter());
        g.fillRect(0.0f, y, (float)metrics::laneHeaderW, (float)effectiveLaneHeight_);
        if (dimLane)
        {
            g.setColour(colours::bg().withAlpha(0.6f));
            g.fillRect(0.0f, y, (float)metrics::laneHeaderW, (float)effectiveLaneHeight_);
        }
        if (isSelected && !dimLane)
        {
            g.setColour(lane.colour.withAlpha(0.3f));
            g.fillRect(0.0f, y, (float)metrics::laneHeaderW, (float)effectiveLaneHeight_);
        }

        // Colour accent strip (dim when lane dimmed)
        g.setColour(dimLane ? lane.colour.withAlpha(0.35f) : lane.colour);
        g.fillRoundedRectangle(2.0f, y + 4.0f, 4.0f, (float)effectiveLaneHeight_ - 8.0f, 2.0f);

        const float leftPad = 18.0f;
        const float nameH = (float)effectiveLaneHeight_ * 0.5f;
        const float btnSize = 18.0f;
        const float btnGap = 6.0f;

        // Lane name first (top), left-aligned with padding
        g.setColour(dimLane ? colours::textDim().withAlpha(0.5f) : colours::text());
        g.setFont(12.0f);
        g.drawText(lane.name, leftPad, (int)y, metrics::laneHeaderW - (int)leftPad - 4, (int)nameH,
                   juce::Justification::centredLeft);

        // Mute and Solo below name, left-aligned and spaced
        float btnY = y + nameH + (nameH - btnSize) * 0.5f;
        float cxM = leftPad + btnSize * 0.5f;
        float cxS = leftPad + btnSize + btnGap + btnSize * 0.5f;
        float cy = btnY + btnSize * 0.5f;
        g.setColour(lane.muted ? colours::muteRed() : colours::panelBorder());
        if (lane.muted)
            g.fillEllipse(cxM - btnSize * 0.5f, cy - btnSize * 0.5f, btnSize, btnSize);
        else
            g.drawEllipse(cxM - btnSize * 0.5f, cy - btnSize * 0.5f, btnSize, btnSize, 1.2f);
        g.setColour(lane.muted ? juce::Colours::white : colours::textDim());
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("M", cxM - 6.0f, cy - 6.0f, 12.0f, 12.0f, juce::Justification::centred);

        g.setColour(lane.solo ? colours::soloGreen() : colours::panelBorder());
        if (lane.solo)
            g.fillEllipse(cxS - btnSize * 0.5f, cy - btnSize * 0.5f, btnSize, btnSize);
        else
            g.drawEllipse(cxS - btnSize * 0.5f, cy - btnSize * 0.5f, btnSize, btnSize, 1.2f);
        g.setColour(lane.solo ? juce::Colours::white : colours::textDim());
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("S", cxS - 6.0f, cy - 6.0f, 12.0f, 12.0f, juce::Justification::centred);

        g.setColour(colours::panelBorder());
        g.drawHorizontalLine((int)(y + effectiveLaneHeight_ - 1), 0.0f, (float)getWidth());
    }

    // "+" add lane area
    float addY = laneToY((int)processor.lanes.size());
    g.setColour(colours::bgLighter().withAlpha(0.5f));
    g.fillRect(0.0f, addY, (float)metrics::laneHeaderW, (float)effectiveLaneHeight_);
    g.setColour(colours::textDim());
    g.setFont(18.0f);
    g.drawText("+", 0, (int)addY, metrics::laneHeaderW, effectiveLaneHeight_,
               juce::Justification::centred);
}

void ArrangementView::paintClipBlocks(juce::Graphics& g)
{
    juce::ScopedLock sl(processor.laneLock);

    bool anySolo = false;
    for (const auto& l : processor.lanes) if (l.solo) { anySolo = true; break; }

    for (auto& cb : clipBlocks)
    {
        auto& lane = processor.lanes[cb.laneIndex];
        auto& region = lane.regions[cb.regionIndex];
        auto& clip = lane.clips[region.clipIndex];
        bool selected = (cb.laneIndex == selLane && cb.regionIndex == selRegion) ||
            std::find_if(selectedClips.begin(), selectedClips.end(),
                [&cb](const std::pair<int,int>& p) { return p.first == cb.laneIndex && p.second == cb.regionIndex; }) != selectedClips.end();
        const bool showCompHandles = false;
        bool dimTrack = lane.muted || region.muted || (anySolo && !lane.solo);

        auto clipColour = clip.colour;
        auto fillCol = dimTrack ? clipColour.withAlpha(0.2f)
            : (region.muted ? clipColour.withAlpha(0.15f) : clipColour.withAlpha(0.65f));

        // Clip body with subtle gradient-like top highlight
        g.setColour(fillCol);
        g.fillRoundedRectangle(cb.bounds, metrics::clipCorner);

        // No selection overlay

        // Top highlight strip for depth
        g.setColour(juce::Colours::white.withAlpha(region.muted ? 0.03f : 0.08f));
        g.fillRoundedRectangle(cb.bounds.getX(), cb.bounds.getY(), cb.bounds.getWidth(), 3.0f, metrics::clipCorner);

        if (!clip.notes.empty() && clip.lengthBeats > 0.0)
        {
            float clipW = cb.bounds.getWidth();
            float clipH = cb.bounds.getHeight();
            float clipX = cb.bounds.getX();
            float clipY = cb.bounds.getY();
            double regionLen = region.endBeat - region.startBeat;

            int minNote = 127, maxNote = 0;
            for (auto& n : clip.notes) { minNote = std::min(minNote, n.noteNumber); maxNote = std::max(maxNote, n.noteNumber); }
            int noteRange = std::max(1, maxNote - minNote + 1);

            // Draw looped content: repeat notes for each loop iteration
            double loopLen = clip.lengthBeats;
            int loopCount = 1;
            if (loopLen > 1.0e-9 && regionLen > loopLen + 1.0e-6)
                loopCount = juce::jmax(1, (int)std::ceil(regionLen / loopLen - 1.0e-9));

            g.setColour(juce::Colours::white.withAlpha(0.4f));
            for (int loop = 0; loop < loopCount; ++loop)
            {
                double loopOffset = loop * loopLen;
                double loopEnd = std::min(loopOffset + loopLen, regionLen);
                for (auto& n : clip.notes)
                {
                    double noteStart = loopOffset + n.startBeat;
                    double noteEnd = noteStart + n.lengthBeats;
                    if (noteStart >= loopEnd) continue;
                    double visibleEnd = std::min(noteEnd, loopEnd);
                    double visibleStart = std::max(noteStart, loopOffset);
                    if (visibleStart >= visibleEnd) continue;
                    float nx = clipX + (float)(visibleStart / regionLen) * clipW;
                    float nw = std::max(1.0f, (float)((visibleEnd - visibleStart) / regionLen) * clipW);
                    float ny = clipY + clipH - ((float)(n.noteNumber - minNote + 1) / noteRange) * (clipH - 4.0f) - 2.0f;
                    float nh = std::max(1.0f, (clipH - 4.0f) / noteRange);
                    g.fillRect(nx, ny, nw, nh);
                }
            }

            // Draw loop boundary markers
            if (regionLen > loopLen)
            {
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                for (double beat = loopLen; beat < regionLen; beat += loopLen)
                {
                    float lx = clipX + (float)(beat / regionLen) * clipW;
                    g.drawVerticalLine((int)lx, clipY, clipY + clipH);
                }
            }
        }

        if (!processor.takeComps.empty() && !processor.takeComps[0].segments.empty())
        {
            const auto& activeC = processor.takeComps[0];
                double rs = region.startBeat;
                double re = region.endBeat;
                std::vector<std::pair<double, double>> parts;
                for (const auto& seg : activeC.segments)
                {
                    if (seg.laneIndex != cb.laneIndex) continue;
                    double a = std::max(seg.startBeat, rs);
                    double b = std::min(seg.endBeat, re);
                    if (b > a + 1.0e-9) parts.emplace_back(a, b);
                }
                std::sort(parts.begin(), parts.end());
                std::vector<std::pair<double, double>> merged;
                for (auto& p : parts)
                {
                    if (merged.empty() || p.first > merged.back().second + 1.0e-8)
                        merged.push_back(p);
                    else
                        merged.back().second = std::max(merged.back().second, p.second);
                }
                float clipX = cb.bounds.getX();
                float clipW = cb.bounds.getWidth();
                float clipY = cb.bounds.getY();
                float clipH = cb.bounds.getHeight();
                double regionLen = region.endBeat - region.startBeat;
                if (regionLen > 1.0e-12)
                {
                    double cur = rs;
                    g.setColour(juce::Colours::black.withAlpha(0.62f));
                    for (auto& p : merged)
                    {
                        if (p.first > cur)
                        {
                            float x0 = clipX + (float)((cur - rs) / regionLen) * clipW;
                            float x1 = clipX + (float)((std::min(p.first, re) - rs) / regionLen) * clipW;
                            g.fillRect(x0, clipY, std::max(1.0f, x1 - x0), clipH);
                        }
                        cur = std::max(cur, p.second);
                    }
                    if (cur < re)
                    {
                        float x0 = clipX + (float)((cur - rs) / regionLen) * clipW;
                        float x1 = clipX + (float)((re - rs) / regionLen) * clipW;
                        g.fillRect(x0, clipY, std::max(1.0f, x1 - x0), clipH);
                    }
                }
        }

        // Muted indicator (non-color cue)
        if (region.muted)
        {
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("M", cb.bounds.reduced(4.0f, 2.0f), juce::Justification::topRight);
        }

        if (selected)
        {
            // Selection glow
            g.setColour(colours::accent().withAlpha(0.15f));
            g.fillRoundedRectangle(cb.bounds.expanded(2.0f), metrics::clipCorner + 1.0f);
            g.setColour(colours::accentBright());
            g.drawRoundedRectangle(cb.bounds, metrics::clipCorner, 2.5f);

            float handleW = 5.0f;
            float handleH = cb.bounds.getHeight() * 0.4f;
            float handleY = cb.bounds.getCentreY() - handleH * 0.5f;
            // Clip resize handles (always)
            g.setColour(colours::textBright().withAlpha(0.7f));
            g.fillRoundedRectangle(cb.bounds.getX() - 1.0f, handleY, handleW, handleH, 2.0f);
            g.fillRoundedRectangle(cb.bounds.getRight() - handleW + 1.0f, handleY, handleW, handleH, 2.0f);
        }
        (void)showCompHandles;

        // Use contrast-aware text color for clip names
        auto textCol = clipColour.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white;
        g.setColour(textCol);
        g.setFont(11.0f);
        g.drawText(clip.name, cb.bounds.reduced(4.0f, 2.0f), juce::Justification::topLeft, true);
    }
}

void ArrangementView::paintCombinedLane(juce::Graphics& g)
{
    float y = combinedLaneY();
    float lH = (float)effectiveLaneHeight_;

    // Combined lane header
    g.setColour(colours::bgLight().brighter(0.05f));
    g.fillRect(0.0f, y, (float)metrics::laneHeaderW, lH);

    // Accent strip
    g.setColour(colours::accent().withAlpha(0.8f));
    g.fillRoundedRectangle(2.0f, y + 4.0f, 4.0f, lH - 8.0f, 2.0f);

    // Label
    const bool compHeader = processor.compsEnabled.load();
    g.setColour(colours::textBright());
    g.setFont(12.0f);
    g.drawText(compHeader ? "COMP" : "COMBINED", 18, (int)y, metrics::laneHeaderW - 22,
               (int)(lH * 0.6f), juce::Justification::centredLeft);

    // Drag hint when there are notes (only when not in comp-only mode)
    if (!processor.compsEnabled.load() && !processor.combinedClip.notes.empty())
    {
        g.setColour(colours::textDim());
        g.setFont(9.0f);
        g.drawText("Drag to DAW", 10, (int)(y + lH * 0.55f),
                   metrics::laneHeaderW - 14, (int)(lH * 0.35f),
                   juce::Justification::centredLeft);
    }

    // Bottom border
    g.setColour(colours::panelBorder().withAlpha(0.6f));
    g.drawHorizontalLine((int)(y + lH - 1), 0.0f, (float)getWidth());

    auto& mc = processor.combinedClip;
    float clipY = y + 2.0f;
    float clipH = lH - 4.0f;

    const juce::Colour combinedBaseGrey(0xff343438);
    const juce::Colour compHilight(0xff565662);

    bool drewCompSegments = false;
    {
        juce::ScopedLock sl(processor.laneLock);
        if (!processor.takeComps.empty())
        {
            const auto segs = comping::sortedSegments(processor.takeComps[0]);
            if (!segs.empty())
            {
                drewCompSegments = true;
                const double span = mc.lengthBeats > 1.0e-9 ? mc.lengthBeats
                    : (double)(processor.arrangementBars.load() * 4);
                const float clipX = beatToX(0.0);
                const float clipEndX = beatToX(span);
                g.setColour(combinedBaseGrey);
                g.fillRect(clipX, clipY, std::max(1.0f, clipEndX - clipX), clipH);

                for (size_t i = 0; i < segs.size(); ++i)
                {
                    const auto& seg = segs[i];
                    float x0 = beatToX(seg.startBeat);
                    float x1 = beatToX(seg.endBeat);
                    g.setColour(compHilight);
                    g.fillRect(x0, clipY, std::max(1.0f, x1 - x0), clipH);
                    g.setColour(juce::Colours::white.withAlpha(0.22f));
                    g.drawRect(x0, clipY, std::max(1.0f, x1 - x0), clipH, 1.0f);
                    if (i > 0)
                    {
                        g.setColour(juce::Colours::white.withAlpha(0.85f));
                        g.drawVerticalLine((int)juce::jlimit(clipX, clipEndX, x0), clipY, clipY + clipH);
                    }
                }

                const double sessionLen = (double)(processor.arrangementBars.load() * 4);
                std::vector<double> boundaries;
                for (const auto& s : segs)
                {
                    if (s.startBeat > 1.0e-4 && s.startBeat < sessionLen - 1.0e-4)
                        boundaries.push_back(s.startBeat);
                    if (s.endBeat > 1.0e-4 && s.endBeat < sessionLen - 1.0e-4)
                        boundaries.push_back(s.endBeat);
                }
                std::sort(boundaries.begin(), boundaries.end());
                boundaries.erase(std::unique(boundaries.begin(), boundaries.end(),
                    [](double a, double b) { return std::abs(a - b) < 1.0e-4; }), boundaries.end());

                if (processor.compsEnabled.load())
                {
                    constexpr float handleHalf = 3.5f;
                    for (double b : boundaries)
                    {
                        float hx = beatToX(b);
                        auto handleRect = juce::Rectangle<float>(hx - handleHalf, clipY + 1.0f,
                                                                 handleHalf * 2.0f, clipH - 2.0f);
                        g.setColour(juce::Colours::white.withAlpha(0.92f));
                        g.fillRoundedRectangle(handleRect, 2.0f);
                        g.setColour(colours::accentBright().withAlpha(0.95f));
                        g.drawRoundedRectangle(handleRect.reduced(0.5f), 2.0f, 1.2f);
                    }
                }
            }
        }
    }

    if (!drewCompSegments)
    {
        if (mc.notes.empty()) return;

        float clipX = beatToX(0.0);
        float clipEndX = beatToX(mc.lengthBeats);
        float clipW = clipEndX - clipX;
        g.setColour(combinedBaseGrey);
        g.fillRect(clipX, clipY, clipW, clipH);
    }
    else if (mc.notes.empty())
        return;

    float clipX = beatToX(0.0);
    float clipEndX = beatToX(mc.lengthBeats > 1.0e-9 ? mc.lengthBeats
        : (double)(processor.arrangementBars.load() * 4));

    // Draw note preview (composite or merged combined)
    int minNote = 127, maxNote = 0;
    for (auto& n : mc.notes) { minNote = std::min(minNote, n.noteNumber); maxNote = std::max(maxNote, n.noteNumber); }
    int noteRange = std::max(1, maxNote - minNote + 1);

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    for (auto& n : mc.notes)
    {
        float nx = beatToX(n.startBeat);
        float nw = std::max(1.0f, (float)(n.lengthBeats / beatsPerPixel));
        float ny = clipY + clipH - ((float)(n.noteNumber - minNote + 1) / noteRange) * (clipH - 4.0f) - 2.0f;
        float nh = std::max(1.0f, (clipH - 4.0f) / noteRange);
        if (nx + nw < clipX || nx > clipEndX) continue;
        g.fillRect(nx, ny, nw, nh);
    }
}

void ArrangementView::paintPlayhead(juce::Graphics& g)
{
    double beat;
    if (processor.hostPlaying.load())
    {
        beat = processor.loopEnabled.load()
            ? processor.mappedBeatPos.load()
            : processor.hostBeatPos.load();
    }
    else
    {
        beat = processor.editPlayheadBeat.load();
    }
    double sessionLen = (double)(processor.arrangementBars.load() * 4);
    beat = std::fmod(beat, sessionLen);
    if (beat < 0) beat += sessionLen;

    float x = beatToX(beat);
    auto phCol = colours::playhead();

    // Glow effect (0 0 8px rgba(239,68,68,0.5) per DAW style guide)
    for (int i = 3; i >= 1; --i)
    {
        float w = (float)(i * 2);
        g.setColour(phCol.withAlpha(0.15f / (float)i));
        g.fillRect(x - w, 0.0f, w * 2.0f, (float)getHeight());
    }

    // Vertical line 2px, red #ef4444
    g.setColour(phCol);
    g.fillRect(x - 1.0f, 0.0f, 2.0f, (float)getHeight());

    // Draggable triangle handle in ruler area (points down)
    juce::Path tri;
    float triW = 8.0f, triH = 8.0f;
    tri.addTriangle(x - triW, 0.0f, x + triW, 0.0f, x, triH);
    g.setColour(phCol);
    g.fillPath(tri);
}

void ArrangementView::paintDropIndicator(juce::Graphics& g)
{
    float x = beatToX(dropBeatPos);
    float y = (dropLaneIdx >= 0) ? laneToY(dropLaneIdx) : (float)rulerH;
    float h = (float)effectiveLaneHeight_;
    g.setColour(colours::accent().withAlpha(0.3f));
    g.fillRect(x, y, 60.0f, h);
    g.setColour(colours::accent());
    g.drawRect(x, y, 60.0f, h, 1.0f);
}

// ── Mouse ────────────────────────────────────────────────────────────────────

bool ArrangementView::isNearRegionEdge(const juce::MouseEvent& e, const ClipBlock& cb,
                                      const CompRegion& /*region*/, EdgeDragTarget& which,
                                      int& outCompBoundaryIdx, bool /*forPress*/) const
{
    constexpr float edgeThreshold = 8.0f;
    outCompBoundaryIdx = -1;
    if (e.position.y < cb.bounds.getY() || e.position.y > cb.bounds.getBottom())
        return false;

    if (std::abs(e.position.x - cb.bounds.getX()) < edgeThreshold)
    { which = EdgeDragTarget::Start; return true; }
    if (std::abs(e.position.x - cb.bounds.getRight()) < edgeThreshold)
    { which = EdgeDragTarget::End; return true; }
    return false;
}

bool ArrangementView::findCompBoundaryAtMouse(float x, float y,
                                              std::vector<std::pair<int, bool>>& outEdges,
                                              double& outBoundaryBeat) const
{
    outEdges.clear();
    if (!processor.compsEnabled.load()) return false;

    const float my = combinedLaneY();
    const float lH = (float)effectiveLaneHeight_;
    if (y < my || y >= my + lH || x < (float)metrics::laneHeaderW)
        return false;

    juce::ScopedLock sl(processor.laneLock);
    if (processor.takeComps.empty()) return false;

    const auto& segs = processor.takeComps[0].segments;
    if (segs.empty()) return false;

    const double sessionLen = (double)(processor.arrangementBars.load() * 4);
    constexpr float hitPx = 7.0f;
    constexpr double beatEps = 2e-3;

    std::vector<double> uniq;
    for (const auto& s : segs)
    {
        if (s.startBeat > beatEps && s.startBeat < sessionLen - beatEps)
            uniq.push_back(s.startBeat);
        if (s.endBeat > beatEps && s.endBeat < sessionLen - beatEps)
            uniq.push_back(s.endBeat);
    }
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end(),
                           [](double a, double b) { return std::abs(a - b) < 1e-4; }),
               uniq.end());

    double bestB = 0.0;
    float bestDist = 1e12f;
    for (double b : uniq)
    {
        const float bx = beatToX(b);
        const float d = std::abs(x - bx);
        if (d < hitPx && d < bestDist)
        {
            bestDist = d;
            bestB = b;
        }
    }
    if (bestDist >= hitPx) return false;

    for (int i = 0; i < (int)segs.size(); ++i)
    {
        const auto& s = segs[(size_t)i];
        if (std::abs(s.startBeat - bestB) < beatEps) outEdges.push_back({ i, true });
        if (std::abs(s.endBeat - bestB) < beatEps) outEdges.push_back({ i, false });
    }
    if (outEdges.empty()) return false;

    outBoundaryBeat = bestB;
    return true;
}

void ArrangementView::mouseMove(const juce::MouseEvent& e)
{
    if (e.position.y < rulerH && e.position.x >= metrics::laneHeaderW)
    {
        // Check playhead triangle handle
        float phX = beatToX(processor.editPlayheadBeat.load());
        if (std::abs(e.position.x - phX) < 10 && e.position.y < 12)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }

        if (processor.loopEnabled.load())
        {
            float lx = beatToX(processor.loopStartBeat.load());
            float rx = beatToX(processor.loopEndBeat.load());
            if (std::abs(e.position.x - lx) < 12 || std::abs(e.position.x - rx) < 12)
            {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
            // Body region - show move cursor
            if (e.position.x > lx + 12 && e.position.x < rx - 12)
            {
                setMouseCursor(juce::MouseCursor::DraggingHandCursor);
                return;
            }
        }
        // Default ruler cursor: IBeam for time selection
        setMouseCursor(juce::MouseCursor::IBeamCursor);
        return;
    }
    // Combined / COMP lane cursors
    {
        float my = combinedLaneY();
        float lH = (float)effectiveLaneHeight_;
        if (e.position.y >= my && e.position.y < my + lH
            && e.position.x >= metrics::laneHeaderW)
        {
            if (processor.compsEnabled.load())
            {
                std::vector<std::pair<int, bool>> dummy;
                double b = 0.0;
                if (findCompBoundaryAtMouse(e.position.x, e.position.y, dummy, b))
                {
                    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                    return;
                }
                setMouseCursor(juce::MouseCursor::NormalCursor);
                return;
            }
            if (!processor.combinedClip.notes.empty())
            {
                setMouseCursor(juce::MouseCursor::DraggingHandCursor);
                return;
            }
        }
    }
    // Comp mode: horizontal swipe cursor over clips (clips are locked — no edge resize)
    if (processor.compsEnabled.load() && e.position.x >= metrics::laneHeaderW
        && e.position.y >= rulerH + (float)effectiveLaneHeight_)
    {
        juce::ScopedLock sl(processor.laneLock);
        for (int ci = (int)clipBlocks.size() - 1; ci >= 0; --ci)
        {
            const auto& cb = clipBlocks[(size_t)ci];
            if (cb.bounds.contains(e.position))
            {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
        }
    }

    if (!processor.compsEnabled.load())
    {
        juce::ScopedLock sl(processor.laneLock);
        for (int ci = (int) clipBlocks.size() - 1; ci >= 0; --ci)
        {
            const auto& cb = clipBlocks[(size_t) ci];
            if (cb.laneIndex >= (int)processor.lanes.size()) continue;
            auto& lane = processor.lanes[cb.laneIndex];
            if (cb.regionIndex >= (int)lane.regions.size()) continue;
            EdgeDragTarget which;
            int compB = -1;
            if (isNearRegionEdge(e, cb, lane.regions[cb.regionIndex], which, compB, false))
            {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
        }
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void ArrangementView::mouseDown(const juce::MouseEvent& e)
{
    if (e.position.y < rulerH && e.position.x >= metrics::laneHeaderW)
    {
        // Check playhead triangle handle drag (top portion of ruler)
        float phX = beatToX(processor.editPlayheadBeat.load());
        if (std::abs(e.position.x - phX) < 10 && e.position.y < 12)
        {
            draggingPlayhead = true;
            return;
        }

        // Check loop handle interactions first (only when loop is active)
        if (processor.loopEnabled.load())
        {
            float lx = beatToX(processor.loopStartBeat.load());
            float rx = beatToX(processor.loopEndBeat.load());
            if (std::abs(e.position.x - lx) < 12) { loopDragging = LoopDragTarget::Start; return; }
            if (std::abs(e.position.x - rx) < 12) { loopDragging = LoopDragTarget::End; return; }
            // Click inside loop region body - drag the whole loop
            if (e.position.x > lx + 12 && e.position.x < rx - 12)
            {
                loopDragging = LoopDragTarget::Body;
                loopDragBodyOffset = xToBeat(e.position.x) - processor.loopStartBeat.load();
                loopDragBodyLength = processor.loopEndBeat.load() - processor.loopStartBeat.load();
                return;
            }
        }

        // Click in ruler area: set playhead AND start time range selection
        double beat = std::max(0.0, xToBeat(e.position.x));
        beat = processor.snapBeat(beat);
        processor.editPlayheadBeat.store(beat);
        rulerDragging = true;
        rulerDragStartBeat = beat;
        hasTimeSelection = false;
        timeSelStartBeat = beat;
        timeSelEndBeat = beat;
        repaint();
        return;
    }

    selLane = -1; selRegion = -1; draggingClip = false; clipDragPending = false;
    edgeDragging = EdgeDragTarget::None;
    hasTimeSelection = false; // Clear ruler selection when clicking in arrangement
    draggingCombinedClip = false; combinedDragInitiated = false;
    selectionBoxDragging = false;
    compSwipeDragging = false;
    compBoundaryDragging = false;

    // Combined / COMP lane: comp boundary handles first, then drag-to-DAW when not in comp mode
    {
        float my = combinedLaneY();
        float lH = (float)effectiveLaneHeight_;
        if (e.position.y >= my && e.position.y < my + lH && e.position.x >= metrics::laneHeaderW)
        {
            std::vector<std::pair<int, bool>> edges;
            double bAt = 0.0;
            if (processor.compsEnabled.load()
                && findCompBoundaryAtMouse(e.position.x, e.position.y, edges, bAt))
            {
                compBoundarySegmentsUndoBefore = processor.getActiveTakeCompSegmentsSnapshot();
                compBoundaryDragging = true;
                compBoundaryDragFromBeat = bAt;
                repaint();
                return;
            }
            if (processor.compsEnabled.load())
                return;
            if (!processor.combinedClip.notes.empty())
            {
                draggingCombinedClip = true;
                combinedDragStartPos = e.position;
                return;
            }
        }
    }

    // Check lane header clicks (M/S buttons, context menu, "+" area)
    if (e.position.x < metrics::laneHeaderW)
    {
        juce::ScopedLock sl(processor.laneLock);
        int numLanes = (int)processor.lanes.size();

        // "+" add-lane area
        float addY = laneToY(numLanes);
        if (e.position.y >= addY && e.position.y < addY + effectiveLaneHeight_)
        {
            if (onAddLaneClicked) onAddLaneClicked();
            repaint();
            return;
        }

        // Check M/S buttons on each lane (below name, left-aligned)
        const float leftPad = 18.0f;
        const float nameH = (float)effectiveLaneHeight_ * 0.5f;
        const float btnSize = 18.0f;
        const float btnGap = 6.0f;
        for (int i = 0; i < numLanes; ++i)
        {
            float y = laneToY(i);
            float btnY = y + nameH + (nameH - btnSize) * 0.5f;
            auto muteRect = juce::Rectangle<float>(leftPad, btnY, btnSize, btnSize);
            auto soloRect = juce::Rectangle<float>(leftPad + btnSize + btnGap, btnY, btnSize, btnSize);

            if (muteRect.contains(e.position))
            {
                processor.lanes[i].muted = !processor.lanes[i].muted;
                repaint();
                return;
            }
            if (soloRect.contains(e.position))
            {
                processor.lanes[i].solo = !processor.lanes[i].solo;
                repaint();
                return;
            }

            // Right-click on lane header -> context menu
            if (e.mods.isRightButtonDown() &&
                e.position.y >= y && e.position.y < y + effectiveLaneHeight_)
            {
                showLaneContextMenu(i);
                return;
            }

            // Left-click on lane header -> select the lane
            if (!e.mods.isRightButtonDown() &&
                e.position.y >= y && e.position.y < y + effectiveLaneHeight_)
            {
                selLane = i;
                selRegion = -1;
                selectedClips.clear();
                repaint();
                return;
            }
        }
    }

    bool hitClip = false;
    juce::ScopedLock sl(processor.laneLock);
    for (int ci = (int) clipBlocks.size() - 1; ci >= 0; --ci)
    {
        auto& cb = clipBlocks[(size_t) ci];
        if (cb.laneIndex >= (int)processor.lanes.size()) continue;
        auto& lane = processor.lanes[cb.laneIndex];
        if (cb.regionIndex >= (int)lane.regions.size()) continue;
        auto& reg = lane.regions[cb.regionIndex];
        EdgeDragTarget which;
        int compB = -1;
        if (!processor.compsEnabled.load()
            && isNearRegionEdge(e, cb, reg, which, compB, true))
        {
            selLane = cb.laneIndex; selRegion = cb.regionIndex;
            edgeDragging = which;
            edgeDragLane = cb.laneIndex;
            edgeDragRegion = cb.regionIndex;
            edgeDragOrigStart = reg.startBeat;
            edgeDragOrigEnd = reg.endBeat;
            edgeDragOrigSelStart = -1.0;
            edgeDragOrigSelEnd = -1.0;
            edgeDragOrigRanges.clear();
            edgeDragLinkedLane = -1;
            edgeDragLinkedRegion = -1;
            edgeDragLinkedOrigSelStart = -1.0;
            edgeDragLinkedOrigSelEnd = -1.0;
            edgeDragLinkedOrigRanges.clear();
            hitClip = true;
            // Cross-lane "linked" comp edges were moving unrelated regions; each region is edited independently.
            repaint();
            return;
        }

        if (cb.bounds.contains(e.position))
        {
            if (processor.compsEnabled.load() && !e.mods.isRightButtonDown())
            {
                compSwipeSegmentsUndoBefore = processor.getActiveTakeCompSegmentsSnapshot();
                compSwipeDragging = true;
                compSwipeLane = cb.laneIndex;
                compSwipeRegion = cb.regionIndex;
                double b = processor.snapBeat(xToBeat(e.position.x));
                b = juce::jlimit(reg.startBeat, reg.endBeat, b);
                compSwipeStartB = compSwipeEndB = b;
                hitClip = true;
                break;
            }
            if (e.mods.isRightButtonDown())
            {
                // Right-click: show context menu (use full selection if this clip is in it)
                bool inSel = std::find_if(selectedClips.begin(), selectedClips.end(),
                    [&cb](const std::pair<int,int>& p) { return p.first == cb.laneIndex && p.second == cb.regionIndex; }) != selectedClips.end();
                if (inSel || selectedClips.empty())
                    showClipContextMenu(cb.laneIndex, cb.regionIndex);
                else
                    showClipContextMenu(cb.laneIndex, cb.regionIndex);
            }
            else
            {
                if (e.mods.isShiftDown())
                {
                    auto it = std::find_if(selectedClips.begin(), selectedClips.end(),
                        [&cb](const std::pair<int,int>& p) { return p.first == cb.laneIndex && p.second == cb.regionIndex; });
                    if (it != selectedClips.end())
                        selectedClips.erase(it);
                    else
                        selectedClips.push_back({ cb.laneIndex, cb.regionIndex });
                }
                else
                {
                    selectedClips.clear();
                    selectedClips.push_back({ cb.laneIndex, cb.regionIndex });
                }
                selLane = cb.laneIndex; selRegion = cb.regionIndex;
                clipDragPending = true;
                clipDragDownPos = e.position;
                clipDragOrigLane = cb.laneIndex;
                clipDragMouseYOffset = e.position.y - laneToY(cb.laneIndex);
                clipDragOrigSelStart = -1.0;
                clipDragOrigSelEnd = -1.0;
                clipDragOrigRanges.clear();
                clipDragOrigBeat = reg.startBeat;
                clipDragOrigEnd = reg.endBeat;
                clipDragMouseOffset = xToBeat(e.position.x) - clipDragOrigBeat;
            }
            hitClip = true;
            break;
        }
    }
    // Click in empty arrangement area: start selection box drag
    if (!processor.compsEnabled.load() && !hitClip && !e.mods.isRightButtonDown()
        && e.position.y >= rulerH + effectiveLaneHeight_
        && e.position.x >= metrics::laneHeaderW)
    {
        selectionBoxDragging = true;
        selectionBoxStart = e.position;
        selectionBoxCurrent = e.position;
        selectedClips.clear();
    }
    repaint();
}

void ArrangementView::mouseDoubleClick(const juce::MouseEvent& e)
{
    for (auto& cb : clipBlocks)
    {
        if (cb.bounds.contains(e.position))
        {
            if (processor.compsEnabled.load())
            {
                const double beat = std::max(0.0, xToBeat(e.position.x));
                const auto beforeDel = processor.getActiveTakeCompSegmentsSnapshot();
                processor.deleteCompSegmentCoveringBeat(cb.laneIndex, beat);
                const auto afterDel = processor.getActiveTakeCompSegmentsSnapshot();
                commitTakeCompUndoIfChanged(processor, beforeDel, afterDel);
                repaint();
                return;
            }
            juce::ScopedLock sl(processor.laneLock);
            auto& lane = processor.lanes[cb.laneIndex];
            auto& region = lane.regions[cb.regionIndex];
            auto& clip = lane.clips[region.clipIndex];
            if (onClipDoubleClicked)
                onClipDoubleClicked(clip, cb.laneIndex, cb.regionIndex);
            return;
        }
    }
    if (e.position.y >= rulerH + effectiveLaneHeight_
        && e.position.x >= metrics::laneHeaderW
        && onEmptyArrangementDoubleClicked)
        onEmptyArrangementDoubleClicked();
}

void ArrangementView::mouseDrag(const juce::MouseEvent& e)
{
    if (selectionBoxDragging)
    {
        selectionBoxCurrent = e.position;
        repaint();
        return;
    }

    if (compBoundaryDragging)
    {
        double nb = processor.snapBeat(std::max(0.0, xToBeat(e.position.x)));
        compBoundaryDragFromBeat = processor.applyCompBoundaryDrag(compBoundaryDragFromBeat, nb);
        repaint();
        return;
    }

    if (compSwipeDragging && compSwipeLane >= 0 && compSwipeRegion >= 0)
    {
        double regStart = 0.0, regEnd = 0.0;
        {
            juce::ScopedLock sl(processor.laneLock);
            if (!processor.isValidRegion(compSwipeLane, compSwipeRegion))
            {
                compSwipeDragging = false;
                return;
            }
            auto& reg = processor.lanes[compSwipeLane].regions[compSwipeRegion];
            regStart = reg.startBeat;
            regEnd = reg.endBeat;
        }
        double b = processor.snapBeat(xToBeat(e.position.x));
        b = juce::jlimit(regStart, regEnd, b);
        compSwipeEndB = b;
        double s = std::min(compSwipeStartB, compSwipeEndB);
        double eB = std::max(compSwipeStartB, compSwipeEndB);
        if (eB - s > 1.0e-6)
            processor.applyCompSwipe(compSwipeLane, s, eB);
        repaint();
        return;
    }

    // Combined clip drag-to-DAW
    if (draggingCombinedClip && !combinedDragInitiated)
    {
        auto dist = e.position.getDistanceFrom(combinedDragStartPos);
        if (dist > 5.0f)
        {
            combinedDragInitiated = true;
            draggingCombinedClip = false;

            // Write combined clip to a temp .mid file
            auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
            auto tempFile = tempDir.getChildFile("PatternFlow-Combined.mid");

            double bpm = processor.hostBpm.load();
            if (bpm <= 0.0) bpm = 120.0;

            bool written = false;
            {
                juce::ScopedLock sl(processor.laneLock);
                double sessionLen = (double)(processor.arrangementBars.load() * 4);
                written = writeMidiFile(processor.combinedClip, tempFile, bpm, sessionLen);
            }
            if (written)
            {
                juce::StringArray files;
                files.add(tempFile.getFullPathName());
                juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
            }
            return;
        }
        return;
    }

    // Playhead drag
    if (draggingPlayhead)
    {
        double beat = std::max(0.0, xToBeat(e.position.x));
        beat = processor.snapBeat(beat);
        processor.editPlayheadBeat.store(beat);
        repaint();
        return;
    }

    // Ruler time range selection drag
    if (rulerDragging)
    {
        double beat = std::max(0.0, xToBeat(e.position.x));
        beat = processor.snapBeat(beat);
        timeSelStartBeat = std::min(rulerDragStartBeat, beat);
        timeSelEndBeat = std::max(rulerDragStartBeat, beat);
        hasTimeSelection = (timeSelEndBeat - timeSelStartBeat) > 0.01;
        repaint();
        return;
    }

    if (loopDragging != LoopDragTarget::None)
    {
        double beat = std::max(0.0, xToBeat(e.position.x));
        beat = processor.snapBeat(beat);
        const double minGap = loopMinSeparationBeats(processor);
        if (loopDragging == LoopDragTarget::Start)
            processor.loopStartBeat.store(std::min(beat, processor.loopEndBeat.load() - minGap));
        else if (loopDragging == LoopDragTarget::End)
            processor.loopEndBeat.store(std::max(beat, processor.loopStartBeat.load() + minGap));
        else if (loopDragging == LoopDragTarget::Body)
        {
            double newStart = std::max(0.0, processor.snapBeat(beat - loopDragBodyOffset));
            processor.loopStartBeat.store(newStart);
            processor.loopEndBeat.store(newStart + loopDragBodyLength);
        }
        repaint();
        return;
    }

    if (processor.compsEnabled.load()
        && (edgeDragging != EdgeDragTarget::None || clipDragPending || draggingClip))
    {
        edgeDragging = EdgeDragTarget::None;
        clipDragPending = false;
        draggingClip = false;
        refresh();
        return;
    }

    // Region edge drag (clip resize)
    if (edgeDragging != EdgeDragTarget::None && edgeDragLane >= 0 && edgeDragRegion >= 0)
    {
        double beat = std::max(0.0, xToBeat(e.position.x));
        beat = processor.snapBeat(beat);
        juce::ScopedLock sl(processor.laneLock);
        if (edgeDragLane >= (int)processor.lanes.size() ||
            edgeDragRegion >= (int)processor.lanes[edgeDragLane].regions.size())
        {
            repaint();
            return;
        }
        auto& region = processor.lanes[edgeDragLane].regions[edgeDragRegion];

        if (edgeDragging == EdgeDragTarget::Start)
        {
            region.startBeat = std::min(beat, region.endBeat - 0.25);
            region.ensureSelectionInBounds();
            processor.rebuildCombinedClip();
        }
        else
        {
            const double sessionLen = (double)(processor.arrangementBars.load() * 4);
            region.endBeat = juce::jmin(std::max(beat, region.startBeat + 0.25), sessionLen);
            region.ensureSelectionInBounds();
            processor.rebuildCombinedClip();
        }
        repaint();
        return;
    }

    // Clip move: wait for a few pixels before starting drag so double-click can open piano roll
    if (clipDragPending && !draggingClip)
    {
        if (e.position.getDistanceFrom(clipDragDownPos) <= 4.0f)
            return;
        draggingClip = true;
        clipDragPending = false;
    }

    if (draggingClip && selLane >= 0 && selRegion >= 0)
    {
        double newBeat = std::max(0.0, xToBeat(e.position.x) - clipDragMouseOffset);
        newBeat = processor.snapBeat(newBeat);
        juce::ScopedLock sl(processor.laneLock);
        if (selLane < (int)processor.lanes.size() && selRegion < (int)processor.lanes[selLane].regions.size())
        {
            auto& region = processor.lanes[selLane].regions[selRegion];
            double len = region.endBeat - region.startBeat;
            double delta = newBeat - region.startBeat;
            region.startBeat = newBeat;
            region.endBeat   = newBeat + len;
            (void)delta;
        }
        // Track which lane the mouse is hovering over for visual feedback
        hoveredLane = yToLane(e.position.y);
        repaint();
    }
}

void ArrangementView::mouseUp(const juce::MouseEvent& e)
{
    if (compBoundaryDragging)
    {
        const auto afterBoundary = processor.getActiveTakeCompSegmentsSnapshot();
        compBoundaryDragging = false;
        commitTakeCompUndoIfChanged(processor, compBoundarySegmentsUndoBefore, afterBoundary);
        compBoundarySegmentsUndoBefore.clear();
        repaint();
        return;
    }

    if (compSwipeDragging)
    {
        const auto afterSwipe = processor.getActiveTakeCompSegmentsSnapshot();
        compSwipeDragging = false;
        compSwipeLane = -1;
        compSwipeRegion = -1;
        commitTakeCompUndoIfChanged(processor, compSwipeSegmentsUndoBefore, afterSwipe);
        compSwipeSegmentsUndoBefore.clear();
        repaint();
        return;
    }

    if (selectionBoxDragging)
    {
        selectionBoxDragging = false;
        float x1 = std::min(selectionBoxStart.x, selectionBoxCurrent.x);
        float x2 = std::max(selectionBoxStart.x, selectionBoxCurrent.x);
        float y1 = std::min(selectionBoxStart.y, selectionBoxCurrent.y);
        float y2 = std::max(selectionBoxStart.y, selectionBoxCurrent.y);
        juce::Rectangle<float> box(x1, y1, x2 - x1, y2 - y1);
        if (box.getWidth() > 2 && box.getHeight() > 2)
        {
            selectedClips.clear();
            for (auto& cb : clipBlocks)
            {
                if (cb.bounds.intersects(box))
                {
                    selectedClips.push_back({ cb.laneIndex, cb.regionIndex });
                }
            }
            if (!selectedClips.empty())
            {
                selLane = selectedClips[0].first;
                selRegion = selectedClips[0].second;
            }
        }
        repaint();
        return;
    }

    if (draggingPlayhead) { draggingPlayhead = false; return; }
    if (draggingCombinedClip) { draggingCombinedClip = false; combinedDragInitiated = false; return; }

    // Edge drag undo (clip resize)
    if (edgeDragging != EdgeDragTarget::None && edgeDragLane >= 0 && edgeDragRegion >= 0)
    {
        juce::ScopedLock sl(processor.laneLock);
        if (edgeDragLane < (int)processor.lanes.size() &&
            edgeDragRegion < (int)processor.lanes[edgeDragLane].regions.size())
        {
            auto& region = processor.lanes[edgeDragLane].regions[edgeDragRegion];

            double newStart = region.startBeat;
            double newEnd = region.endBeat;
            if (std::abs(newStart - edgeDragOrigStart) > 0.001 ||
                std::abs(newEnd - edgeDragOrigEnd) > 0.001)
            {
                region.startBeat = edgeDragOrigStart;
                region.endBeat = edgeDragOrigEnd;
                processor.undoManager.beginNewTransaction();
                processor.undoManager.perform(
                    new MoveRegionAction(processor, edgeDragLane, edgeDragRegion,
                                         edgeDragOrigStart, edgeDragOrigEnd,
                                         newStart, newEnd));
            }
        }
        edgeDragging = EdgeDragTarget::None;
        refresh();
        return;
    }

    if (draggingClip && selLane >= 0 && selRegion >= 0)
    {
        // Ignore drop when released over ruler or combined lane (prevents wrong lane assignment)
        if (e.position.y < rulerH + effectiveLaneHeight_)
        {
            juce::ScopedLock sl(processor.laneLock);
            if (selLane < (int)processor.lanes.size() && selRegion < (int)processor.lanes[selLane].regions.size())
            {
                auto& region = processor.lanes[selLane].regions[selRegion];
                region.startBeat = clipDragOrigBeat;
                region.endBeat = clipDragOrigEnd;
            }
            hoveredLane = -1;
            draggingClip = false;
            clipDragPending = false;
            refresh();
            return;
        }

        int targetLane = yToLane(e.position.y);
        juce::ScopedLock sl(processor.laneLock);
        int numLanes = (int)processor.lanes.size();

        if (selLane < numLanes && selRegion < (int)processor.lanes[selLane].regions.size())
        {
            auto& region = processor.lanes[selLane].regions[selRegion];
            double newStart = region.startBeat;
            double newEnd = region.endBeat;

            bool laneChanged = (targetLane != clipDragOrigLane);
            bool beatChanged = std::abs(newStart - clipDragOrigBeat) > 0.001;

            if (laneChanged)
            {
                // Revert the horizontal position change (undo action handles both)
                region.startBeat = clipDragOrigBeat;
                region.endBeat = clipDragOrigEnd;

                bool createNew = (targetLane >= numLanes);
                int dstLane = createNew ? targetLane : targetLane;
                processor.undoManager.beginNewTransaction();
                processor.undoManager.perform(
                    new MoveRegionToLaneAction(processor, selLane, selRegion,
                                               dstLane, newStart, newEnd, createNew));
                selLane = dstLane;
                selRegion = 0; // Will be at end of dest lane
                // Find actual index
                if (dstLane < (int)processor.lanes.size())
                    selRegion = (int)processor.lanes[dstLane].regions.size() - 1;
            }
            else if (beatChanged)
            {
                region.startBeat = clipDragOrigBeat;
                region.endBeat = clipDragOrigEnd;
                processor.undoManager.beginNewTransaction();
                processor.undoManager.perform(
                    new MoveRegionAction(processor, selLane, selRegion,
                                         clipDragOrigBeat, clipDragOrigEnd,
                                         newStart, newEnd));
            }
        }
    }
    clipDragPending = false;
    hoveredLane = -1;
    loopDragging = LoopDragTarget::None;
    rulerDragging = false;
    draggingClip = false;
    refresh();
}

void ArrangementView::mouseWheelMove(const juce::MouseEvent& e,
                                      const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        // Zoom with Cmd/Ctrl + scroll
        float zoomFactor = wheel.deltaY > 0 ? 0.85f : 1.18f;
        beatsPerPixel = juce::jlimit(0.01f, 2.0f, beatsPerPixel * zoomFactor);
        refresh();
    }
    else if (e.mods.isShiftDown())
    {
        // Horizontal scroll with Shift + scroll
        scrollBeatOffset = std::max(0.0f, scrollBeatOffset - wheel.deltaY * 8.0f);
        refresh();
    }
    else
    {
        // Vertical scroll
        int numLanes;
        { juce::ScopedLock sl(processor.laneLock); numLanes = (int)processor.lanes.size(); }
        float maxScroll = std::max(0.0f, (float)((numLanes + 1) * effectiveLaneHeight_) - (float)(getHeight() - rulerH));
        verticalScrollOffset = juce::jlimit(0.0f, maxScroll, verticalScrollOffset - wheel.deltaY * 40.0f);
        refresh();
    }
}

void ArrangementView::resized()
{
    if (getWidth() > 0 && getHeight() > 0 && !initialZoomDone)
    {
        initialZoomDone = true;
        zoomToFitSession();
    }
    refresh();
}

// ── Drag & Drop ──────────────────────────────────────────────────────────────

bool ArrangementView::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.toString().contains("MidiFileDrag") ||
           details.description.toString().contains("MidiClip");
}

void ArrangementView::itemDragEnter(const SourceDetails&) { draggingOver = true; repaint(); }

void ArrangementView::itemDragMove(const SourceDetails& details)
{
    double beat = std::max(0.0, xToBeat((float)details.localPosition.x));
    dropBeatPos = (float)processor.snapBeat(beat);
    dropLaneIdx = yToLane((float)details.localPosition.y);
    repaint();
}

void ArrangementView::itemDragExit(const SourceDetails&) { draggingOver = false; repaint(); }

void ArrangementView::itemDropped(const SourceDetails& details)
{
    draggingOver = false;

    auto* fileComp = dynamic_cast<juce::FileTreeComponent*>(details.sourceComponent.get());
    juce::File droppedFile;
    if (fileComp != nullptr) droppedFile = fileComp->getSelectedFile();

    if (!droppedFile.existsAsFile() && details.description.isString())
    {
        juce::File f(details.description.toString());
        if (f.existsAsFile()) droppedFile = f;
    }

    if (droppedFile.existsAsFile() && droppedFile.hasFileExtension("mid;midi"))
    {
        auto clip = parseMidiFile(droppedFile);
        double beat = std::max(0.0, xToBeat((float)details.localPosition.x));
        beat = processor.snapBeat(beat);
        int lane = yToLane((float)details.localPosition.y);

        juce::ScopedLock sl(processor.laneLock);
        int numLanes = (int)processor.lanes.size();

        if (lane >= numLanes) { addClipToNewLane(clip, beat); }
        else
        {
            auto& targetLane = processor.lanes[lane];
            bool droppedOnSameClip = false;
            for (int ri = 0; ri < (int)targetLane.regions.size(); ++ri)
            {
                auto& region = targetLane.regions[ri];
                if (beat >= region.startBeat && beat < region.endBeat)
                {
                    auto& existingClip = targetLane.clips[region.clipIndex];
                    if (existingClip.filePath == clip.filePath)
                    {
                        droppedOnSameClip = true;
                        targetLane.expanded = true;
                        int newIdx = (int)targetLane.clips.size();
                        targetLane.clips.push_back(clip);
                        CompRegion compRegion;
                        compRegion.startBeat = region.startBeat;
                        compRegion.endBeat   = region.endBeat;
                        compRegion.clipIndex  = newIdx;
                        compRegion.muted      = true;
                        targetLane.regions.push_back(compRegion);
                    }
                    break;
                }
            }
            if (!droppedOnSameClip) addClipToLane(clip, lane, beat);
        }
    }
    refresh();
}

bool ArrangementView::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (juce::File(path).hasFileExtension("mid;midi"))
            return true;
    return false;
}

void ArrangementView::filesDropped(const juce::StringArray& files, int x, int y)
{
    for (const auto& path : files)
    {
        juce::File f(path);
        if (!f.existsAsFile() || !f.hasFileExtension("mid;midi")) continue;
        auto clip = parseMidiFile(f);
        double beat = std::max(0.0, xToBeat((float)x));
        beat = processor.snapBeat(beat);
        int lane = yToLane((float)y);
        juce::ScopedLock sl(processor.laneLock);
        int numLanes = (int)processor.lanes.size();
        if (lane >= numLanes)
            addClipToNewLane(clip, beat);
        else
            addClipToLane(clip, lane, beat);
    }
    refresh();
}

void ArrangementView::addClipToLane(const MidiClip& clip, int laneIndex, double beatPos)
{
    if (laneIndex >= 0 && laneIndex < (int)processor.lanes.size())
    {
        auto presets = getClipColourPresets();
        MidiClip colouredClip = clip;
        colouredClip.colour = presets[processor.lanes[laneIndex].clips.size() % presets.size()];
        processor.lanes[laneIndex].addClipAtPosition(colouredClip, beatPos);
        processor.clampArrangementToSessionLength();
    }
}

void ArrangementView::addClipToNewLane(const MidiClip& clip, double beatPos)
{
    CompLane newLane;
    auto presets = getClipColourPresets();
    int idx = (int)processor.lanes.size();
    newLane.name   = "Lane " + juce::String(idx + 1);
    newLane.colour = presets[idx % presets.size()];
    MidiClip colouredClip = clip;
    colouredClip.colour = newLane.colour;
    newLane.addClipAtPosition(colouredClip, beatPos);
    processor.lanes.push_back(newLane);
    processor.clampArrangementToSessionLength();
}

void ArrangementView::showClipContextMenu(int laneIdx, int regionIdx)
{
    std::vector<std::pair<int,int>> targets = selectedClips;
    if (targets.empty())
        targets.push_back({ laneIdx, regionIdx });

    juce::PopupMenu menu;
    juce::PopupMenu colourMenu;
    auto presets = getClipColourPresets();
    for (int i = 0; i < (int)presets.size(); ++i) colourMenu.addItem(100 + i, "Colour " + juce::String(i + 1));
    menu.addSubMenu("Clip Colour", colourMenu);

    juce::PopupMenu laneColourMenu;
    for (int i = 0; i < (int)presets.size(); ++i) laneColourMenu.addItem(200 + i, "Colour " + juce::String(i + 1));
    menu.addSubMenu("Lane Colour", laneColourMenu);
    menu.addSeparator();

    { juce::ScopedLock sl(processor.laneLock); auto& region = processor.lanes[laneIdx].regions[regionIdx]; menu.addItem(1, region.muted ? "Unmute" : "Mute"); }
    menu.addItem(2, "Isolate Note...");
    if (targets.size() > 1)
        menu.addItem(7, "Match size of first clip");
    menu.addItem(4, "Duplicate Region");
    menu.addItem(5, "Split at Playhead");
    menu.addSeparator();
    menu.addItem(3, "Delete Region");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, laneIdx, regionIdx, presets, targets](int result)
    {
        if (result == 0) return;
        juce::ScopedLock sl(processor.laneLock);
        if (laneIdx >= (int)processor.lanes.size()) return;
        auto& lane = processor.lanes[laneIdx];
        if (regionIdx >= (int)lane.regions.size()) return;

        if (result >= 100 && result < 200)
        { auto& clip = lane.clips[lane.regions[regionIdx].clipIndex]; clip.colour = presets[result - 100]; }
        else if (result >= 200 && result < 300) lane.colour = presets[result - 200];
        else if (result == 1) { processor.undoManager.beginNewTransaction(); processor.undoManager.perform(new ToggleMuteAction(processor, laneIdx, regionIdx)); }
        else if (result == 2)
        {
            auto& region = lane.regions[regionIdx]; auto& clip = lane.clips[region.clipIndex];
            auto pitches = clip.getDistinctPitches();
            juce::PopupMenu noteMenu; noteMenu.addItem(1000, "All Notes");
            for (int p : pitches) { auto noteName = juce::MidiMessage::getMidiNoteName(p, true, true, 3); noteMenu.addItem(1001 + p, noteName); }
            noteMenu.showMenuAsync(juce::PopupMenu::Options(), [this, laneIdx, regionIdx](int noteResult)
            { if (noteResult == 0) return; juce::ScopedLock sl2(processor.laneLock);
              if (laneIdx >= (int)processor.lanes.size()) return;
              if (regionIdx >= (int)processor.lanes[laneIdx].regions.size()) return;
              processor.lanes[laneIdx].regions[regionIdx].noteFilter = (noteResult == 1000) ? -1 : (noteResult - 1001); repaint(); });
        }
        else if (result == 7 && targets.size() > 1)
        {
            juce::ScopedLock sl(processor.laneLock);
            // Sort by startBeat - first = earliest
            std::vector<std::pair<int,int>> sorted = targets;
            std::sort(sorted.begin(), sorted.end(), [this](const std::pair<int,int>& a, const std::pair<int,int>& b) {
                if (a.first >= (int)processor.lanes.size() || b.first >= (int)processor.lanes.size()) return false;
                auto& ra = processor.lanes[a.first].regions[a.second];
                auto& rb = processor.lanes[b.first].regions[b.second];
                return ra.startBeat < rb.startBeat;
            });
            int fl = sorted[0].first, fr = sorted[0].second;
            if (fl >= (int)processor.lanes.size() || fr >= (int)processor.lanes[fl].regions.size()) return;
            double refLen = processor.lanes[fl].regions[fr].endBeat - processor.lanes[fl].regions[fr].startBeat;
            for (size_t i = 1; i < sorted.size(); ++i)
            {
                int li = sorted[i].first, ri = sorted[i].second;
                if (li >= (int)processor.lanes.size() || ri >= (int)processor.lanes[li].regions.size()) continue;
                auto& region = processor.lanes[li].regions[ri];
                auto& clip = processor.lanes[li].clips[region.clipIndex];
                double newEnd = region.startBeat + refLen;
                region.endBeat = newEnd;
                clip.lengthBeats = refLen;
                clip.notes.erase(std::remove_if(clip.notes.begin(), clip.notes.end(),
                    [refLen](const NoteEvent& n) { return n.startBeat + n.lengthBeats > refLen; }), clip.notes.end());
                for (auto& n : clip.notes)
                    if (n.startBeat + n.lengthBeats > refLen) n.lengthBeats = refLen - n.startBeat;
                region.ensureSelectionInBounds();
            }
            processor.rebuildCombinedClip();
        }
        else if (result == 4) { processor.undoManager.beginNewTransaction(); processor.undoManager.perform(new DuplicateRegionAction(processor, laneIdx, regionIdx)); }
        else if (result == 5)
        {
            double playBeat = processor.hostBeatPos.load();
            auto& reg = lane.regions[regionIdx];
            if (playBeat > reg.startBeat && playBeat < reg.endBeat)
            {
                processor.undoManager.beginNewTransaction();
                processor.undoManager.perform(new SplitRegionAction(processor, laneIdx, regionIdx, playBeat));
            }
        }
        else if (result == 3)
        {
            processor.undoManager.beginNewTransaction();
            processor.undoManager.perform(new RemoveRegionAction(processor, laneIdx, regionIdx));
            selectedClips.clear();
        }
        repaint();
    });
}

// ── Ableton-style Cmd+L Loop ─────────────────────────────────────────────────

void ArrangementView::setLoopToSelection()
{
    if (!hasTimeSelection || timeSelEndBeat <= timeSelStartBeat) return;

    processor.loopStartBeat.store(timeSelStartBeat);
    processor.loopEndBeat.store(timeSelEndBeat);
    processor.loopEnabled.store(true);
    processor.editPlayheadBeat.store(timeSelStartBeat);
    if (onLoopChanged) onLoopChanged();
    refresh();
}

void ArrangementView::setLoopToSelectedClip()
{
    if (selLane < 0 || selRegion < 0) return;

    juce::ScopedLock sl(processor.laneLock);
    if (selLane >= (int)processor.lanes.size()) return;
    auto& lane = processor.lanes[selLane];
    if (selRegion >= (int)lane.regions.size()) return;

    auto& region = lane.regions[selRegion];
    processor.loopStartBeat.store(region.startBeat);
    processor.loopEndBeat.store(region.endBeat);
    processor.loopEnabled.store(true);
    processor.editPlayheadBeat.store(region.startBeat);
    if (onLoopChanged) onLoopChanged();
    refresh();
}

void ArrangementView::toggleLoopFromContext()
{
    // If we have a time range selection, set loop to that range
    if (hasTimeSelection && (timeSelEndBeat - timeSelStartBeat) > 0.01)
    {
        double ls = processor.loopStartBeat.load();
        double le = processor.loopEndBeat.load();
        bool loopOn = processor.loopEnabled.load();

        // If loop is already set to exactly this selection, toggle it off
        if (loopOn &&
            std::abs(ls - timeSelStartBeat) < 0.01 &&
            std::abs(le - timeSelEndBeat) < 0.01)
        {
            processor.loopEnabled.store(false);
            if (onLoopChanged) onLoopChanged();
            refresh();
            return;
        }

        setLoopToSelection();
        return;
    }

    // If a clip is selected, set loop to that clip's region
    if (selLane >= 0 && selRegion >= 0)
    {
        juce::ScopedLock sl(processor.laneLock);
        if (selLane < (int)processor.lanes.size())
        {
            auto& lane = processor.lanes[selLane];
            if (selRegion < (int)lane.regions.size())
            {
                auto& region = lane.regions[selRegion];
                double ls = processor.loopStartBeat.load();
                double le = processor.loopEndBeat.load();
                bool loopOn = processor.loopEnabled.load();

                // If loop is already set to this clip, toggle off
                if (loopOn &&
                    std::abs(ls - region.startBeat) < 0.01 &&
                    std::abs(le - region.endBeat) < 0.01)
                {
                    processor.loopEnabled.store(false);
                    if (onLoopChanged) onLoopChanged();
                    refresh();
                    return;
                }
            }
        }
        setLoopToSelectedClip();
        return;
    }

    // No selection: just toggle loop on/off
    bool nowEnabled = !processor.loopEnabled.load();
    processor.loopEnabled.store(nowEnabled);
    if (nowEnabled)
    {
        if (processor.loopEndBeat.load() <= processor.loopStartBeat.load())
        {
            processor.loopStartBeat.store(0.0);
            processor.loopEndBeat.store(16.0);
        }
        processor.editPlayheadBeat.store(processor.loopStartBeat.load());
    }
    if (onLoopChanged) onLoopChanged();
    refresh();
}

bool ArrangementView::keyPressed(const juce::KeyPress& key)
{
    // Cmd+L / Ctrl+L: Ableton-style loop from selection
    if (key == juce::KeyPress('l', juce::ModifierKeys::commandModifier, 0))
    {
        toggleLoopFromContext();
        return true;
    }

    // Delete / Backspace: delete selected lane or selected clip
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (processor.compsEnabled.load())
            return true;

        juce::ScopedLock sl(processor.laneLock);

        // If a clip/region is selected, delete it
        if (selLane >= 0 && selRegion >= 0
            && selLane < (int)processor.lanes.size()
            && selRegion < (int)processor.lanes[selLane].regions.size())
        {
            processor.lanes[selLane].regions.erase(
                processor.lanes[selLane].regions.begin() + selRegion);
            selRegion = -1;
            refresh();
            return true;
        }

        // If a lane is selected (but no region), delete the lane; selection moves up
        if (selLane >= 0 && selRegion < 0
            && selLane < (int)processor.lanes.size())
        {
            const int deleted = selLane;
            processor.lanes.erase(processor.lanes.begin() + selLane);
            processor.remapLanesAfterDeleteLocked(deleted);
            if (processor.lanes.empty())
            {
                selLane = -1;
                selRegion = -1;
            }
            else
            {
                selLane = juce::jlimit(0, (int)processor.lanes.size() - 1, deleted - 1);
                selRegion = -1;
            }
            refresh();
            return true;
        }

        return true;
    }

    return false;
}

void ArrangementView::showLaneContextMenu(int laneIdx)
{
    juce::PopupMenu menu;
    {
        juce::ScopedLock sl(processor.laneLock);
        if (laneIdx < 0 || laneIdx >= (int)processor.lanes.size()) return;
        auto& lane = processor.lanes[laneIdx];
        menu.addItem(1, lane.muted ? "Unmute Lane" : "Mute Lane");
        menu.addItem(2, lane.solo ? "Unsolo Lane" : "Solo Lane");
    }
    menu.addSeparator();

    juce::PopupMenu colourMenu;
    auto presets = getClipColourPresets();
    for (int i = 0; i < (int)presets.size(); ++i)
        colourMenu.addItem(100 + i, "Colour " + juce::String(i + 1));
    menu.addSubMenu("Lane Colour", colourMenu);
    menu.addSeparator();
    menu.addItem(10, "Delete Lane");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, laneIdx, presets](int result)
    {
        if (result == 0) return;
        juce::ScopedLock sl(processor.laneLock);
        if (laneIdx >= (int)processor.lanes.size()) return;

        if (result == 1) processor.lanes[laneIdx].muted = !processor.lanes[laneIdx].muted;
        else if (result == 2) processor.lanes[laneIdx].solo = !processor.lanes[laneIdx].solo;
        else if (result >= 100 && result < 200) processor.lanes[laneIdx].colour = presets[result - 100];
        else if (result == 10)
        {
            const int deleted = laneIdx;
            processor.lanes.erase(processor.lanes.begin() + laneIdx);
            processor.remapLanesAfterDeleteLocked(deleted);
            if (processor.lanes.empty())
            {
                selLane = -1;
                selRegion = -1;
            }
            else
            {
                selLane = juce::jlimit(0, (int)processor.lanes.size() - 1, deleted - 1);
                selRegion = -1;
            }
        }
        repaint();
    });
}

} // namespace pflow
