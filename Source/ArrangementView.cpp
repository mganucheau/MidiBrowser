#include "ArrangementView.h"
#include "PluginProcessor.h"
#include "UndoActions.h"

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
        auto& lane = processor.lanes[li];
        for (int ri = 0; ri < (int)lane.regions.size(); ++ri)
        {
            auto& r = lane.regions[ri];
            float x1 = beatToX(r.startBeat);
            float x2 = beatToX(r.endBeat);
            float y  = laneToY(li);

            ClipBlock cb;
            cb.bounds      = { x1, y + 2.0f, x2 - x1, (float)metrics::laneHeight - 4.0f };
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
    paintMasterLane(g);
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
        auto& lane = processor.lanes[i];

        // Lane header background
        g.setColour(colours::bgLight());
        g.fillRect(0.0f, y, (float)metrics::laneHeaderW, (float)metrics::laneHeight);

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

    for (auto& cb : clipBlocks)
    {
        auto& lane = processor.lanes[cb.laneIndex];
        auto& region = lane.regions[cb.regionIndex];
        auto& clip = lane.clips[region.clipIndex];
        bool selected = (cb.laneIndex == selLane && cb.regionIndex == selRegion);

        auto clipColour = clip.colour;

        // Clip body with subtle gradient-like top highlight
        g.setColour(region.muted ? clipColour.withAlpha(0.15f) : clipColour.withAlpha(0.65f));
        g.fillRoundedRectangle(cb.bounds, metrics::clipCorner);
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
            int loopCount = std::max(1, (int)std::ceil(regionLen / loopLen));

            g.setColour(juce::Colours::white.withAlpha(0.4f));
            for (int loop = 0; loop < loopCount; ++loop)
            {
                double loopOffset = loop * loopLen;
                for (auto& n : clip.notes)
                {
                    double noteBeat = loopOffset + n.startBeat;
                    if (noteBeat >= regionLen) continue;
                    float nx = clipX + (float)(noteBeat / regionLen) * clipW;
                    float nw = std::max(1.0f, (float)(n.lengthBeats / regionLen) * clipW);
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

            // Edge drag handles
            float handleW = 5.0f;
            float handleH = cb.bounds.getHeight() * 0.4f;
            float handleY = cb.bounds.getCentreY() - handleH * 0.5f;
            g.setColour(colours::textBright().withAlpha(0.7f));
            g.fillRoundedRectangle(cb.bounds.getX() - 1.0f, handleY, handleW, handleH, 2.0f);
            g.fillRoundedRectangle(cb.bounds.getRight() - handleW + 1.0f, handleY, handleW, handleH, 2.0f);
        }

        // Use contrast-aware text color for clip names
        auto textCol = clipColour.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white;
        g.setColour(textCol);
        g.setFont(11.0f);
        g.drawText(clip.name, cb.bounds.reduced(4.0f, 2.0f), juce::Justification::topLeft, true);
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
    g.drawText("MASTER", 10, (int)y, metrics::laneHeaderW - 14,
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

bool ArrangementView::isNearRegionEdge(const juce::MouseEvent& e, const ClipBlock& cb, EdgeDragTarget& which) const
{
    constexpr float edgeThreshold = 8.0f;
    if (std::abs(e.position.x - cb.bounds.getX()) < edgeThreshold &&
        e.position.y >= cb.bounds.getY() && e.position.y <= cb.bounds.getBottom())
    { which = EdgeDragTarget::Start; return true; }
    if (std::abs(e.position.x - cb.bounds.getRight()) < edgeThreshold &&
        e.position.y >= cb.bounds.getY() && e.position.y <= cb.bounds.getBottom())
    { which = EdgeDragTarget::End; return true; }
    return false;
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
    // Master lane: show drag cursor when hoverable
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
    // Check for region edges
    for (auto& cb : clipBlocks)
    {
        EdgeDragTarget which;
        if (isNearRegionEdge(e, cb, which))
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
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

    selLane = -1; selRegion = -1; draggingClip = false; edgeDragging = EdgeDragTarget::None;
    hasTimeSelection = false; // Clear ruler selection when clicking in arrangement
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
                e.position.y >= y && e.position.y < y + metrics::laneHeight)
            {
                showLaneContextMenu(i);
                return;
            }
        }
    }

    for (auto& cb : clipBlocks)
    {
        // Check for edge drag first (loop extend)
        EdgeDragTarget which;
        if (isNearRegionEdge(e, cb, which))
        {
            selLane = cb.laneIndex; selRegion = cb.regionIndex;
            edgeDragging = which;
            edgeDragLane = cb.laneIndex;
            edgeDragRegion = cb.regionIndex;
            juce::ScopedLock sl(processor.laneLock);
            auto& reg = processor.lanes[cb.laneIndex].regions[cb.regionIndex];
            edgeDragOrigStart = reg.startBeat;
            edgeDragOrigEnd = reg.endBeat;
            repaint();
            return;
        }

        if (cb.bounds.contains(e.position))
        {
            selLane = cb.laneIndex; selRegion = cb.regionIndex;
            if (e.mods.isRightButtonDown())
                showClipContextMenu(cb.laneIndex, cb.regionIndex);
            else
            {
                draggingClip = true;
                clipDragOrigLane = cb.laneIndex;
                clipDragMouseYOffset = e.position.y - laneToY(cb.laneIndex);
                juce::ScopedLock sl(processor.laneLock);
                auto& reg = processor.lanes[cb.laneIndex].regions[cb.regionIndex];
                clipDragOrigBeat = reg.startBeat;
                clipDragOrigEnd = reg.endBeat;
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
            auto& lane = processor.lanes[cb.laneIndex];
            auto& region = lane.regions[cb.regionIndex];
            auto& clip = lane.clips[region.clipIndex];
            if (onClipDoubleClicked)
                onClipDoubleClicked(clip, cb.laneIndex, cb.regionIndex);
            return;
        }
    }
}

void ArrangementView::mouseDrag(const juce::MouseEvent& e)
{
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

    // Region edge drag-to-loop
    if (edgeDragging != EdgeDragTarget::None && edgeDragLane >= 0 && edgeDragRegion >= 0)
    {
        double beat = std::max(0.0, xToBeat(e.position.x));
        beat = processor.snapBeat(beat);
        juce::ScopedLock sl(processor.laneLock);
        if (edgeDragLane < (int)processor.lanes.size() &&
            edgeDragRegion < (int)processor.lanes[edgeDragLane].regions.size())
        {
            auto& region = processor.lanes[edgeDragLane].regions[edgeDragRegion];
            if (edgeDragging == EdgeDragTarget::Start)
                region.startBeat = std::min(beat, region.endBeat - 0.25);
            else
                region.endBeat = std::max(beat, region.startBeat + 0.25);
        }
        repaint();
        return;
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
            region.startBeat = newBeat;
            region.endBeat   = newBeat + len;
        }
        // Track which lane the mouse is hovering over for visual feedback
        hoveredLane = yToLane(e.position.y);
        repaint();
    }
}

void ArrangementView::mouseUp(const juce::MouseEvent& e)
{
    if (draggingPlayhead) { draggingPlayhead = false; return; }
    if (draggingMasterClip) { draggingMasterClip = false; masterDragInitiated = false; return; }

    // Edge drag-to-loop undo
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
                processor.undoManager.perform(
                    new MoveRegionAction(processor, edgeDragLane, edgeDragRegion,
                                         edgeDragOrigStart, edgeDragOrigEnd,
                                         newStart, newEnd));
            }
        }
        edgeDragging = EdgeDragTarget::None;
        return;
    }

    if (draggingClip && selLane >= 0 && selRegion >= 0)
    {
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
                int dstLane = createNew ? numLanes : targetLane;
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
                processor.undoManager.perform(
                    new MoveRegionAction(processor, selLane, selRegion,
                                         clipDragOrigBeat, clipDragOrigEnd,
                                         newStart, newEnd));
            }
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
        colouredClip.colour = presets[processor.lanes[laneIndex].clips.size() % presets.size()];
        processor.lanes[laneIndex].addClipAtPosition(colouredClip, beatPos);
        ensureBarsForBeat(beatPos + clip.lengthBeats);
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
    ensureBarsForBeat(beatPos + clip.lengthBeats);
}

