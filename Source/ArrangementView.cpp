#include "ArrangementView.h"
#include "PluginProcessor.h"

namespace pflow {

ArrangementView::ArrangementView(PatternFlowProcessor& proc) : processor(proc)
{
    setOpaque(true);

    btnAddBars.setColour(juce::TextButton::buttonColourId, colours::bgLighter);
    btnAddBars.setColour(juce::TextButton::textColourOffId, colours::text);
    btnAddBars.onClick = [this]
    {
        int expected = processor.arrangementBars.load();
        while (!processor.arrangementBars.compare_exchange_weak(expected, expected + 4)) {}
        refresh();
    };
    addAndMakeVisible(btnAddBars);
}

void ArrangementView::refresh()
{
    rebuildClipBlocks();
    repaint();
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
    return std::max(0, (int)((y + verticalScrollOffset - rulerH) / metrics::laneHeight));
}

float ArrangementView::laneToY(int lane) const
{
    return (float)(lane * metrics::laneHeight) + rulerH - verticalScrollOffset;
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
}

void ArrangementView::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg);
    rebuildClipBlocks();
    paintBeatGrid(g);
    paintRuler(g);
    paintLoopMarkers(g);
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
            g.setColour(colours::textDim);
            g.setFont(14.0f);
            auto area = getLocalBounds().withTrimmedTop(rulerH).withTrimmedLeft(metrics::laneHeaderW);
            g.drawText("Drag MIDI files here or click + to add a lane",
                       area, juce::Justification::centred);
        }
    }
}

void ArrangementView::paintRuler(juce::Graphics& g)
{
    g.setColour(colours::bgLight);
    g.fillRect(metrics::laneHeaderW, 0, getWidth() - metrics::laneHeaderW, rulerH);
    g.setColour(colours::panelBorder);
    g.drawHorizontalLine(rulerH - 1, (float)metrics::laneHeaderW, (float)getWidth());

    int totalBeats = processor.arrangementBars.load() * 4;
    for (double beat = 0; beat <= totalBeats; beat += 4.0)
    {
        float x = beatToX(beat);
        if (x < metrics::laneHeaderW || x > getWidth()) continue;
        g.setColour(colours::textDim);
        g.setFont(12.0f);
        int barNum = (int)(beat / 4.0) + 1;
        g.drawText(juce::String(barNum), (int)x + 2, 0, 30, rulerH - 2,
                   juce::Justification::centredLeft);
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

        if (isBar)        g.setColour(colours::panelBorder.withAlpha(0.5f));
        else if (isBeat)  g.setColour(colours::panelBorder.withAlpha(0.2f));
        else              g.setColour(colours::panelBorder.withAlpha(0.08f));

        g.drawVerticalLine((int)x, (float)rulerH, (float)getHeight());
    }
}

void ArrangementView::paintLoopMarkers(juce::Graphics& g)
{
    if (!processor.loopEnabled.load()) return;

    float lx = beatToX(processor.loopStartBeat.load());
    float rx = beatToX(processor.loopEndBeat.load());

    g.setColour(colours::accent.withAlpha(0.06f));
    g.fillRect(lx, (float)rulerH, rx - lx, (float)(getHeight() - rulerH));

    g.setColour(colours::accent);
    g.fillRect(lx - 1, 0.0f, 3.0f, (float)rulerH);
    g.fillRect(rx - 1, 0.0f, 3.0f, (float)rulerH);

    juce::Path leftTri;
    leftTri.addTriangle(lx, 0.0f, lx + 8.0f, 0.0f, lx, (float)rulerH * 0.6f);
    g.fillPath(leftTri);

    juce::Path rightTri;
    rightTri.addTriangle(rx, 0.0f, rx - 8.0f, 0.0f, rx, (float)rulerH * 0.6f);
    g.fillPath(rightTri);

    g.setColour(colours::accent.withAlpha(0.4f));
    g.drawVerticalLine((int)lx, (float)rulerH, (float)getHeight());
    g.drawVerticalLine((int)rx, (float)rulerH, (float)getHeight());
}

