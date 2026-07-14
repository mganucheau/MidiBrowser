#include "EffectsInspector.h"

namespace pflow {

using namespace fx;

EffectsInspector::EffectsInspector()
    : tempoRow("Tempo", tempoToggle)
    , extendRow("Extend", extendPopup)
    , swingTimeRow("Swing Time", swingTimePopup)
    , quantizeTimeRow("Quantize", quantizeTimePopup)
    , quantizeStrengthSl("Strength", 0, 100, 0, false)
    , swing("Swing", 0, 100, 0, false)
    , pocket("Pocket", -100, 100, 0, true)
    , humanize("Humanize", 0, 100, 0, false)
    , lengthSl("Length", 25, 200, 100, false)
    , dynamicsSl("Dynamics", -100, 100, 0, true)
    , intensitySl("Intensity", 0, 200, 100, false)
    , articulationRow("Articulation", articulationPopup)
    , articulationStrengthSl("Strength", 0, 100, 0, false)
    , velocityRangeSl("Velocity", 1, 127, 1, 127)
    , sustainRow("Sustain Pedal", sustainPopup)
    , complexitySl("Complexity", 0, 100, 50, false)
    , variationsSl("Variations", 0, 16, 0, false)
    , delayTimeRow("Delay", delayTimePopup)
    , delayAmountSl("Delay Amount", 0, 100, 0, false)
    , delayFeedbackSl("Delay Feedback", 0, 100, 40, false)
    , octaveRow("Octave", octaveStepper)
    , octaveRangeRow("Octave Range", octaveRangePopup)
    , pitchRangeRow(pitchMinPopup, pitchMaxPopup)
    , keyRow("Key", keyPopup)
    , modeRow("Mode", modePopup)
    , trimRow("Trim empty measures", trimSwitch)
    , fitRow("Fit to Scale", fitSwitch)
    , mapRow("Map to Root", mapSwitch)
{
    addMouseListener(this, true);

    viewport.setViewedComponent(&body, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    btnReset.setComponentID("btnEffectsReset");
    btnReset.setWantsKeyboardFocus(false);
    btnReset.ghost = true;
    btnReset.iconScale = 1.0f;
    btnReset.setTooltip("Reset unlocked Toolkit sections");
    btnReset.onClick = [this] { resetUnlockedSections(); };
    addAndMakeVisible(btnReset);

    btnEffectsLock.setComponentID("btnLock");
    btnEffectsLock.setWantsKeyboardFocus(false);
    btnEffectsLock.ghost = true;
    btnEffectsLock.iconScale = 1.0f;
    btnEffectsLock.setTooltip("Lock all Toolkit parameters while browsing");
    btnEffectsLock.onClick = [this]
    {
        const bool allOn = (sectionLocks & toolkitLock::All) == toolkitLock::All;
        setAllSectionsLocked(!allOn);
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
    extendPopup.setTooltip("Tile the clip longer so delays can evolve past the file length");
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
    pitchSec.addRow(&keyRow, kRowMinH, [this] {
        return edit.root >= 0 && (edit.fitScale || edit.mapToRoot);
    });
    pitchSec.addRow(&modeRow, kRowMinH, [this] {
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

    playback.onToggle = timing.onToggle = performance.onToggle = pitchSec.onToggle
        = effectsSec.onToggle = [this] { layoutSections(); };

    playback.onReset = [this] { resetPlaybackSection(); };
    playback.onLockToggle = [this] { toggleSectionLock(toolkitLock::Playback); };

    timing.onReset = [this] { resetTimingSection(); };
    timing.onLockToggle = [this] { toggleSectionLock(toolkitLock::Timing); };

    performance.onReset = [this] { resetPerformanceSection(); };
    performance.onLockToggle = [this] { toggleSectionLock(toolkitLock::Performance); };

    pitchSec.onReset = [this]
    {
        resetPitchEditFields();
        setEdit(edit, juce::dontSendNotification);
        notifyEdit();
    };
    pitchSec.onLockToggle = [this] { toggleSectionLock(toolkitLock::Pitch); };

    effectsSec.onReset = [this] { resetEffectsSection(); };
    effectsSec.onLockToggle = [this] { toggleSectionLock(toolkitLock::Effects); };

    for (auto* sec : { &playback, &timing, &performance, &pitchSec, &effectsSec })
        body.addAndMakeVisible(*sec);

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

void EffectsInspector::resetPlaybackSection()
{
    setBpmMultiplier(1.0, juce::dontSendNotification);
    edit.extendMult = 1;
    setEdit(edit, juce::dontSendNotification);
    if (onBpmMultiplierChanged) onBpmMultiplierChanged(bpmMultiplier);
    notifyEdit();
}

void EffectsInspector::resetTimingSection()
{
    groove.swing = 0;
    groove.pocket = 0;
    groove.humanize = 0;
    groove.length = 100;
    groove.swingGridIndex = 1;
    groove.quantizeStrength = 0;
    groove.quantizeGridIndex = (int) QuantizeGrid::Eighth;
    setGroove(groove, juce::dontSendNotification);
    notifyGroove();
}

void EffectsInspector::resetPerformanceSection()
{
    groove.dynamics = 0;
    groove.intensity = 100;
    groove.articulationIndex = (int) Articulation::Off;
    groove.articulationStrength = 0;
    groove.velocityRangeLo = 1;
    groove.velocityRangeHi = 127;
    groove.sustainPedalMode = (int) SustainPedalMode::Off;
    groove.applyArticulationToKnobs();
    setGroove(groove, juce::dontSendNotification);
    syncArticulationKnobsToUi();
    notifyGroove();
}

void EffectsInspector::resetEffectsSection()
{
    groove.complexityTarget = -1;
    groove.variationIndex = 0;
    groove.delayAmount = 0;
    groove.delayFeedback = 40;
    groove.delayTimeIndex = (int) DelayTime::Eighth;
    setGroove(groove, juce::dontSendNotification);
    refreshComplexitySlider();
    notifyGroove();
}

void EffectsInspector::refreshHeaderLockButton()
{
    const bool allOn = (sectionLocks & toolkitLock::All) == toolkitLock::All;
    btnEffectsLock.icon = allOn ? icons::lockClosed : icons::lockOpen;
    btnEffectsLock.active = allOn;
    btnEffectsLock.setTooltip(allOn ? "Unlock all Toolkit parameters"
                                    : "Lock all Toolkit parameters while browsing");
    btnEffectsLock.repaint();
}

void EffectsInspector::setSectionLocks(uint32_t locks)
{
    sectionLocks = locks & toolkitLock::All;
    playback.setLocked((sectionLocks & toolkitLock::Playback) != 0);
    timing.setLocked((sectionLocks & toolkitLock::Timing) != 0);
    performance.setLocked((sectionLocks & toolkitLock::Performance) != 0);
    pitchSec.setLocked((sectionLocks & toolkitLock::Pitch) != 0);
    effectsSec.setLocked((sectionLocks & toolkitLock::Effects) != 0);
    refreshHeaderLockButton();
    repaint();
}

void EffectsInspector::toggleSectionLock(uint32_t bit)
{
    bit &= toolkitLock::All;
    if (bit == 0) return;
    if ((sectionLocks & bit) != 0)
        sectionLocks &= ~bit;
    else
        sectionLocks |= bit;
    setSectionLocks(sectionLocks);
    if (onSectionLocksChanged) onSectionLocksChanged(sectionLocks);
}

void EffectsInspector::setAllSectionsLocked(bool locked)
{
    sectionLocks = locked ? toolkitLock::All : 0;
    setSectionLocks(sectionLocks);
    if (onSectionLocksChanged) onSectionLocksChanged(sectionLocks);
}

void EffectsInspector::resetUnlockedSections()
{
    if ((sectionLocks & toolkitLock::Playback) == 0)
        resetPlaybackSection();
    if ((sectionLocks & toolkitLock::Timing) == 0)
        resetTimingSection();
    if ((sectionLocks & toolkitLock::Performance) == 0)
        resetPerformanceSection();
    if ((sectionLocks & toolkitLock::Pitch) == 0)
    {
        resetPitchEditFields();
        setEdit(edit, juce::dontSendNotification);
        notifyEdit();
    }
    if ((sectionLocks & toolkitLock::Effects) == 0)
        resetEffectsSection();
    if (onResetUnlocked) onResetUnlocked();
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
    refreshDirtySections();
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
    const int headerH = metrics::paneHeaderH();
    auto header = r.removeFromTop(headerH).reduced(10, 4);
    const int iconBtn = metrics::chromeIconButton();
    btnEffectsLock.iconScale = 0.9f;
    btnReset.iconScale = 0.9f;
    btnEffectsLock.setBounds(header.removeFromRight(iconBtn).withSizeKeepingCentre(iconBtn, iconBtn));
    header.removeFromRight(2);
    btnReset.setBounds(header.removeFromRight(iconBtn).withSizeKeepingCentre(iconBtn, iconBtn));

    viewport.setBounds(r);
    body.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()), body.getHeight());

    auto fitSelect = [](fx::FlatPopup& p, fx::InlineRow& row)
    {
        p.setSize(kSelectW, kControlH);
        row.setControlWidth(kSelectW);
    };
    {
        const int tempoW = juce::jmax(kSelectW, tempoToggle.idealWidth());
        tempoToggle.setSize(tempoW, kControlH);
        tempoRow.setControlWidth(tempoW);
    }
    trimRow.setControlWidth(trimSwitch.idealWidth());
    fitSelect(extendPopup, extendRow);
    fitSelect(swingTimePopup, swingTimeRow);
    fitSelect(quantizeTimePopup, quantizeTimeRow);
    fitSelect(articulationPopup, articulationRow);
    fitSelect(sustainPopup, sustainRow);
    fitSelect(delayTimePopup, delayTimeRow);
    fitSelect(octaveRangePopup, octaveRangeRow);
    fitSelect(keyPopup, keyRow);
    fitSelect(modePopup, modeRow);

    octaveStepper.setSize(kSelectW, kControlH);
    octaveRow.setControlWidth(kSelectW);
    pitchRow.stepper.setSize(pitchRow.stepper.idealWidth(), kControlH);
    fitRow.setControlWidth(fitSwitch.idealWidth());
    mapRow.setControlWidth(mapSwitch.idealWidth());
    pitchMinPopup.setSize(72, kControlH);
    pitchMaxPopup.setSize(72, kControlH);

    layoutSections();
}

void EffectsInspector::paint(juce::Graphics& g)
{
    const auto& t = inspectorTokens();
    g.fillAll(t.panelBg);
    g.setColour(t.divider);
    g.fillRect(getLocalBounds().removeFromLeft(1));

    auto header = getLocalBounds().removeFromTop(metrics::paneHeaderH());
    g.setColour(t.divider);
    g.fillRect(header.getX(), header.getBottom() - 1, header.getWidth(), 1);

    // Caps B2 cap bar — 11pt tracked, same language as LIBRARY.
    auto cap = uiFontFixed(11.0f, true);
    cap.setExtraKerningFactor(0.06f);
    g.setColour(t.valueText);
    g.setFont(cap);
    g.drawText("TOOLKIT", 14, 0, 120, header.getHeight(), juce::Justification::centredLeft);
}

void EffectsInspector::mouseDown(const juce::MouseEvent&)
{
    if (onActivated)
        onActivated();
}

} // namespace pflow
