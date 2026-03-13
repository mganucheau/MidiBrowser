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

    // High contrast toggle
    auto* hcToggle = new juce::ToggleButton("High Contrast Mode");
    hcToggle->setColour(juce::ToggleButton::textColourId, colours::text());
    hcToggle->setColour(juce::ToggleButton::tickColourId, colours::accent());
    hcToggle->setToggleState(highContrastEnabled().load(), juce::dontSendNotification);
    hcToggle->setBounds(20, 286, 200, 24);
    hcToggle->onClick = [hcToggle, this]
    {
        highContrastEnabled().store(hcToggle->getToggleState());
        // Trigger full repaint of the editor to apply new colours
        repaint();
        controlPanel.repaint();
        fileBrowser.repaint();
        arrangementView.refresh();
        pianoRoll.repaint();
    };
    content->addAndMakeVisible(hcToggle);

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

    // Arrow key navigation for regions
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey ||
        key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
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

void PatternFlowEditor::timerCallback()
{
    // Animate playhead
    if (processorRef.hostPlaying.load())
        arrangementView.repaint();
}

} // namespace pflow