void ArrangementView::paintLaneHeaders(juce::Graphics& g)
{
    juce::ScopedLock sl(processor.laneLock);

    for (int i = 0; i < (int)processor.lanes.size(); ++i)
    {
        float y = laneToY(i);
        auto& lane = processor.lanes[i];

        g.setColour(colours::bgLight);
        g.fillRect(0.0f, y, (float)metrics::laneHeaderW, (float)metrics::laneHeight);

        g.setColour(lane.colour);
        g.fillRect(0.0f, y + 2.0f, 4.0f, (float)metrics::laneHeight - 4.0f);

        g.setColour(colours::text);
        g.setFont(13.0f);
        g.drawText(lane.name, 10, (int)y, metrics::laneHeaderW - 14,
                   metrics::laneHeight, juce::Justification::centredLeft);

        g.setColour(colours::panelBorder);
        g.drawHorizontalLine((int)(y + metrics::laneHeight - 1), 0.0f, (float)getWidth());
    }

    float addY = laneToY((int)processor.lanes.size());
    g.setColour(colours::bgLight.withAlpha(0.3f));
    g.fillRect(0.0f, addY, (float)metrics::laneHeaderW, (float)metrics::laneHeight);
    g.setColour(colours::textDim);
    g.setFont(20.0f);
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
        g.setColour(region.muted ? clipColour.withAlpha(0.2f) : clipColour.withAlpha(0.7f));
        g.fillRoundedRectangle(cb.bounds, metrics::clipCorner);

        if (!clip.notes.empty() && clip.lengthBeats > 0.0)
        {
            float clipW = cb.bounds.getWidth();
            float clipH = cb.bounds.getHeight();
            float clipX = cb.bounds.getX();
            float clipY = cb.bounds.getY();

            int minNote = 127, maxNote = 0;
            for (auto& n : clip.notes) { minNote = std::min(minNote, n.noteNumber); maxNote = std::max(maxNote, n.noteNumber); }
            int noteRange = std::max(1, maxNote - minNote + 1);

            g.setColour(juce::Colours::white.withAlpha(0.4f));
            for (auto& n : clip.notes)
            {
                float nx = clipX + (float)(n.startBeat / clip.lengthBeats) * clipW;
                float nw = std::max(1.0f, (float)(n.lengthBeats / clip.lengthBeats) * clipW);
                float ny = clipY + clipH - ((float)(n.noteNumber - minNote + 1) / noteRange) * (clipH - 4.0f) - 2.0f;
                float nh = std::max(1.0f, (clipH - 4.0f) / noteRange);
                g.fillRect(nx, ny, nw, nh);
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
            g.setColour(colours::accentBright);
            g.drawRoundedRectangle(cb.bounds, metrics::clipCorner, 2.0f);
        }

        // Use contrast-aware text color for clip names
        auto textCol = clipColour.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white;
        g.setColour(textCol);
        g.setFont(11.0f);
        g.drawText(clip.name, cb.bounds.reduced(4.0f, 2.0f), juce::Justification::topLeft, true);
    }
}

void ArrangementView::paintPlayhead(juce::Graphics& g)
{
    if (processor.hostPlaying.load())
    {
        float x = beatToX(processor.hostBeatPos.load());
        g.setColour(colours::playhead.withAlpha(0.8f));
        g.drawVerticalLine((int)x, 0.0f, (float)getHeight());
    }
}

void ArrangementView::paintDropIndicator(juce::Graphics& g)
{
    float x = beatToX(dropBeatPos);
    float y = (dropLaneIdx >= 0) ? laneToY(dropLaneIdx) : (float)rulerH;
    float h = (float)metrics::laneHeight;
    g.setColour(colours::accent.withAlpha(0.3f));
    g.fillRect(x, y, 60.0f, h);
    g.setColour(colours::accent);
    g.drawRect(x, y, 60.0f, h, 1.0f);
}

// ── Mouse ────────────────────────────────────────────────────────────────────

void ArrangementView::mouseMove(const juce::MouseEvent& e)
{
    if (processor.loopEnabled.load() && e.position.y < rulerH)
    {
        float lx = beatToX(processor.loopStartBeat.load());
        float rx = beatToX(processor.loopEndBeat.load());
        if (std::abs(e.position.x - lx) < 12 || std::abs(e.position.x - rx) < 12)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void ArrangementView::mouseDown(const juce::MouseEvent& e)
{
    if (processor.loopEnabled.load() && e.position.y < rulerH)
    {
        float lx = beatToX(processor.loopStartBeat.load());
        float rx = beatToX(processor.loopEndBeat.load());
        if (std::abs(e.position.x - lx) < 12) { loopDragging = LoopDragTarget::Start; return; }
        if (std::abs(e.position.x - rx) < 12) { loopDragging = LoopDragTarget::End; return; }
    }

    selLane = -1; selRegion = -1; draggingClip = false;

    // Check if "+" add-lane area was clicked
    {
        juce::ScopedLock sl(processor.laneLock);
        int numLanes = (int)processor.lanes.size();
        float addY = laneToY(numLanes);
        if (e.position.x < metrics::laneHeaderW && e.position.y >= addY && e.position.y < addY + metrics::laneHeight)
        {
            if (onAddLaneClicked) onAddLaneClicked();
            repaint();
            return;
        }
    }

    for (auto& cb : clipBlocks)
    {
        if (cb.bounds.contains(e.position))
        {
            selLane = cb.laneIndex; selRegion = cb.regionIndex;
            if (e.mods.isRightButtonDown())
                showClipContextMenu(cb.laneIndex, cb.regionIndex);
            else
            {
                draggingClip = true;
                juce::ScopedLock sl(processor.laneLock);
                clipDragOrigBeat = processor.lanes[cb.laneIndex].regions[cb.regionIndex].startBeat;
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
    if (loopDragging != LoopDragTarget::None)
    {
        double beat = std::max(0.0, xToBeat(e.position.x));
        beat = processor.snapBeat(beat);
        if (loopDragging == LoopDragTarget::Start)
            processor.loopStartBeat.store(std::min(beat, processor.loopEndBeat.load() - 1.0));
        else
            processor.loopEndBeat.store(std::max(beat, processor.loopStartBeat.load() + 1.0));
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
        repaint();
    }
}

void ArrangementView::mouseUp(const juce::MouseEvent&)
{
    loopDragging = LoopDragTarget::None;
    draggingClip = false;
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
        else if (result == 1) { lane.regions[regionIdx].muted = !lane.regions[regionIdx].muted; }
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
        else if (result == 3) lane.removeRegion(regionIdx);
        repaint();
    });
}

} // namespace pflow
