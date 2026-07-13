#include "EffectsInspector.h"

namespace pflow {

using namespace fx;

EffectsInspector::EffectsInspector()
    : tempoRow("Tempo", tempoToggle)
    , swingTimeRow("Swing Time", swingTimePopup)
    , quantizeTimeRow("Quantize Time", quantizeTimePopup)
    , quantizeStrengthSl("Quantize Strength", 0, 100, 0, false)
    , swing("Swing", 0, 100, 0, false)
    , pocket("Pocket", -100, 100, 0, true)
    , humanize("Humanize", 0, 100, 0, false)
    , lengthSl("Length", 25, 200, 100, false)
    , dynamicsSl("Dynamics", -100, 100, 0, true)
    , intensitySl("Intensity", 0, 200, 100, false)
    , articulationRow("Articulation", articulationPopup)
    , articulationStrengthSl("Articulation Strength", 0, 100, 0, false)
    , velocityRangeSl("Velocity Range", 1, 127, 1, 127)
    , sustainRow("Sustain Pedal", sustainPopup)
    , complexitySl("Complexity", 0, 100, 50, false)
    , variationsSl("Variations", 0, 16, 0, false)
    , delayTimeRow("Delay Time", delayTimePopup)
    , delayAmountSl("Delay Amount", 0, 100, 0, false)
    , delayFeedbackSl("Feedback", 0, 100, 40, false)
    , arpModeRow("Arpeggiator", arpModePopup)
    , arpRateRow("Arp Rate", arpRatePopup)
    , arpGateSl("Arp Gate", 10, 100, 70, false)
    , arpOctavesRow("Arp Octaves", arpOctavesPopup)
    , strumDirRow("Strum", strumDirPopup)
    , strumSpeedSl("Strum Speed", 0, 100, 40, false)
    , strumAmountSl("Strum Amount", 0, 100, 0, false)
    , octaveRow("Octave", octaveStepper)
    , octaveRangeRow("Octave Range", octaveRangePopup)
    , pitchRangeRow(pitchMinPopup, pitchMaxPopup)
    , keyModeRow(keyPopup, modePopup)
    , trimRow("Trim empty measures", trimSwitch)
    , extendRow("Extend", extendPopup)
    , fitRow("Fit to Scale", fitSwitch)
    , mapRow("Map to Root", mapSwitch)
{
    addMouseListener(this, true);

    viewport.setViewedComponent(&body, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    btnReset.setComponentID("btnEffectsReset");
    btnReset.setWantsKeyboardFocus(false);
    btnReset.setTooltip("Reset effects and pitch shaping to defaults");
    btnReset.onClick = [this]
    {
        groove = GrooveParams();
        setGroove(groove, juce::dontSendNotification);
        setBpmMultiplier(1.0, juce::dontSendNotification);
        resetPitchEditFields();
        setEdit(edit, juce::dontSendNotification);
        if (onResetGroove) onResetGroove();
        notifyGroove();
        notifyEdit();
        if (onBpmMultiplierChanged) onBpmMultiplierChanged(bpmMultiplier);
    };
    addAndMakeVisible(btnReset);

    btnEffectsLock.setComponentID("btnLock");
    btnEffectsLock.setWantsKeyboardFocus(false);
    btnEffectsLock.setTooltip("Lock effects while browsing clips");
    btnEffectsLock.onClick = [this]
    {
        effectsLocked = !effectsLocked;
        setEffectsLocked(effectsLocked);
        if (onEffectsLockToggled) onEffectsLockToggled(effectsLocked);
    };
    addAndMakeVisible(btnEffectsLock);

    trimSwitch.setComponentID("btnTrim");
    trimSwitch.setTooltip("Remove empty bars from the start and end of the clip");
    trimSwitch.onClick = [this] { if (onTrimClicked) onTrimClicked(); };

    // Hover tips for inspector controls (shown when Settings → Show Tooltips is on).
    swing.setTooltip("Delay offbeat notes for a swung feel");
    pocket.setTooltip("Push or lay back timing (strong beats move less)");
    humanize.setTooltip("Add subtle deterministic timing jitter");
    lengthSl.setTooltip("Scale note durations (100% = original)");
    dynamicsSl.setTooltip("Metric accent contrast (negative inverts)");
    intensitySl.setTooltip("Scale note velocities (100% = original)");
    quantizeStrengthSl.setTooltip("How strongly notes snap to the quantize grid");
    quantizeTimePopup.setTooltip("Grid for quantize snapping");
    swingTimePopup.setTooltip("Swing subdivision (which offbeats are delayed)");
    articulationPopup.setTooltip("Phrasing preset that drives Length, Intensity, and Dynamics");
    articulationStrengthSl.setTooltip("How strongly articulation reshapes phrasing");
    velocityRangeSl.setTooltip("Compress clip velocities into this MIDI range");
    sustainPopup.setTooltip("Generate sustain-pedal automation for preview and export");
    complexitySl.setTooltip("Simplify or enrich the clip relative to its baseline complexity");
    variationsSl.setTooltip("Cycle alternate slight takes of the clip");
    delayTimePopup.setTooltip("Delay tap spacing");
    delayAmountSl.setTooltip("How loud delayed repeats are");
    delayFeedbackSl.setTooltip("How many delay repeats and how long they decay");
    arpModePopup.setTooltip("Arpeggiate held chords (Off leaves notes as written)");
    arpRatePopup.setTooltip("Arpeggio step rate");
    arpGateSl.setTooltip("Note length inside each arpeggio step");
    arpOctavesPopup.setTooltip("How many octaves the arpeggio spans");
    strumDirPopup.setTooltip("Chord strum direction");
    strumSpeedSl.setTooltip("How quickly chord notes fan out");
    strumAmountSl.setTooltip("Strum strength (0 = off)");
    extendPopup.setTooltip("Tile the clip longer so arps and delays can evolve past the file length");
    octaveStepper.setTooltip("Transpose the whole clip by octaves");
    octaveRangePopup.setTooltip("Fold or spread pitches into 1-3 octaves");
    pitchRow.stepper.setTooltip("Transpose the whole clip by semitones");
    pitchMinPopup.setTooltip("Lowest note allowed after folding");
    pitchMaxPopup.setTooltip("Highest note allowed after folding");
    keyPopup.setTooltip("Target key root for Fit to Scale / Map to Root");
    modePopup.setTooltip("Scale mode used with Fit to Scale");
    fitSwitch.setTooltip("Snap pitches into the selected key and mode");
    mapSwitch.setTooltip("Transpose so the clip root matches the selected key");

    tempoToggle.onChange = [this](double m)
    {
        bpmMultiplier = m;
        if (onBpmMultiplierChanged) onBpmMultiplierChanged(bpmMultiplier);
        refreshDirtySections();
    };

    auto wireSlider = [this](FlatSliderRow& s, int knobIdx)
    {
        s.onChange = [this, knobIdx](int v)
        {
            groove.set(knobIdx, v);
            notifyGroove();
        };
    };
    wireSlider(swing, 0);
    wireSlider(pocket, 1);
    wireSlider(humanize, 2);
    wireSlider(dynamicsSl, 3);
    wireSlider(lengthSl, 4);
    wireSlider(intensitySl, 5);

    quantizeStrengthSl.onChange = [this](int v)
    {
        groove.quantizeStrength = v;
        notifyGroove();
    };
    articulationStrengthSl.onChange = [this](int v)
    {
        groove.articulationStrength = v;
        groove.applyArticulationToKnobs();
        syncArticulationKnobsToUi();
        notifyGroove();
    };

    velocityRangeSl.onChange = [this](int lo, int hi)
    {
        groove.velocityRangeLo = lo;
        groove.velocityRangeHi = hi;
        velocityRangeSl.valueText = juce::String(lo) + "-" + juce::String(hi);
        notifyGroove();
    };

    variationsSl.onChange = [this](int v)
    {
        groove.variationIndex = v;
        variationsSl.valueText = v <= 0 ? "Off" : ("#" + juce::String(v));
        notifyGroove();
    };

    complexitySl.onChange = [this](int v)
    {
        if (v == juce::jlimit(0, 100, clipComplexity))
            groove.complexityTarget = -1;
        else
            groove.complexityTarget = v;
        complexitySl.valueText = juce::String(v);
        notifyGroove();
    };

    delayAmountSl.onChange = [this](int v) { groove.delayAmount = v; notifyGroove(); };
    delayFeedbackSl.onChange = [this](int v) { groove.delayFeedback = v; notifyGroove(); };
    arpGateSl.onChange = [this](int v) { groove.arpGate = v; notifyGroove(); };
    strumSpeedSl.onChange = [this](int v) { groove.strumSpeed = v; notifyGroove(); };
    strumAmountSl.onChange = [this](int v) { groove.strumAmount = v; notifyGroove(); };

    swingTimePopup.setItems({ "1/16", "1/8", "1/4", "1/2", "1", "2" }, 1);
    swingTimePopup.onChange = [this](int idx)
    {
        groove.swingGridIndex = idx;
        groove.swingBase = idx == 0 ? SwingBase::Sixteenth : SwingBase::Eighth;
        notifyGroove();
    };

    juce::StringArray qItems;
    for (int i = 0; i < (int) QuantizeGrid::Count; ++i)
        qItems.add(quantizeGridLabel((QuantizeGrid) i));
    quantizeTimePopup.setItems(qItems, (int) QuantizeGrid::Eighth);
    quantizeTimePopup.onChange = [this](int idx)
    {
        groove.quantizeGridIndex = idx;
        notifyGroove();
    };

    juce::StringArray artItems;
    for (int i = 0; i < (int) Articulation::Count; ++i)
        artItems.add(articulationLabel((Articulation) i));
    articulationPopup.setItems(artItems, (int) Articulation::Off);
    articulationPopup.onChange = [this](int idx)
    {
        groove.articulationIndex = idx;
        if (idx == (int) Articulation::Off)
        {
            groove.articulationStrength = 0;
            articulationStrengthSl.setValue(0, juce::dontSendNotification);
            articulationStrengthSl.valueText = "0%";
        }
        groove.applyArticulationToKnobs();
        syncArticulationKnobsToUi();
        notifyGroove();
    };

    extendPopup.setItems({ "Off", "x2", "x4", "x8" }, 0);
    extendPopup.onChange = [this](int idx)
    {
        static const int kMult[] = { 1, 2, 4, 8 };
        edit.extendMult = kMult[juce::jlimit(0, 3, idx)];
        notifyEdit();
    };

    juce::StringArray susItems;
    for (int i = 0; i < (int) SustainPedalMode::Count; ++i)
        susItems.add(sustainPedalLabel((SustainPedalMode) i));
    sustainPopup.setItems(susItems, 0);
    sustainPopup.onChange = [this](int idx)
    {
        groove.sustainPedalMode = idx;
        notifyGroove();
    };

    juce::StringArray delayItems;
    for (int i = 0; i < (int) DelayTime::Count; ++i)
        delayItems.add(delayTimeLabel((DelayTime) i));
    delayTimePopup.setItems(delayItems, (int) DelayTime::Eighth);
    delayTimePopup.onChange = [this](int idx)
    {
        groove.delayTimeIndex = idx;
        notifyGroove();
    };

    juce::StringArray arpModeItems;
    for (int i = 0; i < (int) ArpMode::Count; ++i)
        arpModeItems.add(arpModeLabel((ArpMode) i));
    arpModePopup.setItems(arpModeItems, 0);
    arpModePopup.onChange = [this](int idx)
    {
        groove.arpModeIndex = idx;
        notifyGroove();
    };

    juce::StringArray arpRateItems;
    for (int i = 0; i < (int) DelayTime::Count; ++i)
        arpRateItems.add(delayTimeLabel((DelayTime) i));
    arpRatePopup.setItems(arpRateItems, (int) DelayTime::Sixteenth);
    arpRatePopup.onChange = [this](int idx)
    {
        groove.arpRateIndex = idx;
        notifyGroove();
    };

    arpOctavesPopup.setItems({ "1", "2", "3", "4" }, 0);
    arpOctavesPopup.onChange = [this](int idx)
    {
        groove.arpOctaves = juce::jlimit(1, 4, idx + 1);
        notifyGroove();
    };

    juce::StringArray strumItems;
    for (int i = 0; i < (int) StrumDirection::Count; ++i)
        strumItems.add(strumDirectionLabel((StrumDirection) i));
    strumDirPopup.setItems(strumItems, 0);
    strumDirPopup.onChange = [this](int idx)
    {
        groove.strumDirectionIndex = idx;
        notifyGroove();
    };

    octaveStepper.minV = -3;
    octaveStepper.maxV = 3;
    octaveStepper.format = [](int v) { return signedIntText(v); };
    octaveStepper.onChange = [this](int v)
    {
        edit.octave = v;
        refreshPitchAnnotation();
        notifyEdit();
    };

    octaveRangePopup.setItems({ "Off", "1", "2", "3" }, 0);
    octaveRangePopup.onChange = [this](int idx)
    {
        edit.octaveRange = juce::jlimit(0, 3, idx);
        notifyEdit();
    };

    juce::StringArray noteItems;
    for (int m = 0; m <= 127; ++m)
        noteItems.add(pitchName(m));
    pitchMinPopup.setItems(noteItems, 0);
    pitchMaxPopup.setItems(noteItems, 127);
    pitchMinPopup.onChange = [this](int idx)
    {
        edit.pitchMin = juce::jlimit(0, 127, idx);
        if (edit.pitchMax < edit.pitchMin)
        {
            edit.pitchMax = edit.pitchMin;
            pitchMaxPopup.setIndex(edit.pitchMax, juce::dontSendNotification);
        }
        notifyEdit();
    };
    pitchMaxPopup.onChange = [this](int idx)
    {
        edit.pitchMax = juce::jlimit(0, 127, idx);
        if (edit.pitchMin > edit.pitchMax)
        {
            edit.pitchMin = edit.pitchMax;
            pitchMinPopup.setIndex(edit.pitchMin, juce::dontSendNotification);
        }
        notifyEdit();
    };

    pitchRow.stepper.onChange = [this](int v)
    {
        edit.pitchShift = v;
        refreshPitchAnnotation();
        notifyEdit();
    };

    juce::StringArray keys;
    for (int i = 0; i < 12; ++i) keys.add(kNoteNames[(size_t) i]);
    keyPopup.setItems(keys, 0);
    keyPopup.onChange = [this](int idx)
    {
        edit.root = idx;
        refreshPitchAnnotation();
        notifyEdit();
    };

    juce::StringArray modes;
    for (int i = 0; i < kNumModes; ++i) modes.add(modeName((Mode) i));
    modePopup.setItems(modes, 0);
    modePopup.onChange = [this](int idx)
    {
        edit.mode = (Mode) idx;
        refreshPitchAnnotation();
        notifyEdit();
    };

    fitSwitch.onClick = [this]
    {
        edit.fitScale = fitSwitch.getToggleState();
        refreshPitchAnnotation();
        notifyEdit();
    };

    mapSwitch.onClick = [this]
    {
        edit.mapToRoot = mapSwitch.getToggleState();
        notifyEdit();
    };

    // Dirty predicates — when a section is folded, only dirty rows stay visible.
    playback.addRow(&tempoRow, kRowMinH, [this] { return std::abs(bpmMultiplier - 1.0) > 1.0e-6; });
    playback.addRow(&trimRow, kRowMinH, [this] { return edit.hasTrim(); });
    playback.addRow(&extendRow, kRowMinH, [this] { return edit.extendMult > 1; });

    timing.addRow(&quantizeTimeRow, kRowMinH, [this] {
        return groove.quantizeStrength > 0
            && groove.quantizeGridIndex != (int) QuantizeGrid::Eighth;
    });
    timing.addRow(&quantizeStrengthSl, kSliderRowH, [this] { return groove.quantizeStrength > 0; });
    timing.addRow(&swingTimeRow, kRowMinH, [this] { return groove.swingGridIndex != 1; });
    timing.addRow(&swing, kSliderRowH, [this] { return groove.swing != 0; });
    timing.addRow(&lengthSl, kSliderRowH, [this] { return groove.length != 100; });
    timing.addRow(&pocket, kSliderRowH, [this] { return groove.pocket != 0; });
    timing.addRow(&humanize, kSliderRowH, [this] { return groove.humanize != 0; });

    performance.addRow(&articulationRow, kRowMinH, [this] {
        return groove.articulationIndex != (int) Articulation::Off;
    });
    performance.addRow(&articulationStrengthSl, kSliderRowH, [this] {
        return groove.articulationIndex != (int) Articulation::Off && groove.articulationStrength > 0;
    });
    performance.addRow(&dynamicsSl, kSliderRowH, [this] { return groove.dynamics != 0; });
    performance.addRow(&intensitySl, kSliderRowH, [this] { return groove.intensity != 100; });
    performance.addRow(&velocityRangeSl, kSliderRowH, [this] {
        return groove.velocityRangeLo > 1 || groove.velocityRangeHi < 127;
    });
    performance.addRow(&sustainRow, kRowMinH, [this] {
        return groove.sustainPedalMode != (int) SustainPedalMode::Off;
    });

    pitchSec.addRow(&octaveRow, kRowMinH, [this] { return edit.octave != 0; });
    pitchSec.addRow(&octaveRangeRow, kRowMinH, [this] { return edit.octaveRange != 0; });
    pitchSec.addRow(&pitchRow, kRowMinH, [this] { return edit.pitchShift != 0; });
    pitchSec.addRow(&pitchRangeRow, kRowMinH, [this] { return edit.hasPitchRange(); });
    pitchSec.addRow(&keyModeRow, kRowMinH, [this] {
        return edit.root >= 0 && (edit.fitScale || edit.mapToRoot);
    });
    pitchSec.addRow(&fitRow, kRowMinH, [this] { return edit.fitScale; });
    pitchSec.addRow(&mapRow, kRowMinH, [this] { return edit.mapToRoot; });

    effectsSec.addRow(&complexitySl, kSliderRowH, [this] { return groove.complexityTarget >= 0; });
    effectsSec.addRow(&variationsSl, kSliderRowH, [this] { return groove.variationIndex > 0; });
    effectsSec.addRow(&delayTimeRow, kRowMinH, [this] {
        return groove.delayAmount > 0 && groove.delayTimeIndex != (int) DelayTime::Eighth;
    });
    effectsSec.addRow(&delayAmountSl, kSliderRowH, [this] { return groove.delayAmount > 0; });
    effectsSec.addRow(&delayFeedbackSl, kSliderRowH, [this] {
        return groove.delayAmount > 0 && groove.delayFeedback != 40;
    });
    effectsSec.addRow(&arpModeRow, kRowMinH, [this] {
        return groove.arpModeIndex != (int) ArpMode::Off;
    });
    effectsSec.addRow(&arpRateRow, kRowMinH, [this] {
        return groove.arpModeIndex != (int) ArpMode::Off
            && groove.arpRateIndex != (int) DelayTime::Sixteenth;
    });
    effectsSec.addRow(&arpGateSl, kSliderRowH, [this] {
        return groove.arpModeIndex != (int) ArpMode::Off && groove.arpGate != 70;
    });
    effectsSec.addRow(&arpOctavesRow, kRowMinH, [this] {
        return groove.arpModeIndex != (int) ArpMode::Off && groove.arpOctaves != 1;
    });
    effectsSec.addRow(&strumDirRow, kRowMinH, [this] {
        return groove.strumAmount > 0 && groove.strumDirectionIndex != (int) StrumDirection::Up;
    });
    effectsSec.addRow(&strumSpeedSl, kSliderRowH, [this] {
        return groove.strumAmount > 0 && groove.strumSpeed != 40;
    });
    effectsSec.addRow(&strumAmountSl, kSliderRowH, [this] { return groove.strumAmount > 0; });

    for (auto* sec : { &playback, &timing, &performance, &pitchSec, &effectsSec })
    {
        sec->onToggle = [this] { layoutSections(); };
        body.addAndMakeVisible(*sec);
    }

    setGroove(GrooveParams{}, juce::dontSendNotification);
    setEdit(ClipEdit{}, juce::dontSendNotification);
    setBpmMultiplier(1.0, juce::dontSendNotification);
}

void EffectsInspector::resetPitchEditFields()
{
    edit.octave = 0;
    edit.pitchShift = 0;
    edit.octaveRange = 0;
    edit.pitchMin = 0;
    edit.pitchMax = 127;
    edit.extendMult = 1;
    edit.fitScale = false;
    edit.mapToRoot = false;
    edit.root = -1;
    edit.mode = Mode::Ionian;
}

void EffectsInspector::syncArticulationKnobsToUi()
{
    lengthSl.setValue(groove.length, juce::dontSendNotification);
    intensitySl.setValue(groove.intensity, juce::dontSendNotification);
    dynamicsSl.setValue(groove.dynamics, juce::dontSendNotification);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], groove.length);
    intensitySl.valueText = grooveValueText(kKnobDefs[5], groove.intensity);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], groove.dynamics);
    lengthSl.repaint();
    intensitySl.repaint();
    dynamicsSl.repaint();
}

