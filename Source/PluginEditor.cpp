#include "PluginEditor.h"
#include "UndoActions.h"
#include "MidiFileData.h"
#include <algorithm>
#include <cmath>

namespace pflow {

namespace {

constexpr float kLabelFontH   = 12.0f;
constexpr float kTitleFontH     = 80.0f; // 2× prior 40pt wordmark
constexpr float kHeaderComboFontH = 15.0f; // matches ControlPanel scale combos (LabelLarge)

int textWidthPx(float fontHeight, const juce::String& t)
{
    return juce::roundToInt(std::ceil(
        juce::Font(juce::FontOptions(fontHeight)).getStringWidthFloat(t)));
}

int maxHeaderGridLabelWidthPx(float fontHeight)
{
    const char* labels[] = { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T", "Off" };
    int m = 0;
    for (auto* s : labels)
        m = juce::jmax(m, textWidthPx(fontHeight, s));
    return m;
}

int maxHeaderSessionLabelWidthPx(float fontHeight)
{
    const char* labels[] = { "4 bars", "8 bars", "16 bars" };
    int m = 0;
    for (auto* s : labels)
        m = juce::jmax(m, textWidthPx(fontHeight, s));
    return m;
}

int gridComboIdFromProcessorGS(PatternFlowProcessor::GridSize gs)
{
    using GS = PatternFlowProcessor::GridSize;
    switch (gs)
    {
        case GS::Beat: return 1;
        case GS::HalfBeat: return 2;
        case GS::QuarterBeat: return 3;
        case GS::Eighth: return 4;
        case GS::EighthTriplet: return 5;
        case GS::SixteenthTriplet: return 6;
        case GS::Off: return 7;
        case GS::Bar: return 1;
        case GS::Sixteenth: return 3;
        case GS::ThirtySecond: return 4;
        default: return 3;
    }
}

} // namespace

// ── Header components (FigmaExample parity) ───────────────────────────────────
void PatternFlowEditor::RecordButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    // Ensure no rectangular focus/outline is drawn by JUCE; we fully own visuals here.
    g.setOpacity(1.0f);
    auto r = getLocalBounds().toFloat();
    auto cx = r.getCentreX();
    auto cy = r.getCentreY();
    const bool hover = over || down;

