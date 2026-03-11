#include "PianoRollEditor.h"

namespace pflow {

PianoRollEditor::PianoRollEditor()
{
    auto setupTabBtn = [this](juce::TextButton& btn, EditorViewMode mode)
    {
        btn.setColour(juce::TextButton::buttonColourId, colours::bgLight);
        btn.setColour(juce::TextButton::textColourOffId, colours::textDim);
        btn.onClick = [this, mode, &btn]
        {
            viewMode = mode;
            // Update button highlights
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
    btnClose.onClick = [this] { clearClip(); if (auto* p = getParentComponent()) p->resized(); };
    addAndMakeVisible(btnClose);
}

void PianoRollEditor::setClip(const MidiClip& clip)
{
    currentClip = clip;
    clipLoaded = true;

    // Center view on clip notes
    if (!clip.notes.empty())
    {
        int minNote = 127, maxNote = 0;
        for (auto& n : clip.notes)
        {
            minNote = std::min(minNote, n.noteNumber);
            maxNote = std::max(maxNote, n.noteNumber);
        }
        scrollNoteY = std::max(0, minNote - 6);
    }

    repaint();
}

void PianoRollEditor::clearClip()
{
    clipLoaded = false;
    currentClip = {};
    repaint();
}

// ── Coordinate helpers ───────────────────────────────────────────────────────

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

// ── Paint ────────────────────────────────────────────────────────────────────

void PianoRollEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel);

    if (!clipLoaded)
    {
        g.setColour(colours::textDim);
        g.setFont(13.0f);
        g.drawText("Double-click a MIDI clip to open the editor",
                   getLocalBounds(), juce::Justification::centred);
        return;
    }

    // Top separator
    g.setColour(colours::panelBorder);
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());

    switch (viewMode)
    {
        case EditorViewMode::Notes:
            paintNoteGrid(g);
            paintNotes(g);
            break;
        case EditorViewMode::Expression:
            paintNoteGrid(g);
            paintExpressionView(g);
            break;
        case EditorViewMode::Automation:
            paintAutomationView(g);
            break;
    }

    paintPianoKeys(g);
}

void PianoRollEditor::paintPianoKeys(juce::Graphics& g)
{
    int tabH = 28;

    // Piano key area background
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

        // Note label for C notes
        if (n % 12 == 0)
        {
            g.setColour(colours::textDim);
            g.setFont(9.0f);
            g.drawText("C" + juce::String(n / 12 - 2),
                       2, (int)y, pianoKeyWidth - 4, (int)noteHeight,
                       juce::Justification::centredLeft);
        }
    }

    // Right border
    g.setColour(colours::panelBorder);
    g.drawVerticalLine(pianoKeyWidth, (float)tabH, (float)getHeight());
}

void PianoRollEditor::paintNoteGrid(juce::Graphics& g)
{
    int tabH = 28;
    int topNote = yToNote((float)tabH);
    int botNote = yToNote((float)getHeight());

    // Horizontal lines (note rows)
    for (int n = botNote; n <= topNote + 1; ++n)
    {
        float y = noteToY(n);
        bool black = isBlackKey(n);

        g.setColour(black ? colours::pianoBlackKey.withAlpha(0.3f) : juce::Colours::transparentBlack);
        g.fillRect((float)pianoKeyWidth, y, (float)(getWidth() - pianoKeyWidth), noteHeight);

        g.setColour(colours::pianoGrid.withAlpha(0.3f));
        g.drawHorizontalLine((int)y, (float)pianoKeyWidth, (float)getWidth());
    }

    // Vertical lines (beats)
    for (double beat = std::floor(scrollBeatX);
         beat < scrollBeatX + getWidth() / pixelsPerBeat + 1;
         beat += 0.25)
    {
        float x = beatToX(beat);
        if (x < pianoKeyWidth) continue;

        bool isBar = (std::fmod(beat, 4.0) < 0.01);
        bool isBeat = (std::fmod(beat, 1.0) < 0.01);

        g.setColour(isBar ? colours::pianoGrid.withAlpha(0.6f)
                   : (isBeat ? colours::pianoGrid.withAlpha(0.3f)
                             : colours::pianoGrid.withAlpha(0.1f)));
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
        float w = std::max(2.0f, (float)note.lengthBeats * pixelsPerBeat);

        if (x + w < pianoKeyWidth || x > getWidth()) continue;

        bool selected = (i == selectedNote);

        // Note body
        float velAlpha = 0.5f + 0.5f * (note.velocity / 127.0f);
        g.setColour(selected ? colours::accentBright : colours::noteBlock.withAlpha(velAlpha));
        g.fillRoundedRectangle(x, y + 1.0f, w, noteHeight - 2.0f, 2.0f);

        // Velocity indicator (brighter left edge)
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRect(x, y + 1.0f, 2.0f, noteHeight - 2.0f);

        if (selected)
        {
            g.setColour(colours::accentBright);
            g.drawRoundedRectangle(x, y + 1.0f, w, noteHeight - 2.0f, 2.0f, 1.0f);
        }
    }
}

