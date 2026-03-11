#include "ArrangementView.h"
#include "PluginProcessor.h"

namespace pflow {

ArrangementView::ArrangementView(PatternFlowProcessor& proc) : processor(proc)
{
    setOpaque(true);
}

void ArrangementView::refresh()
{
    rebuildClipBlocks();
    repaint();
}

// ── Coordinate helpers ───────────────────────────────────────────────────────

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
    int lane = (int)(y / metrics::laneHeight);
    return lane;
}

float ArrangementView::laneToY(int lane) const
{
    return (float)(lane * metrics::laneHeight);
}

// ── Build visual blocks ──────────────────────────────────────────────────────

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

// ── Paint ────────────────────────────────────────────────────────────────────

void ArrangementView::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg);

    rebuildClipBlocks();
    paintBeatGrid(g);
    paintLaneHeaders(g);
    paintClipBlocks(g);
    paintPlayhead(g);

    if (draggingOver)
        paintDropIndicator(g);
}

void ArrangementView::paintBeatGrid(juce::Graphics& g)
{
    g.setColour(colours::panelBorder.withAlpha(0.3f));
    float startX = (float)metrics::laneHeaderW;

    for (double beat = std::floor(scrollBeatOffset);
         beat < scrollBeatOffset + getWidth() * beatsPerPixel + 4;
         beat += 1.0)
    {
        float x = beatToX(beat);
        if (x < startX) continue;

        bool isBar = (std::fmod(beat, 4.0) < 0.01);
        g.setColour(isBar ? colours::panelBorder.withAlpha(0.5f)
                          : colours::panelBorder.withAlpha(0.15f));
        g.drawVerticalLine((int)x, 0.0f, (float)getHeight());

        if (isBar)
        {
            g.setColour(colours::textDim);
            g.setFont(10.0f);
            int barNum = (int)(beat / 4.0) + 1;
            g.drawText(juce::String(barNum), (int)x + 2, 0, 30, 14,
                       juce::Justification::centredLeft);
        }
    }
}

void ArrangementView::paintLaneHeaders(juce::Graphics& g)
{
    juce::ScopedLock sl(processor.laneLock);

    for (int i = 0; i < (int)processor.lanes.size(); ++i)
    {
        float y = laneToY(i);
        auto& lane = processor.lanes[i];

        // Header background
        g.setColour(colours::bgLight);
        g.fillRect(0.0f, y, (float)metrics::laneHeaderW, (float)metrics::laneHeight);

        // Colour indicator
        g.setColour(lane.colour);
        g.fillRect(0.0f, y + 2.0f, 4.0f, (float)metrics::laneHeight - 4.0f);

        // Lane name
        g.setColour(colours::text);
        g.setFont(11.0f);
        g.drawText(lane.name, 8, (int)y, metrics::laneHeaderW - 12,
                   metrics::laneHeight, juce::Justification::centredLeft);

        // Separator
        g.setColour(colours::panelBorder);
        g.drawHorizontalLine((int)(y + metrics::laneHeight - 1),
                             0.0f, (float)getWidth());
    }

    // "+" zone below last lane
    float addY = laneToY((int)processor.lanes.size());
    g.setColour(colours::bgLight.withAlpha(0.3f));
    g.fillRect(0.0f, addY, (float)metrics::laneHeaderW, (float)metrics::laneHeight);
    g.setColour(colours::textDim);
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

        // Clip background
        auto clipColour = clip.colour;
        g.setColour(region.muted ? clipColour.withAlpha(0.2f) : clipColour.withAlpha(0.7f));
        g.fillRoundedRectangle(cb.bounds, metrics::clipCorner);

        // Mini waveform / note bars
        if (!clip.notes.empty())
        {
            float clipW = cb.bounds.getWidth();
            float clipH = cb.bounds.getHeight();
            float clipX = cb.bounds.getX();
            float clipY = cb.bounds.getY();

            // Find note range
            int minNote = 127, maxNote = 0;
            for (auto& n : clip.notes)
            {
                minNote = std::min(minNote, n.noteNumber);
                maxNote = std::max(maxNote, n.noteNumber);
            }
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

        // Selection outline
        if (selected)
        {
            g.setColour(colours::accentBright);
            g.drawRoundedRectangle(cb.bounds, metrics::clipCorner, 2.0f);
        }

        // Clip name
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        g.drawText(clip.name, cb.bounds.reduced(4.0f, 2.0f),
                   juce::Justification::topLeft, true);
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
    float y = (dropLaneIdx >= 0) ? laneToY(dropLaneIdx) : 0.0f;
    float h = (float)metrics::laneHeight;

    g.setColour(colours::accent.withAlpha(0.3f));
    g.fillRect(x, y, 60.0f, h);
    g.setColour(colours::accent);
    g.drawRect(x, y, 60.0f, h, 1.0f);
}

// ── Mouse ────────────────────────────────────────────────────────────────────

void ArrangementView::mouseDown(const juce::MouseEvent& e)
{
    // Hit test clip blocks
    selLane = -1;
    selRegion = -1;

    for (auto& cb : clipBlocks)
    {
        if (cb.bounds.contains(e.position))
        {
            selLane = cb.laneIndex;
            selRegion = cb.regionIndex;

            if (e.mods.isRightButtonDown())
                showClipContextMenu(cb.laneIndex, cb.regionIndex);

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
                onClipDoubleClicked(clip);
            return;
        }
    }
}

void ArrangementView::mouseUp(const juce::MouseEvent&) {}
void ArrangementView::mouseDrag(const juce::MouseEvent&) {}

void ArrangementView::resized() { refresh(); }

// ── Drag & Drop ──────────────────────────────────────────────────────────────

bool ArrangementView::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.toString().contains("MidiFileDrag") ||
           details.description.toString().contains("MidiClip");
}

