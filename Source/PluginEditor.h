#pragma once
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "FileBrowserPanel.h"
#include "Theme.h"

namespace pflow {

class MidiBrowserEditor : public juce::AudioProcessorEditor,
                          public juce::Timer
{
public:
    explicit MidiBrowserEditor(MidiBrowserProcessor&);
    ~MidiBrowserEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    MidiBrowserProcessor& processorRef;
    PatternFlowLookAndFeel lnf;

    juce::Label lblTitle;

    juce::Label lblAbletonHint;
    bool showAbletonHint_ = false;

    FileBrowserPanel fileBrowser;

    struct TruncateModeKeyListener;
    std::unique_ptr<TruncateModeKeyListener> truncateModeKeys_;

    static void addTruncateKeyListenerRecursive(juce::Component* c, juce::KeyListener* l);
    static void removeTruncateKeyListenerRecursive(juce::Component* c, juce::KeyListener* l);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBrowserEditor)
};

} // namespace pflow
