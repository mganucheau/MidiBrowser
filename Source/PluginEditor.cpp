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

    // Title
    lblTitle.setText("PatternFlow", juce::dontSendNotification);
    lblTitle.setFont(juce::Font(juce::FontOptions(16.0f).withStyle("Bold")));
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
    addAndMakeVisible(controlPanel);

    // File browser
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
    arrangementView.onClipDoubleClicked = [this](const MidiClip& clip, int laneIdx, int regionIdx)
    {
        pianoRoll.setClip(clip, laneIdx, regionIdx);
        resized();
    };
    addAndMakeVisible(arrangementView);

    // Piano roll (starts hidden)
    pianoRoll.onClipEdited = [this](const MidiClip& editedClip, int laneIdx, int regionIdx)
    {
        juce::ScopedLock sl(processorRef.laneLock);
        if (laneIdx >= 0 && laneIdx < (int)processorRef.lanes.size())
        {
            auto& lane = processorRef.lanes[laneIdx];
            if (regionIdx >= 0 && regionIdx < (int)lane.regions.size())
            {
                int clipIdx = lane.regions[regionIdx].clipIndex;
                if (clipIdx >= 0 && clipIdx < (int)lane.clips.size())
                    lane.clips[clipIdx] = editedClip;
            }
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

    auto* versionLabel = new juce::Label({}, juce::String("Version ") + version::number);
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

    // Alt+Arrow: MOVE selected region (timeline shift or lane change)
    if (key.getModifiers().isAltDown() && arrangementView.selLane >= 0 && arrangementView.selRegion >= 0 &&
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
                processorRef.undoManager.perform(
                    new MoveRegionAction(processorRef, lane, reg,
                                         oldStart, oldEnd, newStart, newStart + len));
            }
        }
        else if (key.getKeyCode() == juce::KeyPress::rightKey)
        {
            double newStart = processorRef.snapBeat(region.startBeat + 1.0);
            double oldStart = region.startBeat, oldEnd = region.endBeat;
            processorRef.undoManager.perform(
                new MoveRegionAction(processorRef, lane, reg,
                                     oldStart, oldEnd, newStart, newStart + len));
        }
        else if (key.getKeyCode() == juce::KeyPress::upKey && lane > 0)
        {
            int dstLane = lane - 1;
            double s = region.startBeat, e2 = region.endBeat;
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
            processorRef.undoManager.perform(
                new MoveRegionToLaneAction(processorRef, lane, reg, dstLane, s, e2, createNew));
            arrangementView.selLane = dstLane;
            arrangementView.selRegion = (int)processorRef.lanes[dstLane].regions.size() - 1;
        }
        arrangementView.refresh();
        return true;
    }

    // Arrow key navigation for regions (no modifier)
    if ((key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey ||
         key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) &&
        !key.getModifiers().isAltDown())
    {
        juce::ScopedLock sl(processorRef.laneLock);
        int numLanes = (int)processorRef.lanes.size();
        if (numLanes == 0) return false;

        if (arrangementView.selLane < 0)
        {
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

        int lane = arrangementView.selLane;
        int reg = arrangementView.selRegion;

        if (key == juce::KeyPress::leftKey)
        {
            if (reg > 0) reg--;
        }
        else if (key == juce::KeyPress::rightKey)
        {
            if (lane < numLanes && reg < (int)processorRef.lanes[lane].regions.size() - 1) reg++;
        }
        else if (key == juce::KeyPress::upKey)
        {
            for (int li = lane - 1; li >= 0; --li)
            {
                if (!processorRef.lanes[li].regions.empty())
                { lane = li; reg = std::min(reg, (int)processorRef.lanes[li].regions.size() - 1); break; }
            }
        }
        else if (key == juce::KeyPress::downKey)
        {
            for (int li = lane + 1; li < numLanes; ++li)
            {
                if (!processorRef.lanes[li].regions.empty())
                { lane = li; reg = std::min(reg, (int)processorRef.lanes[li].regions.size() - 1); break; }
            }
        }

        arrangementView.selLane = lane;
        arrangementView.selRegion = reg;
        arrangementView.refresh();
        return true;
    }

    // M key to mute/unmute selected region
    if (key == juce::KeyPress('m') && arrangementView.selLane >= 0 && arrangementView.selRegion >= 0)
    {
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
            double playBeat = processorRef.hostBeatPos.load();
            juce::ScopedLock sl(processorRef.laneLock);
            if (arrangementView.selLane < (int)processorRef.lanes.size())
            {
                auto& lane = processorRef.lanes[arrangementView.selLane];
                if (arrangementView.selRegion < (int)lane.regions.size())
                {
                    auto& reg = lane.regions[arrangementView.selRegion];
                    if (playBeat > reg.startBeat && playBeat < reg.endBeat)
                        processorRef.undoManager.perform(
                            new SplitRegionAction(processorRef, arrangementView.selLane,
                                                  arrangementView.selRegion, playBeat));
                }
            }
            arrangementView.refresh();
        }
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
                int loopCount = std::max(1, (int)std::ceil(regionLen / loopLen));

                for (int loop = 0; loop < loopCount; ++loop)
                {
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
                        track.addEvent(juce::MidiMessage::noteOn(note.channel, note.noteNumber, (juce::uint8)note.velocity), startTick);
                        track.addEvent(juce::MidiMessage::noteOff(note.channel, note.noteNumber), endTick);
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

} // namespace pflow
