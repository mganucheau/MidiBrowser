#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "FileBrowserPanel.h"
#include "Theme.h"

namespace pflow {

class MidiBrowserEditor : public juce::AudioProcessorEditor,
                          public juce::Timer,
                          public juce::ComboBox::Listener
{
public:
    explicit MidiBrowserEditor(MidiBrowserProcessor&);
    ~MidiBrowserEditor() override;

    void resized() override;
    void timerCallback() override;
    void comboBoxChanged(juce::ComboBox*) override;

private:
    MidiBrowserProcessor& processorRef;
    PatternFlowLookAndFeel lnf;

    juce::Label lblTitle;
    juce::Label lblBpm { {}, "120 BPM" };
    juce::Label lblSession { {}, "Wrap" };
    juce::ComboBox cmbSessionBars;

    FileBrowserPanel fileBrowser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiBrowserEditor)
};

} // namespace pflow
