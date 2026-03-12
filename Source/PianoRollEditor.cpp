#include "PianoRollEditor.h"
#include "PluginProcessor.h"

namespace pflow {

PianoRollEditor::PianoRollEditor(PatternFlowProcessor& proc) : processor(proc)
{
    auto setupTabBtn = [this](juce::TextButton& btn, EditorViewMode mode)
    {
        btn.setColour(juce::TextButton::buttonColourId, colours::bgLight);
        btn.setColour(juce::TextButton::textColourOffId, colours::textDim);
        btn.onClick = [this, mode]
        {
            viewMode = mode;
            btnNotes.setColour(juce::TextButton::buttonColourId,
                viewMode == EditorViewMode::Notes ? colours::accent : colours::bgLight);
            btnExpression.setColour(juce::TextButton::buttonColourId,
                viewMode == EditorViewMode::Expression ? colours::accent : colours::bgLight);
            btnAutomation.setColour(juce::TextButton::buttonColourId,
                viewMode == EditorViewMode::Automation ? colours::accent : colours::bgLight);
            repaint();
        };
        addAndMakeVisible(btn);
    };

    setupTabBtn(btnNotes, EditorViewMode::Notes);
    setupTabBtn(btnExpression, EditorViewMode::Expression);
    setupTabBtn(btnAutomation, EditorViewMode::Automation);
    btnNotes.setColour(juce::TextButton::buttonColourId, colours::accent);

    btnClose.setColour(juce::TextButton::buttonColourId, colours::bgLight);
    btnClose.setColour(juce::TextButton::textColourOffId, colours::textDim);
    btnClose.onClick = [this] { clearClip(); if (onCloseRequested) onCloseRequested(); };
    addAndMakeVisible(btnClose);
}

void PianoRollEditor::setClip(const MidiClip& clip, int laneIdx, int regionIdx)
{
    currentClip = clip;
    clipLoaded = true;
    editLaneIdx = laneIdx;
    editRegionIdx = regionIdx;
    autoZoomToNotes();
    repaint();
}

void PianoRollEditor::clearClip()
{
    clipLoaded = false;
    currentClip = {};
    editLaneIdx = -1;
    editRegionIdx = -1;
    repaint();
}

void PianoRollEditor::autoZoomToNotes()
{
    if (currentClip.notes.empty()) return;

    int minNote = 127, maxNote = 0;
    for (auto& n : currentClip.notes)
    {
        minNote = std::min(minNote, n.noteNumber);
        maxNote = std::max(maxNote, n.noteNumber);
    }

    int range = maxNote - minNote + 1;
    int tabH = 28;
    float areaH = std::max(100.0f, (float)(getHeight() - tabH));
    noteHeight = std::max(4.0f, std::min(16.0f, areaH / (float)(range + 4)));
    scrollNoteY = std::max(0, minNote - 2);

    float areaW = std::max(100.0f, (float)(getWidth() - pianoKeyWidth));
    pixelsPerBeat = std::max(20.0f, std::min(200.0f, areaW / (float)currentClip.lengthBeats));
    scrollBeatX = 0.0f;
}

float PianoRollEditor::noteToY(int noteNum) const
{
    int tabH = 28;
    float areaH = (float)(getHeight() - tabH);
    return tabH + areaH - (float)(noteNum - scrollNoteY + 1) * noteHeight;
}

int PianoRollEditor::yToNote(float y) const
{
    int tabH = 28;
    float areaH = (float)(getHeight() - tabH);
    return scrollNoteY + (int)((areaH - (y - tabH)) / noteHeight);
}

float PianoRollEditor::beatToX(double beat) const
{
    return pianoKeyWidth + (float)(beat - scrollBeatX) * pixelsPerBeat;
}

double PianoRollEditor::xToBeat(float x) const
{
    return (double)(x - pianoKeyWidth) / pixelsPerBeat + scrollBeatX;
}

bool PianoRollEditor::isBlackKey(int noteNum)
{
    int n = noteNum % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

bool PianoRollEditor::isNearRightEdge(const juce::MouseEvent& e, int noteIdx) const
{
    auto& note = currentClip.notes[noteIdx];
    float nx = beatToX(note.startBeat);
    float nw = std::max(8.0f, (float)note.lengthBeats * pixelsPerBeat);
    return std::abs(e.position.x - (nx + nw)) < 6.0f;
}

// ── Paint ────────────────────────────────────────────────────────────────────

void PianoRollEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel);

