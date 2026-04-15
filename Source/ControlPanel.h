#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <tuple>
#include <functional>
#include "MidiFileData.h"
#include "Theme.h"

namespace pflow {

class PatternFlowProcessor;

class ControlPanel : public juce::Component,
                     public juce::ComboBox::Listener,
                     public juce::Timer
{
public:
    explicit ControlPanel(PatternFlowProcessor& proc);

    void resized() override;
    void paint(juce::Graphics&) override;
    void refreshComponentColours();
    void comboBoxChanged(juce::ComboBox*) override;
    void timerCallback() override;

    std::function<void()> onC0Clicked;
    /** Arrangement should repaint when comp mode or comp content changes. */
    std::function<void()> onTakeCompSwitched;

    void refreshTakeCompUI();

    /** Keep rotary loop sliders aligned with the current session length + grid interval. */
    void updateLoopSliderIntervals();
    /** Push processor loop beats into sliders (used after grid/session changes). */
    void syncLoopSliderValuesFromProcessor();

    /** Call after grid snap or session length changes: reclamp loops + refresh sliders. */
    void syncLoopsAfterSessionOrGridChange();

private:
    PatternFlowProcessor& processor;

    juce::TextButton    btnLoopToggle { "Loop" };
    juce::TextButton    btnTransposeToggle { "Transpose" };
    juce::ComboBox      cmbScaleRoot;
    juce::ComboBox      cmbScaleType;
    juce::ComboBox      cmbOctave;
    juce::TextButton    btnC0 { "C0" };

    juce::TextButton    btnComp { "Comp" };
    juce::Slider        sldRandomRegions;
    juce::TextButton    btnRandomComp { "Random" };
    juce::TextButton    btnSwapComp { "Swap" };
    juce::TextButton    btnDefaultComp { "Default" };

    juce::Slider        sldLoopStart;
    juce::TextButton    btnLoopSync { juce::String::charToString(0x221E) }; // ∞
    juce::Slider        sldLoopEnd;

    bool blinkOn_ = false;
    int divX1_ = 0;
    int divX2_ = 0;

    /** Last integer region count applied (for randomizing on each step while dragging the knob). */
    int lastRandomKnobInt = 8;

    void clampLoopBeatsToSession();
    void updateLoopControlColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanel)
};

} // namespace pflow
