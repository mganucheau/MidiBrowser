#include "PianoRollEditor.h"
#include "PluginProcessor.h"
#include "UndoActions.h"

namespace pflow {

PianoRollEditor::PianoRollEditor(PatternFlowProcessor& proc) : processor(proc)
{
    auto setupTabBtn = [this](juce::TextButton& btn, EditorViewMode mode)
    {
        btn.setColour(juce::TextButton::buttonColourId, colours::bgLight());
        btn.setColour(juce::TextButton::textColourOffId, colours::textDim());
        btn.onClick = [this, mode]
        {
            viewMode = mode;
            btnNotes.setColour(juce::TextButton::buttonColourId,
                viewMode == EditorViewMode::Notes ? colours::accent() : colours::bgLight());
            btnExpression.setColour(juce::TextButton::buttonColourId,
                viewMode == EditorViewMode::Expression ? colours::accent() : colours::bgLight());
            btnAutomation.setColour(juce::TextButton::buttonColourId,
                viewMode == EditorViewMode::Automation ? colours::accent() : colours::bgLight());
            repaint();
        };
        addAndMakeVisible(btn);
    };

    setupTabBtn(btnNotes, EditorViewMode::Notes);
    setupTabBtn(btnExpression, EditorViewMode::Expression);
    setupTabBtn(btnAutomation, EditorViewMode::Automation);
    btnNotes.setColour(juce::TextButton::buttonColourId, colours::accent());

    btnClose.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnClose.setColour(juce::TextButton::textColourOffId, colours::textDim());
    btnClose.onClick = [this] { clearClip(); if (onCloseRequested) onCloseRequested(); };
    addAndMakeVisible(btnClose);

    addKeyListener(this);
    setWantsKeyboardFocus(true);
}

PianoRollEditor::~PianoRollEditor()
{
    removeKeyListener(this);
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
    g.fillAll(colours::panel());

    if (!clipLoaded)
    {
        g.setColour(colours::textDim());
        g.setFont(13.0f);
        g.drawText(juce::String::charToString(0x266B) + "  Double-click any clip to open it here",
                   getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setColour(colours::panelBorder());
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());

    // Accent highlight at top edge
    g.setColour(colours::accent().withAlpha(0.1f));
    g.fillRect(0.0f, 0.0f, (float)getWidth(), 2.0f);

    switch (viewMode)
    {
        case EditorViewMode::Notes:      paintNoteGrid(g); paintNotes(g); break;
        case EditorViewMode::Expression: paintNoteGrid(g); paintExpressionView(g); break;
        case EditorViewMode::Automation: paintAutomationView(g); break;
    }
    paintStartLine(g);
    paintPianoKeys(g);
}

void PianoRollEditor::paintPianoKeys(juce::Graphics& g)
{
    int tabH = 28;
    g.setColour(colours::panel());
    g.fillRect(0, tabH, pianoKeyWidth, getHeight() - tabH);

    int topNote = yToNote((float)tabH);
    int botNote = yToNote((float)getHeight());

    for (int n = botNote; n <= topNote + 1; ++n)
    {
        float y = noteToY(n);
        bool black = isBlackKey(n);
        g.setColour(black ? colours::pianoBlackKey() : colours::pianoWhiteKey());
        g.fillRect(0.0f, y, (float)pianoKeyWidth, noteHeight);
        g.setColour(colours::panelBorder().withAlpha(0.3f));
        g.drawHorizontalLine((int)y, 0.0f, (float)pianoKeyWidth);

        if (n % 12 == 0)
        {
            g.setColour(colours::textDim());
            g.setFont(10.0f);
            g.drawText("C" + juce::String(n / 12 - 2), 2, (int)y, pianoKeyWidth - 4, (int)noteHeight, juce::Justification::centredLeft);
        }
    }
    g.setColour(colours::panelBorder());
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
        g.setColour(black ? colours::pianoBlackKey().withAlpha(0.3f) : juce::Colours::transparentBlack);
        g.fillRect((float)pianoKeyWidth, y, (float)(getWidth() - pianoKeyWidth), noteHeight);
        g.setColour(colours::pianoGrid().withAlpha(0.3f));
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
        g.setColour(isBar ? colours::pianoGrid().withAlpha(0.6f)
                   : (isBeat ? colours::pianoGrid().withAlpha(0.3f) : colours::pianoGrid().withAlpha(0.1f)));
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

        bool selected = (i == selectedNote) || (selectedNotes.count(i) > 0);
        float velAlpha = 0.5f + 0.5f * (note.velocity / 127.0f);
        g.setColour(selected ? colours::accentBright() : colours::noteBlock().withAlpha(velAlpha));
        g.fillRoundedRectangle(x, y + 1.0f, w, noteHeight - 2.0f, 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRect(x, y + 1.0f, 2.0f, noteHeight - 2.0f);

        if (selected)
        {
            g.setColour(colours::accentBright());
            g.drawRoundedRectangle(x, y + 1.0f, w, noteHeight - 2.0f, 2.0f, 1.5f);
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
        g.setColour(colours::accent().withAlpha(0.7f));
        g.fillRect(x, barY, w, velH);
        g.setColour(colours::accentBright());
        g.fillRect(x, barY, w, 2.0f);
    }
    g.setColour(colours::textDim()); g.setFont(11.0f);
    g.drawText("Velocity", pianoKeyWidth + 4, tabH + 2, 60, 14, juce::Justification::centredLeft);
}

void PianoRollEditor::paintAutomationView(juce::Graphics& g)
{
    int tabH = 28;
    float areaH = (float)(getHeight() - tabH);
    g.setColour(colours::textDim()); g.setFont(11.0f);
    g.drawText("CC 1 (Mod Wheel)", pianoKeyWidth + 4, tabH + 2, 120, 14, juce::Justification::centredLeft);
    g.setColour(colours::pianoGrid().withAlpha(0.3f));
    float centerY = tabH + areaH * 0.5f;
    g.drawHorizontalLine((int)centerY, (float)pianoKeyWidth, (float)getWidth());

    for (double beat = std::floor(scrollBeatX); beat < scrollBeatX + getWidth() / pixelsPerBeat + 1; beat += 1.0)
    {
        float x = beatToX(beat);
        if (x < pianoKeyWidth) continue;
        bool isBar = (std::fmod(beat, 4.0) < 0.001);
        g.setColour(isBar ? colours::pianoGrid().withAlpha(0.4f) : colours::pianoGrid().withAlpha(0.15f));
        g.drawVerticalLine((int)x, (float)tabH, (float)getHeight());
    }
    g.setColour(colours::textDim().withAlpha(0.5f)); g.setFont(10.0f);
    g.drawText("Draw automation points by clicking",
               getLocalBounds().withTrimmedTop(tabH).withTrimmedLeft(pianoKeyWidth), juce::Justification::centred);
}

void PianoRollEditor::paintStartLine(juce::Graphics& g)
{
    if (!clipLoaded) return;

    float startLineX = beatToX(currentClip.clipStartOffset);
    if (startLineX < pianoKeyWidth || startLineX > getWidth()) return;

    int tabH = 28;

    // Vertical line
    g.setColour(colours::playhead().withAlpha(0.7f));
    g.drawVerticalLine((int)startLineX, (float)tabH, (float)getHeight());

    // Draggable handle at top (small triangle pointing right)
    juce::Path handle;
    handle.addTriangle(startLineX - 5.0f, (float)tabH,
                       startLineX + 5.0f, (float)tabH + 5.0f,
                       startLineX - 5.0f, (float)tabH + 10.0f);
    g.setColour(colours::playhead());
    g.fillPath(handle);
}

bool PianoRollEditor::isNearStartLine(float x) const
{
    float startLineX = beatToX(currentClip.clipStartOffset);
    return std::abs(x - startLineX) < 8.0f;
}

void PianoRollEditor::refreshComponentColours()
{
    auto activeTab = [this](juce::TextButton& btn, EditorViewMode mode)
    {
        btn.setColour(juce::TextButton::buttonColourId,
                      viewMode == mode ? colours::accent() : colours::bgLight());
        btn.setColour(juce::TextButton::textColourOffId,
                      viewMode == mode ? colours::textBright() : colours::textDim());
    };
    activeTab(btnNotes, EditorViewMode::Notes);
    activeTab(btnExpression, EditorViewMode::Expression);
    activeTab(btnAutomation, EditorViewMode::Automation);

    btnClose.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnClose.setColour(juce::TextButton::textColourOffId, colours::textDim());
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

    // Check start line handle
    if (e.position.x >= pianoKeyWidth && isNearStartLine(e.position.x) && e.position.y <= 38.0f)
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }

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
    selectedNote = -1; resizingNote = false; velocityDragNote = -1; draggingStartLine = false;

    // Check for start line drag (near handle at top of line)
    if (viewMode == EditorViewMode::Notes && e.position.x >= pianoKeyWidth &&
        isNearStartLine(e.position.x) && e.position.y <= 38.0f)
    {
        draggingStartLine = true;
        startLineDragStartX = e.position.x;
        startLineDragOrigOffset = currentClip.clipStartOffset;
        return;
    }

    // Velocity bar dragging in Expression view
    if (viewMode == EditorViewMode::Expression)
    {
        int tabH = 28;
        float areaH = (float)(getHeight() - tabH);
        for (int i = 0; i < (int)currentClip.notes.size(); ++i)
        {
            auto& note = currentClip.notes[i];
            float x = beatToX(note.startBeat);
            float w = std::max(3.0f, (float)note.lengthBeats * pixelsPerBeat * 0.5f);
            if (e.position.x >= x && e.position.x <= x + w + 4.0f)
            {
                velocityDragNote = i;
                velocityDragOrigVel = note.velocity;
                velocityDragStartY = e.position.y;
                dragOrigNote = note;
                selectedNote = i;
                repaint();
                return;
            }
        }
        repaint();
        return;
    }

    if (viewMode == EditorViewMode::Notes)
    {
        // Piano key click: select all notes on that pitch
        if (e.position.x < pianoKeyWidth && e.position.y >= 28)
        {
            int pitch = yToNote(e.position.y);
            selectedNotes.clear();
            for (int i = 0; i < (int)currentClip.notes.size(); ++i)
            {
                if (currentClip.notes[i].noteNumber == pitch)
                    selectedNotes.insert(i);
            }
            if (!selectedNotes.empty())
                selectedNote = *selectedNotes.begin();
            repaint();
            return;
        }

        // Clear multi-selection unless Shift is held
        if (!e.mods.isShiftDown())
            selectedNotes.clear();

        for (int i = 0; i < (int)currentClip.notes.size(); ++i)
        {
            auto& note = currentClip.notes[i];
            float nx = beatToX(note.startBeat); float ny = noteToY(note.noteNumber);
            float nw = std::max(8.0f, (float)note.lengthBeats * pixelsPerBeat);
            if (e.position.x >= nx && e.position.x <= nx + nw && e.position.y >= ny && e.position.y <= ny + noteHeight)
            {
                selectedNote = i;
                if (e.mods.isShiftDown())
                    selectedNotes.insert(i);
                dragOrigNote = note;
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
            // Undoable delete
            processor.undoManager.perform(
                new DeleteNoteAction(processor, editLaneIdx, editRegionIdx, selectedNote));
            currentClip.notes.erase(currentClip.notes.begin() + selectedNote);
            selectedNotes.erase(selectedNote);
            selectedNote = -1; draggingNote = false; resizingNote = false;
        }
    }
    repaint();
}

void PianoRollEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (!clipLoaded) return;

    // Start line dragging
    if (draggingStartLine)
    {
        double beat = std::max(0.0, xToBeat(e.position.x));
        beat = processor.snapBeat(beat);
        currentClip.clipStartOffset = juce::jlimit(0.0, currentClip.lengthBeats - 0.25, beat);
        repaint();
        return;
    }

    // Velocity dragging in Expression view
    if (velocityDragNote >= 0 && velocityDragNote < (int)currentClip.notes.size())
    {
        float deltaY = velocityDragStartY - e.position.y;
        int newVel = juce::jlimit(1, 127, velocityDragOrigVel + (int)(deltaY * 0.8f));
        currentClip.notes[velocityDragNote].velocity = newVel;
        repaint();
        return;
    }

    if (selectedNote < 0) return;

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
    // Start line drag complete
    if (draggingStartLine)
    {
        draggingStartLine = false;
        if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
        return;
    }

    // Velocity drag complete
    if (velocityDragNote >= 0 && velocityDragNote < (int)currentClip.notes.size())
    {
        auto& note = currentClip.notes[velocityDragNote];
        if (note.velocity != dragOrigNote.velocity)
        {
            processor.undoManager.perform(
                new EditNoteAction(processor, editLaneIdx, editRegionIdx,
                                   velocityDragNote, dragOrigNote, note));
            if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
        }
        velocityDragNote = -1;
        return;
    }

    if ((draggingNote || resizingNote) && selectedNote >= 0)
    {
        auto& note = currentClip.notes[selectedNote];
        // Only record undo if the note actually changed
        if (note.startBeat != dragOrigNote.startBeat ||
            note.noteNumber != dragOrigNote.noteNumber ||
            note.lengthBeats != dragOrigNote.lengthBeats)
        {
            processor.undoManager.perform(
                new EditNoteAction(processor, editLaneIdx, editRegionIdx,
                                   selectedNote, dragOrigNote, note));
        }
        if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
    }
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
        // Undoable add
        processor.undoManager.perform(
            new AddNoteAction(processor, editLaneIdx, editRegionIdx, ne));
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

bool PianoRollEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (!clipLoaded || viewMode != EditorViewMode::Notes) return false;
    if (selectedNotes.empty() && selectedNote < 0) return false;

    // Build working set of indices to move
    std::set<int> toMove = selectedNotes;
    if (toMove.empty() && selectedNote >= 0)
        toMove.insert(selectedNote);

    int pitchDelta = 0;
    double beatDelta = 0.0;

    if (key == juce::KeyPress::upKey)        pitchDelta = 1;
    else if (key == juce::KeyPress::downKey)  pitchDelta = -1;
    else if (key == juce::KeyPress::leftKey)  beatDelta = -0.25;
    else if (key == juce::KeyPress::rightKey) beatDelta = 0.25;
    else return false;

    // Validate moves won't go out of range
    for (int idx : toMove)
    {
        if (idx < 0 || idx >= (int)currentClip.notes.size()) continue;
        auto& n = currentClip.notes[idx];
        if (juce::jlimit(0, 127, n.noteNumber + pitchDelta) != n.noteNumber + pitchDelta) return true;
        if (n.startBeat + beatDelta < 0.0) return true;
    }

    // Apply moves with undo
    for (int idx : toMove)
    {
        if (idx < 0 || idx >= (int)currentClip.notes.size()) continue;
        NoteEvent oldNote = currentClip.notes[idx];
        currentClip.notes[idx].noteNumber = juce::jlimit(0, 127, oldNote.noteNumber + pitchDelta);
        currentClip.notes[idx].startBeat = std::max(0.0, oldNote.startBeat + beatDelta);
        processor.undoManager.perform(
            new EditNoteAction(processor, editLaneIdx, editRegionIdx,
                               idx, oldNote, currentClip.notes[idx]));
    }

    if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
    repaint();
    return true;
}

void PianoRollEditor::quantizeSelectedNotes()
{
    if (!clipLoaded) return;
    std::set<int> toQuantize = selectedNotes;
    if (toQuantize.empty() && selectedNote >= 0) toQuantize.insert(selectedNote);
    if (toQuantize.empty())
    {
        // Quantize all notes if none selected
        for (int i = 0; i < (int)currentClip.notes.size(); ++i)
            toQuantize.insert(i);
    }

    for (int idx : toQuantize)
    {
        if (idx < 0 || idx >= (int)currentClip.notes.size()) continue;
        NoteEvent oldNote = currentClip.notes[idx];
        double snapped = processor.snapBeat(oldNote.startBeat);
        if (std::abs(snapped - oldNote.startBeat) > 0.001)
        {
            currentClip.notes[idx].startBeat = snapped;
            processor.undoManager.perform(
                new EditNoteAction(processor, editLaneIdx, editRegionIdx,
                                   idx, oldNote, currentClip.notes[idx]));
        }
    }
    if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
    repaint();
}

void PianoRollEditor::transposeSelectedNotes(int semitones)
{
    if (!clipLoaded) return;
    std::set<int> toTranspose = selectedNotes;
    if (toTranspose.empty() && selectedNote >= 0) toTranspose.insert(selectedNote);
    if (toTranspose.empty()) return;

    // Validate
    for (int idx : toTranspose)
    {
        if (idx < 0 || idx >= (int)currentClip.notes.size()) continue;
        int newPitch = currentClip.notes[idx].noteNumber + semitones;
        if (newPitch < 0 || newPitch > 127) return;
    }

    for (int idx : toTranspose)
    {
        if (idx < 0 || idx >= (int)currentClip.notes.size()) continue;
        NoteEvent oldNote = currentClip.notes[idx];
        currentClip.notes[idx].noteNumber = juce::jlimit(0, 127, oldNote.noteNumber + semitones);
        processor.undoManager.perform(
            new EditNoteAction(processor, editLaneIdx, editRegionIdx,
                               idx, oldNote, currentClip.notes[idx]));
    }
    if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
    repaint();
}

} // namespace pflow