    if (!clipLoaded)
    {
        g.setColour(colours::textDim);
        g.setFont(11.0f);
        g.drawText("Double-click a clip to edit", getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setColour(colours::panelBorder);
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());

    switch (viewMode)
    {
        case EditorViewMode::Notes:      paintNoteGrid(g); paintNotes(g); break;
        case EditorViewMode::Expression: paintNoteGrid(g); paintExpressionView(g); break;
        case EditorViewMode::Automation: paintAutomationView(g); break;
    }
    paintPianoKeys(g);
}

void PianoRollEditor::paintPianoKeys(juce::Graphics& g)
{
    int tabH = 28;
    g.setColour(colours::panel);
    g.fillRect(0, tabH, pianoKeyWidth, getHeight() - tabH);

    int topNote = yToNote((float)tabH);
    int botNote = yToNote((float)getHeight());

    for (int n = botNote; n <= topNote + 1; ++n)
    {
        float y = noteToY(n);
        bool black = isBlackKey(n);
        g.setColour(black ? colours::pianoBlackKey : colours::pianoWhiteKey);
        g.fillRect(0.0f, y, (float)pianoKeyWidth, noteHeight);
        g.setColour(colours::panelBorder.withAlpha(0.3f));
        g.drawHorizontalLine((int)y, 0.0f, (float)pianoKeyWidth);

        if (n % 12 == 0)
        {
            g.setColour(colours::textDim);
            g.setFont(8.0f);
            g.drawText("C" + juce::String(n / 12 - 2), 2, (int)y, pianoKeyWidth - 4, (int)noteHeight, juce::Justification::centredLeft);
        }
    }
    g.setColour(colours::panelBorder);
    g.drawVerticalLine(pianoKeyWidth, (float)tabH, (float)getHeight());
}

void PianoRollEditor::paintNoteGrid(juce::Graphics& g)
{
    int tabH = 28;
    int topNote = yToNote((float)tabH);
    int botNote = yToNote((float)getHeight());

    for (int n = botNote; n <= topNote + 1; ++n)
    {
        float y = noteToY(n);
        bool black = isBlackKey(n);
        g.setColour(black ? colours::pianoBlackKey.withAlpha(0.3f) : juce::Colours::transparentBlack);
        g.fillRect((float)pianoKeyWidth, y, (float)(getWidth() - pianoKeyWidth), noteHeight);
        g.setColour(colours::pianoGrid.withAlpha(0.3f));
        g.drawHorizontalLine((int)y, (float)pianoKeyWidth, (float)getWidth());
    }

    double gridDiv = 0.25;
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
        default:              gridDiv = 0.25; break;
    }

    for (double beat = std::floor(scrollBeatX); beat < scrollBeatX + getWidth() / pixelsPerBeat + 1; beat += gridDiv)
    {
        float x = beatToX(beat);
        if (x < pianoKeyWidth) continue;
        bool isBar = (std::fmod(beat, 4.0) < 0.001);
        bool isBeat = (std::fmod(beat, 1.0) < 0.001);
        g.setColour(isBar ? colours::pianoGrid.withAlpha(0.6f)
                   : (isBeat ? colours::pianoGrid.withAlpha(0.3f) : colours::pianoGrid.withAlpha(0.1f)));
        g.drawVerticalLine((int)x, (float)tabH, (float)getHeight());
    }
}