void ArrangementView::itemDragEnter(const SourceDetails&)
{
    draggingOver = true;
    repaint();
}

void ArrangementView::itemDragMove(const SourceDetails& details)
{
    dropBeatPos = std::max(0.0, xToBeat((float)details.localPosition.x));
    dropLaneIdx = yToLane((float)details.localPosition.y);
    repaint();
}

void ArrangementView::itemDragExit(const SourceDetails&)
{
    draggingOver = false;
    repaint();
}

void ArrangementView::itemDropped(const SourceDetails& details)
{
    draggingOver = false;

    // Try to get file from drag
    auto* fileComp = dynamic_cast<juce::FileTreeComponent*>(details.sourceComponent.get());
    juce::File droppedFile;

    if (fileComp != nullptr)
        droppedFile = fileComp->getSelectedFile();

    if (!droppedFile.existsAsFile())
    {
        // Check external file drop
        if (details.description.isString())
        {
            juce::File f(details.description.toString());
            if (f.existsAsFile())
                droppedFile = f;
        }
    }

    if (droppedFile.existsAsFile() && droppedFile.hasFileExtension("mid;midi"))
    {
        auto clip = parseMidiFile(droppedFile);
        double beat = std::max(0.0, xToBeat((float)details.localPosition.x));
        int lane = yToLane((float)details.localPosition.y);

        juce::ScopedLock sl(processor.laneLock);
        int numLanes = (int)processor.lanes.size();

        if (lane >= numLanes)
        {
            // Drop below last lane → create new lane
            addClipToNewLane(clip, beat);
        }
        else
        {
            // Check if dropped on same clip → open comp view
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
                        // Add as a comp take
                        int newIdx = (int)targetLane.clips.size();
                        targetLane.clips.push_back(clip);
                        // Create a muted region for the comp
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

            if (!droppedOnSameClip)
                addClipToLane(clip, lane, beat);
        }
    }

    refresh();
}

void ArrangementView::addClipToLane(const MidiClip& clip, int laneIndex, double beatPos)
{
    // laneLock already held by caller or we acquire it
    if (laneIndex >= 0 && laneIndex < (int)processor.lanes.size())
    {
        auto presets = getClipColourPresets();
        MidiClip colouredClip = clip;
        colouredClip.colour = presets[processor.lanes[laneIndex].clips.size() % presets.size()];
        processor.lanes[laneIndex].addClipAtPosition(colouredClip, beatPos);
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
}

// ── Context menu ─────────────────────────────────────────────────────────────

void ArrangementView::showClipContextMenu(int laneIdx, int regionIdx)
{
    juce::PopupMenu menu;

    // Colour submenu
    juce::PopupMenu colourMenu;
    auto presets = getClipColourPresets();
    for (int i = 0; i < (int)presets.size(); ++i)
        colourMenu.addItem(100 + i, "Colour " + juce::String(i + 1));
    menu.addSubMenu("Clip Colour", colourMenu);

    // Lane colour
    juce::PopupMenu laneColourMenu;
    for (int i = 0; i < (int)presets.size(); ++i)
        laneColourMenu.addItem(200 + i, "Colour " + juce::String(i + 1));
    menu.addSubMenu("Lane Colour", laneColourMenu);

    menu.addSeparator();

    // Mute toggle
    {
        juce::ScopedLock sl(processor.laneLock);
        auto& region = processor.lanes[laneIdx].regions[regionIdx];
        menu.addItem(1, region.muted ? "Unmute" : "Mute");
    }

    // Per-note comp isolation
    menu.addItem(2, "Isolate Note...");
    menu.addItem(3, "Delete Region");

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this, laneIdx, regionIdx, presets](int result)
    {
        if (result == 0) return;

        juce::ScopedLock sl(processor.laneLock);

        if (result >= 100 && result < 200)
        {
            auto& clip = processor.lanes[laneIdx].clips[
                processor.lanes[laneIdx].regions[regionIdx].clipIndex];
            clip.colour = presets[result - 100];
        }
        else if (result >= 200 && result < 300)
        {
            processor.lanes[laneIdx].colour = presets[result - 200];
        }
        else if (result == 1)
        {
            auto& region = processor.lanes[laneIdx].regions[regionIdx];
            region.muted = !region.muted;
        }
        else if (result == 2)
        {
            // Show note isolation submenu
            auto& lane = processor.lanes[laneIdx];
            auto& region = lane.regions[regionIdx];
            auto& clip = lane.clips[region.clipIndex];
            auto pitches = clip.getDistinctPitches();

            juce::PopupMenu noteMenu;
            noteMenu.addItem(1000, "All Notes");
            for (int p : pitches)
            {
                auto noteName = juce::MidiMessage::getMidiNoteName(p, true, true, 3);
                noteMenu.addItem(1001 + p, noteName);
            }

            noteMenu.showMenuAsync(juce::PopupMenu::Options(),
                [this, laneIdx, regionIdx](int noteResult)
            {
                if (noteResult == 0) return;
                juce::ScopedLock sl2(processor.laneLock);
                auto& region2 = processor.lanes[laneIdx].regions[regionIdx];
                region2.noteFilter = (noteResult == 1000) ? -1 : (noteResult - 1001);
                repaint();
            });
        }
        else if (result == 3)
        {
            processor.lanes[laneIdx].removeRegion(regionIdx);
        }

        repaint();
    });
}

} // namespace pflow
