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
    auto titleBar = b.removeFromTop(28);
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
