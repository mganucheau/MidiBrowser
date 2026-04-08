#include "PianoRollEditor.h"
#include "PluginProcessor.h"
#include "UndoActions.h"
#include <algorithm>

namespace pflow {

PianoRollEditor::PianoRollEditor(PatternFlowProcessor& proc) : processor(proc)
{
    auto setupTabBtn = [this](juce::TextButton& btn, EditorViewMode mode)
    {
        btn.onClick = [this, mode]
        {
            viewMode = mode;
            syncPianoRollTabPills();
            ccCombo.setVisible(viewMode == EditorViewMode::Automation);
            selectedAutomationPoint = -1;
            if (clipLoaded) autoZoomToNotes();
            repaint();
        };
        addAndMakeVisible(btn);
    };

    setupTabBtn(btnNotes, EditorViewMode::Notes);
    setupTabBtn(btnExpression, EditorViewMode::Expression);
    setupTabBtn(btnAutomation, EditorViewMode::Automation);
    syncPianoRollTabPills();

    ccCombo.addItem("CC 1 (Mod Wheel)", 1);
    ccCombo.addItem("CC 7 (Volume)", 7);
    ccCombo.addItem("CC 10 (Pan)", 10);
    ccCombo.addItem("CC 11 (Expression)", 11);
    ccCombo.addItem("CC 64 (Sustain)", 64);
    ccCombo.addItem("CC 71 (Resonance)", 71);
    ccCombo.addItem("CC 74 (Cutoff)", 74);
    ccCombo.addItem("CC 91 (Reverb)", 91);
    ccCombo.addItem("CC 93 (Chorus)", 93);
    ccCombo.setSelectedId(1);
    ccCombo.setColour(juce::ComboBox::backgroundColourId, colours::bgLight());
    ccCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    ccCombo.onChange = [this] { selectedAutomationPoint = -1; repaint(); };
    addAndMakeVisible(ccCombo);

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

void PianoRollEditor::syncPianoRollTabPills()
{
    auto apply = [](juce::TextButton& btn, bool active)
    {
        btn.setComponentID(active ? "ActivePill" : "ActionButton");
        btn.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    };
    apply(btnNotes, viewMode == EditorViewMode::Notes);
    apply(btnExpression, viewMode == EditorViewMode::Expression);
    apply(btnAutomation, viewMode == EditorViewMode::Automation);
    btnNotes.repaint();
    btnExpression.repaint();
    btnAutomation.repaint();
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
    float contentH = (float)(getHeight() - tabH);
    float areaH = (viewMode == EditorViewMode::Expression) ? contentH * (2.0f / 3.0f) : contentH;
    areaH = std::max(100.0f, areaH);
    noteHeight = std::max(4.0f, std::min(16.0f, areaH / (float)(range + 4)));
    scrollNoteY = std::max(0, minNote - 2);

    float areaW = std::max(100.0f, (float)(getWidth() - pianoKeyWidth));
    double lenBeats = std::max(0.25, currentClip.lengthBeats);
    pixelsPerBeat = std::max(20.0f, std::min(200.0f, areaW / (float)lenBeats));
    scrollBeatX = 0.0f;
}

float PianoRollEditor::noteToY(int noteNum) const
{
    int tabH = 28;
    float areaH = getNoteAreaHeight();
    return tabH + areaH - (float)(noteNum - scrollNoteY + 1) * noteHeight;
}

int PianoRollEditor::yToNote(float y) const
{
    int tabH = 28;
    float areaH = getNoteAreaHeight();
    return scrollNoteY + (int)((areaH - (y - tabH)) / noteHeight);
}

float PianoRollEditor::beatToX(double beat) const
{
    return pianoKeyWidth + (float)(beat - scrollBeatX) * pixelsPerBeat;
}

double PianoRollEditor::xToBeat(float x) const
{
    float ppb = (pixelsPerBeat > 1e-6f) ? pixelsPerBeat : 20.0f;
    return (double)(x - pianoKeyWidth) / ppb + scrollBeatX;
}

float PianoRollEditor::getNoteAreaHeight() const
{
    int tabH = 28;
    float contentH = (float)(getHeight() - tabH);
    if (viewMode == EditorViewMode::Expression)
        return contentH * (2.0f / 3.0f);
    return contentH;
}

juce::Rectangle<int> PianoRollEditor::getExpressionStripBounds() const
{
    int tabH = 28;
    float contentH = (float)(getHeight() - tabH);
    if (viewMode != EditorViewMode::Expression) return {};
    float noteAreaH = contentH * (2.0f / 3.0f);
    int stripTop = tabH + (int)noteAreaH;
    int stripH = (int)(contentH / 3.0f);
    return juce::Rectangle<int>(pianoKeyWidth, stripTop, getWidth() - pianoKeyWidth, stripH);
}

juce::Rectangle<int> PianoRollEditor::getAutomationContentBounds() const
{
    int tabH = 28;
    return juce::Rectangle<int>(pianoKeyWidth, tabH, getWidth() - pianoKeyWidth, getHeight() - tabH);
}

int PianoRollEditor::getCurrentAutomationCC() const
{
    return ccCombo.getSelectedId() > 0 ? (int)ccCombo.getSelectedId() : 1;
}

std::vector<AutomationPoint>& PianoRollEditor::getAutomationPointsForCurrentCC()
{
    int cc = getCurrentAutomationCC();
    return currentClip.automation[cc];
}

void PianoRollEditor::ensureAutomationBounds(int cc)
{
    auto& pts = currentClip.automation[cc];
    if (pts.empty())
    {
        pts.push_back({ 0.0, 0 });
        pts.push_back({ currentClip.lengthBeats, 0 });
        return;
    }
    std::sort(pts.begin(), pts.end(), [](const AutomationPoint& a, const AutomationPoint& b) { return a.beat < b.beat; });
}

int PianoRollEditor::hitTestAutomationPoint(float x, float y) const
{
    auto r = getAutomationContentBounds();
    if (!r.contains(x, y)) return -1;
    int cc = getCurrentAutomationCC();
    auto it = currentClip.automation.find(cc);
    if (it == currentClip.automation.end()) return -1;
    const auto& pts = it->second;
    float pointRadius = 6.0f;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        float px = beatToX(pts[i].beat);
        float py = r.getY() + (1.0f - pts[i].value / 127.0f) * r.getHeight();
        if (std::abs(x - px) <= pointRadius && std::abs(y - py) <= pointRadius)
            return (int)i;
    }
    return -1;
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
    float noteAreaH = getNoteAreaHeight();
    int keyAreaBottom = tabH + (int)noteAreaH;
    g.setColour(colours::panel());
    g.fillRect(0, tabH, pianoKeyWidth, keyAreaBottom - tabH);

    int topNote = yToNote((float)tabH);
    int botNote = yToNote((float)keyAreaBottom);

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
    g.drawVerticalLine(pianoKeyWidth, (float)tabH, (float)keyAreaBottom);

    if (viewMode == EditorViewMode::Expression)
    {
        g.setColour(colours::panelBorder().withAlpha(0.6f));
        g.drawHorizontalLine(keyAreaBottom, 0.0f, (float)getWidth());
        g.setColour(colours::panel());
        g.fillRect(0, keyAreaBottom, pianoKeyWidth, getHeight() - keyAreaBottom);
    }
}

void PianoRollEditor::paintNoteGrid(juce::Graphics& g)
{
    int tabH = 28;
    float noteAreaH = getNoteAreaHeight();
    int gridBottom = tabH + (int)noteAreaH;
    int topNote = yToNote((float)tabH);
    int botNote = yToNote((float)gridBottom);

    for (int n = botNote; n <= topNote + 1; ++n)
    {
        float y = noteToY(n);
        bool black = isBlackKey(n);
        g.setColour(black ? colours::pianoBlackKey().withAlpha(0.5f) : juce::Colours::transparentBlack);
        g.fillRect((float)pianoKeyWidth, y, (float)(getWidth() - pianoKeyWidth), noteHeight);
        g.setColour(colours::pianoGrid().withAlpha(0.55f));
        g.drawHorizontalLine((int)y, (float)pianoKeyWidth, (float)getWidth());
    }

    int gs = processor.gridSnap.load();
    double gridDiv = (gs == (int)PatternFlowProcessor::GridSize::Off)
        ? 0.25
        : PatternFlowProcessor::getGridDivision((PatternFlowProcessor::GridSize)gs);

    for (double beat = std::floor(scrollBeatX); beat < scrollBeatX + getWidth() / pixelsPerBeat + 1; beat += gridDiv)
    {
        float x = beatToX(beat);
        if (x < pianoKeyWidth) continue;
        bool isBar = (std::fmod(beat, 4.0) < 0.001);
        bool isBeat = (std::fmod(beat, 1.0) < 0.001);
        g.setColour(isBar ? colours::pianoGrid().withAlpha(0.8f)
                   : (isBeat ? colours::pianoGrid().withAlpha(0.5f) : colours::pianoGrid().withAlpha(0.25f)));
        g.drawVerticalLine((int)x, (float)tabH, (float)gridBottom);
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
    auto strip = getExpressionStripBounds();
    if (strip.isEmpty()) return;

    g.setColour(colours::panel().darker(0.02f));
    g.fillRect(strip);

    for (auto& note : currentClip.notes)
    {
        float x = beatToX(note.startBeat);
        float w = std::max(4.0f, (float)note.lengthBeats * pixelsPerBeat * 0.6f);
        if (x + w < strip.getX() || x > strip.getRight()) continue;
        float velNorm = note.velocity / 127.0f;
        float barH = velNorm * (strip.getHeight() - 8.0f);
        float barY = (float)strip.getBottom() - barH - 4.0f;
        g.setColour(colours::accent().withAlpha(0.8f));
        g.fillRect(x, barY, w, barH);
        g.setColour(colours::accentBright());
        g.fillRect(x, barY, w, 2.0f);
    }

    g.setColour(colours::textDim());
    g.setFont(11.0f);
    g.drawText("Velocity", strip.withTrimmedBottom(strip.getHeight() - 16).reduced(4, 0), juce::Justification::centredLeft);
}

void PianoRollEditor::paintAutomationView(juce::Graphics& g)
{
    auto r = getAutomationContentBounds();
    if (r.isEmpty()) return;

    g.setColour(colours::panel().darker(0.02f));
    g.fillRect(r);

    int cc = getCurrentAutomationCC();
    auto it = currentClip.automation.find(cc);
    std::vector<AutomationPoint> ptsSorted;
    if (it != currentClip.automation.end())
    {
        ptsSorted = it->second;
        std::sort(ptsSorted.begin(), ptsSorted.end(), [](const AutomationPoint& a, const AutomationPoint& b) { return a.beat < b.beat; });
    }

    g.setColour(colours::pianoGrid().withAlpha(0.5f));
    for (double beat = std::floor(scrollBeatX); beat < scrollBeatX + (double)r.getWidth() / pixelsPerBeat + 1; beat += 1.0)
    {
        float x = beatToX(beat);
        if (x < r.getX()) continue;
        bool isBar = (std::fmod(beat, 4.0) < 0.001);
        g.setColour(isBar ? colours::pianoGrid().withAlpha(0.6f) : colours::pianoGrid().withAlpha(0.3f));
        g.drawVerticalLine((int)x, (float)r.getY(), (float)r.getBottom());
    }
    g.setColour(colours::pianoGrid().withAlpha(0.5f));
    g.drawHorizontalLine(r.getY() + r.getHeight() / 2, (float)r.getX(), (float)r.getRight());

    if (ptsSorted.size() >= 2)
    {
        juce::Path path;
        for (size_t i = 0; i < ptsSorted.size(); ++i)
        {
            float px = beatToX(ptsSorted[i].beat);
            float py = (float)r.getY() + (1.0f - ptsSorted[i].value / 127.0f) * r.getHeight();
            if (i == 0) path.startNewSubPath(px, py);
            else path.lineTo(px, py);
        }
        g.setColour(colours::accent().withAlpha(0.8f));
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

    for (size_t i = 0; i < ptsSorted.size(); ++i)
    {
        float px = beatToX(ptsSorted[i].beat);
        float py = (float)r.getY() + (1.0f - ptsSorted[i].value / 127.0f) * r.getHeight();
        bool sel = (selectedAutomationPoint >= 0 && (size_t)selectedAutomationPoint == i);
        g.setColour(sel ? colours::accentBright() : colours::accent());
        g.fillEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f);
        g.setColour(colours::panelBorder());
        g.drawEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f, 1.0f);
    }
}

void PianoRollEditor::paintStartLine(juce::Graphics& g)
{
    if (!clipLoaded) return;

    float startLineX = beatToX(currentClip.clipStartOffset);
    if (startLineX < pianoKeyWidth || startLineX > getWidth()) return;

    int tabH = 28;
    int lineBottom = tabH + (int)getNoteAreaHeight();

    // Vertical line (only in note area)
    g.setColour(colours::playhead().withAlpha(0.7f));
    g.drawVerticalLine((int)startLineX, (float)tabH, (float)lineBottom);

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
    syncPianoRollTabPills();
    ccCombo.setColour(juce::ComboBox::backgroundColourId, colours::bgLight());
    ccCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);

