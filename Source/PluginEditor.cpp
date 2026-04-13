#include "PluginEditor.h"
#include "UndoActions.h"
#include "MidiFileData.h"
#include <algorithm>
#include <cmath>

namespace pflow {

namespace {

constexpr float kToggleFontH  = 13.0f;
constexpr float kTextBtnFontH = 12.0f;
constexpr float kLabelFontH   = 11.0f;

int textWidthPx(float fontHeight, const juce::String& t)
{
    return juce::roundToInt(std::ceil(
        juce::Font(juce::FontOptions(fontHeight)).getStringWidthFloat(t)));
}

int minToggleWidth(int comboPad, const juce::String& label)
{
    const int tick = juce::roundToInt(std::ceil(kToggleFontH * 1.1f));
    const int textLeft = 4 + tick + 10;
    return textLeft + textWidthPx(kToggleFontH, label) + comboPad * 2 + 10;
}

int minTextButtonWidth(int comboPad, const juce::String& label)
{
    return textWidthPx(kTextBtnFontH, label) + comboPad * 2 + 18;
}

} // namespace

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
    setSize(1120, 480);
    setResizable(true, true);
    setResizeLimits(1120, 480, 2400, 1600);
    setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

    lblTitle.setText("PatternFlow", juce::dontSendNotification);
    lblTitle.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
    lblTitle.setColour(juce::Label::textColourId, colours::text());
    addAndMakeVisible(lblTitle);

    juce::Path recCircle;
    recCircle.addEllipse(0.0f, 0.0f, 1.0f, 1.0f);
    btnRecord.setShape(recCircle, false, true, true);
    updateRecordButton();
    btnRecord.setTooltip("Toggle MIDI recording");
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

    cmbGridSnap.addItem("Off", 1);
    cmbGridSnap.addItem("Bar", 2);
    cmbGridSnap.addItem("Beat", 3);
    cmbGridSnap.addItem("1/2", 4);
    cmbGridSnap.addItem("1/4", 5);
    cmbGridSnap.addItem("1/8", 6);
    cmbGridSnap.addItem("1/16", 7);
    cmbGridSnap.addItem("1/8T", 8);
    cmbGridSnap.addItem("1/16T", 9);
    cmbGridSnap.setSelectedId(3);
    cmbGridSnap.setTooltip("Grid snap");
    cmbGridSnap.addListener(this);
    addAndMakeVisible(cmbGridSnap);

    // Loop is now controlled from the control panel (start/end + ∞ sync).

    lblSessionBars.setColour(juce::Label::textColourId, colours::textDim());
    lblSessionBars.setFont(juce::Font(juce::FontOptions(kLabelFontH)));
    lblSessionBars.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(lblSessionBars);
    {
        const int barOptions[] = { 4, 8, 16 };
        for (int i = 0; i < 3; ++i)
            cmbSessionBars.addItem(juce::String(barOptions[i]), i + 1);
        int bars = processorRef.arrangementBars.load();
        int id = 1;
        for (int i = 0; i < 3; ++i)
            if (bars <= barOptions[i]) { id = i + 1; break; }
            else id = i + 2;
        if (bars > 16) { processorRef.arrangementBars.store(16); id = 3; }
        cmbSessionBars.setSelectedId(id);
    }
    cmbSessionBars.setTooltip("Session length in bars");
    cmbSessionBars.addListener(this);
    addAndMakeVisible(cmbSessionBars);

    btnStep.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnStep.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnStep.setComponentID("ActionButton");
    btnStep.setTooltip("Arrange clips in stair-step order");
    btnExtend.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnExtend.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnExtend.setComponentID("ActionButton");
    btnExtend.setTooltip("Align all clips to session start, extend to session end (loop if needed)");
    btnTrim.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnTrim.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnTrim.setComponentID("ActionButton");
    btnTrim.setTooltip("Remove empty measures from each selected clip");