int EffectsInspector::resolvedPitchMidi() const
{
    const int refPc = edit.root >= 0 ? edit.root : (clipRootPc >= 0 ? clipRootPc : 0);
    int midi = juce::jlimit(0, 127, 60 + refPc + edit.octave * 12 + edit.pitchShift);
    if (edit.root >= 0 && edit.fitScale)
        midi = fitToScale(midi, edit.root, edit.mode);
    return midi;
}

void EffectsInspector::refreshPitchAnnotation()
{
    const int refPc = edit.root >= 0 ? edit.root : (clipRootPc >= 0 ? clipRootPc : -1);
    pitchRow.annotation = pitchScaleAnnotation(resolvedPitchMidi(), refPc, edit.mode);
    pitchRow.repaint();
}

void EffectsInspector::refreshComplexitySlider()
{
    const int v = groove.resolvedComplexityTarget(clipComplexity);
    complexitySl.setValue(v, juce::dontSendNotification);
    complexitySl.valueText = juce::String(v);
    complexitySl.repaint();
}

void EffectsInspector::refreshDirtySections()
{
    for (auto* sec : { &playback, &timing, &performance, &pitchSec, &effectsSec })
        sec->refreshDirtyRows();
    layoutSections();
}

void EffectsInspector::setEffectsLocked(bool locked)
{
    effectsLocked = locked;
    btnEffectsLock.icon = locked ? icons::lockClosed : icons::lockOpen;
    btnEffectsLock.active = locked;
    btnEffectsLock.setTooltip(locked ? "Unlock effects while browsing"
                                     : "Lock effects while browsing clips");
    btnEffectsLock.repaint();
    repaint();
}