    if (isRecording_)
    {
        // bg-red-600 + glow + pulse
        const double t = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        // Tailwind animate-pulse is ~2s ease-in-out; approximate with a sine.
        const float s = 0.5f + 0.5f * std::sin((float)(t * juce::MathConstants<double>::twoPi / 2.0)); // 2s period
        const float pulse = 0.50f + 0.50f * s; // 0.5..1.0
        // Glow must stay inside bounds; otherwise JUCE clips and you see box corners.
        g.setColour(colours::recordRed().withAlpha(0.55f * pulse));
        for (int i = 0; i < 4; ++i)
        {
            const float inset = 0.5f + (float)i * 0.9f;
            g.fillEllipse(r.reduced(inset));
        }

        g.setColour(juce::Colour(0xffdc2626));
        g.fillEllipse(r.reduced(1.0f));

        // inner white dot (lucide Circle fill)
        g.setColour(juce::Colours::white);
        g.fillEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    }
    else
    {
        // bg #252525, border #333, hover border #444
        g.setColour(colours::bgLighter());
        g.fillEllipse(r.reduced(1.0f));

        g.setColour(hover ? juce::Colour(0xff444444) : juce::Colour(0xff333333));
        g.drawEllipse(r.reduced(1.0f), 1.0f);

        // inner red dot (lucide Circle, red-500)
        g.setColour(colours::recordRed());
        g.fillEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    }
}

PatternFlowEditor::PatternFlowEditor(PatternFlowProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      controlPanel(p),
      arrangementView(p),
      pianoRoll(p)
{
    setLookAndFeel(&lnf);
    applyAppTheme(processorRef.appThemeId.load());
    lnf.refreshColours();
    // Default size = minimum resize limits (comfortable baseline)
    setSize(1120, 600);
    setResizable(true, true);
    setResizeLimits(1120, 600, 2400, 1600);
    setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

    lblTitle.setText("Pattern Flow", juce::dontSendNotification);
    lblTitle.setFont(juce::Font(juce::FontOptions(kTitleFontH).withStyle("Medium")));
    lblTitle.setMinimumHorizontalScale(1.0f);
    lblTitle.setColour(juce::Label::textColourId, colours::text());
    addAndMakeVisible(lblTitle);

    updateRecordButton();
    btnRecord.setTooltip("Toggle MIDI recording");
    btnRecord.setWantsKeyboardFocus(false);
    btnRecord.setMouseClickGrabsKeyboardFocus(false);
    btnRecord.onClick = [this]
    {
        if (processorRef.recording.load())
            processorRef.stopRecording();
        else
            processorRef.startRecording();
        updateRecordButton();
        arrangementView.refresh();
    };
    addAndMakeVisible(btnRecord);

    btnHalfTime.setClickingTogglesState(true);
    btnHalfTime.setComponentID("ActionButton");
    btnHalfTime.setTooltip("Half-time playhead (0.5×)");
    btnHalfTime.onClick = [this]
    {
        const bool on = btnHalfTime.getToggleState();
        if (on) btnDoubleTime.setToggleState(false, juce::dontSendNotification);
        processorRef.playheadTempoMul.store(on ? 0.5 : (btnDoubleTime.getToggleState() ? 2.0 : 1.0));
    };
    addAndMakeVisible(btnHalfTime);

    btnDoubleTime.setClickingTogglesState(true);
    btnDoubleTime.setComponentID("ActionButton");
    btnDoubleTime.setTooltip("Double-time playhead (2×)");
    btnDoubleTime.onClick = [this]
    {
        const bool on = btnDoubleTime.getToggleState();
        if (on) btnHalfTime.setToggleState(false, juce::dontSendNotification);
        processorRef.playheadTempoMul.store(on ? 2.0 : (btnHalfTime.getToggleState() ? 0.5 : 1.0));
    };
    addAndMakeVisible(btnDoubleTime);

    btnBpmDisplay.setComponentID("ActionButton");
    btnBpmDisplay.setTooltip("Playhead BPM (after Half/Double)");
    btnBpmDisplay.setWantsKeyboardFocus(false);
    btnBpmDisplay.setMouseClickGrabsKeyboardFocus(false);
    btnBpmDisplay.onClick = [] {};
    addAndMakeVisible(btnBpmDisplay);

    addAndMakeVisible(headerDividerBeforeRecord_);

    btnExport.setComponentID("ActionButton");
    btnExport.setClickingTogglesState(false);
    btnExport.setTooltip("Export Main MIDI to DAW (drag-drop)");
    btnExport.onClick = [this]
    {
        btnExport.setToggleState(false, juce::dontSendNotification);

        // Best-effort: start an external file drag (hosts don't expose “insert clip” APIs to plugins).
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto tempFile = tempDir.getChildFile("PatternFlow-Main.mid");
        double bpm = processorRef.getPlayheadBpm();
        if (bpm <= 0.0) bpm = 120.0;
        bool written = false;
        {
            juce::ScopedLock sl(processorRef.laneLock);
            double sessionLen = (double)(processorRef.arrangementBars.load() * 4);
            written = writeMidiFile(processorRef.combinedClip, tempFile, bpm, sessionLen);
        }
        if (written)
        {
            juce::StringArray files;
            files.add(tempFile.getFullPathName());
            juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
        }
    };
    addAndMakeVisible(btnExport);

    // Grid: labels follow note divisions (1/4 = quarter note = 1 beat, etc.)
    cmbHeaderGrid.addItem("1/4", 1);
    cmbHeaderGrid.addItem("1/8", 2);
    cmbHeaderGrid.addItem("1/16", 3);
    cmbHeaderGrid.addItem("1/32", 4);
    cmbHeaderGrid.addItem("1/8T", 5);
    cmbHeaderGrid.addItem("1/16T", 6);
    cmbHeaderGrid.addItem("Off", 7);
    cmbHeaderGrid.setSelectedId(gridComboIdFromProcessorGS((PatternFlowProcessor::GridSize)processorRef.gridSnap.load()));
    cmbHeaderGrid.setTooltip("Grid snap / arrangement lines");
    cmbHeaderGrid.addListener(this);
    addAndMakeVisible(cmbHeaderGrid);

    lblSessionBars.setColour(juce::Label::textColourId, colours::textDim());
    lblSessionBars.setFont(juce::Font(juce::FontOptions(kLabelFontH)));
    lblSessionBars.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(lblSessionBars);
    lblSessionBars.setVisible(false);
    {
        const int barOptions[] = { 4, 8, 16 };
        for (int i = 0; i < 3; ++i)
            cmbHeaderSession.addItem(juce::String(barOptions[i]) + " bars", i + 1);
        int bars = processorRef.arrangementBars.load();
        int id = 1;
        for (int i = 0; i < 3; ++i)
            if (bars <= barOptions[i]) { id = i + 1; break; }
            else id = i + 2;
        if (bars > 16) { processorRef.arrangementBars.store(16); id = 3; }
        cmbHeaderSession.setSelectedId(id);
    }
    cmbHeaderSession.setTooltip("Session length in bars");
    cmbHeaderSession.addListener(this);
    addAndMakeVisible(cmbHeaderSession);

    btnStep.setComponentID("ActionButton");
    btnStep.setTooltip("Arrange clips in stair-step order");
    btnExtend.setComponentID("ActionButton");
    btnExtend.setTooltip("Align all clips to session start, extend to session end (loop if needed)");
    btnTrim.setComponentID("ActionButton");
    btnTrim.setTooltip("Remove empty measures from each selected clip");
    btnStep.setClickingTogglesState(false);
    btnExtend.setClickingTogglesState(false);
    btnTrim.setClickingTogglesState(false);
    btnStep.setToggleState(false, juce::dontSendNotification);
    btnExtend.setToggleState(false, juce::dontSendNotification);
    btnTrim.setToggleState(false, juce::dontSendNotification);

    btnStep.onClick = [this]
    {
        btnStep.setToggleState(false, juce::dontSendNotification);
        juce::ScopedLock sl(processorRef.laneLock);
        int numLanes = (int)processorRef.lanes.size();
        if (numLanes < 1) return;
        double sessionLen = (double)(processorRef.arrangementBars.load() * 4);
        double segLen = sessionLen / (double)numLanes;
        if (segLen < 0.25) segLen = 0.25;
        auto presets = getClipColourPresets();
        std::vector<CompLane> newLanes;
        double curEnd = 0.0;
        for (int li = 0; li < numLanes; ++li)
        {
            auto& srcLane = processorRef.lanes[li];
            int bestRi = -1;
            double bestStart = 1e99;
            for (int ri = 0; ri < (int)srcLane.regions.size(); ++ri)
            {
                auto& r = srcLane.regions[ri];
                if (r.clipIndex < 0 || r.clipIndex >= (int)srcLane.clips.size()) continue;
                if (r.startBeat < bestStart) { bestStart = r.startBeat; bestRi = ri; }
            }
            if (bestRi < 0) continue;
            auto& r = srcLane.regions[bestRi];
            MidiClip clip = srcLane.clips[r.clipIndex];
            double useLen = segLen;
            if (clip.lengthBeats > useLen)
            {
                clip.lengthBeats = useLen;
                clip.notes.erase(std::remove_if(clip.notes.begin(), clip.notes.end(),
                    [useLen](const NoteEvent& n) { return n.startBeat + n.lengthBeats > useLen; }), clip.notes.end());
                for (auto& note : clip.notes)
                    if (note.startBeat + note.lengthBeats > useLen) note.lengthBeats = useLen - note.startBeat;
            }
            CompLane lane;
            lane.name = "Lane " + juce::String(li + 1);
            lane.colour = presets[li % presets.size()];
            lane.clips.push_back(std::move(clip));
            CompRegion reg = r;
            reg.clipIndex = 0;
            reg.startBeat = curEnd;
            reg.endBeat = curEnd + useLen;
            lane.regions.push_back(reg);
            newLanes.push_back(std::move(lane));
            curEnd += useLen;
        }
        processorRef.lanes = std::move(newLanes);
        processorRef.rebuildCombinedClip();
        arrangementView.refresh();
    };
    btnExtend.onClick = [this]
    {
        btnExtend.setToggleState(false, juce::dontSendNotification);
        juce::ScopedLock sl(processorRef.laneLock);
        double sessionLen = (double)(processorRef.arrangementBars.load() * 4);
        for (auto& lane : processorRef.lanes)
        {
            for (auto& region : lane.regions)
            {
                if (region.clipIndex < 0 || region.clipIndex >= (int)lane.clips.size()) continue;
                region.startBeat = 0.0;
                region.endBeat = sessionLen;
                region.ensureSelectionInBounds();
            }
        }
        processorRef.rebuildCombinedClip();
        arrangementView.refresh();
    };
    btnTrim.onClick = [this]
    {
        btnTrim.setToggleState(false, juce::dontSendNotification);
        // Apply trim to all clips (no selection required).
        std::vector<std::pair<int, int>> all;
        {
            juce::ScopedLock sl(processorRef.laneLock);
            for (int li = 0; li < (int)processorRef.lanes.size(); ++li)
                for (int ri = 0; ri < (int)processorRef.lanes[li].regions.size(); ++ri)
                    all.push_back({ li, ri });
        }
        processorRef.trimEmptyMeasuresInSelectedClips(all);
        arrangementView.refresh();
        if (pianoRoll.hasClip())
        {
            int li = pianoRoll.getEditLaneIndex();
            int ri = pianoRoll.getEditRegionIndex();
            juce::ScopedLock sl(processorRef.laneLock);
            if (processorRef.isValidRegion(li, ri))
            {
                auto& lane = processorRef.lanes[li];
                int ci = lane.regions[ri].clipIndex;
                if (ci >= 0 && ci < (int)lane.clips.size())
                    pianoRoll.setClip(lane.clips[ci], li, ri);
            }
        }
        resized();
    };
    addAndMakeVisible(btnStep);
    addAndMakeVisible(btnExtend);
    addAndMakeVisible(btnTrim);
    addAndMakeVisible(headerDivider_);

    refreshTransportColours();

    btnSettings.setButtonText(juce::String::charToString(0x2699));
    btnSettings.setComponentID("HeaderIcon");
    btnSettings.setTooltip("Settings & Info");
    btnSettings.onClick = [this] { showSettingsDialog(); };
    addAndMakeVisible(btnSettings);

    // Keyboard shortcuts - add as key listener to children so 'f', etc. work when they have focus
    addKeyListener(this);
    setWantsKeyboardFocus(true);
    controlPanel.addKeyListener(this);
    arrangementView.addKeyListener(this);
    fileBrowser.addKeyListener(this);
    pianoRoll.addKeyListener(this);

    // Accessibility descriptions
    setTitle("PatternFlow Editor");
    setDescription("Main editor window for PatternFlow MIDI composition tool");
    controlPanel.setTitle("Control Panel");
    controlPanel.setDescription("Comping tools and scale quantisation");
    fileBrowser.setTitle("File Browser");
    fileBrowser.setDescription("Browse and select MIDI files to add to the arrangement");
    arrangementView.setTitle("Arrangement View");
    arrangementView.setDescription("Arrange MIDI clips on lanes. Use arrow keys to navigate regions.");
    pianoRoll.setTitle("Piano Roll Editor");
    pianoRoll.setDescription("Edit individual MIDI notes in the selected clip");
    btnSettings.setTitle("Settings");
    btnSettings.setDescription("Open settings and about");

    // Make panels focusable for keyboard navigation
    controlPanel.setWantsKeyboardFocus(true);
    fileBrowser.setWantsKeyboardFocus(true);
    arrangementView.setWantsKeyboardFocus(true);
    pianoRoll.setWantsKeyboardFocus(true);

    addAndMakeVisible(controlPanel);
    controlPanel.onTakeCompSwitched = [this] { arrangementView.refresh(); };

    controlPanel.onC0Clicked = [this]
    {
        juce::ScopedLock sl(processorRef.laneLock);
        int minNote = 127, maxNote = 0;
        for (auto& lane : processorRef.lanes)
            for (auto& clip : lane.clips)
                for (auto& n : clip.notes)
                {
                    minNote = std::min(minNote, n.noteNumber);
                    maxNote = std::max(maxNote, n.noteNumber);
                }
        if (minNote > maxNote) return;
        if (minNote >= 0 && maxNote <= 11) return;  // Already in C0 range
        int range = maxNote - minNote + 1;
        for (auto& lane : processorRef.lanes)
        {
            for (auto& region : lane.regions)
                region.noteFilter = -1;  // Clear pitch filter so 0-11 notes play
            for (auto& clip : lane.clips)
                for (auto& n : clip.notes)
                {
                    int nn;
                    if (range <= 12)
                        nn = n.noteNumber - minNote;  // Transpose so whole range fits in 0-11
                    else
                        nn = n.noteNumber % 12;     // Fold into pitch class (0-11)
                    n.noteNumber = juce::jlimit(0, 127, nn);
                }
        }
        processorRef.rebuildCombinedClip();
        arrangementView.refresh();
        // Refresh piano roll with updated clip so display and edits stay in sync
        if (pianoRoll.hasClip())
        {
            int li = pianoRoll.getEditLaneIndex();
            int ri = pianoRoll.getEditRegionIndex();
            if (li >= 0 && li < (int)processorRef.lanes.size())
            {
                auto& lane = processorRef.lanes[li];
                if (ri >= 0 && ri < (int)lane.regions.size())
                {
                    int ci = lane.regions[ri].clipIndex;
                    if (ci >= 0 && ci < (int)lane.clips.size())
                        pianoRoll.setClip(lane.clips[ci], li, ri);
                }
            }
            pianoRoll.repaint();
        }
    };

    // Transpose is now a live toggle in the ControlPanel.

    arrangementView.onAddLaneClicked = [this]
    {
        juce::ScopedLock sl(processorRef.laneLock);
        CompLane newLane;
        int idx = (int)processorRef.lanes.size();
        auto presets = getClipColourPresets();
        newLane.name   = "Lane " + juce::String(idx + 1);
        newLane.colour = presets[idx % presets.size()];
        processorRef.lanes.push_back(newLane);
        arrangementView.refresh();
    };
    arrangementView.onLoopChanged = [] { /* loop UI lives in control panel */ };

    // File browser - restore last directory
    if (processorRef.lastBrowserDir.isNotEmpty())
    {
        juce::File dir(processorRef.lastBrowserDir);
        if (dir.isDirectory())
            fileBrowser.setRootDirectory(dir);
    }
    fileBrowser.onDirectoryChanged = [this](const juce::String& path)
    {
        processorRef.lastBrowserDir = path;
    };
    fileBrowser.onClipDoubleClicked = [this](const MidiClip& clip)
    {
        double beat = processorRef.hostPlaying.load() ? processorRef.hostBeatPos.load() : processorRef.editPlayheadBeat.load();
        arrangementView.addClipToNewLane(clip, beat);
        arrangementView.refresh();
        resized();
    };
    fileBrowser.onClipAddToNewLane = [this](const MidiClip& clip)
    {
        double beat = processorRef.hostPlaying.load() ? processorRef.hostBeatPos.load() : processorRef.editPlayheadBeat.load();
        arrangementView.addClipToNewLane(clip, beat);
        arrangementView.refresh();
        resized();
    };
    fileBrowser.onGetPlayheadState = [this]
    {
        double beat = processorRef.hostPlaying.load() ? processorRef.hostBeatPos.load() : processorRef.editPlayheadBeat.load();
        return std::make_tuple(beat, processorRef.loopStartBeat.load(), processorRef.loopEndBeat.load(), processorRef.loopEnabled.load());
    };
    fileBrowser.onGetSessionLengthBeats = [this]
    {
        return (double)(processorRef.arrangementBars.load() * 4);
    };
    fileBrowser.onClipAddFromBrowser = [this](const MidiClip& clip)
    {
        double beat = processorRef.hostPlaying.load() ? processorRef.hostBeatPos.load() : processorRef.editPlayheadBeat.load();
        juce::ScopedLock sl(processorRef.laneLock);
        bool hasLane1 = processorRef.lanes.size() >= 1 && processorRef.lanes[0].regions.empty();
        if (hasLane1)
            arrangementView.addClipToLane(clip, 0, beat);
        else
            arrangementView.addClipToNewLane(clip, beat);
        arrangementView.refresh();
        resized();
    };
    fileBrowser.onClipAddToFocusedLaneColumn = [this](const MidiClip& clip)
    {
        double beat = processorRef.hostPlaying.load() ? processorRef.hostBeatPos.load() : processorRef.editPlayheadBeat.load();
        juce::ScopedLock sl(processorRef.laneLock);
        int lane = arrangementView.selLane;
        if (lane < 0) lane = 0;
        if (processorRef.lanes.empty())
        {
            arrangementView.addClipToNewLane(clip, beat);
        }
        else
        {
            lane = juce::jlimit(0, (int)processorRef.lanes.size() - 1, lane);
            arrangementView.addClipToLane(clip, lane, beat);
        }
        arrangementView.refresh();
        resized();
    };
    addAndMakeVisible(fileBrowser);

    arrangementView.onClipDoubleClicked = [this](const MidiClip& clip, int laneIdx, int regionIdx)
    {
        pianoRoll.setClip(clip, laneIdx, regionIdx);
        resized();
    };
    arrangementView.onEmptyArrangementDoubleClicked = [this]
    {
        pianoRoll.clearClip();
        resized();
    };
    addAndMakeVisible(arrangementView);

    leftResizer_ = std::make_unique<ResizerStrip>(ResizerStrip::Direction::Vertical,
        browserWidth_, metrics::browserMinWidth, metrics::browserMaxWidth,
        [this]() { return getWidth(); },
        [this]() { resized(); });
    addAndMakeVisible(*leftResizer_);

    bottomResizer_ = std::make_unique<ResizerStrip>(ResizerStrip::Direction::Horizontal,
        pianoRollHeight_, 100, 800,
        [this]() {
            return getHeight() - 2 * metrics::titleBarH;
        },
        [this]() { resized(); });
    addAndMakeVisible(*bottomResizer_);

    // Piano roll (starts hidden)
    pianoRoll.onClipEdited = [this](const MidiClip& editedClip, int laneIdx, int regionIdx)
    {
        juce::ScopedLock sl(processorRef.laneLock);
        if (laneIdx >= 0 && laneIdx < (int)processorRef.lanes.size())
        {
            auto& lane = processorRef.lanes[laneIdx];
            if (regionIdx >= 0 && regionIdx < (int)lane.regions.size())
            {
                auto& region = lane.regions[regionIdx];
                int clipIdx = region.clipIndex;
                if (clipIdx >= 0 && clipIdx < (int)lane.clips.size())
                {
                    double oldClipLen = lane.clips[clipIdx].lengthBeats;
                    double regionLen = region.endBeat - region.startBeat;
                    bool regionMatchedClip = std::abs(regionLen - oldClipLen) < 1e-4;
                    lane.clips[clipIdx] = editedClip;
                    if (regionMatchedClip)
                    {
                        region.endBeat = region.startBeat + editedClip.lengthBeats;
                        region.ensureSelectionInBounds();
                    }
                }
            }
        }
        processorRef.rebuildCombinedClip();
        arrangementView.refresh();
    };
    addAndMakeVisible(pianoRoll);

    // Repaint timer for playhead animation
    startTimerHz(30);
    controlPanel.refreshTakeCompUI();
}

void PatternFlowEditor::parentHierarchyChanged()
{
    if (isShowing())
    {
        grabKeyboardFocus();
        juce::Timer::callAfterDelay(100, [this]() { if (isVisible()) grabKeyboardFocus(); });
        juce::Timer::callAfterDelay(400, [this]() { if (isVisible()) grabKeyboardFocus(); });
    }
}

void PatternFlowEditor::focusGained(juce::Component::FocusChangeType)
{
    grabKeyboardFocus();
}

PatternFlowEditor::~PatternFlowEditor()
{
    removeKeyListener(this);
    controlPanel.removeKeyListener(this);
    arrangementView.removeKeyListener(this);
    fileBrowser.removeKeyListener(this);
    pianoRoll.removeKeyListener(this);
    setLookAndFeel(nullptr);
}

void PatternFlowEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg());
    // Toolbar: row1 = logo + transport + settings; row2 = comp + scale
    g.setColour(colours::bgLight());
    const int headerH = metrics::titleBarH + metrics::controlStripH;
    g.fillRect(0.0f, 0.0f, (float)getWidth(), (float)headerH);
    g.setColour(colours::panelBorder());
    g.drawHorizontalLine(metrics::titleBarH - 1, 0.0f, (float)getWidth());
    g.drawHorizontalLine(headerH - 1, 0.0f, (float)getWidth());

