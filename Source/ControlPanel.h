#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiFileData.h"
#include "Theme.h"

namespace pflow {

class PatternFlowProcessor;

// ── Top control bar: knobs, scale, split, root note, grid snap ──────────────
class ControlPanel : public juce::Component,
                     public juce::Slider::Listener,
                     public juce::ComboBox::Listener
{
public:
    explicit ControlPanel(PatternFlowProcessor& proc);

    void resized() override;
    void paint(juce::Graphics&) override;
    void refreshComponentColours();

    void sliderValueChanged(juce::Slider*) override;
    void comboBoxChanged(juce::ComboBox*) override;

private:
    PatternFlowProcessor& processor;

    // Knobs
    juce::Slider knobHumanTiming;
    juce::Slider knobHumanVelocity;
    juce::Slider knobFeel;
    juce::Slider knobIntonation;
    juce::Label  lblHumanTiming   { {}, "Timing" };
    juce::Label  lblHumanVelocity { {}, "Velocity" };
    juce::Label  lblFeel          { {}, "Feel" };
    juce::Label  lblIntonation    { {}, "Intonation" };

    // Scale
    juce::ToggleButton btnScaleEnable { "Scale" };
    juce::ComboBox     cmbScaleRoot;
    juce::ComboBox     cmbScaleType;

    // Root note remap
    juce::ComboBox     cmbRootNote;
    juce::Label        lblRootNote { {}, "Root" };

    // MIDI split
    juce::ToggleButton btnSplitEnable { "Split" };
    juce::TextButton   btnSplitEdit   { "Edit..." };

    // Grid snap selector
    juce::ComboBox     cmbGridSnap;
    juce::Label        lblGridSnap { {}, "Grid" };

    // New lane button
    juce::TextButton   btnAddLane { "+ Lane" };

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& tooltip);

public:
    // Expose add-lane button callback
    std::function<void()> onAddLane;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanel)
};

} // namespace pflow