void EffectsInspector::setPitchLocked(bool locked)
{
    pitchLocked = locked;
    juce::ignoreUnused(pitchLocked);
}

void EffectsInspector::setHasClip(bool has)
{
    hasClip = has;
    repaint();
}

void EffectsInspector::setClipRoot(int rootPc)
{
    clipRootPc = rootPc;
    refreshPitchAnnotation();
}

void EffectsInspector::setClipComplexity(int complexity)
{
    clipComplexity = juce::jlimit(0, 100, complexity);
    complexitySl.setDefault(clipComplexity);
    refreshComplexitySlider();
}

void EffectsInspector::setTrimState(bool active, bool enabled, const juce::String&)
{
    trimSwitch.setToggleState(active, juce::dontSendNotification);
    trimSwitch.setEnabled(enabled);
    trimSwitch.repaint();
    refreshDirtySections();
}

void EffectsInspector::setBpmMultiplier(double mult, juce::NotificationType)
{
    bpmMultiplier = juce::jlimit(0.25, 4.0, mult);
    tempoToggle.setMultiplier(bpmMultiplier, juce::dontSendNotification);
}

void EffectsInspector::setGroove(const GrooveParams& g, juce::NotificationType)
{
    groove = g;
    swing.setValue(g.swing, juce::dontSendNotification);
    pocket.setValue(g.pocket, juce::dontSendNotification);
    humanize.setValue(g.humanize, juce::dontSendNotification);
    dynamicsSl.setValue(g.dynamics, juce::dontSendNotification);
    lengthSl.setValue(g.length, juce::dontSendNotification);
    intensitySl.setValue(g.intensity, juce::dontSendNotification);
    quantizeStrengthSl.setValue(g.quantizeStrength, juce::dontSendNotification);
    articulationStrengthSl.setValue(g.articulationStrength, juce::dontSendNotification);
    delayAmountSl.setValue(g.delayAmount, juce::dontSendNotification);
    delayFeedbackSl.setValue(g.delayFeedback, juce::dontSendNotification);
    arpGateSl.setValue(g.arpGate, juce::dontSendNotification);
    strumSpeedSl.setValue(g.strumSpeed, juce::dontSendNotification);
    strumAmountSl.setValue(g.strumAmount, juce::dontSendNotification);
    velocityRangeSl.setRange(g.velocityRangeLo, g.velocityRangeHi, juce::dontSendNotification);
    variationsSl.setValue(g.variationIndex, juce::dontSendNotification);

    swingTimePopup.setIndex(juce::jlimit(0, 5, g.swingGridIndex), juce::dontSendNotification);
    quantizeTimePopup.setIndex(juce::jlimit(0, (int) QuantizeGrid::Count - 1, g.quantizeGridIndex),
                               juce::dontSendNotification);
    articulationPopup.setIndex(juce::jlimit(0, (int) Articulation::Count - 1, g.articulationIndex),
                               juce::dontSendNotification);
    sustainPopup.setIndex(juce::jlimit(0, (int) SustainPedalMode::Count - 1, g.sustainPedalMode),
                          juce::dontSendNotification);
    delayTimePopup.setIndex(juce::jlimit(0, (int) DelayTime::Count - 1, g.delayTimeIndex),
                            juce::dontSendNotification);
    arpModePopup.setIndex(juce::jlimit(0, (int) ArpMode::Count - 1, g.arpModeIndex),
                          juce::dontSendNotification);
    arpRatePopup.setIndex(juce::jlimit(0, (int) DelayTime::Count - 1, g.arpRateIndex),
                          juce::dontSendNotification);
    arpOctavesPopup.setIndex(juce::jlimit(0, 3, g.arpOctaves - 1), juce::dontSendNotification);
    strumDirPopup.setIndex(juce::jlimit(0, (int) StrumDirection::Count - 1, g.strumDirectionIndex),
                           juce::dontSendNotification);

    swing.valueText = grooveValueText(kKnobDefs[0], g.swing);
    pocket.valueText = grooveValueText(kKnobDefs[1], g.pocket);
    humanize.valueText = grooveValueText(kKnobDefs[2], g.humanize);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], g.dynamics);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], g.length);
    intensitySl.valueText = grooveValueText(kKnobDefs[5], g.intensity);
    quantizeStrengthSl.valueText = juce::String(g.quantizeStrength) + "%";
    articulationStrengthSl.valueText = juce::String(g.articulationStrength) + "%";
    delayAmountSl.valueText = juce::String(g.delayAmount) + "%";
    delayFeedbackSl.valueText = juce::String(g.delayFeedback) + "%";
    arpGateSl.valueText = juce::String(g.arpGate) + "%";
    strumSpeedSl.valueText = juce::String(g.strumSpeed) + "%";
    strumAmountSl.valueText = juce::String(g.strumAmount) + "%";
    velocityRangeSl.valueText = juce::String(g.velocityRangeLo) + "-" + juce::String(g.velocityRangeHi);
    variationsSl.valueText = g.variationIndex <= 0 ? "Off" : ("#" + juce::String(g.variationIndex));
    refreshComplexitySlider();
    refreshDirtySections();
    repaint();
}

