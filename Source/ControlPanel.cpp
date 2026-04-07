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

    // Session bars selector
    lblSessionBars.setColour(juce::Label::textColourId, colours::textDim());
    lblSessionBars.setFont(juce::Font(12.0f));
    addAndMakeVisible(lblSessionBars);

    const int barOptions[] = { 4, 8, 16, 32, 64 };
    for (int i = 0; i < 5; ++i)
        cmbSessionBars.addItem(juce::String(barOptions[i]), i + 1);
    cmbSessionBars.setSelectedId(2); // 8 bars default
    cmbSessionBars.setTooltip("Session length in bars");
    cmbSessionBars.addListener(this);
    addAndMakeVisible(cmbSessionBars);

    // Add lane button with icon
    btnAddLane.setButtonText("+ Lane");
    btnAddLane.setColour(juce::TextButton::buttonColourId, colours::accent());
    btnAddLane.setColour(juce::TextButton::textColourOffId, colours::textBright());
    btnAddLane.setTooltip("Add a new lane (keyboard: click + in arrangement)");
    btnAddLane.onClick = [this] { if (onAddLane) onAddLane(); };
    addAndMakeVisible(btnAddLane);

    // Trim button
    btnTrim.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnTrim.setColour(juce::TextButton::textColourOffId, colours::text());
    btnTrim.setTooltip("Trim empty measures from the end of each clip");
    btnTrim.onClick = [this] { if (onTrimClips) onTrimClips(); };
    addAndMakeVisible(btnTrim);

    // Record button
    updateRecordButton();
    btnRecord.setTooltip("Toggle MIDI recording into a new lane");
    btnRecord.onClick = [this] { if (onRecordToggle) onRecordToggle(); };
    addAndMakeVisible(btnRecord);

    // Comp controls
    btnCompEnable.setColour(juce::ToggleButton::textColourId, colours::text());
    btnCompEnable.setColour(juce::ToggleButton::tickColourId, colours::accent());
    btnCompEnable.setTooltip("Enable Quick Swipe Comping mode");
    btnCompEnable.onClick = [this]
    {
        processor.compEnabled.store(btnCompEnable.getToggleState());
    };
    addAndMakeVisible(btnCompEnable);

    rebuildCompCombo();
    cmbComp.setTooltip("Select active comp");
    cmbComp.onChange = [this]
    {
        int idx = cmbComp.getSelectedId() - 1;
        if (idx >= 0)
            processor.setActiveComp(idx);
    };
    addAndMakeVisible(cmbComp);

    btnCompAdd.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnCompAdd.setColour(juce::TextButton::textColourOffId, colours::text());
    btnCompAdd.setTooltip("Add a new comp");
    btnCompAdd.onClick = [this]
    {
        processor.addComp();
        rebuildCompCombo();
    };
    addAndMakeVisible(btnCompAdd);

    btnCompDel.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnCompDel.setColour(juce::TextButton::textColourOffId, colours::text());
    btnCompDel.setTooltip("Remove the active comp");
    btnCompDel.onClick = [this]
    {
        if ((int)processor.comps.size() > 1)
        {
            processor.removeComp(processor.activeCompIndex);
            rebuildCompCombo();
        }
    };
    addAndMakeVisible(btnCompDel);
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

    lblSessionBars.setBounds(settingsX + 198, botRowY, 30, secH);
    cmbSessionBars.setBounds(settingsX + 228, botRowY, 58, secH);

    // Split section (right of scale)
    int splitX = settingsX + 236;
    btnSplitEnable.setBounds(splitX, topRowY, 55, secH);
    btnSplitEdit.setBounds(splitX + 57, topRowY, 55, secH);

    // Comp controls (right of split)
    int compX = splitX + 120;
    btnCompEnable.setBounds(compX, topRowY, 60, secH);
    cmbComp.setBounds(compX + 62, topRowY, 80, secH);
    btnCompAdd.setBounds(compX + 144, topRowY, 24, secH);
    btnCompDel.setBounds(compX + 170, topRowY, 24, secH);

    // Trim + Record + Add lane (far right, vertically centered)
    btnTrim.setBounds(b.getRight() - 216, b.getCentreY() - 13, 52, 26);
    btnRecord.setBounds(b.getRight() - 152, b.getCentreY() - 13, 52, 26);
    btnAddLane.setBounds(b.getRight() - 76, b.getCentreY() - 13, 72, 26);
}

void ControlPanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::bgLight());

    // Bottom separator
    g.setColour(colours::panelBorder());
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());

    // Subtle accent line at top
    g.setColour(colours::accent().withAlpha(0.12f));
    g.fillRect(0.0f, 0.0f, (float)getWidth(), 2.0f);

    // Section dividers between knobs and settings
    auto b = getLocalBounds().reduced(metrics::padding, 4);
    int dividerX = b.getX() + 4 + metrics::knobSpacing * 4 + 4;
    g.setColour(colours::panelBorder().withAlpha(0.4f));
    g.drawVerticalLine(dividerX, (float)(b.getY() + 4), (float)(b.getBottom() - 4));
}

void ControlPanel::rebuildCompCombo()
{
    cmbComp.clear(juce::dontSendNotification);
    for (int i = 0; i < (int)processor.comps.size(); ++i)
        cmbComp.addItem(processor.comps[static_cast<size_t>(i)].name, i + 1);
    if (processor.comps.empty())
        cmbComp.addItem("(none)", 1);
    cmbComp.setSelectedId(processor.activeCompIndex + 1, juce::dontSendNotification);
}

void ControlPanel::updateRecordButton()
{
    bool isRec = processor.recording.load();
    if (isRec)
    {
        btnRecord.setButtonText("Stop");
        btnRecord.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdc2626));
        btnRecord.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        btnRecord.setButtonText("Rec");
        btnRecord.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8b2020));
        btnRecord.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
}

void ControlPanel::refreshComponentColours()
{
    // Toggle buttons
    btnScaleEnable.setColour(juce::ToggleButton::textColourId, colours::text());
    btnScaleEnable.setColour(juce::ToggleButton::tickColourId, colours::accent());
    btnSplitEnable.setColour(juce::ToggleButton::textColourId, colours::text());
    btnSplitEnable.setColour(juce::ToggleButton::tickColourId, colours::accent());

    // Labels
    lblRootNote.setColour(juce::Label::textColourId, colours::textDim());
    lblGridSnap.setColour(juce::Label::textColourId, colours::textDim());
    lblSessionBars.setColour(juce::Label::textColourId, colours::textDim());
    lblHumanTiming.setColour(juce::Label::textColourId, colours::textDim());
    lblHumanVelocity.setColour(juce::Label::textColourId, colours::textDim());
    lblFeel.setColour(juce::Label::textColourId, colours::textDim());
    lblIntonation.setColour(juce::Label::textColourId, colours::textDim());

    // Buttons
    btnSplitEdit.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnSplitEdit.setColour(juce::TextButton::textColourOffId, colours::text());
    btnTrim.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnTrim.setColour(juce::TextButton::textColourOffId, colours::text());
    btnAddLane.setColour(juce::TextButton::buttonColourId, colours::accent());
    btnAddLane.setColour(juce::TextButton::textColourOffId, colours::textBright());

    // Comp controls
    btnCompEnable.setColour(juce::ToggleButton::textColourId, colours::text());
    btnCompEnable.setColour(juce::ToggleButton::tickColourId, colours::accent());
    btnCompAdd.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnCompAdd.setColour(juce::TextButton::textColourOffId, colours::text());
    btnCompDel.setColour(juce::TextButton::buttonColourId, colours::bgLight());
    btnCompDel.setColour(juce::TextButton::textColourOffId, colours::text());
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
    else if (combo == &cmbSessionBars)
    {
        const int barOptions[] = { 4, 8, 16, 32, 64 };
        int idx = combo->getSelectedId() - 1;
        if (idx >= 0 && idx < 5)
        {
            int bars = barOptions[idx];
            processor.arrangementBars.store(bars);
            processor.loopEndBeat.store((double)(bars * 4));
            if (onSessionBarsChanged) onSessionBarsChanged(bars);
        }
    }
}

} // namespace pflow