void PianoRollEditor::paintNotes(juce::Graphics& g)
{
    for (int i = 0; i < (int)currentClip.notes.size(); ++i)
    {
        auto& note = currentClip.notes[i];
        float x = beatToX(note.startBeat);
        float y = noteToY(note.noteNumber);
        float w = std::max(4.0f, (float)note.lengthBeats * pixelsPerBeat);
        if (x + w < pianoKeyWidth || x > getWidth()) continue;

        bool selected = (i == selectedNote);
        float velAlpha = 0.5f + 0.5f * (note.velocity / 127.0f);
        g.setColour(selected ? colours::accentBright : colours::noteBlock.withAlpha(velAlpha));
        g.fillRoundedRectangle(x, y + 1.0f, w, noteHeight - 2.0f, 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRect(x, y + 1.0f, 2.0f, noteHeight - 2.0f);

        if (selected)
        {
            g.setColour(colours::accentBright);
            g.drawRoundedRectangle(x, y + 1.0f, w, noteHeight - 2.0f, 2.0f, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.fillRect(x + w - 3.0f, y + 2.0f, 2.0f, noteHeight - 4.0f);
        }
    }
}

void PianoRollEditor::paintExpressionView(juce::Graphics& g)
{
    int tabH = 28;
    float areaH = (float)(getHeight() - tabH);
    for (auto& note : currentClip.notes)
    {
        float x = beatToX(note.startBeat);
        float w = std::max(3.0f, (float)note.lengthBeats * pixelsPerBeat * 0.5f);
        float velH = (note.velocity / 127.0f) * (areaH - 20.0f);
        if (x < pianoKeyWidth || x > getWidth()) continue;
        float barY = getHeight() - velH - 10.0f;
        g.setColour(colours::accent.withAlpha(0.7f));
        g.fillRect(x, barY, w, velH);
        g.setColour(colours::accentBright);
        g.fillRect(x, barY, w, 2.0f);
    }
    g.setColour(colours::textDim); g.setFont(9.0f);
    g.drawText("Velocity", pianoKeyWidth + 4, tabH + 2, 60, 14, juce::Justification::centredLeft);
}

void PianoRollEditor::paintAutomationView(juce::Graphics& g)
{
    int tabH = 28;
    float areaH = (float)(getHeight() - tabH);
    g.setColour(colours::textDim); g.setFont(9.0f);
    g.drawText("CC 1 (Mod Wheel)", pianoKeyWidth + 4, tabH + 2, 120, 14, juce::Justification::centredLeft);
    g.setColour(colours::pianoGrid.withAlpha(0.3f));
    float centerY = tabH + areaH * 0.5f;
    g.drawHorizontalLine((int)centerY, (float)pianoKeyWidth, (float)getWidth());

    for (double beat = std::floor(scrollBeatX); beat < scrollBeatX + getWidth() / pixelsPerBeat + 1; beat += 1.0)
    {
        float x = beatToX(beat);
        if (x < pianoKeyWidth) continue;
        bool isBar = (std::fmod(beat, 4.0) < 0.001);
        g.setColour(isBar ? colours::pianoGrid.withAlpha(0.4f) : colours::pianoGrid.withAlpha(0.15f));
        g.drawVerticalLine((int)x, (float)tabH, (float)getHeight());
    }
    g.setColour(colours::textDim.withAlpha(0.5f)); g.setFont(10.0f);
    g.drawText("Draw automation points by clicking",
               getLocalBounds().withTrimmedTop(tabH).withTrimmedLeft(pianoKeyWidth), juce::Justification::centred);
}

void PianoRollEditor::resized()
{
    int tabH = 24; int tabW = 70; int x = pianoKeyWidth + 4;
    btnNotes.setBounds(x, 2, tabW, tabH); x += tabW + 2;
    btnExpression.setBounds(x, 2, tabW, tabH); x += tabW + 2;
    btnAutomation.setBounds(x, 2, tabW, tabH);
    btnClose.setBounds(getWidth() - 28, 2, 24, tabH);
    if (clipLoaded) autoZoomToNotes();
}

// ── Mouse ────────────────────────────────────────────────────────────────────

void PianoRollEditor::mouseMove(const juce::MouseEvent& e)
{
    if (!clipLoaded || viewMode != EditorViewMode::Notes) return;
    for (int i = 0; i < (int)currentClip.notes.size(); ++i)
    {
        auto& note = currentClip.notes[i];
        float nx = beatToX(note.startBeat); float ny = noteToY(note.noteNumber);
        float nw = std::max(8.0f, (float)note.lengthBeats * pixelsPerBeat);
        if (e.position.y >= ny && e.position.y <= ny + noteHeight && e.position.x >= nx && e.position.x <= nx + nw)
        {
            if (isNearRightEdge(e, i)) { setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }
        }
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void PianoRollEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!clipLoaded) return;
    selectedNote = -1; resizingNote = false;

    if (viewMode == EditorViewMode::Notes)
    {
        for (int i = 0; i < (int)currentClip.notes.size(); ++i)
        {
            auto& note = currentClip.notes[i];
            float nx = beatToX(note.startBeat); float ny = noteToY(note.noteNumber);
            float nw = std::max(8.0f, (float)note.lengthBeats * pixelsPerBeat);
            if (e.position.x >= nx && e.position.x <= nx + nw && e.position.y >= ny && e.position.y <= ny + noteHeight)
            {
                selectedNote = i;
                if (isNearRightEdge(e, i))
                { resizingNote = true; resizeOrigLen = note.lengthBeats; dragStartX = e.position.x; }
                else
                { draggingNote = true; dragStartX = e.position.x; dragStartY = e.position.y;
                  dragNoteOrigBeat = note.startBeat; dragNoteOrigPitch = note.noteNumber; }
                break;
            }
        }
        if (selectedNote >= 0 && e.mods.isRightButtonDown())
        {
            currentClip.notes.erase(currentClip.notes.begin() + selectedNote);
            selectedNote = -1; draggingNote = false; resizingNote = false;
            if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
        }
    }
    repaint();
}

void PianoRollEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (!clipLoaded || selectedNote < 0) return;

    if (resizingNote)
    {
        auto& note = currentClip.notes[selectedNote];
        double deltaBeat = (double)(e.position.x - dragStartX) / pixelsPerBeat;
        double newLen = resizeOrigLen + deltaBeat;
        newLen = processor.snapBeat(std::max(0.125, newLen));
        note.lengthBeats = std::max(0.125, newLen);
        repaint();
        return;
    }

    if (draggingNote)
    {
        auto& note = currentClip.notes[selectedNote];
        double deltaBeat = (double)(e.position.x - dragStartX) / pixelsPerBeat;
        double newBeat = dragNoteOrigBeat + deltaBeat;
        newBeat = processor.snapBeat(newBeat);
        note.startBeat = std::max(0.0, newBeat);
        note.noteNumber = juce::jlimit(0, 127, yToNote(e.position.y));
        repaint();
    }
}

void PianoRollEditor::mouseUp(const juce::MouseEvent&)
{
    if ((draggingNote || resizingNote) && onClipEdited)
        onClipEdited(currentClip, editLaneIdx, editRegionIdx);
    draggingNote = false;
    resizingNote = false;
}

void PianoRollEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!clipLoaded || viewMode != EditorViewMode::Notes) return;
    if (e.position.x > pianoKeyWidth)
    {
        double beat = processor.snapBeat(xToBeat(e.position.x));
        int noteNum = juce::jlimit(0, 127, yToNote(e.position.y));

        double noteLen = 1.0;
        int gs = processor.gridSnap.load();
        using GS = PatternFlowProcessor::GridSize;
        switch ((GS)gs)
        {
            case GS::Bar:         noteLen = 4.0;  break;
            case GS::Beat:        noteLen = 1.0;  break;
            case GS::HalfBeat:    noteLen = 0.5;  break;
            case GS::QuarterBeat: noteLen = 0.25; break;
            case GS::Eighth:      noteLen = 0.5;  break;
            case GS::Sixteenth:   noteLen = 0.25; break;
            default:              noteLen = 0.25; break;
        }

        NoteEvent ne;
        ne.startBeat = beat; ne.noteNumber = noteNum; ne.velocity = 100;
        ne.lengthBeats = noteLen; ne.channel = 1;
        currentClip.notes.push_back(ne);
        selectedNote = (int)currentClip.notes.size() - 1;
        if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
        repaint();
    }
}

void PianoRollEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        pixelsPerBeat = juce::jlimit(10.0f, 200.0f, pixelsPerBeat + wheel.deltaY * 20.0f);
    else if (e.mods.isShiftDown())
        scrollBeatX = std::max(0.0f, scrollBeatX - wheel.deltaY * 2.0f);
    else
        scrollNoteY = juce::jlimit(0, 115, scrollNoteY + (int)(wheel.deltaY * -4.0f));
    repaint();
}

} // namespace pflow