void EffectsInspector::setEdit(const ClipEdit& e, juce::NotificationType)
{
    edit = e;
    octaveStepper.setValue(e.octave, juce::dontSendNotification);
    octaveRangePopup.setIndex(juce::jlimit(0, 3, e.octaveRange), juce::dontSendNotification);
    pitchRow.stepper.setValue(e.pitchShift, juce::dontSendNotification);
    pitchMinPopup.setIndex(juce::jlimit(0, 127, e.pitchMin), juce::dontSendNotification);
    pitchMaxPopup.setIndex(juce::jlimit(0, 127, e.pitchMax), juce::dontSendNotification);
    {
        int extIdx = 0;
        if (e.extendMult == 2) extIdx = 1;
        else if (e.extendMult == 4) extIdx = 2;
        else if (e.extendMult == 8) extIdx = 3;
        extendPopup.setIndex(extIdx, juce::dontSendNotification);
    }
    keyPopup.setIndex(e.root >= 0 ? e.root : 0, juce::dontSendNotification);
    modePopup.setIndex((int) e.mode, juce::dontSendNotification);
    fitSwitch.setToggleState(e.fitScale, juce::dontSendNotification);
    mapSwitch.setToggleState(e.mapToRoot, juce::dontSendNotification);
    refreshPitchAnnotation();
    refreshDirtySections();
}