    if (themeTransitionAlpha_ > 0.0f && themeTransitionSnapshot_.isValid())
    {
        g.setOpacity(themeTransitionAlpha_);
        g.drawImageAt(themeTransitionSnapshot_, 0, 0);
        g.setOpacity(1.0f);
    }
}

void PatternFlowEditor::showSettingsDialog()
{
    auto* dialog = new juce::DialogWindow::LaunchOptions();
    const int panelW = 420;
    const int rowH = 28;
    int y = 12;

    auto* content = new juce::Component();
    content->setSize(panelW, 580);

    auto addLabel = [&](const juce::String& text, int w)
    {
        auto* l = new juce::Label({}, text);
        l->setFont(juce::Font(juce::FontOptions(12.0f)));
        l->setColour(juce::Label::textColourId, colours::textDim());
        l->setBounds(20, y, w, rowH);
        content->addAndMakeVisible(l);
    };

    addLabel("MIDI In", 80);
    auto* cmbMidiIn = new juce::ComboBox();
    cmbMidiIn->addItem("(Host)", 1);
    cmbMidiIn->setSelectedId(1);
    cmbMidiIn->setBounds(110, y, 260, rowH);
    cmbMidiIn->setColour(juce::ComboBox::backgroundColourId, colours::bgLight());
    cmbMidiIn->setColour(juce::ComboBox::textColourId, colours::textBright());
    content->addAndMakeVisible(cmbMidiIn);
    y += rowH + 8;

    addLabel("MIDI Out", 80);
    auto* cmbMidiOut = new juce::ComboBox();
    cmbMidiOut->addItem("(Host)", 1);
    cmbMidiOut->setSelectedId(1);
    cmbMidiOut->setBounds(110, y, 260, rowH);
    cmbMidiOut->setColour(juce::ComboBox::backgroundColourId, colours::bgLight());
    cmbMidiOut->setColour(juce::ComboBox::textColourId, colours::textBright());
    content->addAndMakeVisible(cmbMidiOut);
    y += rowH + 8;

    addLabel("MIDI Channel", 80);
    auto* cmbChannel = new juce::ComboBox();
    for (int ch = 1; ch <= 16; ++ch) cmbChannel->addItem(juce::String(ch), ch);
    cmbChannel->setSelectedId(1);
    cmbChannel->setBounds(110, y, 80, rowH);
    cmbChannel->setColour(juce::ComboBox::backgroundColourId, colours::bgLight());
    cmbChannel->setColour(juce::ComboBox::textColourId, colours::textBright());
    content->addAndMakeVisible(cmbChannel);
    y += rowH + 16;

    auto* titleLabel = new juce::Label({}, version::name);
    titleLabel->setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
    titleLabel->setColour(juce::Label::textColourId, colours::accent());
    titleLabel->setBounds(20, y, 360, 24);
    content->addAndMakeVisible(titleLabel);
    y += 26;

    auto* versionLabel = new juce::Label({}, juce::String("v") + build_info::kVersion);
    versionLabel->setFont(juce::Font(juce::FontOptions(12.0f)));
    versionLabel->setColour(juce::Label::textColourId, colours::textDim());
    versionLabel->setBounds(20, y, 360, 18);
    content->addAndMakeVisible(versionLabel);
    y += 22;

    auto* buildLabel = new juce::Label({}, juce::String(build_info::kGitDateIso) + "\n"
        + build_info::kGitHash + " — " + build_info::kGitSubject);
    buildLabel->setFont(juce::Font(juce::FontOptions(10.0f)));
    buildLabel->setColour(juce::Label::textColourId, colours::textDim());
    buildLabel->setBounds(20, y, 380, 44);
    buildLabel->setMinimumHorizontalScale(1.0f);
    content->addAndMakeVisible(buildLabel);
    y += 48;

    auto* descLabel = new juce::Label({}, version::desc);
    descLabel->setFont(juce::Font(juce::FontOptions(11.0f)));
    descLabel->setColour(juce::Label::textColourId, colours::text());
    descLabel->setBounds(20, y, 380, 60);
    descLabel->setMinimumHorizontalScale(1.0f);
    content->addAndMakeVisible(descLabel);
    y += 64;

    auto* licenseEditor = new juce::TextEditor();
    licenseEditor->setMultiLine(true, true);
    licenseEditor->setReadOnly(true);
    licenseEditor->setScrollbarsShown(true);
    licenseEditor->setColour(juce::TextEditor::backgroundColourId, colours::bgLight());
    licenseEditor->setColour(juce::TextEditor::textColourId, colours::textDim());
    licenseEditor->setColour(juce::TextEditor::outlineColourId, colours::panelBorder());
    licenseEditor->setFont(juce::Font(juce::FontOptions(11.0f)));
    licenseEditor->setText(version::license);
    licenseEditor->setBounds(20, y, 380, 120);
    content->addAndMakeVisible(licenseEditor);

    auto* viewport = new juce::Viewport();
    viewport->setViewedComponent(content, false);
    viewport->setScrollBarsShown(true, false);
    viewport->setSize(panelW, 400);

    auto* wrapper = new juce::Component();
    wrapper->setSize(panelW, 460);
    wrapper->addAndMakeVisible(viewport);
    viewport->setBounds(0, 0, panelW, 400);

    auto* closeBtn = new juce::TextButton("Close");
    closeBtn->setColour(juce::TextButton::buttonColourId, colours::accent());
    closeBtn->setColour(juce::TextButton::textColourOffId, colours::textBright());
    closeBtn->setBounds(panelW / 2 - 45, 408, 90, 28);
    closeBtn->onClick = [wrapper]
    {
        if (auto* dw = wrapper->findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    wrapper->addAndMakeVisible(closeBtn);

    dialog->content.setOwned(wrapper);
    dialog->dialogTitle = "Settings & Info";
    dialog->dialogBackgroundColour = colours::bg();
    dialog->escapeKeyTriggersCloseButton = true;
    dialog->useNativeTitleBar = false;
    dialog->resizable = true;
    dialog->launchAsync();
}

bool PatternFlowEditor::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    // Return key in file browser: add to new lane (must run first to avoid triggering tab focus)
    if (key == juce::KeyPress::returnKey)
    {
        if (key.getModifiers().isCommandDown())
        {
            fileBrowser.grabKeyboardFocus();
            return true;
        }
        if (originatingComponent && (originatingComponent == &fileBrowser || fileBrowser.isParentOf(originatingComponent)))
        {
            if (fileBrowser.tryAddSelectedToNewLane())
                return true;
        }
    }
    // Undo: Cmd/Ctrl + Z
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
    {
        if (processorRef.undoManager.undo())
        {
            arrangementView.refresh();
            // Refresh piano roll if open
            if (pianoRoll.hasClip())
            {
                int li = pianoRoll.getEditLaneIndex();
                int ri = pianoRoll.getEditRegionIndex();
                juce::ScopedLock sl(processorRef.laneLock);
                if (li >= 0 && li < (int)processorRef.lanes.size())
                {
                    auto& lane = processorRef.lanes[li];
                    if (ri >= 0 && ri < (int)lane.regions.size())
                    {
                        int ci = lane.regions[ri].clipIndex;
                        if (ci >= 0 && ci < (int)lane.clips.size())
                            pianoRoll.setClip(lane.clips[ci], li, ri);
                    }
                }
            }
        }
        return true;
    }

    // Redo: Cmd/Ctrl + Shift + Z
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (processorRef.undoManager.redo())
        {
            arrangementView.refresh();
            if (pianoRoll.hasClip())
            {
                int li = pianoRoll.getEditLaneIndex();
                int ri = pianoRoll.getEditRegionIndex();
                juce::ScopedLock sl(processorRef.laneLock);
                if (li >= 0 && li < (int)processorRef.lanes.size())
                {
                    auto& lane = processorRef.lanes[li];
                    if (ri >= 0 && ri < (int)lane.regions.size())
                    {
                        int ci = lane.regions[ri].clipIndex;
                        if (ci >= 0 && ci < (int)lane.clips.size())
                            pianoRoll.setClip(lane.clips[ci], li, ri);
                    }
                }
            }
        }
        return true;
    }

    // Delete selected region (undoable)
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (arrangementView.selLane >= 0 && arrangementView.selRegion >= 0)
        {
            processorRef.undoManager.beginNewTransaction();
            processorRef.undoManager.perform(
                new RemoveRegionAction(processorRef, arrangementView.selLane, arrangementView.selRegion));
            arrangementView.selLane = -1;
            arrangementView.selRegion = -1;
            arrangementView.refresh();
            return true;
        }
    }

    // Zoom in: Cmd/Ctrl + =
    if (key == juce::KeyPress('+', juce::ModifierKeys::commandModifier, 0) ||
        key == juce::KeyPress('=', juce::ModifierKeys::commandModifier, 0))
    {
        arrangementView.beatsPerPixel = std::max(0.01f, arrangementView.beatsPerPixel * 0.8f);
        arrangementView.refresh();
        return true;
    }

    // Zoom out: Cmd/Ctrl + -
    if (key == juce::KeyPress('-', juce::ModifierKeys::commandModifier, 0))
    {
        arrangementView.beatsPerPixel = std::min(2.0f, arrangementView.beatsPerPixel * 1.25f);
        arrangementView.refresh();
        return true;
    }

    // Arrow keys: move selected clip (left/right = timeline, up/down = between lanes)
    if (arrangementView.selLane >= 0 && arrangementView.selRegion >= 0 &&
        (key.getKeyCode() == juce::KeyPress::leftKey || key.getKeyCode() == juce::KeyPress::rightKey ||
         key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey))
    {
        juce::ScopedLock sl(processorRef.laneLock);
        int numLanes = (int)processorRef.lanes.size();
        int lane = arrangementView.selLane;
        int reg = arrangementView.selRegion;
        if (lane >= numLanes || reg >= (int)processorRef.lanes[lane].regions.size())
            return true;

        auto& region = processorRef.lanes[lane].regions[reg];
        double len = region.endBeat - region.startBeat;

        if (key.getKeyCode() == juce::KeyPress::leftKey)
        {
            double newStart = processorRef.snapBeat(std::max(0.0, region.startBeat - 1.0));
            if (std::abs(newStart - region.startBeat) > 0.001)
            {
                double oldStart = region.startBeat, oldEnd = region.endBeat;
                processorRef.undoManager.beginNewTransaction();
                processorRef.undoManager.perform(
                    new MoveRegionAction(processorRef, lane, reg,
                                         oldStart, oldEnd, newStart, newStart + len));
            }
        }
        else if (key.getKeyCode() == juce::KeyPress::rightKey)
        {
            double newStart = processorRef.snapBeat(region.startBeat + 1.0);
            double oldStart = region.startBeat, oldEnd = region.endBeat;
            processorRef.undoManager.beginNewTransaction();
            processorRef.undoManager.perform(
                new MoveRegionAction(processorRef, lane, reg,
                                     oldStart, oldEnd, newStart, newStart + len));
        }
        else if (key.getKeyCode() == juce::KeyPress::upKey && lane > 0)
        {
            int dstLane = lane - 1;
            double s = region.startBeat, e2 = region.endBeat;
            processorRef.undoManager.beginNewTransaction();
            processorRef.undoManager.perform(
                new MoveRegionToLaneAction(processorRef, lane, reg, dstLane, s, e2, false));
            arrangementView.selLane = dstLane;
            arrangementView.selRegion = (int)processorRef.lanes[dstLane].regions.size() - 1;
        }
        else if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            int dstLane = lane + 1;
            bool createNew = (dstLane >= numLanes);
            double s = region.startBeat, e2 = region.endBeat;
            processorRef.undoManager.beginNewTransaction();
            processorRef.undoManager.perform(
                new MoveRegionToLaneAction(processorRef, lane, reg, dstLane, s, e2, createNew));
            arrangementView.selLane = dstLane;
            arrangementView.selRegion = (int)processorRef.lanes[dstLane].regions.size() - 1;
        }
        arrangementView.refresh();
        return true;
    }

    // Arrow keys with no selection: select first available region
    if ((key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey ||
         key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) &&
        arrangementView.selLane < 0)
    {
        juce::ScopedLock sl(processorRef.laneLock);
        int numLanes = (int)processorRef.lanes.size();
        for (int li = 0; li < numLanes; ++li)
        {
            if (!processorRef.lanes[li].regions.empty())
            {
                arrangementView.selLane = li;
                arrangementView.selRegion = 0;
                arrangementView.refresh();
                return true;
            }
        }
        return false;
    }

    // M key to mute/unmute selected region
    if (key == juce::KeyPress('m') && arrangementView.selLane >= 0 && arrangementView.selRegion >= 0)
    {
        processorRef.undoManager.beginNewTransaction();
        processorRef.undoManager.perform(
            new ToggleMuteAction(processorRef, arrangementView.selLane, arrangementView.selRegion));
        arrangementView.refresh();
        return true;
    }

    // Cmd+D: Duplicate selected region
    if (key == juce::KeyPress('d', juce::ModifierKeys::commandModifier, 0))
    {
        if (arrangementView.selLane >= 0 && arrangementView.selRegion >= 0)
        {
            processorRef.undoManager.beginNewTransaction();
            processorRef.undoManager.perform(
                new DuplicateRegionAction(processorRef, arrangementView.selLane, arrangementView.selRegion));
            arrangementView.refresh();
        }
        return true;
    }

    // Cmd+B: Split region at playhead
    if (key == juce::KeyPress('b', juce::ModifierKeys::commandModifier, 0))
    {
        if (arrangementView.selLane >= 0 && arrangementView.selRegion >= 0)
        {
            double playBeat;
            if (processorRef.hostPlaying.load())
                playBeat = processorRef.loopEnabled.load()
                    ? processorRef.mappedBeatPos.load()
                    : processorRef.hostBeatPos.load();
            else
                playBeat = processorRef.editPlayheadBeat.load();
            juce::ScopedLock sl(processorRef.laneLock);
            if (arrangementView.selLane < (int)processorRef.lanes.size())
            {
                auto& lane = processorRef.lanes[arrangementView.selLane];
                if (arrangementView.selRegion < (int)lane.regions.size())
                {
                    auto& reg = lane.regions[arrangementView.selRegion];
                    if (playBeat > reg.startBeat && playBeat < reg.endBeat)
                    {
                        processorRef.undoManager.beginNewTransaction();
                        processorRef.undoManager.perform(
                            new SplitRegionAction(processorRef, arrangementView.selLane,
                                                  arrangementView.selRegion, playBeat));
                    }
                }
            }
            arrangementView.refresh();
        }
        return true;
    }

    // F: Toggle file browser
    if (key == juce::KeyPress('f', juce::ModifierKeys::noModifiers, 0))
    {
        fileBrowserVisible_ = !fileBrowserVisible_;
        resized();
        return true;
    }
    // L: Toggle loop on/off (uses selection area if set, else 4 bars default)
    if (key == juce::KeyPress('l', juce::ModifierKeys::noModifiers, 0))
    {
        bool nowEnabled = !processorRef.loopEnabled.load();
        if (nowEnabled)
        {
            double startB = 0.0, endB = 16.0;
            if (arrangementView.hasTimeSelection && (arrangementView.timeSelEndBeat - arrangementView.timeSelStartBeat) > 0.01)
            {
                startB = arrangementView.timeSelStartBeat;
                endB = arrangementView.timeSelEndBeat;
            }
            else if (processorRef.loopEndBeat.load() <= processorRef.loopStartBeat.load())
            {
                juce::ScopedLock sl(processorRef.laneLock);
                for (const auto& lane : processorRef.lanes)
                    for (const auto& reg : lane.regions)
                    {
                        if (startB == 0.0 && endB == 16.0) { startB = reg.startBeat; endB = reg.endBeat; }
                        else { startB = std::min(startB, reg.startBeat); endB = std::max(endB, reg.endBeat); }
                    }
                if (endB <= startB) { startB = 0.0; endB = 16.0; }
            }
            processorRef.loopStartBeat.store(startB);
            processorRef.loopEndBeat.store(endB > startB ? endB : startB + 16.0);
        }
        processorRef.loopEnabled.store(nowEnabled);
        // loop UI lives in control panel
        arrangementView.refresh();
        return true;
    }
    // Cmd+L: Set loop from selection/clip (Ableton-style)
    if (key == juce::KeyPress('l', juce::ModifierKeys::commandModifier, 0))
    {
        arrangementView.toggleLoopFromContext();
        return true;
    }

    // Cmd+E: Export MIDI
    if (key == juce::KeyPress('e', juce::ModifierKeys::commandModifier, 0))
    {
        exportMidi();
        return true;
    }

    // Cmd+Q: Quantize selected notes in piano roll
    if (key == juce::KeyPress('q', juce::ModifierKeys::commandModifier, 0))
    {
        if (pianoRoll.hasClip())
            pianoRoll.quantizeSelectedNotes();
        return true;
    }

    // Cmd+Shift+Up: Transpose selected notes up an octave
    if (key == juce::KeyPress(juce::KeyPress::upKey,
                              juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (pianoRoll.hasClip())
            pianoRoll.transposeSelectedNotes(12);
        return true;
    }

    // Cmd+Shift+Down: Transpose selected notes down an octave
    if (key == juce::KeyPress(juce::KeyPress::downKey,
                              juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (pianoRoll.hasClip())
            pianoRoll.transposeSelectedNotes(-12);
        return true;
    }

    // Cmd+0: Zoom to fit
    if (key == juce::KeyPress('0', juce::ModifierKeys::commandModifier, 0))
    {
        zoomToFit();
        return true;
    }

    // Tab to cycle focus between panels
    if (key == juce::KeyPress::tabKey)
    {
        if (controlPanel.hasKeyboardFocus(true))
            fileBrowser.grabKeyboardFocus();
        else if (fileBrowser.hasKeyboardFocus(true))
            arrangementView.grabKeyboardFocus();
        else if (arrangementView.hasKeyboardFocus(true))
        {
            if (pianoRoll.isVisible())
                pianoRoll.grabKeyboardFocus();
            else
                controlPanel.grabKeyboardFocus();
        }
        else
            controlPanel.grabKeyboardFocus();
        return true;
    }

    return false;
}

// ── ResizerStrip (VS Code-style panel resize) ───────────────────────────────

PatternFlowEditor::ResizerStrip::ResizerStrip(Direction d, int& valueRef, int minVal, int maxVal,
                                              std::function<int()> getTotalSize, std::function<void()> onResize)
    : direction_(d), valueRef_(&valueRef), minVal_(minVal), maxVal_(maxVal),
      getTotalSize_(std::move(getTotalSize)), onResize_(std::move(onResize))
{
    setMouseCursor(d == Direction::Vertical ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
}

void PatternFlowEditor::ResizerStrip::paint(juce::Graphics& g)
{
    g.fillAll(colours::panelBorder());
    if (direction_ == Direction::Vertical)
        g.fillRect(getWidth() / 2.0f - 1.0f, 0.0f, 2.0f, (float)getHeight());
    else
        g.fillRect(0.0f, getHeight() / 2.0f - 1.0f, (float)getWidth(), 2.0f);
}

void PatternFlowEditor::ResizerStrip::mouseDown(const juce::MouseEvent&)
{
    dragStartValue_ = *valueRef_;
    dragStartPos_    = (direction_ == Direction::Vertical) ? getMouseXYRelative().x : getMouseXYRelative().y;
}

void PatternFlowEditor::ResizerStrip::mouseDrag(const juce::MouseEvent& e)
{
    int pos = (direction_ == Direction::Vertical) ? e.getPosition().x : e.getPosition().y;
    int delta = pos - dragStartPos_;
    int total = getTotalSize_();
    int newVal = (direction_ == Direction::Vertical)
        ? juce::jlimit(minVal_, maxVal_, dragStartValue_ + delta)
        : juce::jlimit(minVal_, std::min(maxVal_, total - 50), dragStartValue_ - delta);
    *valueRef_ = newVal;
    if (onResize_) onResize_();
}

void PatternFlowEditor::resized()
{
    auto b = getLocalBounds();

    // Row 1: [title] | [record + combos + …] | [settings]
    const int pad = metrics::titleBarPadding;
    const int gearPad = juce::roundToInt(12.0f * metrics::uiScale);
    const int gearIconPx = 40;
    const int gearBtnW = gearPad * 2 + gearIconPx;
    auto topRow = b.removeFromTop(metrics::titleBarH);
    const int px = metrics::padding; // scaled
    topRow.removeFromLeft(px);
    topRow.removeFromRight(px);

    const int btnH = juce::jlimit(22, 28, topRow.getHeight() - 4); // match ControlPanel scale row
    const int rowY = topRow.getY() + (topRow.getHeight() - btnH) / 2;
    const int titleW = textWidthPx(kTitleFontH, "Pattern Flow");
    const int leftClusterW = titleW + 6;
    auto leftCluster = topRow.removeFromLeft(leftClusterW);

    lblTitle.setBounds(leftCluster.getX(), topRow.getY(), titleW + 6, topRow.getHeight());

    auto gearArea = topRow.removeFromRight(gearBtnW + pad * 2);
    btnSettings.setBounds(gearArea.getX() + (gearArea.getWidth() - gearBtnW) / 2,
                         rowY,
                         gearBtnW, btnH);

    const int exportW = gearPad * 2 + textWidthPx(15.0f, "Export") + 10;
    auto exportArea = topRow.removeFromRight(exportW + pad);
    btnExport.setBounds(exportArea.getX() + (exportArea.getWidth() - exportW) / 2,
                        rowY,
                        exportW, btnH);

    const int chipPadX = juce::roundToInt((float)metrics::comboTextPadding * metrics::uiScale);
    const int chipChevronRoom = juce::roundToInt(12.0f * metrics::uiScale);
    const int gridW = chipPadX * 2 + maxHeaderGridLabelWidthPx(kHeaderComboFontH) + chipChevronRoom;
    const int sessionComboW = chipPadX * 2 + maxHeaderSessionLabelWidthPx(kHeaderComboFontH) + chipChevronRoom;
    const int recW = btnH;
    const int bpmW = chipPadX * 2 + textWidthPx(15.0f, "240") + 10;
    const int halfW = chipPadX * 2 + textWidthPx(15.0f, "Half");
    const int dblW = chipPadX * 2 + textWidthPx(15.0f, "Double");

    const int actionIconW = (int)std::round(13.0f * metrics::uiScale);
    const int actionGap = (int)std::round(6.0f * metrics::uiScale);
    const int actionPadX = (int)std::round(12.0f * metrics::uiScale); // px-3 scaled
    const int stepW = actionPadX * 2 + actionIconW + actionGap + textWidthPx(15.0f, "Step");
    const int extendW = actionPadX * 2 + actionIconW + actionGap + textWidthPx(15.0f, "Extend");
    const int trimW = actionPadX * 2 + actionIconW + actionGap + textWidthPx(15.0f, "Trim");
    const int dividerW = 1;
    const int dividerH = 24;
    const int dividerGap = (int)std::round(12.0f * metrics::uiScale);
    const int groupGap = (int)std::round(8.0f * metrics::uiScale);

    const int rightClusterW = bpmW + groupGap + halfW + groupGap + dblW + dividerGap + dividerW + dividerGap
                              + recW + groupGap + gridW + groupGap + sessionComboW + dividerGap + dividerW + dividerGap
                              + stepW + groupGap + extendW + groupGap + trimW;
    int x = topRow.getRight() - rightClusterW;
    btnBpmDisplay.setBounds(x, rowY, bpmW, btnH);
    x += bpmW + groupGap;
    btnHalfTime.setBounds(x, rowY, halfW, btnH);
    x += halfW + groupGap;
    btnDoubleTime.setBounds(x, rowY, dblW, btnH);
    x += dblW + dividerGap;
    headerDividerBeforeRecord_.setBounds(x, rowY + (btnH - dividerH) / 2, dividerW, dividerH);
    x += dividerW + dividerGap;
    btnRecord.setBounds(x, rowY, recW, btnH);
    x += recW + groupGap;
    cmbHeaderGrid.setBounds(x, rowY, gridW, btnH);
    x += gridW + groupGap;
    cmbHeaderSession.setBounds(x, rowY, sessionComboW, btnH);
    x += sessionComboW + dividerGap;
    headerDivider_.setBounds(x, rowY + (btnH - dividerH) / 2, dividerW, dividerH);
    x += dividerW + dividerGap;
    btnStep.setBounds(x, rowY, stepW, btnH);
    x += stepW + groupGap;
    btnExtend.setBounds(x, rowY, extendW, btnH);
    x += extendW + groupGap;
    btnTrim.setBounds(x, rowY, trimW, btnH);

    // FigmaExample toolbar doesn't show a "Bars" label; session length is in the dropdown.
    lblSessionBars.setBounds(0, 0, 0, 0);

    auto controlStrip = b.removeFromTop(metrics::controlStripH);
    controlStrip.removeFromLeft(pad);
    controlStrip.removeFromRight(pad);
    controlPanel.setBounds(controlStrip);

    int contentH = b.getHeight();

    // Resizable: browser (left) | resizer | arrangement | [resizer | piano when visible]
    if (fileBrowserVisible_)
    {
        browserWidth_ = juce::jlimit(metrics::browserMinWidth, metrics::browserMaxWidth, browserWidth_);
        fileBrowser.setBounds(b.removeFromLeft(browserWidth_));
        if (leftResizer_)
            leftResizer_->setBounds(b.removeFromLeft(resizerStripSize));
    }
    else
    {
        fileBrowser.setBounds(0, 0, 0, 0);
        if (leftResizer_)
            leftResizer_->setBounds(0, 0, 0, 0);
    }

    bool showPianoRoll = pianoRoll.hasClip();
    pianoRollHeight_ = juce::jlimit(100, std::max(100, contentH - 100), pianoRollHeight_);

    if (showPianoRoll)
    {
        pianoRoll.setBounds(b.removeFromBottom(pianoRollHeight_));
        if (bottomResizer_)
            bottomResizer_->setBounds(b.removeFromBottom(resizerStripSize));
        pianoRoll.setVisible(true);
    }
    else
    {
        pianoRoll.setBounds(0, 0, 0, 0);
        pianoRoll.setVisible(false);
        if (bottomResizer_)
            bottomResizer_->setBounds(0, 0, 0, 0);
    }

    arrangementView.setBounds(b);
}

void PatternFlowEditor::timerCallback()
{
    // Animate playhead and preview (preview syncs to playhead/loop)
    if (processorRef.hostPlaying.load())
        arrangementView.repaint();
    fileBrowser.repaint();  // Preview syncs to playhead position

    // Push preview state to processor for MIDI output
    processorRef.setPreviewState(fileBrowser.getPreviewClip(), fileBrowser.getHasPreviewClip(),
                                 fileBrowser.getPreviewMuted(), fileBrowser.getPreviewSoloed());

    // Update record button state (recording may auto-stop when transport stops)
    static bool lastRecState = false;
    bool recNow = processorRef.recording.load();
    if (recNow != lastRecState)
    {
        lastRecState = recNow;
        updateRecordButton();
        if (!recNow)
            arrangementView.refresh();
    }
    if (recNow)
        btnRecord.repaint(); // animate pulse/glow like FigmaExample
    // Refresh arrangement periodically during recording so new notes appear in combined lane
    else if (processorRef.hostPlaying.load())
    {
        static int recRefreshCounter = 0;
        if (++recRefreshCounter >= 5) { recRefreshCounter = 0; arrangementView.refresh(); }
    }

    // Playhead BPM display
    const int bpmInt = juce::jmax(1, (int)std::round(processorRef.getPlayheadBpm()));
    const auto bpmText = juce::String(bpmInt);
    if (btnBpmDisplay.getButtonText() != bpmText)
        btnBpmDisplay.setButtonText(bpmText);

    // Theme crossfade (approx Material motion: fast-out, slow-in-ish)
    if (themeTransitionAlpha_ > 0.0f)
    {
        const auto now = juce::Time::getMillisecondCounter();
        const float t = (float)(now - themeTransitionStartMs_) / 180.0f; // 180ms
        const float eased = 1.0f - juce::jlimit(0.0f, 1.0f, t);
        // Simple ease-out (quadratic)
        themeTransitionAlpha_ = eased * eased;
        if (themeTransitionAlpha_ <= 0.001f)
        {
            themeTransitionAlpha_ = 0.0f;
            themeTransitionSnapshot_ = {};
        }
        repaint();
    }
}

void PatternFlowEditor::exportMidi()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Export MIDI", juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.mid");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File{}) return;
        if (!file.hasFileExtension("mid")) file = file.withFileExtension("mid");

        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote(480);
        double bpm = processorRef.hostBpm.load();
        if (bpm <= 0) bpm = 120.0;

        juce::ScopedLock sl(processorRef.laneLock);
        for (int li = 0; li < (int)processorRef.lanes.size(); ++li)
        {
            auto& lane = processorRef.lanes[li];
            if (lane.muted) continue;

            juce::MidiMessageSequence track;
            // Add track name
            track.addEvent(juce::MidiMessage::textMetaEvent(3, lane.name));

            for (auto& region : lane.regions)
            {
                if (region.muted) continue;
                if (region.clipIndex < 0 || region.clipIndex >= (int)lane.clips.size()) continue;
                auto& clip = lane.clips[region.clipIndex];
                double regionLen = region.endBeat - region.startBeat;
                double loopLen = clip.lengthBeats;
                int loopCount = 1;
                if (loopLen > 1.0e-9 && regionLen > loopLen + 1.0e-6)
                    loopCount = juce::jmax(1, (int)std::ceil(regionLen / loopLen - 1.0e-9));

                for (int loop = 0; loop < loopCount; ++loop)
                {
                    int pitchOffset = juce::jlimit(-127, 127, clip.rootNoteOffset);
                    for (auto& note : clip.notes)
                    {
                        if (region.noteFilter >= 0 && note.noteNumber != region.noteFilter) continue;
                        double noteBeat = loop * loopLen + note.startBeat;
                        if (noteBeat >= regionLen) continue;
                        double absStart = region.startBeat + noteBeat;
                        double absEnd = std::min(absStart + note.lengthBeats,
                                                 region.startBeat + regionLen);
                        double startTick = absStart * 480.0;
                        double endTick = absEnd * 480.0;
                        int pitch = juce::jlimit(0, 127, note.noteNumber + pitchOffset);
                        track.addEvent(juce::MidiMessage::noteOn(note.channel, pitch, (juce::uint8)note.velocity), startTick);
                        track.addEvent(juce::MidiMessage::noteOff(note.channel, pitch), endTick);
                    }
                }
            }
            track.sort();
            track.updateMatchedPairs();
            midiFile.addTrack(track);
        }

        juce::FileOutputStream stream(file);
        if (stream.openedOk())
        {
            stream.setPosition(0);
            stream.truncate();
            midiFile.writeTo(stream);
        }
    });
}

