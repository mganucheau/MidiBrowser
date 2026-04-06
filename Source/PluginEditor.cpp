#include "PluginEditor.h"
#include "UndoActions.h"

namespace pflow {

PatternFlowEditor::PatternFlowEditor(PatternFlowProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      controlPanel(p),
      arrangementView(p),
      pianoRoll(p)
{
    setLookAndFeel(&lnf);
    setSize(1100, 680);
    setResizable(true, true);
    setResizeLimits(800, 480, 2400, 1600);

    // Title with music icon
    lblTitle.setText(juce::String::charToString(0x266B) + " PatternFlow", juce::dontSendNotification);
    lblTitle.setFont(juce::Font(juce::FontOptions(15.0f).withStyle("Bold")));
    lblTitle.setColour(juce::Label::textColourId, colours::accent());
    addAndMakeVisible(lblTitle);

    // About button
    btnAbout.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnAbout.setColour(juce::TextButton::textColourOffId, colours::textDim());
    btnAbout.onClick = [this] { showAboutDialog(); };
    addAndMakeVisible(btnAbout);

    // Theme toggle button
    updateThemeButton();
    btnTheme.onClick = [this]
    {
        darkModeEnabled().store(!darkModeEnabled().load());
        lnf.refreshColours();
        updateThemeButton();

        // Update per-component colors for all panels
        lblTitle.setColour(juce::Label::textColourId, colours::accent());
        btnAbout.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
        btnAbout.setColour(juce::TextButton::textColourOffId, colours::textDim());
        controlPanel.refreshComponentColours();
        fileBrowser.refreshComponentColours();
        arrangementView.refreshComponentColours();
        pianoRoll.refreshComponentColours();

        repaint();
        controlPanel.repaint();
        fileBrowser.repaint();
        arrangementView.refresh();
        pianoRoll.repaint();
    };
    addAndMakeVisible(btnTheme);

    // Keyboard shortcuts
    addKeyListener(this);
    setWantsKeyboardFocus(true);

    // Accessibility descriptions
    setTitle("PatternFlow Editor");
    setDescription("Main editor window for PatternFlow MIDI composition tool");
    controlPanel.setTitle("Control Panel");
    controlPanel.setDescription("Humanisation knobs, scale settings, and MIDI split controls");
    fileBrowser.setTitle("File Browser");
    fileBrowser.setDescription("Browse and select MIDI files to add to the arrangement");
    arrangementView.setTitle("Arrangement View");
    arrangementView.setDescription("Arrange MIDI clips on lanes. Use arrow keys to navigate regions.");
    pianoRoll.setTitle("Piano Roll Editor");
    pianoRoll.setDescription("Edit individual MIDI notes in the selected clip");
    btnAbout.setTitle("About");
    btnAbout.setDescription("Show information about PatternFlow");

    // Make panels focusable for keyboard navigation
    controlPanel.setWantsKeyboardFocus(true);
    fileBrowser.setWantsKeyboardFocus(true);
    arrangementView.setWantsKeyboardFocus(true);
    pianoRoll.setWantsKeyboardFocus(true);

    // Control panel
    controlPanel.onAddLane = [this]
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
    controlPanel.onSessionBarsChanged = [this](int /*bars*/)
    {
        processorRef.rebuildMasterClip();
        arrangementView.zoomToFitSession();
    };
    controlPanel.onTrimClips = [this]
    {
        juce::ScopedLock sl(processorRef.laneLock);
        for (auto& lane : processorRef.lanes)
        {
            for (auto& clip : lane.clips)
            {
                if (clip.notes.empty()) continue;

                double maxBeat = 0.0;
                for (auto& n : clip.notes)
                    maxBeat = std::max(maxBeat, n.startBeat + n.lengthBeats);

                double trimmedBars = std::ceil(maxBeat / 4.0);
                double trimmedLen = std::max(4.0, trimmedBars * 4.0);

                if (trimmedLen < clip.lengthBeats)
                    clip.lengthBeats = trimmedLen;
            }
        }
        processorRef.rebuildMasterClip();
        arrangementView.refresh();
    };
    controlPanel.onRecordToggle = [this]
    {
        if (processorRef.recording.load())
        {
            processorRef.stopRecording();
        }
        else
        {
            processorRef.startRecording();
        }
        controlPanel.updateRecordButton();
        arrangementView.refresh();
    };
    addAndMakeVisible(controlPanel);

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
        pianoRoll.setClip(clip);
        resized();
    };
    addAndMakeVisible(fileBrowser);

    // Arrangement view - "+" lane click
    arrangementView.onAddLaneClicked = [this]
    {
        if (controlPanel.onAddLane) controlPanel.onAddLane();
    };
    arrangementView.onClipDoubleClicked = [this](const MidiClip& clip, int laneIdx, int clipIdx)
    {
        if (laneIdx < 0)
        {
            pianoRoll.clearClip();
            resized();
            return;
        }
        pianoRoll.setClip(clip, laneIdx, clipIdx);
        resized();
    };
    addAndMakeVisible(arrangementView);

    // Piano roll (starts hidden)
    pianoRoll.onClipEdited = [this](const MidiClip& editedClip, int laneIdx, int clipIdx)
    {
        juce::ScopedLock sl(processorRef.laneLock);
        if (laneIdx >= 0 && laneIdx < (int)processorRef.lanes.size())
        {
            auto& lane = processorRef.lanes[static_cast<size_t>(laneIdx)];
            if (clipIdx >= 0 && clipIdx < (int)lane.clips.size())
                lane.clips[static_cast<size_t>(clipIdx)] = editedClip;
        }
        arrangementView.refresh();
    };
    addAndMakeVisible(pianoRoll);

    // Repaint timer for playhead animation
    startTimerHz(30);
}

PatternFlowEditor::~PatternFlowEditor()
{
    removeKeyListener(this);
    setLookAndFeel(nullptr);
}

void PatternFlowEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg());
    // Subtle accent gradient at top edge
    g.setColour(colours::accent().withAlpha(0.08f));
    g.fillRect(0.0f, 0.0f, (float)getWidth(), 1.0f);
}