void EffectsInspector::notifyGroove()
{
    swing.valueText = grooveValueText(kKnobDefs[0], groove.swing);
    pocket.valueText = grooveValueText(kKnobDefs[1], groove.pocket);
    humanize.valueText = grooveValueText(kKnobDefs[2], groove.humanize);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], groove.dynamics);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], groove.length);
    intensitySl.valueText = grooveValueText(kKnobDefs[5], groove.intensity);
    quantizeStrengthSl.valueText = juce::String(groove.quantizeStrength) + "%";
    articulationStrengthSl.valueText = juce::String(groove.articulationStrength) + "%";
    delayAmountSl.valueText = juce::String(groove.delayAmount) + "%";
    delayFeedbackSl.valueText = juce::String(groove.delayFeedback) + "%";
    arpGateSl.valueText = juce::String(groove.arpGate) + "%";
    strumSpeedSl.valueText = juce::String(groove.strumSpeed) + "%";
    strumAmountSl.valueText = juce::String(groove.strumAmount) + "%";
    velocityRangeSl.valueText = juce::String(groove.velocityRangeLo) + "-"
                              + juce::String(groove.velocityRangeHi);
    variationsSl.valueText = groove.variationIndex <= 0 ? "Off"
                           : ("#" + juce::String(groove.variationIndex));
    if (onGrooveChanged) onGrooveChanged(groove);
    refreshDirtySections();
}

