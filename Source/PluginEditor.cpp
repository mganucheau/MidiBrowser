#include "PluginEditor.h"

namespace pflow {

MidiBrowserEditor::MidiBrowserEditor(MidiBrowserProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setLookAndFeel(&lnf);
    applyAppTheme(processorRef.appThemeId.load());
    lnf.refreshColours();

    setSize(520, 720);
    setResizable(true, true);
    setResizeLimits(320, 400, 1200, 2000);

    lblTitle.setText("Midi Browser", juce::dontSendNotification);
    lblTitle.setFont(fontFor(TextStyle::TitleLarge));
    lblTitle.setColour(juce::Label::textColourId, colours::text());
    addAndMakeVisible(lblTitle);

    lblBpm.setFont(fontFor(TextStyle::BodyMedium));
    lblBpm.setColour(juce::Label::textColourId, colours::textDim());
    lblBpm.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(lblBpm);

    lblSession.setFont(fontFor(TextStyle::LabelSmall));
    lblSession.setColour(juce::Label::textColourId, colours::textDim());
    addAndMakeVisible(lblSession);

    cmbSessionBars.addItem("4 bars", 1);
    cmbSessionBars.addItem("8 bars", 2);
    cmbSessionBars.addItem("16 bars", 3);
    {
        const int bars = processorRef.syncSessionBars.load();
        int id = 1;
        if (bars >= 16) id = 3;
        else if (bars >= 8) id = 2;
        cmbSessionBars.setSelectedId(id, juce::dontSendNotification);
    }
    cmbSessionBars.setTooltip("When the host is not looping, wrap the playhead over this session length for preview sync.");
    cmbSessionBars.addListener(this);
    addAndMakeVisible(cmbSessionBars);

    if (processorRef.lastBrowserDir.isNotEmpty())
    {
        const juce::File dir(processorRef.lastBrowserDir);
        if (dir.isDirectory())
            fileBrowser.setRootDirectory(dir);
    }
    fileBrowser.onDirectoryChanged = [this](const juce::String& path) {
        processorRef.lastBrowserDir = path;
    };
    fileBrowser.onGetPlayheadState = [this] {
        const double beat = processorRef.hostBeatPos.load();
        const bool loopOn = processorRef.hostLoopActive.load();
        return std::make_tuple(beat,
                               processorRef.hostLoopPpqStart.load(),
                               processorRef.hostLoopPpqEnd.load(),
                               loopOn);
    };
    fileBrowser.onGetSessionLengthBeats = [this] {
        return (double) (juce::jmax(1, processorRef.syncSessionBars.load()) * 4);
    };
    addAndMakeVisible(fileBrowser);

    startTimerHz(30);
}

MidiBrowserEditor::~MidiBrowserEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MidiBrowserEditor::resized()
{
    auto r = getLocalBounds();
    const int headerH = juce::jmax(44, metrics::titleBarH);
    auto header = r.removeFromTop(headerH);

    lblTitle.setBounds(header.removeFromLeft(200).reduced(8, 4));
    cmbSessionBars.setBounds(header.removeFromRight(100).reduced(4, 6));
    lblSession.setBounds(header.removeFromRight(52).reduced(4, 4));
    lblBpm.setBounds(header.reduced(8, 4));

    fileBrowser.setBounds(r);
}

void MidiBrowserEditor::timerCallback()
{
    fileBrowser.repaint();
    processorRef.setPreviewState(fileBrowser.getPreviewClip(), fileBrowser.getHasPreviewClip(),
                                 fileBrowser.getPreviewMuted(), false);

    const int bpmInt = juce::jmax(1, (int) std::round(processorRef.hostBpm.load()));
    const auto t = juce::String(bpmInt) + " BPM";
    if (lblBpm.getText() != t)
        lblBpm.setText(t, juce::dontSendNotification);
}

void MidiBrowserEditor::comboBoxChanged(juce::ComboBox* box)
{
    if (box != &cmbSessionBars) return;
    const int id = cmbSessionBars.getSelectedId();
    int bars = 4;
    if (id == 2) bars = 8;
    else if (id == 3) bars = 16;
    processorRef.syncSessionBars.store(bars);
}

} // namespace pflow
