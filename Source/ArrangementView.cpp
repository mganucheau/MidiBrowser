#include "ArrangementView.h"
#include "PluginProcessor.h"
#include "UndoActions.h"
#include <algorithm>

namespace pflow {

ArrangementView::ArrangementView(PatternFlowProcessor& proc) : processor(proc)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);

    btnAddBars.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnAddBars.setColour(juce::TextButton::textColourOffId, colours::text());
    btnAddBars.onClick = [this]
    {
        int expected = processor.arrangementBars.load();
        while (!processor.arrangementBars.compare_exchange_weak(expected, expected + 4)) {}
        refresh();
    };
    addAndMakeVisible(btnAddBars);

    // Loop toggle button
    updateLoopButton();
    btnLoop.setTooltip("Toggle loop (Cmd+L). Click-drag ruler to select range, then Cmd+L to loop it.");
    btnLoop.onClick = [this]
    {
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
        updateLoopButton();
        refresh();
    };
    addAndMakeVisible(btnLoop);
}

void ArrangementView::refresh()
{
    processor.rebuildMasterClip();
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
    btnAddBars.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnAddBars.setColour(juce::TextButton::textColourOffId, colours::text());
    updateLoopButton();
}

void ArrangementView::updateLoopButton()
{
    bool loopOn = processor.loopEnabled.load();
    btnLoop.setColour(juce::TextButton::buttonColourId,
                      loopOn ? colours::accent() : colours::bgLighter());
    btnLoop.setColour(juce::TextButton::textColourOffId,
                      loopOn ? colours::textBright() : colours::text());
}

double ArrangementView::xToBeat(float x) const
{
    return (double)(x - metrics::laneHeaderW) * beatsPerPixel + scrollBeatOffset;
}

float ArrangementView::beatToX(double beat) const
{
    return (float)((beat - scrollBeatOffset) / beatsPerPixel) + metrics::laneHeaderW;
}

int ArrangementView::yToLane(float y) const
{
    return std::max(0, (int)((y + verticalScrollOffset - rulerH - metrics::laneHeight) / metrics::laneHeight));
}

float ArrangementView::laneToY(int lane) const
{
    // Lane 0 starts below the master lane
    return (float)((lane + 1) * metrics::laneHeight) + rulerH - verticalScrollOffset;
}

float ArrangementView::masterLaneY() const
{
    return (float)rulerH - verticalScrollOffset;
}