    btnClose.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnClose.setColour(juce::TextButton::textColourOffId, colours::textDim());
}

void PianoRollEditor::resized()
{
    int tabH = 24; int tabW = 70; int x = pianoKeyWidth + 4;
    btnNotes.setBounds(x, 2, tabW, tabH); x += tabW + 2;
    btnExpression.setBounds(x, 2, tabW, tabH); x += tabW + 2;
    btnAutomation.setBounds(x, 2, tabW, tabH); x += tabW + 4;
    ccCombo.setBounds(x, 2, 140, tabH);
    ccCombo.setVisible(viewMode == EditorViewMode::Automation);
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
    automationDragPoint = -1;

    // Automation view: click on point to select/drag, or click empty to add point
    if (viewMode == EditorViewMode::Automation)
    {
        int hit = hitTestAutomationPoint(e.position.x, e.position.y);
        if (hit >= 0)
        {
            selectedAutomationPoint = hit;
            automationDragPoint = hit;
            automationDragOldPoints = getAutomationPointsForCurrentCC();
            repaint();
            return;
        }
        auto r = getAutomationContentBounds();
        if (r.contains(e.position.toInt()) && r.getHeight() > 0)
        {
            double beat = processor.snapBeat(xToBeat(e.position.x));
            beat = juce::jlimit(0.0, currentClip.lengthBeats, beat);
            int value = (int)(127.0 * (1.0 - (e.position.y - r.getY()) / (float)r.getHeight()));
            value = juce::jlimit(0, 127, value);
            int cc = getCurrentAutomationCC();
            std::vector<AutomationPoint> oldPts = currentClip.automation[cc];
            ensureAutomationBounds(cc);
            auto& pts = currentClip.automation[cc];
            AutomationPoint newPt { beat, value };
            auto it = std::lower_bound(pts.begin(), pts.end(), newPt, [](const AutomationPoint& a, const AutomationPoint& b) { return a.beat < b.beat; });
            pts.insert(it, newPt);
            processor.undoManager.beginNewTransaction();
            processor.undoManager.perform(new SetAutomationAction(processor, editLaneIdx, editRegionIdx, cc, oldPts, pts));
            selectedAutomationPoint = (int)(it - pts.begin());
            if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
            repaint();
            return;
        }
        selectedAutomationPoint = -1;
        repaint();
        return;
    }

    // Check for start line drag (near handle at top of line)
    if (viewMode == EditorViewMode::Notes && e.position.x >= pianoKeyWidth &&
        isNearStartLine(e.position.x) && e.position.y <= 38.0f)
    {
        draggingStartLine = true;
        startLineDragStartX = e.position.x;
        startLineDragOrigOffset = currentClip.clipStartOffset;
        return;
    }

    // Velocity bar dragging in Expression view (only in the bottom 1/3 strip)
    if (viewMode == EditorViewMode::Expression)
    {
        auto strip = getExpressionStripBounds();
        if (strip.contains(e.position.toInt()))
        {
            for (int i = 0; i < (int)currentClip.notes.size(); ++i)
            {
                auto& note = currentClip.notes[i];
                float x = beatToX(note.startBeat);
                float w = std::max(4.0f, (float)note.lengthBeats * pixelsPerBeat * 0.6f);
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
            processor.undoManager.beginNewTransaction();
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

    // Automation point drag
    if (automationDragPoint >= 0 && viewMode == EditorViewMode::Automation)
    {
        auto& pts = getAutomationPointsForCurrentCC();
        if (automationDragPoint < (int)pts.size())
        {
            double beat = xToBeat(e.position.x);
            beat = processor.snapBeat(juce::jlimit(0.0, currentClip.lengthBeats, beat));
            auto r = getAutomationContentBounds();
            int value = (r.getHeight() > 0)
                ? (int)(127.0 * (1.0 - (e.position.y - r.getY()) / (float)r.getHeight()))
                : 64;
            value = juce::jlimit(0, 127, value);
            pts[automationDragPoint].beat = beat;
            pts[automationDragPoint].value = value;
            repaint();
        }
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

    // Automation point drag complete
    if (automationDragPoint >= 0 && viewMode == EditorViewMode::Automation)
    {
        int cc = getCurrentAutomationCC();
        std::vector<AutomationPoint> newPts = getAutomationPointsForCurrentCC();
        if (newPts != automationDragOldPoints)
        {
            processor.undoManager.beginNewTransaction();
            processor.undoManager.perform(new SetAutomationAction(processor, editLaneIdx, editRegionIdx, cc, automationDragOldPoints, newPts));
            if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
        }
        automationDragPoint = -1;
        return;
    }

    // Velocity drag complete
    if (velocityDragNote >= 0 && velocityDragNote < (int)currentClip.notes.size())
    {
        auto& note = currentClip.notes[velocityDragNote];
        if (note.velocity != dragOrigNote.velocity)
        {
            processor.undoManager.beginNewTransaction();
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
            processor.undoManager.beginNewTransaction();
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

        int gs = processor.gridSnap.load();
        double noteLen = (gs == (int)PatternFlowProcessor::GridSize::Off)
            ? 0.25
            : PatternFlowProcessor::getGridDivision((PatternFlowProcessor::GridSize)gs);

        NoteEvent ne;
        ne.startBeat = beat; ne.noteNumber = noteNum; ne.velocity = 100;
        ne.lengthBeats = noteLen; ne.channel = 1;
        // Undoable add
        processor.undoManager.beginNewTransaction();
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
    if (!clipLoaded) return false;

    // Automation view: Delete remove point, Up/Down adjust value
    if (viewMode == EditorViewMode::Automation && clipLoaded)
    {
        auto& pts = getAutomationPointsForCurrentCC();
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            if (selectedAutomationPoint >= 0 && selectedAutomationPoint < (int)pts.size() && pts.size() > 1)
            {
                int cc = getCurrentAutomationCC();
                std::vector<AutomationPoint> oldPts = pts;
                pts.erase(pts.begin() + selectedAutomationPoint);
                processor.undoManager.beginNewTransaction();
                processor.undoManager.perform(new SetAutomationAction(processor, editLaneIdx, editRegionIdx, cc, oldPts, pts));
                selectedAutomationPoint = juce::jlimit(-1, (int)pts.size() - 1, selectedAutomationPoint - 1);
                if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
                repaint();
                return true;
            }
        }
        else if ((key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) && selectedAutomationPoint >= 0 && selectedAutomationPoint < (int)pts.size())
        {
            int delta = key == juce::KeyPress::upKey ? 1 : -1;
            int newVal = juce::jlimit(0, 127, pts[selectedAutomationPoint].value + delta);
            if (newVal != pts[selectedAutomationPoint].value)
            {
                int cc = getCurrentAutomationCC();
                std::vector<AutomationPoint> oldPts = pts;
                pts[selectedAutomationPoint].value = newVal;
                processor.undoManager.beginNewTransaction();
                processor.undoManager.perform(new SetAutomationAction(processor, editLaneIdx, editRegionIdx, cc, oldPts, pts));
                if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
                repaint();
                return true;
            }
        }
    }

    // Expression view: Up/Down adjust velocity of selected note(s)
    if (viewMode == EditorViewMode::Expression)
    {
        std::set<int> toEdit = selectedNotes;
        if (toEdit.empty() && selectedNote >= 0) toEdit.insert(selectedNote);
        if (toEdit.empty()) return false;

        int velDelta = 0;
        if (key == juce::KeyPress::upKey)   velDelta = 1;
        else if (key == juce::KeyPress::downKey) velDelta = -1;
        else return false;

        for (int idx : toEdit)
        {
            if (idx < 0 || idx >= (int)currentClip.notes.size()) continue;
            NoteEvent oldNote = currentClip.notes[idx];
            int newVel = juce::jlimit(1, 127, oldNote.velocity + velDelta);
            if (newVel == oldNote.velocity) continue;
            currentClip.notes[idx].velocity = newVel;
            processor.undoManager.beginNewTransaction();
            processor.undoManager.perform(
                new EditNoteAction(processor, editLaneIdx, editRegionIdx, idx, oldNote, currentClip.notes[idx]));
        }
        if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
        repaint();
        return true;
    }

    if (viewMode != EditorViewMode::Notes) return false;
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
        processor.undoManager.beginNewTransaction();
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
            processor.undoManager.beginNewTransaction();
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
        processor.undoManager.beginNewTransaction();
        processor.undoManager.perform(
            new EditNoteAction(processor, editLaneIdx, editRegionIdx,
                               idx, oldNote, currentClip.notes[idx]));
    }
    if (onClipEdited) onClipEdited(currentClip, editLaneIdx, editRegionIdx);
    repaint();
}

} // namespace pflow
