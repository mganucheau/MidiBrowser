#include "ControlPanel.h"
#include "PluginProcessor.h"

namespace pflow {

ControlPanel::ControlPanel(PatternFlowProcessor& proc) : processor(proc)
{
    // Setup knobs with tooltips
    setupKnob(knobHumanTiming,   lblHumanTiming,   "Randomise note timing (0-100%)");
    setupKnob(knobHumanVelocity, lblHumanVelocity, "Randomise note velocity (0-100%)");
    setupKnob(knobFeel,          lblFeel,           "Swing / groove feel amount");
    setupKnob(knobIntonation,    lblIntonation,     "Pitch micro-variation amount");

    // Scale enable
    btnScaleEnable.setColour(juce::ToggleButton::textColourId, colours::text());
    btnScaleEnable.setColour(juce::ToggleButton::tickColourId, colours::accent());
    btnScaleEnable.setTooltip("Enable scale quantisation");
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
    cmbScaleRoot.setTooltip("Scale root note");
    cmbScaleRoot.addListener(this);
    addAndMakeVisible(cmbScaleRoot);

    // Scale type
    for (int i = 0; i < (int)ScaleType::Count; ++i)
        cmbScaleType.addItem(scaleTypeName((ScaleType)i), i + 1);
    cmbScaleType.setSelectedId(2); // Major
    cmbScaleType.setTooltip("Scale type");
    cmbScaleType.addListener(this);
    addAndMakeVisible(cmbScaleType);
    // Sync processor with UI defaults
    processor.scaleRoot.store(0);  // C
    processor.scaleType.store(1);  // Major

    // Root note remap
    lblRootNote.setColour(juce::Label::textColourId, colours::textDim());
    lblRootNote.setFont(juce::Font(12.0f));
    addAndMakeVisible(lblRootNote);

    for (int oct = -2; oct <= 8; ++oct)
    {
        int midiNote = (oct + 2) * 12;
        cmbRootNote.addItem("C" + juce::String(oct), midiNote + 1);
    }
    cmbRootNote.setSelectedId(25); // C1 = MIDI 24, id=25
    cmbRootNote.setTooltip("Root note for transpose");
    cmbRootNote.addListener(this);
    addAndMakeVisible(cmbRootNote);

    // MIDI split
    btnSplitEnable.setColour(juce::ToggleButton::textColourId, colours::text());
    btnSplitEnable.setColour(juce::ToggleButton::tickColourId, colours::accent());
    btnSplitEnable.setTooltip("Enable MIDI keyboard split routing");
    btnSplitEnable.onClick = [this]
    {
        processor.splitEnabled.store(btnSplitEnable.getToggleState());
    };
    addAndMakeVisible(btnSplitEnable);

    btnSplitEdit.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnSplitEdit.setColour(juce::TextButton::textColourOffId, colours::text());
    btnSplitEdit.setTooltip("Edit split rules");
    btnSplitEdit.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Drums: C1-B1 -> Ch1, C2-B2 -> Ch2");
        menu.addItem(2, "Octave Split: Low -> Ch1, High -> Ch2");
        menu.addItem(3, "Clear All Rules");

        menu.showMenuAsync(juce::PopupMenu::Options(),
            [this](int result)
        {
            if (result == 1)
            {
                juce::ScopedLock sl(processor.splitLock);
                processor.splitRules.clear();
                processor.splitRules.push_back({24, 35, 0});
                processor.splitRules.push_back({36, 47, 1});
            }
            else if (result == 2)
            {
                juce::ScopedLock sl(processor.splitLock);
                processor.splitRules.clear();
                processor.splitRules.push_back({0, 59, 0});
                processor.splitRules.push_back({60, 127, 1});
            }
            else if (result == 3)
            {
                juce::ScopedLock sl(processor.splitLock);
                processor.splitRules.clear();
            }
        });
    };
    addAndMakeVisible(btnSplitEdit);

    // Grid snap selector
    lblGridSnap.setColour(juce::Label::textColourId, colours::textDim());
    lblGridSnap.setFont(juce::Font(12.0f));
    addAndMakeVisible(lblGridSnap);