    btnStep.onClick = [this]
    {
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
        processorRef.trimEmptyMeasuresInSelectedClips(arrangementView.selectedClips);
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

    refreshTransportColours();

    btnSettings.setButtonText(juce::String::charToString(0x2699));
    btnSettings.setComponentID("Settings");
    btnSettings.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnSettings.setColour(juce::TextButton::textColourOffId, colours::text());
    btnSettings.setColour(juce::TextButton::textColourOnId, colours::text());
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
    arrangementView.onLoopChanged = [this] { /* loop UI lives in control panel */ };

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
    const int headerH = 2 * metrics::titleBarH;
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

    addLabel("Theme", 80);
    auto* cmbTheme = new juce::ComboBox();
    // 10 light + 10 dark presets (Material Design 3-inspired)
    for (int i = 0; i < (int)themePresets().size(); ++i)
        cmbTheme->addItem(themePresets()[(size_t)i].name, i + 1);
    cmbTheme->setSelectedId(processorRef.appThemeId.load() + 1, juce::dontSendNotification);
    cmbTheme->setBounds(110, y, 260, rowH);
    cmbTheme->setColour(juce::ComboBox::backgroundColourId, colours::bgLight());
    cmbTheme->setColour(juce::ComboBox::textColourId, colours::textBright());
    content->addAndMakeVisible(cmbTheme);
    y += rowH + 8;

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

    cmbTheme->onChange = [this, cmbTheme]()
    {
        int themeId = cmbTheme->getSelectedId() - 1;
        processorRef.appThemeId.store(themeId);
        // Theme transition: snapshot old UI then fade to new.
        auto snapshot = createComponentSnapshot(getLocalBounds());
        applyAppTheme(themeId);
        lnf.refreshColours();
        lblTitle.setColour(juce::Label::textColourId, colours::text());
        btnSettings.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btnSettings.setColour(juce::TextButton::textColourOffId, colours::text());
        controlPanel.refreshComponentColours();
        refreshTransportColours();
        fileBrowser.refreshComponentColours();
        arrangementView.refreshComponentColours();
        pianoRoll.refreshComponentColours();
        themeTransitionSnapshot_ = snapshot;
        themeTransitionAlpha_ = 1.0f;
        themeTransitionStartMs_ = juce::Time::getMillisecondCounter();
        startTimerHz(60);
        repaint();
        arrangementView.refresh();
        pianoRoll.repaint();
    };

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

    // Row 1: logo | transport (centered) | settings; row 2: control panel (comp | scale)
    const int pad = metrics::titleBarPadding;
    const int logoW = 120;
    const int gearSize = 36;
    const int comboPad = metrics::comboTextPadding;
    const int stepGap = 8;

    auto topRow = b.removeFromTop(metrics::titleBarH);
    lblTitle.setBounds(topRow.removeFromLeft(logoW).reduced(pad, pad));
    auto gearArea = topRow.removeFromRight(gearSize + pad * 2);
    btnSettings.setBounds(gearArea.getX() + (gearArea.getWidth() - gearSize) / 2,
                         gearArea.getY() + (gearArea.getHeight() - gearSize) / 2,
                         gearSize, gearSize);

    const int btnH = juce::jlimit(28, 34, topRow.getHeight() - 8);
    const int rowY = topRow.getY() + (topRow.getHeight() - btnH) / 2;
    const int recSize = juce::jmin(28, btnH);
    const int gridW = textWidthPx(kTextBtnFontH, "1/16T") + comboPad * 2 + 30;
    const int barsLblW = textWidthPx(kLabelFontH, "Bars") + comboPad + 6;
    const int sessionComboW = textWidthPx(kTextBtnFontH, "16") + comboPad * 2 + 34;
    const int stepW = minTextButtonWidth(comboPad, "Step");
    const int extendW = minTextButtonWidth(comboPad, "Extend");
    const int trimW = minTextButtonWidth(comboPad, "Trim");
    const int transportW = recSize + stepGap + gridW + stepGap + barsLblW + 4 + sessionComboW
                             + stepGap + stepW + stepGap + extendW + stepGap + trimW;
    int x = topRow.getX() + (topRow.getWidth() - transportW) / 2;
    btnRecord.setBounds(x, rowY + (btnH - recSize) / 2, recSize, recSize);
    x += recSize + stepGap;
    cmbGridSnap.setBounds(x, rowY, gridW, btnH);
    x += gridW + stepGap;
    lblSessionBars.setBounds(x, rowY, barsLblW, btnH);
    x += barsLblW + 4;
    cmbSessionBars.setBounds(x, rowY, sessionComboW, btnH);
    x += sessionComboW + stepGap;
    btnStep.setBounds(x, rowY, stepW, btnH);
    x += stepW + stepGap;
    btnExtend.setBounds(x, rowY, extendW, btnH);
    x += extendW + stepGap;
    btnTrim.setBounds(x, rowY, trimW, btnH);

    auto controlStrip = b.removeFromTop(metrics::titleBarH);
    controlStrip.removeFromLeft(pad);
    controlStrip.removeFromRight(pad);
    controlPanel.setBounds(controlStrip);

    int contentH = b.getHeight();
    int contentW = b.getWidth();

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
    // Refresh arrangement periodically during recording so new notes appear in combined lane
    else if (recNow && processorRef.hostPlaying.load())
    {
        static int recRefreshCounter = 0;
        if (++recRefreshCounter >= 5) { recRefreshCounter = 0; arrangementView.refresh(); }
    }

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
    if (isRec)
        btnRecord.setColours(colours::recordRed(), colours::recordRed().brighter(0.1f), colours::recordRed());
    else
        btnRecord.setColours(colours::bgLighter(), colours::bgLighter().brighter(0.1f), colours::bgLighter());
}

void PatternFlowEditor::refreshTransportColours()
{
    lblSessionBars.setColour(juce::Label::textColourId, colours::textDim());
    btnStep.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnStep.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnExtend.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnExtend.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnTrim.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnTrim.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

void PatternFlowEditor::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == &cmbSessionBars)
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
            arrangementView.zoomToFitSession();
        }
    }
    else if (combo == &cmbGridSnap)
    {
        using GS = PatternFlowProcessor::GridSize;
        int id = combo->getSelectedId();
        GS gs = GS::Beat;
        switch (id)
        {
            case 1: gs = GS::Off;              break;
            case 2: gs = GS::Bar;              break;
            case 3: gs = GS::Beat;             break;
            case 4: gs = GS::HalfBeat;         break;
            case 5: gs = GS::QuarterBeat;      break;
            case 6: gs = GS::Eighth;           break;
            case 7: gs = GS::Sixteenth;        break;
            case 8: gs = GS::EighthTriplet;    break;
            case 9: gs = GS::SixteenthTriplet; break;
        }
        processorRef.gridSnap.store((int)gs);
    }
}

} // namespace pflow