void PatternFlowEditor::showAboutDialog()
{
    auto* dialog = new juce::DialogWindow::LaunchOptions();

    auto* content = new juce::Component();
    content->setSize(400, 380);

    auto* titleLabel = new juce::Label({}, version::name);
    titleLabel->setFont(juce::Font(juce::FontOptions(22.0f).withStyle("Bold")));
    titleLabel->setColour(juce::Label::textColourId, colours::accent());
    titleLabel->setBounds(20, 12, 360, 30);
    content->addAndMakeVisible(titleLabel);

    auto* versionLabel = new juce::Label({}, juce::String("Version ") + version::number
                                            + "  (" + version::buildDate + ")");
    versionLabel->setFont(juce::Font(juce::FontOptions(13.0f)));
    versionLabel->setColour(juce::Label::textColourId, colours::textDim());
    versionLabel->setBounds(20, 42, 360, 20);
    content->addAndMakeVisible(versionLabel);

    auto* descLabel = new juce::Label({}, version::desc);
    descLabel->setFont(juce::Font(juce::FontOptions(13.0f)));
    descLabel->setColour(juce::Label::textColourId, colours::text());
    descLabel->setBounds(20, 70, 360, 50);
    descLabel->setMinimumHorizontalScale(1.0f);
    content->addAndMakeVisible(descLabel);

    auto* licenseEditor = new juce::TextEditor();
    licenseEditor->setMultiLine(true, true);
    licenseEditor->setReadOnly(true);
    licenseEditor->setScrollbarsShown(true);
    licenseEditor->setColour(juce::TextEditor::backgroundColourId, colours::bgLight());
    licenseEditor->setColour(juce::TextEditor::textColourId, colours::textDim());
    licenseEditor->setColour(juce::TextEditor::outlineColourId, colours::panelBorder());
    licenseEditor->setFont(juce::Font(juce::FontOptions(11.0f)));
    licenseEditor->setText(version::license);
    licenseEditor->setBounds(20, 128, 360, 150);
    content->addAndMakeVisible(licenseEditor);

    auto* themeLabel = new juce::Label({}, "Use the theme button in the title bar to switch between Light and Dark mode.");
    themeLabel->setFont(juce::Font(juce::FontOptions(11.0f)));
    themeLabel->setColour(juce::Label::textColourId, colours::textDim());
    themeLabel->setBounds(20, 286, 360, 24);
    themeLabel->setMinimumHorizontalScale(1.0f);
    content->addAndMakeVisible(themeLabel);

    auto* closeBtn = new juce::TextButton("Close");
    closeBtn->setColour(juce::TextButton::buttonColourId, colours::accent());
    closeBtn->setColour(juce::TextButton::textColourOffId, colours::textBright());
    closeBtn->setBounds(155, 340, 90, 28);
    closeBtn->onClick = [content]
    {
        if (auto* dw = content->findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    content->addAndMakeVisible(closeBtn);

    dialog->content.setOwned(content);
    dialog->dialogTitle = "About PatternFlow";
    dialog->dialogBackgroundColour = colours::bg();
    dialog->escapeKeyTriggersCloseButton = true;
    dialog->useNativeTitleBar = false;
    dialog->resizable = false;

    dialog->launchAsync();
}

bool PatternFlowEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    // Undo: Cmd/Ctrl + Z
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
    {
        if (processorRef.undoManager.undo())
        {
            arrangementView.refresh();
            if (pianoRoll.hasClip())
            {
                int li = pianoRoll.getEditLaneIndex();
                int ci = pianoRoll.getEditRegionIndex();
                juce::ScopedLock sl(processorRef.laneLock);
                if (li >= 0 && li < (int)processorRef.lanes.size())
                {
                    auto& lane = processorRef.lanes[static_cast<size_t>(li)];
                    if (ci >= 0 && ci < (int)lane.clips.size())
                        pianoRoll.setClip(lane.clips[static_cast<size_t>(ci)], li, ci);
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
                int ci = pianoRoll.getEditRegionIndex();
                juce::ScopedLock sl(processorRef.laneLock);
                if (li >= 0 && li < (int)processorRef.lanes.size())
                {
                    auto& lane = processorRef.lanes[static_cast<size_t>(li)];
                    if (ci >= 0 && ci < (int)lane.clips.size())
                        pianoRoll.setClip(lane.clips[static_cast<size_t>(ci)], li, ci);
                }
            }
        }
        return true;
    }

    // Delete selected clip (undoable)
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (arrangementView.selLane >= 0 && arrangementView.selClip >= 0)
        {
            processorRef.undoManager.perform(
                new RemoveClipAction(processorRef, arrangementView.selLane, arrangementView.selClip));
            arrangementView.selLane = -1;
            arrangementView.selClip = -1;
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
    if (arrangementView.selLane >= 0 && arrangementView.selClip >= 0 &&
        (key.getKeyCode() == juce::KeyPress::leftKey || key.getKeyCode() == juce::KeyPress::rightKey ||
         key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey))
    {
        juce::ScopedLock sl(processorRef.laneLock);
        int numLanes = (int)processorRef.lanes.size();
        int lane = arrangementView.selLane;
        int ci = arrangementView.selClip;
        if (lane >= numLanes || ci >= (int)processorRef.lanes[static_cast<size_t>(lane)].clips.size())
            return true;

        double clipStart = processorRef.lanes[static_cast<size_t>(lane)].clipStarts[static_cast<size_t>(ci)];

        if (key.getKeyCode() == juce::KeyPress::leftKey)
        {
            double newStart = processorRef.snapBeat(std::max(0.0, clipStart - 1.0));
            if (std::abs(newStart - clipStart) > 0.001)
                processorRef.undoManager.perform(
                    new MoveClipAction(processorRef, lane, ci, clipStart, newStart));
        }
        else if (key.getKeyCode() == juce::KeyPress::rightKey)
        {
            double newStart = processorRef.snapBeat(clipStart + 1.0);
            processorRef.undoManager.perform(
                new MoveClipAction(processorRef, lane, ci, clipStart, newStart));
        }
        else if (key.getKeyCode() == juce::KeyPress::upKey && lane > 0)
        {
            int dstLane = lane - 1;
            processorRef.undoManager.perform(
                new MoveClipToLaneAction(processorRef, lane, ci, dstLane, clipStart, false));
            arrangementView.selLane = dstLane;
            arrangementView.selClip = (int)processorRef.lanes[static_cast<size_t>(dstLane)].clips.size() - 1;
        }
        else if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            int dstLane = lane + 1;
            bool createNew = (dstLane >= numLanes);
            processorRef.undoManager.perform(
                new MoveClipToLaneAction(processorRef, lane, ci, dstLane, clipStart, createNew));
            arrangementView.selLane = dstLane;
            arrangementView.selClip = (int)processorRef.lanes[static_cast<size_t>(dstLane)].clips.size() - 1;
        }
        arrangementView.refresh();
        return true;
    }

    // Arrow keys with no selection: select first available clip
    if ((key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey ||
         key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) &&
        arrangementView.selLane < 0)
    {
        juce::ScopedLock sl(processorRef.laneLock);
        int numLanes = (int)processorRef.lanes.size();
        for (int li = 0; li < numLanes; ++li)
        {
            if (!processorRef.lanes[static_cast<size_t>(li)].clips.empty())
            {
                arrangementView.selLane = li;
                arrangementView.selClip = 0;
                arrangementView.refresh();
                return true;
            }
        }
        return false;
    }

    // Cmd+D: Duplicate selected clip
    if (key == juce::KeyPress('d', juce::ModifierKeys::commandModifier, 0))
    {
        if (arrangementView.selLane >= 0 && arrangementView.selClip >= 0)
        {
            processorRef.undoManager.perform(
                new DuplicateClipAction(processorRef, arrangementView.selLane, arrangementView.selClip));
            arrangementView.refresh();
        }
        return true;
    }

    // Cmd+L: Toggle loop from selection/clip (Ableton-style)
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

void PatternFlowEditor::resized()
{
    auto b = getLocalBounds();

    // Title bar (thin)
    auto titleBar = b.removeFromTop(28);
    btnAbout.setBounds(titleBar.removeFromRight(28).reduced(2));
    btnTheme.setBounds(titleBar.removeFromRight(56).reduced(2));
    lblTitle.setBounds(titleBar.reduced(metrics::padding, 2));

    // Control panel
    controlPanel.setBounds(b.removeFromTop(metrics::controlPanelH));

    // File browser takes full remaining height on the left
    fileBrowser.setBounds(b.removeFromLeft(metrics::browserWidth));

    // Right side: arrangement + optional piano roll
    bool showPianoRoll = pianoRoll.hasClip();
    if (showPianoRoll)
        pianoRoll.setBounds(b.removeFromBottom(metrics::pianoRollH));
    else
        pianoRoll.setBounds(0, 0, 0, 0);

    pianoRoll.setVisible(showPianoRoll);

    // Arrangement fills the rest of the right side
    arrangementView.setBounds(b);
}

void PatternFlowEditor::updateThemeButton()
{
    bool dark = darkModeEnabled().load();
    btnTheme.setButtonText(dark ? "Light" : "Dark");
    btnTheme.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnTheme.setColour(juce::TextButton::textColourOffId, colours::textDim());
}

void PatternFlowEditor::timerCallback()
{
    // Animate playhead
    if (processorRef.hostPlaying.load())
        arrangementView.repaint();

    // Update record button state (recording may auto-stop when transport stops)
    static bool lastRecState = false;
    bool recNow = processorRef.recording.load();
    if (recNow != lastRecState)
    {
        lastRecState = recNow;
        controlPanel.updateRecordButton();
        if (!recNow)
            arrangementView.refresh();
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
            auto& lane = processorRef.lanes[static_cast<size_t>(li)];
            if (lane.muted) continue;

            juce::MidiMessageSequence track;
            track.addEvent(juce::MidiMessage::textMetaEvent(3, lane.name));

            for (int ci = 0; ci < (int)lane.clips.size(); ++ci)
            {
                auto& clip = lane.clips[static_cast<size_t>(ci)];
                double clipStart = lane.clipStarts[static_cast<size_t>(ci)];

                for (auto& note : clip.notes)
                {
                    double absStart = clipStart + note.startBeat;
                    double absEnd = absStart + note.lengthBeats;
                    double startTick = absStart * 480.0;
                    double endTick = absEnd * 480.0;
                    track.addEvent(juce::MidiMessage::noteOn(note.channel, note.noteNumber, (juce::uint8)note.velocity), startTick);
                    track.addEvent(juce::MidiMessage::noteOff(note.channel, note.noteNumber), endTick);
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
        for (int ci = 0; ci < (int)lane.clips.size(); ++ci)
            maxBeat = std::max(maxBeat, lane.clipStarts[static_cast<size_t>(ci)] + lane.clips[static_cast<size_t>(ci)].lengthBeats);

    if (maxBeat <= 0.0) maxBeat = processorRef.arrangementBars.load() * 4.0;

    float availableW = (float)(arrangementView.getWidth() - metrics::laneHeaderW);
    if (availableW <= 0) availableW = 400.0f;

    arrangementView.beatsPerPixel = juce::jlimit(0.01f, 2.0f, (float)(maxBeat / (double)availableW));
    arrangementView.scrollBeatOffset = 0.0f;
    arrangementView.verticalScrollOffset = 0.0f;
    arrangementView.refresh();
}

} // namespace pflow
