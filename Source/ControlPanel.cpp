#include "ControlPanel.h"
#include "PluginProcessor.h"

namespace pflow {

ControlPanel::ControlPanel(PatternFlowProcessor& proc) : processor(proc)
{
    // Setup knobs
    setupKnob(knobHumanTiming,   lblHumanTiming);
    setupKnob(knobHumanVelocity, lblHumanVelocity);
    setupKnob(knobFeel,          lblFeel);
    setupKnob(knobIntonation,    lblIntonation);

    // Scale enable
    btnScaleEnable.setColour(juce::ToggleButton::textColourId, colours::text);
    btnScaleEnable.setColour(juce::ToggleButton::tickColourId, colours::accent);
    btnScaleEnable.onClick = [this]
    {
        processor.scaleEnabled.store(btnScaleEnable.getToggleState());
    };
    addAndMakeVisible(btnScaleEnable);

    // Scale root (C, C#, D, ... B)
    const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    for (int i = 0; i < 12; ++i)
        cmbScaleRoot.addItem(noteNames[i], i + 1);
    cmbScaleRoot.setSelectedId(1);
    cmbScaleRoot.addListener(this);
    addAndMakeVisible(cmbScaleRoot);

    // Scale type
    for (int i = 0; i < (int)ScaleType::Count; ++i)
        cmbScaleType.addItem(scaleTypeName((ScaleType)i), i + 1);
    cmbScaleType.setSelectedId(2); // Major
    cmbScaleType.addListener(this);
    addAndMakeVisible(cmbScaleType);

    // Root note remap
    lblRootNote.setColour(juce::Label::textColourId, colours::textDim);
    lblRootNote.setFont(juce::Font(12.0f));
    addAndMakeVisible(lblRootNote);

    for (int oct = -2; oct <= 8; ++oct)
    {
        int midiNote = (oct + 2) * 12;
        cmbRootNote.addItem("C" + juce::String(oct), midiNote + 1);
    }
    cmbRootNote.setSelectedId(25); // C1 = MIDI 24, id=25
    cmbRootNote.addListener(this);
    addAndMakeVisible(cmbRootNote);

    // MIDI split
    btnSplitEnable.setColour(juce::ToggleButton::textColourId, colours::text);
    btnSplitEnable.setColour(juce::ToggleButton::tickColourId, colours::accent);
    btnSplitEnable.onClick = [this]
    {
        processor.splitEnabled.store(btnSplitEnable.getToggleState());
    };
    addAndMakeVisible(btnSplitEnable);

    btnSplitEdit.setColour(juce::TextButton::buttonColourId, colours::bgLight);
    btnSplitEdit.setColour(juce::TextButton::textColourOffId, colours::text);
    btnSplitEdit.onClick = [this]
    {
        // Simple split editor popup
        juce::PopupMenu menu;
        menu.addItem(1, "Drums: C1-B1 -> Ch1, C2-B2 -> Ch2");
        menu.addItem(2, "Octave Split: Low -> Ch1, High -> Ch2");
        menu.addItem(3, "Clear All Rules");

        menu.showMenuAsync(juce::PopupMenu::Options(),
            [this](int result)
        {
            if (result == 1)
            {
                processor.splitRules.clear();
                processor.splitRules.push_back({24, 35, 0});   // C1-B1 -> output 0
                processor.splitRules.push_back({36, 47, 1});   // C2-B2 -> output 1
            }
            else if (result == 2)
            {
                processor.splitRules.clear();
                processor.splitRules.push_back({0, 59, 0});    // Low -> output 0
                processor.splitRules.push_back({60, 127, 1});  // High -> output 1
            }
            else if (result == 3)
            {
                processor.splitRules.clear();
            }
        });
    };
    addAndMakeVisible(btnSplitEdit);

    // Add lane button
    btnAddLane.setColour(juce::TextButton::buttonColourId, colours::accent);
    btnAddLane.setColour(juce::TextButton::textColourOffId, colours::textBright);
    btnAddLane.onClick = [this] { if (onAddLane) onAddLane(); };
    addAndMakeVisible(btnAddLane);
}

void ControlPanel::setupKnob(juce::Slider& knob, juce::Label& label)
{
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange(0.0, 1.0, 0.01);
    knob.setValue(0.0);
    knob.addListener(this);
    addAndMakeVisible(knob);

    label.setColour(juce::Label::textColourId, colours::textDim);
    label.setFont(juce::Font(11.0f));
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void ControlPanel::resized()
{
    auto b = getLocalBounds().reduced(metrics::padding, 2);
    int knobS = metrics::knobSize;
    int knobSpace = metrics::knobSpacing;

    // Knobs
    int x = b.getX();
    int cy = b.getCentreY();

    auto placeKnob = [&](juce::Slider& knob, juce::Label& lbl)
    {
        knob.setBounds(x, cy - knobS / 2 - 2, knobS, knobS);
        lbl.setBounds(x - 2, cy + knobS / 2 - 2, knobS + 4, metrics::knobLabelH);
        x += knobSpace;
    };

    placeKnob(knobHumanTiming,   lblHumanTiming);
    placeKnob(knobHumanVelocity, lblHumanVelocity);
    placeKnob(knobFeel,          lblFeel);
    placeKnob(knobIntonation,    lblIntonation);

    x += 8;

    // Scale section
    int secH = 24;
    btnScaleEnable.setBounds(x, cy - secH + 2, 60, secH - 2);
    cmbScaleRoot.setBounds(x + 62, cy - secH + 2, 50, secH - 2);
    cmbScaleType.setBounds(x + 114, cy - secH + 2, 115, secH - 2);

    // Root note
    lblRootNote.setBounds(x, cy + 4, 34, secH - 4);
    cmbRootNote.setBounds(x + 34, cy + 4, 58, secH - 4);

    x += 236;

    // Split
    btnSplitEnable.setBounds(x, cy - secH + 2, 55, secH - 2);
    btnSplitEdit.setBounds(x + 57, cy - secH + 2, 55, secH - 2);

    // Add lane (far right)
    btnAddLane.setBounds(b.getRight() - 76, cy - 13, 72, 26);
}

void ControlPanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::bgLight);
    g.setColour(colours::panelBorder);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());
}

void ControlPanel::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &knobHumanTiming)
        processor.humanTiming.store((float)slider->getValue());
    else if (slider == &knobHumanVelocity)
        processor.humanVelocity.store((float)slider->getValue());
    else if (slider == &knobFeel)
        processor.humanFeel.store((float)slider->getValue());
    else if (slider == &knobIntonation)
        processor.intonation.store((float)slider->getValue());
}

void ControlPanel::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == &cmbScaleRoot)
        processor.scaleRoot.store(combo->getSelectedId() - 1);
    else if (combo == &cmbScaleType)
        processor.scaleType.store(combo->getSelectedId() - 1);
    else if (combo == &cmbRootNote)
        processor.rootNoteRemap.store(combo->getSelectedId() - 1);
}

} // namespace pflow