void ArrangementView::rebuildClipBlocks()
{
    clipBlocks.clear();
    juce::ScopedLock sl(processor.laneLock);

    for (int li = 0; li < (int)processor.lanes.size(); ++li)
    {
        auto& lane = processor.lanes[static_cast<size_t>(li)];
        for (int ci = 0; ci < (int)lane.clips.size(); ++ci)
        {
            double startBeat = lane.clipStarts[static_cast<size_t>(ci)];
            double endBeat = startBeat + lane.clips[static_cast<size_t>(ci)].lengthBeats;
            float x1 = beatToX(startBeat);
            float x2 = beatToX(endBeat);
            float y  = laneToY(li);

            ClipBlock cb;
            cb.bounds     = { x1, y + 2.0f, x2 - x1, (float)metrics::laneHeight - 4.0f };
            cb.laneIndex  = li;
            cb.clipIndex  = ci;
            clipBlocks.push_back(cb);
        }
    }

    // During cross-lane drag, move the dragged clip's visual position to the hovered lane
    if (draggingClip && selLane >= 0 && selClip >= 0 && hoveredLane >= 0 && hoveredLane != selLane)
    {
        for (auto& cb : clipBlocks)
        {
            if (cb.laneIndex == selLane && cb.clipIndex == selClip)
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
    paintMasterLane(g);
    if (processor.compEnabled.load())
        paintCompLane(g);
    paintLaneHeaders(g);
    paintClipBlocks(g);
    paintPlayhead(g);
    if (draggingOver) paintDropIndicator(g);

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
    // Ruler background
    g.setColour(colours::bgLight());
    g.fillRect(metrics::laneHeaderW, 0, getWidth() - metrics::laneHeaderW, rulerH);

    // Corner block (top-left, matching header area)
    g.setColour(colours::bgLight());
    g.fillRect(0, 0, metrics::laneHeaderW, rulerH);
    g.setColour(colours::textDim().withAlpha(0.6f));
    g.setFont(11.0f);
    g.drawText(juce::String::charToString(0x23F1), 0, 0, metrics::laneHeaderW, rulerH,
               juce::Justification::centred);

    // Bottom separator
    g.setColour(colours::panelBorder().withAlpha(0.6f));
    g.drawHorizontalLine(rulerH - 1, 0.0f, (float)getWidth());

    // Bar numbers with beat tick marks
    int totalBeats = processor.arrangementBars.load() * 4;
    for (double beat = 0; beat <= totalBeats; beat += 1.0)
    {
        float x = beatToX(beat);
        if (x < metrics::laneHeaderW || x > getWidth()) continue;
        bool isBar = (std::fmod(beat, 4.0) < 0.001);

        if (isBar)
        {
            // Bar number
            g.setColour(colours::text());
            g.setFont(11.0f);
            int barNum = (int)(beat / 4.0) + 1;
            g.drawText(juce::String(barNum), (int)x + 3, 1, 24, rulerH - 4,
                       juce::Justification::centredLeft);
            // Tick mark
            g.setColour(colours::textDim().withAlpha(0.4f));
            g.drawVerticalLine((int)x, (float)(rulerH - 6), (float)(rulerH - 1));
        }
        else
        {
            // Sub-beat tick
            g.setColour(colours::textDim().withAlpha(0.2f));
            g.drawVerticalLine((int)x, (float)(rulerH - 4), (float)(rulerH - 1));
        }
    }
}

void ArrangementView::paintBeatGrid(juce::Graphics& g)
{
    float startX = (float)metrics::laneHeaderW;
    int totalBeats = processor.arrangementBars.load() * 4;

    float endX = beatToX(totalBeats);
    if (endX < getWidth())
    {
        g.setColour(juce::Colour(0x15000000));
        g.fillRect(endX, (float)rulerH, (float)getWidth() - endX, (float)(getHeight() - rulerH));
    }

    double gridDiv = 1.0;
    int gs = processor.gridSnap.load();
    using GS = PatternFlowProcessor::GridSize;
    switch ((GS)gs)
    {
        case GS::Bar:         gridDiv = 4.0;  break;
        case GS::Beat:        gridDiv = 1.0;  break;
        case GS::HalfBeat:    gridDiv = 0.5;  break;
        case GS::QuarterBeat: gridDiv = 0.25; break;
        case GS::Eighth:      gridDiv = 0.5;  break;
        case GS::Sixteenth:   gridDiv = 0.25; break;
        default:              gridDiv = 1.0;  break;
    }

    for (double beat = 0; beat <= totalBeats; beat += gridDiv)
    {
        float x = beatToX(beat);
        if (x < startX) continue;

        bool isBar  = (std::fmod(beat, 4.0) < 0.001);
        bool isBeat = (std::fmod(beat, 1.0) < 0.001);

        if (isBar)        g.setColour(colours::panelBorder().withAlpha(0.5f));
        else if (isBeat)  g.setColour(colours::panelBorder().withAlpha(0.2f));
        else              g.setColour(colours::panelBorder().withAlpha(0.08f));

        g.drawVerticalLine((int)x, (float)rulerH, (float)getHeight());
    }
}

void ArrangementView::paintLoopMarkers(juce::Graphics& g)
{
    if (!processor.loopEnabled.load()) return;

    float lx = beatToX(processor.loopStartBeat.load());
    float rx = beatToX(processor.loopEndBeat.load());
    float rH = (float)rulerH;
    auto accentCol = colours::accent();

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

    for (int i = 0; i < (int)processor.lanes.size(); ++i)
    {
        float y = laneToY(i);
        auto& lane = processor.lanes[static_cast<size_t>(i)];

        // Lane header background (highlight if selected)
        bool isSelected = (i == selLane && selClip < 0);
        g.setColour(isSelected ? colours::bgLight().brighter(0.25f) : colours::bgLight());
        g.fillRect(0.0f, y, (float)metrics::laneHeaderW, (float)metrics::laneHeight);
        if (isSelected)
        {
            g.setColour(lane.colour.withAlpha(0.3f));
            g.fillRect(0.0f, y, (float)metrics::laneHeaderW, (float)metrics::laneHeight);
        }

        // Colour accent strip on left
        g.setColour(lane.colour);
        g.fillRoundedRectangle(2.0f, y + 4.0f, 4.0f, (float)metrics::laneHeight - 8.0f, 2.0f);

        // Lane icon (circle with lane colour)
        float iconCX = 16.0f, iconCY = y + (float)metrics::laneHeight * 0.3f;
        g.setColour(lane.colour.withAlpha(0.25f));
        g.fillEllipse(iconCX - 5.0f, iconCY - 5.0f, 10.0f, 10.0f);
        g.setColour(lane.colour);
        g.fillEllipse(iconCX - 3.0f, iconCY - 3.0f, 6.0f, 6.0f);

        // Lane name
        g.setColour(colours::text());
        g.setFont(12.0f);
        g.drawText(lane.name, 26, (int)y, metrics::laneHeaderW - 30,
                   (int)(metrics::laneHeight * 0.5f), juce::Justification::centredLeft);

        // Mute / Solo buttons
        float btnY = y + (float)metrics::laneHeight * 0.55f;
        float btnW = 20.0f, btnH = 15.0f;
        auto muteRect = juce::Rectangle<float>(12.0f, btnY, btnW, btnH);
        g.setColour(lane.muted ? juce::Colour(0xffef5350) : colours::bgLighter());
        g.fillRoundedRectangle(muteRect, 3.0f);
        g.setColour(lane.muted ? juce::Colours::white : colours::textDim());
        g.setFont(9.0f);
        g.drawText("M", muteRect, juce::Justification::centred);

        auto soloRect = juce::Rectangle<float>(36.0f, btnY, btnW, btnH);
        g.setColour(lane.solo ? juce::Colour(0xffffc107) : colours::bgLighter());
        g.fillRoundedRectangle(soloRect, 3.0f);
        g.setColour(lane.solo ? juce::Colours::black : colours::textDim());
        g.setFont(9.0f);
        g.drawText("S", soloRect, juce::Justification::centred);

        // Volume icon
        g.setColour(colours::textDim().withAlpha(0.5f));
        g.setFont(10.0f);
        g.drawText(juce::String::charToString(0x266B), (int)(metrics::laneHeaderW - 20), (int)btnY, 16, (int)btnH, juce::Justification::centred);

        g.setColour(colours::panelBorder().withAlpha(0.5f));
        g.drawHorizontalLine((int)(y + metrics::laneHeight - 1), 0.0f, (float)getWidth());
    }

    // "+" add lane area
    float addY = laneToY((int)processor.lanes.size());
    g.setColour(colours::bgLight().withAlpha(0.2f));
    g.fillRect(0.0f, addY, (float)metrics::laneHeaderW, (float)metrics::laneHeight);
    g.setColour(colours::textDim());
    g.setFont(18.0f);
    g.drawText("+", 0, (int)addY, metrics::laneHeaderW, metrics::laneHeight,
               juce::Justification::centred);
}

void ArrangementView::paintClipBlocks(juce::Graphics& g)
{
    juce::ScopedLock sl(processor.laneLock);

    // Determine if comp mode is active for dimming unselected segments
    bool compActive = processor.compEnabled.load();
    const Comp* activeComp = nullptr;
    if (compActive && !processor.comps.empty())
        activeComp = &processor.getActiveComp();

    for (auto& cb : clipBlocks)
    {
        if (cb.laneIndex >= (int)processor.lanes.size()) continue;
        auto& lane = processor.lanes[static_cast<size_t>(cb.laneIndex)];
        if (cb.clipIndex >= (int)lane.clips.size()) continue;
        auto& clip = lane.clips[static_cast<size_t>(cb.clipIndex)];
        bool selected = (cb.laneIndex == selLane && cb.clipIndex == selClip);

        auto clipColour = clip.colour;

        // Dim clips that are not active in the current comp
        bool dimmed = false;
        if (activeComp != nullptr)
        {
            double clipStart = lane.clipStarts[static_cast<size_t>(cb.clipIndex)];
            double clipEnd = clipStart + clip.lengthBeats;
            // Check if any segment in the comp selects this lane for any portion of this clip
            bool anySelected = false;
            for (auto& seg : activeComp->segments)
            {
                if (seg.laneIndex == cb.laneIndex &&
                    seg.startBeat < clipEnd && seg.endBeat > clipStart)
                {
                    anySelected = true;
                    break;
                }
            }
            dimmed = !anySelected;
        }

        // Clip body
        float bodyAlpha = dimmed ? 0.25f : 0.65f;
        g.setColour(clipColour.withAlpha(bodyAlpha));
        g.fillRoundedRectangle(cb.bounds, metrics::clipCorner);
        // Top highlight strip
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRoundedRectangle(cb.bounds.getX(), cb.bounds.getY(), cb.bounds.getWidth(), 3.0f, metrics::clipCorner);

        if (!clip.notes.empty() && clip.lengthBeats > 0.0)
        {
            float clipW = cb.bounds.getWidth();
            float clipH = cb.bounds.getHeight();
            float clipX = cb.bounds.getX();
            float clipY = cb.bounds.getY();
            double clipLen = clip.lengthBeats;

            int minNote = 127, maxNote = 0;
            for (auto& n : clip.notes) { minNote = std::min(minNote, n.noteNumber); maxNote = std::max(maxNote, n.noteNumber); }
            int noteRange = std::max(1, maxNote - minNote + 1);

            g.setColour(juce::Colours::white.withAlpha(dimmed ? 0.15f : 0.4f));
            for (auto& n : clip.notes)
            {
                if (n.startBeat >= clipLen) continue;
                float nx = clipX + (float)(n.startBeat / clipLen) * clipW;
                float nw = std::max(1.0f, (float)(n.lengthBeats / clipLen) * clipW);
                float ny = clipY + clipH - ((float)(n.noteNumber - minNote + 1) / noteRange) * (clipH - 4.0f) - 2.0f;
                float nh = std::max(1.0f, (clipH - 4.0f) / noteRange);
                g.fillRect(nx, ny, nw, nh);
            }
        }

        if (selected)
        {
            g.setColour(colours::accent().withAlpha(0.15f));
            g.fillRoundedRectangle(cb.bounds.expanded(2.0f), metrics::clipCorner + 1.0f);
            g.setColour(colours::accentBright());
            g.drawRoundedRectangle(cb.bounds, metrics::clipCorner, 2.5f);
        }

        // Clip name
        auto textCol = clipColour.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white;
        g.setColour(textCol);
        g.setFont(11.0f);
        g.drawText(clip.name, cb.bounds.reduced(4.0f, 2.0f), juce::Justification::topLeft, true);
    }

    // ── Comp swipe preview on take lane ──
    if (compSwiping && compSwipeLane >= 0 && compSwipeLane < (int)processor.lanes.size())
    {
        double sStart = std::min(compSwipeStartBeat, compSwipeEndBeat);
        double sEnd   = std::max(compSwipeStartBeat, compSwipeEndBeat);
        if (sEnd > sStart)
        {
            float sx = beatToX(sStart);
            float ex = beatToX(sEnd);
            float sy = laneToY(compSwipeLane);

            juce::Colour swipeCol = processor.lanes[static_cast<size_t>(compSwipeLane)].colour;
            g.setColour(swipeCol.withAlpha(0.2f));
            g.fillRect(sx, sy, ex - sx, (float)metrics::laneHeight);
            g.setColour(swipeCol.withAlpha(0.7f));
            g.drawRect(sx, sy, ex - sx, (float)metrics::laneHeight, 2.0f);
        }
    }

    // ── Comp selected segment highlights on take lanes ──
    if (compActive && activeComp != nullptr)
    {
        for (auto& seg : activeComp->segments)
        {
            if (seg.laneIndex < 0 || seg.laneIndex >= (int)processor.lanes.size()) continue;
            float sx = beatToX(seg.startBeat);
            float ex = beatToX(seg.endBeat);
            float sy = laneToY(seg.laneIndex);

            // Subtle highlight bar at bottom of selected region
            juce::Colour segCol = processor.lanes[static_cast<size_t>(seg.laneIndex)].colour;
            g.setColour(segCol.withAlpha(0.12f));
            g.fillRect(sx, sy, ex - sx, (float)metrics::laneHeight);
            g.setColour(segCol.withAlpha(0.5f));
            g.fillRect(sx, sy + (float)metrics::laneHeight - 3.0f, ex - sx, 3.0f);
        }
    }
}

void ArrangementView::paintMasterLane(juce::Graphics& g)
{
    float y = masterLaneY();
    float lH = (float)metrics::laneHeight;

    // Master lane header
    g.setColour(colours::bgLight().brighter(0.05f));
    g.fillRect(0.0f, y, (float)metrics::laneHeaderW, lH);

    // Accent strip
    g.setColour(juce::Colour(0xffaaaaaa));
    g.fillRoundedRectangle(2.0f, y + 4.0f, 4.0f, lH - 8.0f, 2.0f);

    // Label
    g.setColour(colours::textBright());
    g.setFont(12.0f);
    bool compOn = processor.compEnabled.load();
    g.drawText(compOn ? "COMP" : "MASTER", 10, (int)y, metrics::laneHeaderW - 14,
               (int)(lH * 0.6f), juce::Justification::centredLeft);

    // Drag hint when there are notes
    if (!processor.masterClip.notes.empty())
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

    // Master clip content area
    auto& mc = processor.masterClip;
    if (mc.notes.empty()) return;

    int totalBeats = processor.arrangementBars.load() * 4;
    float clipX = beatToX(0.0);
    float clipEndX = beatToX((double)totalBeats);
    float clipW = clipEndX - clipX;
    float clipY = y + 2.0f;
    float clipH = lH - 4.0f;

    // Subtle background for the clip area
    g.setColour(juce::Colour(0xff888888).withAlpha(0.15f));
    g.fillRect(clipX, clipY, clipW, clipH);

    // Draw note preview
    int minNote = 127, maxNote = 0;
    for (auto& n : mc.notes) { minNote = std::min(minNote, n.noteNumber); maxNote = std::max(maxNote, n.noteNumber); }
    int noteRange = std::max(1, maxNote - minNote + 1);

    g.setColour(juce::Colours::white.withAlpha(0.45f));
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

void ArrangementView::paintCompLane(juce::Graphics& g)
{
    // The comp lane is painted over the master lane area, showing colored
    // segments from the active comp that indicate which lane is selected
    // at each point in time.

    if (processor.comps.empty()) return;
    const auto& comp = processor.getActiveComp();
    if (comp.segments.empty()) return;

    float y = masterLaneY();
    float lH = (float)metrics::laneHeight;
    float contentY = y + 2.0f;
    float contentH = lH - 4.0f;

    juce::ScopedLock sl(processor.laneLock);

    for (auto& seg : comp.segments)
    {
        float sx = beatToX(seg.startBeat);
        float ex = beatToX(seg.endBeat);
        if (ex < metrics::laneHeaderW || sx > getWidth()) continue;

        // Get the lane colour for this segment
        juce::Colour segCol(0xff888888);
        if (seg.laneIndex >= 0 && seg.laneIndex < (int)processor.lanes.size())
            segCol = processor.lanes[static_cast<size_t>(seg.laneIndex)].colour;

        // Colored block
        g.setColour(segCol.withAlpha(0.55f));
        g.fillRect(sx, contentY, ex - sx, contentH);

        // Top accent strip
        g.setColour(segCol.withAlpha(0.85f));
        g.fillRect(sx, contentY, ex - sx, 3.0f);

        // Lane label inside block
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(9.0f);
        juce::String label = "L" + juce::String(seg.laneIndex + 1);
        if (seg.laneIndex < (int)processor.lanes.size())
            label = processor.lanes[static_cast<size_t>(seg.laneIndex)].name;
        if ((ex - sx) > 30.0f)
            g.drawText(label, juce::Rectangle<float>(sx + 4, contentY + 4, ex - sx - 8, 12.0f),
                       juce::Justification::centredLeft, true);
    }

    // Transition markers between adjacent segments
    for (size_t i = 1; i < comp.segments.size(); ++i)
    {
        auto& prev = comp.segments[i - 1];
        auto& cur  = comp.segments[i];

        // Draw marker if segments are adjacent (or very close)
        if (std::abs(cur.startBeat - prev.endBeat) < 0.01)
        {
            float mx = beatToX(cur.startBeat);
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.drawVerticalLine((int)mx, contentY, contentY + contentH);
        }
    }

    // Draw active swipe preview
    if (compSwiping && compSwipeLane >= 0)
    {
        double sStart = std::min(compSwipeStartBeat, compSwipeEndBeat);
        double sEnd   = std::max(compSwipeStartBeat, compSwipeEndBeat);
        float sx = beatToX(sStart);
        float ex = beatToX(sEnd);

        juce::Colour swipeCol(0xff888888);
        if (compSwipeLane < (int)processor.lanes.size())
            swipeCol = processor.lanes[static_cast<size_t>(compSwipeLane)].colour;

        g.setColour(swipeCol.withAlpha(0.35f));
        g.fillRect(sx, contentY, ex - sx, contentH);
        g.setColour(swipeCol.withAlpha(0.8f));
        g.drawRect(sx, contentY, ex - sx, contentH, 1.5f);
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

    float x = beatToX(beat);
    auto phCol = colours::playhead();

    // Vertical line through entire height
    g.setColour(phCol.withAlpha(0.8f));
    g.drawVerticalLine((int)x, 0.0f, (float)getHeight());

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
    float h = (float)metrics::laneHeight;
    g.setColour(colours::accent().withAlpha(0.3f));
    g.fillRect(x, y, 60.0f, h);
    g.setColour(colours::accent());
    g.drawRect(x, y, 60.0f, h, 1.0f);
}

// ── Mouse ────────────────────────────────────────────────────────────────────

void ArrangementView::mouseMove(const juce::MouseEvent& e)
{
    if (e.position.y < rulerH && e.position.x >= metrics::laneHeaderW)
    {
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
            if (e.position.x > lx + 12 && e.position.x < rx - 12)
            {
                setMouseCursor(juce::MouseCursor::DraggingHandCursor);
                return;
            }
        }
        setMouseCursor(juce::MouseCursor::IBeamCursor);
        return;
    }
    // Master lane
    {
        float my = masterLaneY();
        float lH = (float)metrics::laneHeight;
        if (e.position.y >= my && e.position.y < my + lH
            && e.position.x >= metrics::laneHeaderW
            && !processor.masterClip.notes.empty())
        {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            return;
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

    selLane = -1; selClip = -1; draggingClip = false;
    hasTimeSelection = false;
    draggingMasterClip = false; masterDragInitiated = false;

    // Master lane click: prepare for drag-to-DAW
    {
        float my = masterLaneY();
        float lH = (float)metrics::laneHeight;
        if (e.position.y >= my && e.position.y < my + lH
            && e.position.x >= metrics::laneHeaderW
            && !processor.masterClip.notes.empty())
        {
            draggingMasterClip = true;
            masterDragStartPos = e.position;
            return;
        }
    }

    // Check lane header clicks (M/S buttons, context menu, "+" area)
    if (e.position.x < metrics::laneHeaderW)
    {
        juce::ScopedLock sl(processor.laneLock);
        int numLanes = (int)processor.lanes.size();

        // "+" add-lane area
        float addY = laneToY(numLanes);
        if (e.position.y >= addY && e.position.y < addY + metrics::laneHeight)
        {
            if (onAddLaneClicked) onAddLaneClicked();
            repaint();
            return;
        }

        // Check M/S buttons on each lane
        for (int i = 0; i < numLanes; ++i)
        {
            float y = laneToY(i);
            float btnY = y + (float)metrics::laneHeight * 0.55f;
            float btnW = 18.0f, btnH = 14.0f;

            auto muteRect = juce::Rectangle<float>(10.0f, btnY, btnW, btnH);
            auto soloRect = juce::Rectangle<float>(32.0f, btnY, btnW, btnH);

            if (muteRect.contains(e.position))
            {
                processor.lanes[static_cast<size_t>(i)].muted = !processor.lanes[static_cast<size_t>(i)].muted;
                repaint();
                return;
            }
            if (soloRect.contains(e.position))
            {
                processor.lanes[static_cast<size_t>(i)].solo = !processor.lanes[static_cast<size_t>(i)].solo;
                repaint();
                return;
            }

            // Right-click on lane header -> context menu
            if (e.mods.isRightButtonDown() &&
                e.position.y >= y && e.position.y < y + metrics::laneHeight)
            {
                showLaneContextMenu(i);
                return;
            }

            // Left-click on lane header -> select the lane
            if (!e.mods.isRightButtonDown() &&
                e.position.y >= y && e.position.y < y + metrics::laneHeight)
            {
                selLane = i;
                selClip = -1;
                repaint();
                return;
            }
        }
    }

    // ── Comp swipe: when comp enabled, clicking on a lane's content area starts a swipe ──
    if (processor.compEnabled.load() && e.position.x >= metrics::laneHeaderW && !e.mods.isRightButtonDown())
    {
        int lane = yToLane(e.position.y);
        juce::ScopedLock sl(processor.laneLock);
        if (lane >= 0 && lane < (int)processor.lanes.size())
        {
            double beat = std::max(0.0, xToBeat(e.position.x));
            beat = processor.snapBeat(beat);
            compSwiping = true;
            compSwipeLane = lane;
            compSwipeStartBeat = beat;
            compSwipeEndBeat = beat;
            repaint();
            return;
        }
    }

    for (auto& cb : clipBlocks)
    {
        if (cb.bounds.contains(e.position))
        {
            selLane = cb.laneIndex; selClip = cb.clipIndex;
            if (e.mods.isRightButtonDown())
                showClipContextMenu(cb.laneIndex, cb.clipIndex);
            else
            {
                draggingClip = true;
                clipDragOrigLane = cb.laneIndex;
                clipDragMouseYOffset = e.position.y - laneToY(cb.laneIndex);
                juce::ScopedLock sl(processor.laneLock);
                clipDragOrigBeat = processor.lanes[static_cast<size_t>(cb.laneIndex)].clipStarts[static_cast<size_t>(cb.clipIndex)];
                clipDragMouseOffset = xToBeat(e.position.x) - clipDragOrigBeat;
            }
            break;
        }
    }
    repaint();
}

void ArrangementView::mouseDoubleClick(const juce::MouseEvent& e)
{
    for (auto& cb : clipBlocks)
    {
        if (cb.bounds.contains(e.position))
        {
            juce::ScopedLock sl(processor.laneLock);
            if (cb.laneIndex < (int)processor.lanes.size())
            {
                auto& lane = processor.lanes[static_cast<size_t>(cb.laneIndex)];
                if (cb.clipIndex < (int)lane.clips.size())
                {
                    auto& clip = lane.clips[static_cast<size_t>(cb.clipIndex)];
                    if (onClipDoubleClicked)
                        onClipDoubleClicked(clip, cb.laneIndex, cb.clipIndex);
                }
            }
            return;
        }
    }

    // Double-click on empty space -> close the piano roll
    if (onClipDoubleClicked)
        onClipDoubleClicked(MidiClip{}, -1, -1);
}

void ArrangementView::mouseDrag(const juce::MouseEvent& e)
{
    // Comp swipe drag
    if (compSwiping)
    {
        double beat = std::max(0.0, xToBeat(e.position.x));
        beat = processor.snapBeat(beat);
        compSwipeEndBeat = beat;
        repaint();
        return;
    }

    // Master clip drag-to-DAW
    if (draggingMasterClip && !masterDragInitiated)
    {
        auto dist = e.position.getDistanceFrom(masterDragStartPos);
        if (dist > 5.0f)
        {
            masterDragInitiated = true;
            draggingMasterClip = false;

            // Write master clip to a temp .mid file
            auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
            auto tempFile = tempDir.getChildFile("PatternFlow-Master.mid");

            double bpm = processor.hostBpm.load();
            if (bpm <= 0.0) bpm = 120.0;

            bool written = false;
            {
                juce::ScopedLock sl(processor.laneLock);
                written = writeMidiFile(processor.masterClip, tempFile, bpm);
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
        if (loopDragging == LoopDragTarget::Start)
            processor.loopStartBeat.store(std::min(beat, processor.loopEndBeat.load() - 1.0));
        else if (loopDragging == LoopDragTarget::End)
            processor.loopEndBeat.store(std::max(beat, processor.loopStartBeat.load() + 1.0));
        else if (loopDragging == LoopDragTarget::Body)
        {
            double newStart = std::max(0.0, processor.snapBeat(beat - loopDragBodyOffset));
            processor.loopStartBeat.store(newStart);
            processor.loopEndBeat.store(newStart + loopDragBodyLength);
        }
        repaint();
        return;
    }

    if (draggingClip && selLane >= 0 && selClip >= 0)
    {
        double newBeat = std::max(0.0, xToBeat(e.position.x) - clipDragMouseOffset);
        newBeat = processor.snapBeat(newBeat);
        juce::ScopedLock sl(processor.laneLock);
        if (selLane < (int)processor.lanes.size() && selClip < (int)processor.lanes[static_cast<size_t>(selLane)].clipStarts.size())
        {
            processor.lanes[static_cast<size_t>(selLane)].clipStarts[static_cast<size_t>(selClip)] = newBeat;
        }
        hoveredLane = yToLane(e.position.y);
        repaint();
    }
}

void ArrangementView::mouseUp(const juce::MouseEvent& e)
{
    // Finalise comp swipe
    if (compSwiping)
    {
        compSwiping = false;
        double sStart = std::min(compSwipeStartBeat, compSwipeEndBeat);
        double sEnd   = std::max(compSwipeStartBeat, compSwipeEndBeat);

        // Minimum swipe distance: at least one grid unit
        if (sEnd - sStart < 0.01)
        {
            // Single click: select a 1-beat range (or snap grid unit)
            sEnd = sStart + 1.0;
            sEnd = processor.snapBeat(sEnd);
            if (sEnd <= sStart) sEnd = sStart + 1.0;
        }

        if (compSwipeLane >= 0 && sEnd > sStart)
        {
            processor.compSwipe(compSwipeLane, sStart, sEnd);
            processor.rebuildMasterClip();
        }
        compSwipeLane = -1;
        refresh();
        return;
    }

    if (draggingPlayhead) { draggingPlayhead = false; return; }
    if (draggingMasterClip) { draggingMasterClip = false; masterDragInitiated = false; return; }

    if (draggingClip && selLane >= 0 && selClip >= 0)
    {
        int targetLane = yToLane(e.position.y);
        double newBeatPos;
        bool laneChanged, beatChanged;
        int numLanes;
        {
            juce::ScopedLock sl(processor.laneLock);
            numLanes = (int)processor.lanes.size();
            if (selLane < numLanes && selClip < (int)processor.lanes[static_cast<size_t>(selLane)].clipStarts.size())
            {
                newBeatPos = processor.lanes[static_cast<size_t>(selLane)].clipStarts[static_cast<size_t>(selClip)];
                laneChanged = (targetLane != clipDragOrigLane);
                beatChanged = std::abs(newBeatPos - clipDragOrigBeat) > 0.001;

                // Revert to original position — the undo action will apply the new one
                processor.lanes[static_cast<size_t>(selLane)].clipStarts[static_cast<size_t>(selClip)] = clipDragOrigBeat;
            }
            else
            {
                laneChanged = false;
                beatChanged = false;
                newBeatPos = clipDragOrigBeat;
            }
        }
        if (laneChanged)
        {
            bool createNew = (targetLane >= numLanes);
            int dstLane = createNew ? numLanes : targetLane;
            processor.undoManager.perform(
                new MoveClipToLaneAction(processor, selLane, selClip,
                                         dstLane, newBeatPos, createNew));
            selLane = dstLane;
            {
                juce::ScopedLock sl(processor.laneLock);
                if (dstLane < (int)processor.lanes.size())
                    selClip = (int)processor.lanes[static_cast<size_t>(dstLane)].clips.size() - 1;
            }
        }
        else if (beatChanged)
        {
            processor.undoManager.perform(
                new MoveClipAction(processor, selLane, selClip,
                                   clipDragOrigBeat, newBeatPos));
        }
    }
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
        float maxScroll = std::max(0.0f, (float)((numLanes + 1) * metrics::laneHeight) - (float)(getHeight() - rulerH));
        verticalScrollOffset = juce::jlimit(0.0f, maxScroll, verticalScrollOffset - wheel.deltaY * 40.0f);
        refresh();
    }
}

void ArrangementView::resized()
{
    btnAddBars.setBounds(getWidth() - 70, 2, 66, rulerH - 4);
    btnLoop.setBounds(getWidth() - 140, 2, 46, rulerH - 4);
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

        if (lane >= numLanes)
            addClipToNewLane(clip, beat);
        else
            addClipToLane(clip, lane, beat);
    }
    refresh();
}

void ArrangementView::ensureBarsForBeat(double endBeat)
{
    int neededBars = (int)std::ceil(endBeat / 4.0);
    int newBars = ((neededBars + 3) / 4) * 4;
    int expected = processor.arrangementBars.load();
    while (newBars > expected)
    {
        if (processor.arrangementBars.compare_exchange_weak(expected, newBars))
            break;
    }
}

void ArrangementView::addClipToLane(const MidiClip& clip, int laneIndex, double beatPos)
{
    if (laneIndex >= 0 && laneIndex < (int)processor.lanes.size())
    {
        auto presets = getClipColourPresets();
        MidiClip colouredClip = clip;
        colouredClip.colour = presets[processor.lanes[static_cast<size_t>(laneIndex)].clips.size() % presets.size()];
        processor.lanes[static_cast<size_t>(laneIndex)].addClip(colouredClip, beatPos);
        ensureBarsForBeat(beatPos + clip.lengthBeats);
    }
}

void ArrangementView::addClipToNewLane(const MidiClip& clip, double beatPos)
{
    CompLane newLane;
    auto presets = getClipColourPresets();
    int idx = (int)processor.lanes.size();
    newLane.name   = "Lane " + juce::String(idx + 1);
    newLane.colour = presets[static_cast<size_t>(idx) % presets.size()];
    MidiClip colouredClip = clip;
    colouredClip.colour = newLane.colour;
    newLane.addClip(colouredClip, beatPos);
    processor.lanes.push_back(newLane);
    ensureBarsForBeat(beatPos + clip.lengthBeats);
}

void ArrangementView::showClipContextMenu(int laneIdx, int clipIdx)
{
    juce::PopupMenu menu;
    juce::PopupMenu colourMenu;
    auto presets = getClipColourPresets();
    for (int i = 0; i < (int)presets.size(); ++i)
        colourMenu.addItem(100 + i, "Colour " + juce::String(i + 1));
    menu.addSubMenu("Clip Colour", colourMenu);

    juce::PopupMenu laneColourMenu;
    for (int i = 0; i < (int)presets.size(); ++i)
        laneColourMenu.addItem(200 + i, "Colour " + juce::String(i + 1));
    menu.addSubMenu("Lane Colour", laneColourMenu);
    menu.addSeparator();

    menu.addItem(4, "Duplicate Clip");
    menu.addSeparator();
    menu.addItem(3, "Delete Clip");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, laneIdx, clipIdx, presets](int result)
    {
        if (result == 0) return;

        if (result >= 100 && result < 200)
        {
            juce::ScopedLock sl(processor.laneLock);
            if (laneIdx >= (int)processor.lanes.size()) return;
            auto& lane = processor.lanes[static_cast<size_t>(laneIdx)];
            if (clipIdx >= (int)lane.clips.size()) return;
            lane.clips[static_cast<size_t>(clipIdx)].colour = presets[static_cast<size_t>(result - 100)];
        }
        else if (result >= 200 && result < 300)
        {
            juce::ScopedLock sl(processor.laneLock);
            if (laneIdx >= (int)processor.lanes.size()) return;
            processor.lanes[static_cast<size_t>(laneIdx)].colour = presets[static_cast<size_t>(result - 200)];
        }
        else if (result == 4)
        {
            processor.undoManager.perform(new DuplicateClipAction(processor, laneIdx, clipIdx));
        }
        else if (result == 3)
        {
            processor.undoManager.perform(new RemoveClipAction(processor, laneIdx, clipIdx));
        }
        refresh();
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
    updateLoopButton();
    refresh();
}

void ArrangementView::setLoopToSelectedClip()
{
    if (selLane < 0 || selClip < 0) return;

    juce::ScopedLock sl(processor.laneLock);
    if (selLane >= (int)processor.lanes.size()) return;
    auto& lane = processor.lanes[static_cast<size_t>(selLane)];
    if (selClip >= (int)lane.clips.size()) return;

    double clipStart = lane.clipStarts[static_cast<size_t>(selClip)];
    double clipEnd = clipStart + lane.clips[static_cast<size_t>(selClip)].lengthBeats;
    processor.loopStartBeat.store(clipStart);
    processor.loopEndBeat.store(clipEnd);
    processor.loopEnabled.store(true);
    processor.editPlayheadBeat.store(clipStart);
    updateLoopButton();
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
            updateLoopButton();
            refresh();
            return;
        }

        setLoopToSelection();
        return;
    }

    // If a clip is selected, set loop to that clip
    if (selLane >= 0 && selClip >= 0)
    {
        juce::ScopedLock sl(processor.laneLock);
        if (selLane < (int)processor.lanes.size())
        {
            auto& lane = processor.lanes[static_cast<size_t>(selLane)];
            if (selClip < (int)lane.clips.size())
            {
                double clipStart = lane.clipStarts[static_cast<size_t>(selClip)];
                double clipEnd = clipStart + lane.clips[static_cast<size_t>(selClip)].lengthBeats;
                double ls = processor.loopStartBeat.load();
                double le = processor.loopEndBeat.load();
                bool loopOn = processor.loopEnabled.load();

                if (loopOn &&
                    std::abs(ls - clipStart) < 0.01 &&
                    std::abs(le - clipEnd) < 0.01)
                {
                    processor.loopEnabled.store(false);
                    updateLoopButton();
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
    updateLoopButton();
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
        juce::ScopedLock sl(processor.laneLock);

        // If a clip is selected, delete it
        if (selLane >= 0 && selClip >= 0
            && selLane < (int)processor.lanes.size()
            && selClip < (int)processor.lanes[static_cast<size_t>(selLane)].clips.size())
        {
            processor.lanes[static_cast<size_t>(selLane)].removeClip(selClip);
            selClip = -1;
            refresh();
            return true;
        }

        // If a lane is selected (but no clip), delete the lane
        if (selLane >= 0 && selClip < 0
            && selLane < (int)processor.lanes.size())
        {
            processor.lanes.erase(processor.lanes.begin() + selLane);
            selLane = -1;
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
        else if (result == 10) processor.lanes.erase(processor.lanes.begin() + laneIdx);
        repaint();
    });
}

} // namespace pflow
