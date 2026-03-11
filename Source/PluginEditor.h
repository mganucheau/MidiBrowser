#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "FileBrowserPanel.h"
#include "ArrangementView.h"
#include "PianoRollEditor.h"
#include "ControlPanel.h"
#include "Theme.h"

namespace pflow {

class PatternFlowEditor : public juce::AudioProcessorEditor,
                          public juce::DragAndDropContainer,
                          public juce::Timer
{
public:
    explicit PatternFlowEditor(PatternFlowProcessor&);
    ~PatternFlowEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    PatternFlowProcessor& processorRef;

    PatternFlowLookAndFeel lnf;

    // Panels
    ControlPanel      controlPanel;
    FileBrowserPanel  fileBrowser;
    ArrangementView   arrangementView;
    PianoRollEditor   pianoRoll;

    // Title bar
    juce::Label lblTitle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternFlowEditor)
};

} // namespace pflow
