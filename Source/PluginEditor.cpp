#include "PluginEditor.h"

namespace pflow {

struct MidiBrowserEditor::TruncateModeKeyListener : juce::KeyListener
{
    FileBrowserPanel* browser = nullptr;
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        if (browser == nullptr) return false;
        if (key.getModifiers().isAnyModifierKeyDown()) return false;
        const int code = key.getKeyCode();
        if (code != 't' && code != 'T') return false;
        browser->toggleTrimEmptyMeasuresMode();
        return true;
    }
};

void MidiBrowserEditor::addTruncateKeyListenerRecursive(juce::Component* c, juce::KeyListener* l)
{
    if (c == nullptr || l == nullptr) return;
    c->addKeyListener(l);
    for (int i = 0; i < c->getNumChildComponents(); ++i)
        addTruncateKeyListenerRecursive(c->getChildComponent(i), l);
}

void MidiBrowserEditor::removeTruncateKeyListenerRecursive(juce::Component* c, juce::KeyListener* l)
{
    if (c == nullptr || l == nullptr) return;
    c->removeKeyListener(l);
    for (int i = 0; i < c->getNumChildComponents(); ++i)
        removeTruncateKeyListenerRecursive(c->getChildComponent(i), l);
}

MidiBrowserEditor::MidiBrowserEditor(MidiBrowserProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setLookAndFeel(&lnf);
    applyAppTheme(processorRef.appThemeId.load());
    lnf.refreshColours();

    setSize(300, 520);
    setResizable(true, true);
    setResizeLimits(280, 480, 1200, 2000);

    lblTitle.setText("Midi Browser", juce::dontSendNotification);
    lblTitle.setFont(fontFor(TextStyle::LargeTitle));
    lblTitle.setJustificationType(juce::Justification::centredLeft);
    lblTitle.setColour(juce::Label::textColourId, colours::text());
    lblTitle.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(lblTitle);

    {
        juce::PluginHostType host;
        showAbletonHint_ = host.isAbletonLive();
    }
    if (showAbletonHint_)
    {
        lblAbletonHint.setText(
            "Ableton: use on its own MIDI track. Set your instrument track's MIDI From to this "
            "plugin and Monitor to In.",
            juce::dontSendNotification);
        lblAbletonHint.setFont(fontFor(TextStyle::Footnote));
        lblAbletonHint.setJustificationType(juce::Justification::centredLeft);
        lblAbletonHint.setColour(juce::Label::textColourId, colours::textDim());
        lblAbletonHint.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(lblAbletonHint);
    }

    if (processorRef.lastBrowserDir.isNotEmpty())
    {
        const juce::File dir(processorRef.lastBrowserDir);
        if (dir.isDirectory())
            fileBrowser.setRootDirectory(dir);
    }
    fileBrowser.setTrimEmptyMeasuresMode(processorRef.trimEmptyMeasuresPreview);
    fileBrowser.onDirectoryChanged = [this](const juce::String& path) {
        processorRef.lastBrowserDir = path;
    };
    fileBrowser.onTrimModeChanged = [this](bool enabled) {
        processorRef.trimEmptyMeasuresPreview = enabled;
    };
    fileBrowser.onGetSavedFolders = [this] {
        return processorRef.savedBrowserDirs;
    };
    fileBrowser.onSaveFolder = [this](const juce::String& path) {
        processorRef.addSavedBrowserDir(path);
    };
    fileBrowser.onRemoveSavedFolder = [this](const juce::String& path) {
        processorRef.removeSavedBrowserDir(path);
    };
    fileBrowser.onGetHostPlaying = [this] { return processorRef.hostPlaying.load(); };
    fileBrowser.onGetPlayheadState = [this] {
        return std::make_tuple(processorRef.hostBeatPos.load(),
                               processorRef.hostLoopPpqStart.load(),
                               processorRef.hostLoopPpqEnd.load(),
                               processorRef.hostLoopActive.load());
    };
    fileBrowser.onGetSessionLengthBeats = [this] {
        return (double) (juce::jmax(1, processorRef.syncSessionBars.load()) * 4);
    };
    addAndMakeVisible(fileBrowser);

    truncateModeKeys_ = std::make_unique<TruncateModeKeyListener>();
    truncateModeKeys_->browser = &fileBrowser;
    addTruncateKeyListenerRecursive(this, truncateModeKeys_.get());

    startTimerHz(30);
}

MidiBrowserEditor::~MidiBrowserEditor()
{
    if (truncateModeKeys_)
        removeTruncateKeyListenerRecursive(this, truncateModeKeys_.get());
    stopTimer();
    setLookAndFeel(nullptr);
}

void MidiBrowserEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg());

    g.setColour(colours::separator());
    g.drawHorizontalLine(metrics::editorHeaderH, 0.0f, (float) getWidth());
}

void MidiBrowserEditor::resized()
{
    auto r = getLocalBounds();
    const int pad = metrics::pluginPad;

    auto header = r.removeFromTop(metrics::editorHeaderH).reduced(pad, 8);
    lblTitle.setBounds(header);

    if (showAbletonHint_)
        lblAbletonHint.setBounds(r.removeFromTop(40).reduced(pad, 4));

    fileBrowser.setBounds(r.reduced(pad, 0).withTrimmedBottom(pad));
}

void MidiBrowserEditor::timerCallback()
{
    fileBrowser.repaint();
    processorRef.setPreviewState(fileBrowser.getPreviewClip(), fileBrowser.getHasPreviewClip(),
                                 fileBrowser.getPreviewMuted(), false);
}

} // namespace pflow