    cmbGridSnap.addItem("Off", 1);
    cmbGridSnap.addItem("Bar", 2);
    cmbGridSnap.addItem("Beat", 3);
    cmbGridSnap.addItem("1/2", 4);
    cmbGridSnap.addItem("1/4", 5);
    cmbGridSnap.addItem("1/8", 6);
    cmbGridSnap.addItem("1/16", 7);
    cmbGridSnap.setSelectedId(3); // Beat
    cmbGridSnap.setTooltip("Grid snap resolution");
    cmbGridSnap.addListener(this);
    addAndMakeVisible(cmbGridSnap);

    // Add lane button
    btnAddLane.setColour(juce::TextButton::buttonColourId, colours::accent());
    btnAddLane.setColour(juce::TextButton::textColourOffId, colours::textBright());
    btnAddLane.setTooltip("Add a new lane (keyboard: click + in arrangement)");
    btnAddLane.onClick = [this] { if (onAddLane) onAddLane(); };
    addAndMakeVisible(btnAddLane);
}

void ControlPanel::setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& tooltip)
{
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange(0.0, 1.0, 0.01);
    knob.setValue(0.0);
    knob.setTooltip(tooltip);
    knob.addListener(this);
    addAndMakeVisible(knob);

    label.setColour(juce::Label::textColourId, colours::textDim());
    label.setFont(juce::Font(12.0f));
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void ControlPanel::resized()
{
    auto b = getLocalBounds().reduced(metrics::padding, 4);
    int knobS = metrics::knobSize;
    int knobSpace = metrics::knobSpacing;

    // ── Knobs on the left, vertically centered ──
    int knobRowCY = b.getY() + (knobS / 2) + 2;
    int x = b.getX() + 4;

    auto placeKnob = [&](juce::Slider& knob, juce::Label& lbl)
    {
        knob.setBounds(x, knobRowCY - knobS / 2, knobS, knobS);
        lbl.setBounds(x - 4, knobRowCY + knobS / 2 - 2, knobS + 8, metrics::knobLabelH);
        x += knobSpace;
    };

    placeKnob(knobHumanTiming,   lblHumanTiming);
    placeKnob(knobHumanVelocity, lblHumanVelocity);
    placeKnob(knobFeel,          lblFeel);
    placeKnob(knobIntonation,    lblIntonation);

    // ── Settings to the right of knobs, arranged in two rows ──
    int settingsX = x + 12;
    int secH = 24;
    int topRowY = b.getY() + 4;
    int botRowY = b.getBottom() - secH - 2;

    // Top row: Scale enable + root + type
    btnScaleEnable.setBounds(settingsX, topRowY, 60, secH);
    cmbScaleRoot.setBounds(settingsX + 62, topRowY, 50, secH);
    cmbScaleType.setBounds(settingsX + 114, topRowY, 115, secH);

    // Bottom row: Root note + Grid snap
    lblRootNote.setBounds(settingsX, botRowY, 34, secH);
    cmbRootNote.setBounds(settingsX + 34, botRowY, 58, secH);

    lblGridSnap.setBounds(settingsX + 100, botRowY, 30, secH);
    cmbGridSnap.setBounds(settingsX + 130, botRowY, 60, secH);

    // Split section (right of scale)
    int splitX = settingsX + 236;
    btnSplitEnable.setBounds(splitX, topRowY, 55, secH);
    btnSplitEdit.setBounds(splitX + 57, topRowY, 55, secH);

    // Add lane (far right, vertically centered)
    btnAddLane.setBounds(b.getRight() - 76, b.getCentreY() - 13, 72, 26);
}

void ControlPanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::bgLight());
    g.setColour(colours::panelBorder());
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
    else if (combo == &cmbGridSnap)
    {
        // Map combo IDs to GridSize enum
        using GS = PatternFlowProcessor::GridSize;
        int id = combo->getSelectedId();
        GS gs = GS::Beat;
        switch (id)
        {
            case 1: gs = GS::Off;          break;
            case 2: gs = GS::Bar;          break;
            case 3: gs = GS::Beat;         break;
            case 4: gs = GS::HalfBeat;     break;
            case 5: gs = GS::QuarterBeat;  break;
            case 6: gs = GS::Eighth;       break;
            case 7: gs = GS::Sixteenth;    break;
        }
        processor.gridSnap.store((int)gs);
    }
}

} // namespace pflow