void PianoRollEditor::paintExpressionView(juce::Graphics& g)
{
    // Velocity bars for each note
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

    // Labels
    g.setColour(colours::textDim);
    g.setFont(10.0f);
    g.drawText("Velocity", pianoKeyWidth + 4, tabH + 2, 60, 14,
               juce::Justification::centredLeft);
}

void PianoRollEditor::paintAutomationView(juce::Graphics& g)
{
    int tabH = 28;
    float areaH = (float)(getHeight() - tabH);

    // Draw a placeholder automation lane
    g.setColour(colours::textDim);
    g.setFont(10.0f);
    g.drawText("CC 1 (Mod Wheel)", pianoKeyWidth + 4, tabH + 2, 120, 14,
               juce::Justification::centredLeft);

    // Dashed center line
    g.setColour(colours::pianoGrid.withAlpha(0.3f));
    float centerY = tabH + areaH * 0.5f;
    g.drawHorizontalLine((int)centerY, (float)pianoKeyWidth, (float)getWidth());

    // Vertical beat lines
    for (double beat = std::floor(scrollBeatX);
         beat < scrollBeatX + getWidth() / pixelsPerBeat + 1;
         beat += 1.0)
    {
        float x = beatToX(beat);
        if (x < pianoKeyWidth) continue;

        bool isBar = (std::fmod(beat, 4.0) < 0.01);
        g.setColour(isBar ? colours::pianoGrid.withAlpha(0.4f)
                          : colours::pianoGrid.withAlpha(0.15f));
        g.drawVerticalLine((int)x, (float)tabH, (float)getHeight());
    }

    g.setColour(colours::textDim.withAlpha(0.5f));
    g.setFont(12.0f);
    g.drawText("Draw automation points by clicking",
               getLocalBounds().withTrimmedTop(tabH).withTrimmedLeft(pianoKeyWidth),
               juce::Justification::centred);
}

// ── Resized ──────────────────────────────────────────────────────────────────

void PianoRollEditor::resized()
{
    int tabH = 26;
    int tabW = 80;
    int x = pianoKeyWidth + 4;

    btnNotes.setBounds(x, 1, tabW, tabH);
    x += tabW + 2;
    btnExpression.setBounds(x, 1, tabW, tabH);
    x += tabW + 2;
    btnAutomation.setBounds(x, 1, tabW, tabH);

    btnClose.setBounds(getWidth() - 30, 1, 26, tabH);
}

// ── Mouse interaction ────────────────────────────────────────────────────────

void PianoRollEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!clipLoaded) return;

    selectedNote = -1;

    if (viewMode == EditorViewMode::Notes)
    {
        // Hit test notes
        for (int i = 0; i < (int)currentClip.notes.size(); ++i)
        {
            auto& note = currentClip.notes[i];
            float nx = beatToX(note.startBeat);
            float ny = noteToY(note.noteNumber);
            float nw = std::max(2.0f, (float)note.lengthBeats * pixelsPerBeat);

            if (e.position.x >= nx && e.position.x <= nx + nw &&
                e.position.y >= ny && e.position.y <= ny + noteHeight)
            {
                selectedNote = i;
                draggingNote = true;
                dragStartX = e.position.x;
                dragStartY = e.position.y;
                break;
            }
        }
    }

    repaint();
}

void PianoRollEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (!clipLoaded || !draggingNote || selectedNote < 0) return;

    auto& note = currentClip.notes[selectedNote];

    // Move note
    double newBeat = xToBeat(e.position.x);
    int newNote = yToNote(e.position.y);

    note.startBeat  = std::max(0.0, newBeat);
    note.noteNumber = juce::jlimit(0, 127, newNote);

    repaint();
}

void PianoRollEditor::mouseUp(const juce::MouseEvent&)
{
    if (draggingNote && onClipEdited)
        onClipEdited(currentClip);

    draggingNote = false;
}

void PianoRollEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!clipLoaded || viewMode != EditorViewMode::Notes) return;

    if (e.position.x > pianoKeyWidth)
    {
        // Add a new note
        double beat = xToBeat(e.position.x);
        int noteNum = yToNote(e.position.y);

        // Snap to nearest quarter beat
        beat = std::round(beat * 4.0) / 4.0;

        NoteEvent ne;
        ne.startBeat  = beat;
        ne.noteNumber = juce::jlimit(0, 127, noteNum);
        ne.velocity   = 100;
        ne.lengthBeats = 0.25;
        ne.channel     = 1;

        currentClip.notes.push_back(ne);
        selectedNote = (int)currentClip.notes.size() - 1;

        if (onClipEdited)
            onClipEdited(currentClip);

        repaint();
    }
}

void PianoRollEditor::mouseWheelMove(const juce::MouseEvent& e,
                                      const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        // Zoom
        pixelsPerBeat = juce::jlimit(10.0f, 200.0f,
                                      pixelsPerBeat + wheel.deltaY * 20.0f);
    }
    else if (e.mods.isShiftDown())
    {
        // Horizontal scroll
        scrollBeatX = std::max(0.0f, scrollBeatX - wheel.deltaY * 2.0f);
    }
    else
    {
        // Vertical scroll
        scrollNoteY = juce::jlimit(0, 115, scrollNoteY + (int)(wheel.deltaY * -4.0f));
    }
    repaint();
}

} // namespace pflow