void EffectsInspector::notifyEdit()
{
    if (onEditChanged) onEditChanged(edit);
    refreshDirtySections();
}

void EffectsInspector::layoutSections()
{
    const int w = juce::jmax(1, body.getWidth());
    int y = 0;
    for (auto* sec : { &playback, &timing, &performance, &pitchSec, &effectsSec })
    {
        const int h = sec->idealHeight();
        sec->setBounds(0, y, w, h);
        sec->resized();
        y += h;
    }
    body.setSize(w, juce::jmax(y, viewport.getMaximumVisibleHeight()));
}

void EffectsInspector::resized()
{
    auto r = getLocalBounds();
    const int headerH = metrics::listHeaderH();
    auto header = r.removeFromTop(headerH).reduced(10, 4);
    const int iconBtn = metrics::chromeIconButton();
    btnEffectsLock.ghost = true;
    btnEffectsLock.iconScale = 1.0f;
    btnReset.ghost = true;
    btnReset.iconScale = 1.0f;
    btnEffectsLock.setBounds(header.removeFromRight(iconBtn).withSizeKeepingCentre(iconBtn, iconBtn));
    header.removeFromRight(4);
    btnReset.setBounds(header.removeFromRight(iconBtn).withSizeKeepingCentre(iconBtn, iconBtn));

    viewport.setBounds(r);
    body.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()), body.getHeight());

    tempoToggle.setSize(tempoToggle.idealWidth(), kControlH);
    tempoRow.setControlWidth(tempoToggle.idealWidth());
    trimRow.setControlWidth(trimSwitch.idealWidth());
    extendPopup.setSize(extendPopup.idealWidth(), kControlH);
    extendRow.setControlWidth(extendPopup.idealWidth());

    swingTimePopup.setSize(swingTimePopup.idealWidth(), kControlH);
    swingTimeRow.setControlWidth(swingTimePopup.idealWidth());
    quantizeTimePopup.setSize(quantizeTimePopup.idealWidth(), kControlH);
    quantizeTimeRow.setControlWidth(quantizeTimePopup.idealWidth());
    articulationPopup.setSize(articulationPopup.idealWidth(), kControlH);
    articulationRow.setControlWidth(articulationPopup.idealWidth());
    sustainPopup.setSize(sustainPopup.idealWidth(), kControlH);
    sustainRow.setControlWidth(sustainPopup.idealWidth());

    delayTimePopup.setSize(delayTimePopup.idealWidth(), kControlH);
    delayTimeRow.setControlWidth(delayTimePopup.idealWidth());
    arpModePopup.setSize(arpModePopup.idealWidth(), kControlH);
    arpModeRow.setControlWidth(arpModePopup.idealWidth());
    arpRatePopup.setSize(arpRatePopup.idealWidth(), kControlH);
    arpRateRow.setControlWidth(arpRatePopup.idealWidth());
    arpOctavesPopup.setSize(arpOctavesPopup.idealWidth(), kControlH);
    arpOctavesRow.setControlWidth(arpOctavesPopup.idealWidth());
    strumDirPopup.setSize(strumDirPopup.idealWidth(), kControlH);
    strumDirRow.setControlWidth(strumDirPopup.idealWidth());

    octaveStepper.setSize(octaveStepper.idealWidth(), kControlH);
    octaveRow.setControlWidth(octaveStepper.idealWidth());
    octaveRangePopup.setSize(octaveRangePopup.idealWidth(), kControlH);
    octaveRangeRow.setControlWidth(octaveRangePopup.idealWidth());
    pitchRow.stepper.setSize(pitchRow.stepper.idealWidth(), kControlH);
    fitRow.setControlWidth(fitSwitch.idealWidth());
    mapRow.setControlWidth(mapSwitch.idealWidth());
    keyModeRow.setSize(keyPopup.idealWidth() + modePopup.idealWidth() + 7, kRowMinH);
    pitchRangeRow.setSize(pitchMinPopup.idealWidth() + pitchMaxPopup.idealWidth() + 7, kRowMinH);

    layoutSections();
}

void EffectsInspector::paint(juce::Graphics& g)
{
    const auto& t = inspectorTokens();
    g.fillAll(t.panelBg);
    g.setColour(t.divider);
    g.fillRect(getLocalBounds().removeFromLeft(1));

    auto header = getLocalBounds().removeFromTop(metrics::listHeaderH());
    g.setColour(t.divider);
    g.fillRect(header.getX(), header.getBottom() - 1, header.getWidth(), 1);

    g.setColour(t.headerText);
    g.setFont(inspectorFont(true));
    g.drawText("Effects", 14, 0, 120, header.getHeight(), juce::Justification::centredLeft);
}

void EffectsInspector::mouseDown(const juce::MouseEvent&)
{
    if (onActivated)
        onActivated();
}

} // namespace pflow
