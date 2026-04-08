#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <tuple>
#include <functional>
#include "MidiFileData.h"
#include "Theme.h"

namespace pflow {

class PatternFlowProcessor;

class ControlPanel : public juce::Component,
                     public juce::ComboBox::Listener
{
public:
    explicit ControlPanel(PatternFlowProcessor& proc);

    void resized() override;
    void paint(juce::Graphics&) override;
    void refreshComponentColours();
    void comboBoxChanged(juce::ComboBox*) override;

    std::function<void()> onC0Clicked;
    std::function<void()> onTransposeClicked;
    /** Arrangement should repaint when comp mode or comp content changes. */
    std::function<void()> onTakeCompSwitched;

    void refreshTakeCompUI();

private:
    PatternFlowProcessor& processor;

    juce::ToggleButton  btnScaleToggle { "Scale" };
    juce::ComboBox      cmbScaleRoot;
    juce::ComboBox      cmbScaleType;
    juce::TextButton    btnC0 { "C0" };
    juce::TextButton    btnTranspose { "Transpose" };

    juce::ToggleButton  btnComp { "Comp" };
    juce::Slider        sldRandomRegions;
    juce::TextButton    btnRandomComp { "Random" };
    juce::TextButton    btnSwapComp { "Swap" };

    /** Last integer region count applied (for randomizing on each step while dragging the knob). */
    int lastRandomKnobInt = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanel)
};

} // namespace pflow