void PatternFlowEditor::zoomToFit()
{
    juce::ScopedLock sl(processorRef.laneLock);
    double maxBeat = 0.0;
    for (auto& lane : processorRef.lanes)
        for (auto& region : lane.regions)
            maxBeat = std::max(maxBeat, region.endBeat);

    if (maxBeat <= 0.0) maxBeat = processorRef.arrangementBars.load() * 4.0;

    float availableW = (float)(arrangementView.getWidth() - metrics::laneHeaderW);
    if (availableW <= 0) availableW = 400.0f;

    arrangementView.beatsPerPixel = juce::jlimit(0.01f, 2.0f, (float)(maxBeat / (double)availableW));
    arrangementView.scrollBeatOffset = 0.0f;
    arrangementView.verticalScrollOffset = 0.0f;
    arrangementView.refresh();
}

void PatternFlowEditor::updateRecordButton()
{
    bool isRec = processorRef.recording.load();
    btnRecord.setRecording(isRec);
}

void PatternFlowEditor::refreshTransportColours()
{
    lblSessionBars.setColour(juce::Label::textColourId, colours::textDim());
}

void PatternFlowEditor::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == &cmbHeaderSession)
    {
        const int barOptions[] = { 4, 8, 16 };
        int idx = combo->getSelectedId() - 1;
        if (idx >= 0 && idx < 3)
        {
            int bars = barOptions[idx];
            const double sessionBeats = (double)(bars * 4);
            processorRef.arrangementBars.store(bars);
            processorRef.loopStartBeat.store(0.0);
            processorRef.loopEndBeat.store(sessionBeats);
            processorRef.clampArrangementToSessionLength();
            controlPanel.syncLoopsAfterSessionOrGridChange();
            arrangementView.zoomToFitSession();
        }
    }
    else if (combo == &cmbHeaderGrid)
    {
        using GS = PatternFlowProcessor::GridSize;
        const int id = combo->getSelectedId();
        GS gs = GS::QuarterBeat;
        switch (id)
        {
            case 1: gs = GS::Beat;              break; // 1/4 note
            case 2: gs = GS::HalfBeat;          break; // 1/8
            case 3: gs = GS::QuarterBeat;       break; // 1/16
            case 4: gs = GS::Eighth;            break; // 1/32
            case 5: gs = GS::EighthTriplet;     break;
            case 6: gs = GS::SixteenthTriplet;  break;
            case 7: gs = GS::Off;               break;
        }
        processorRef.gridSnap.store((int)gs);
        controlPanel.syncLoopsAfterSessionOrGridChange();
        arrangementView.repaint();
    }
}

} // namespace pflow
