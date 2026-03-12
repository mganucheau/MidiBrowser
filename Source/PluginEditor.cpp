#include "PluginEditor.h"

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
    lblTitle.setColour(juce::Label::textColourId, colours::accent);
    addAndMakeVisible(lblTitle);

    // About button
    btnAbout.setColour(juce::TextButton::buttonColourId, colours::bgLighter);
    btnAbout.setColour(juce::TextButton::textColourOffId, colours::textDim);
    btnAbout.onClick = [this] { showAboutDialog(); };
    addAndMakeVisible(btnAbout);

    // Keyboard shortcuts
    addKeyListener(this);
    setWantsKeyboardFocus(true);

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
    g.fillAll(colours::bg);
}

void PatternFlowEditor::showAboutDialog()
{
    auto* dialog = new juce::DialogWindow::LaunchOptions();

    auto* content = new juce::Component();
    content->setSize(400, 340);

    auto* titleLabel = new juce::Label({}, version::name);
    titleLabel->setFont(juce::Font(juce::FontOptions(22.0f).withStyle("Bold")));
    titleLabel->setColour(juce::Label::textColourId, colours::accent);
    titleLabel->setBounds(20, 12, 360, 30);
    content->addAndMakeVisible(titleLabel);

    auto* versionLabel = new juce::Label({}, juce::String("Version ") + version::number);
    versionLabel->setFont(juce::Font(juce::FontOptions(13.0f)));
    versionLabel->setColour(juce::Label::textColourId, colours::textDim);
    versionLabel->setBounds(20, 42, 360, 20);
    content->addAndMakeVisible(versionLabel);

    auto* descLabel = new juce::Label({}, version::desc);
    descLabel->setFont(juce::Font(juce::FontOptions(13.0f)));
    descLabel->setColour(juce::Label::textColourId, colours::text);
    descLabel->setBounds(20, 70, 360, 50);
    descLabel->setMinimumHorizontalScale(1.0f);
    content->addAndMakeVisible(descLabel);

    auto* licenseEditor = new juce::TextEditor();
    licenseEditor->setMultiLine(true, true);
    licenseEditor->setReadOnly(true);
    licenseEditor->setScrollbarsShown(true);
    licenseEditor->setColour(juce::TextEditor::backgroundColourId, colours::bgLight);
    licenseEditor->setColour(juce::TextEditor::textColourId, colours::textDim);
    licenseEditor->setColour(juce::TextEditor::outlineColourId, colours::panelBorder);
    licenseEditor->setFont(juce::Font(juce::FontOptions(11.0f)));
    licenseEditor->setText(version::license);
    licenseEditor->setBounds(20, 128, 360, 160);
    content->addAndMakeVisible(licenseEditor);

    auto* closeBtn = new juce::TextButton("Close");
    closeBtn->setColour(juce::TextButton::buttonColourId, colours::accent);
    closeBtn->setColour(juce::TextButton::textColourOffId, colours::textBright);
    closeBtn->setBounds(155, 300, 90, 28);
    closeBtn->onClick = [content]
    {
        if (auto* dw = content->findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    content->addAndMakeVisible(closeBtn);

    dialog->content.setOwned(content);
    dialog->dialogTitle = "About PatternFlow";
    dialog->dialogBackgroundColour = colours::bg;
    dialog->escapeKeyTriggersCloseButton = true;
    dialog->useNativeTitleBar = false;
    dialog->resizable = false;

    dialog->launchAsync();
}

bool PatternFlowEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    // Delete selected region
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (arrangementView.selLane >= 0 && arrangementView.selRegion >= 0)
        {
            juce::ScopedLock sl(processorRef.laneLock);
            if (arrangementView.selLane < (int)processorRef.lanes.size())
            {
                auto& lane = processorRef.lanes[arrangementView.selLane];
                if (arrangementView.selRegion < (int)lane.regions.size())
                {
                    lane.removeRegion(arrangementView.selRegion);
                    arrangementView.selLane = -1;
                    arrangementView.selRegion = -1;
                    arrangementView.refresh();
                    return true;
                }
            }
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