void ArrangementView::showClipContextMenu(int laneIdx, int regionIdx)
{
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
    menu.addItem(4, "Duplicate Region");
    menu.addItem(5, "Split at Playhead");
    menu.addSeparator();
    menu.addItem(3, "Delete Region");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, laneIdx, regionIdx, presets](int result)
    {
        if (result == 0) return;
        juce::ScopedLock sl(processor.laneLock);
        if (laneIdx >= (int)processor.lanes.size()) return;
        auto& lane = processor.lanes[laneIdx];
        if (regionIdx >= (int)lane.regions.size()) return;

        if (result >= 100 && result < 200)
        { auto& clip = lane.clips[lane.regions[regionIdx].clipIndex]; clip.colour = presets[result - 100]; }
        else if (result >= 200 && result < 300) lane.colour = presets[result - 200];
        else if (result == 1) { processor.undoManager.perform(new ToggleMuteAction(processor, laneIdx, regionIdx)); }
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
        else if (result == 4) { processor.undoManager.perform(new DuplicateRegionAction(processor, laneIdx, regionIdx)); }
        else if (result == 5)
        {
            double playBeat = processor.hostBeatPos.load();
            auto& reg = lane.regions[regionIdx];
            if (playBeat > reg.startBeat && playBeat < reg.endBeat)
                processor.undoManager.perform(new SplitRegionAction(processor, laneIdx, regionIdx, playBeat));
        }
        else if (result == 3) { processor.undoManager.perform(new RemoveRegionAction(processor, laneIdx, regionIdx)); }
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
    updateLoopButton();
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
