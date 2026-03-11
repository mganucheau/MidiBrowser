#include "PluginEditor.h"

namespace pflow {

PatternFlowEditor::PatternFlowEditor(PatternFlowProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      controlPanel(p),
      arrangementView(p)
{
    setLookAndFeel(&lnf);
    setSize(1100, 680);
    setResizable(true, true);
    setResizeLimits(800, 480, 2400, 1600);

    // Title
    lblTitle.setText("PatternFlow", juce::dontSendNotification);
    lblTitle.setFont(juce::Font(14.0f, juce::Font::bold));
    lblTitle.setColour(juce::Label::textColourId, colours::accent);
    addAndMakeVisible(lblTitle);

    // Control panel
    controlPanel.onAddLane = [this]
    {
        juce::ScopedLock sl(processor.laneLock);
        CompLane newLane;
        int idx = (int)processor.lanes.size();
        auto presets = getClipColourPresets();
        newLane.name   = "Lane " + juce::String(idx + 1);
        newLane.colour = presets[idx % presets.size()];
        processor.lanes.push_back(newLane);
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

    // Arrangement view
    arrangementView.onClipDoubleClicked = [this](const MidiClip& clip)
    {
        pianoRoll.setClip(clip);
        resized();
    };
    addAndMakeVisible(arrangementView);

    // Piano roll (starts hidden)
    pianoRoll.onClipEdited = [this](const MidiClip& /*editedClip*/)
    {
        // TODO: write edited clip back to the lane data
        arrangementView.refresh();
    };
    addAndMakeVisible(pianoRoll);

    // Repaint timer for playhead animation
    startTimerHz(30);
}

PatternFlowEditor::~PatternFlowEditor()
{
    setLookAndFeel(nullptr);
}

void PatternFlowEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg);
}

void PatternFlowEditor::resized()
{
    auto b = getLocalBounds();

    // Title bar (thin)
    auto titleBar = b.removeFromTop(24);
    lblTitle.setBounds(titleBar.reduced(metrics::padding, 2));

    // Control panel
    controlPanel.setBounds(b.removeFromTop(metrics::controlPanelH));

    // Piano roll (if open)
    bool showPianoRoll = pianoRoll.hasClip();
    if (showPianoRoll)
        pianoRoll.setBounds(b.removeFromBottom(metrics::pianoRollH));
    else
        pianoRoll.setBounds(0, 0, 0, 0);

    pianoRoll.setVisible(showPianoRoll);

    // File browser on left
    fileBrowser.setBounds(b.removeFromLeft(metrics::browserWidth));

    // Arrangement fills the rest
    arrangementView.setBounds(b);
}

void PatternFlowEditor::timerCallback()
{
    // Animate playhead
    if (processor.hostPlaying.load())
        arrangementView.repaint();
}

} // namespace pflow
