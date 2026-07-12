#include "EffectsInspector.h"

namespace pflow {

using namespace fx;

EffectsInspector::EffectsInspector()
    : tempoRow("Tempo", tempoToggle)
    , swing("Swing", 0, 100, 0, false)
    , swingStyleRow("Swing Style", swingStylePopup)
    , pocket("Pocket", -100, 100, 0, true)
    , humanize("Humanize", 0, 100, 0, false)
    , dynamicsSl("Dynamics", -100, 100, 0, true)
    , intensitySl("Intensity", 0, 200, 100, false)
    , lengthSl("Length", 25, 200, 100, false)
    , octaveRow("Octave", octaveStepper)
    , keyModeRow(keyPopup, modePopup)
    , trimRow("Trim", btnTrim)
    , fitRow("Fit to Scale", fitSwitch)
    , mapRow("Map to Root", mapSwitch)
{
    viewport.setViewedComponent(&body, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    btnReset.setComponentID("btnEffectsReset");
    btnReset.onClick = [this]
    {
        groove = GrooveParams();
        setGroove(groove, juce::dontSendNotification);
        setBpmMultiplier(1.0, juce::dontSendNotification);
        if (onResetGroove) onResetGroove();
        notifyGroove();
        if (onBpmMultiplierChanged) onBpmMultiplierChanged(bpmMultiplier);
    };
    addAndMakeVisible(btnReset);

    btnEffectsLock.setComponentID("btnLock");
    btnEffectsLock.onClick = [this]
    {
        effectsLocked = !effectsLocked;
        setEffectsLocked(effectsLocked);
        if (onEffectsLockToggled) onEffectsLockToggled(effectsLocked);
    };
    addAndMakeVisible(btnEffectsLock);

    btnTrim.setComponentID("btnTrim");
    btnTrim.onClick = [this] { if (onTrimClicked) onTrimClicked(); };

    tempoToggle.onChange = [this](double m)
    {
        bpmMultiplier = m;
        if (onBpmMultiplierChanged) onBpmMultiplierChanged(bpmMultiplier);
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

    swingStylePopup.setItems({ "1/16", "1/8", "1/4", "1/2", "1", "2" }, 1);
    swingStylePopup.onChange = [this](int idx)
    {
        groove.swingGridIndex = idx;
        groove.swingBase = idx == 0 ? SwingBase::Sixteenth : SwingBase::Eighth;
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

    playback.addRow(&tempoRow);
    playback.addRow(&trimRow);

    timing.addRow(&swing, kSliderRowH);
    timing.addRow(&swingStyleRow);
    timing.addRow(&pocket, kSliderRowH);
    timing.addRow(&humanize, kSliderRowH);

    performance.addRow(&dynamicsSl, kSliderRowH);
    performance.addRow(&intensitySl, kSliderRowH);
    performance.addRow(&lengthSl, kSliderRowH);

    pitchSec.addRow(&octaveRow);
    pitchSec.addRow(&pitchRow);
    pitchSec.addRow(&keyModeRow);
    pitchSec.addRow(&fitRow);
    pitchSec.addRow(&mapRow);

    for (auto* sec : { &playback, &timing, &performance, &pitchSec })
    {
        sec->onToggle = [this] { layoutSections(); };
        body.addAndMakeVisible(*sec);
    }

    setGroove(GrooveParams{}, juce::dontSendNotification);
    setEdit(ClipEdit{}, juce::dontSendNotification);
    setBpmMultiplier(1.0, juce::dontSendNotification);
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

void EffectsInspector::setEffectsLocked(bool locked)
{
    effectsLocked = locked;
    btnEffectsLock.icon = locked ? icons::lockClosed : icons::lockOpen;
    btnEffectsLock.active = locked;
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

void EffectsInspector::setTrimState(bool active, bool enabled, const juce::String& label)
{
    btnTrim.active = active;
    btnTrim.setEnabled(enabled);
    btnTrim.label = label;
    btnTrim.repaint();
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
    swingStylePopup.setIndex(juce::jlimit(0, 5, g.swingGridIndex), juce::dontSendNotification);
    swing.valueText = grooveValueText(kKnobDefs[0], g.swing);
    pocket.valueText = grooveValueText(kKnobDefs[1], g.pocket);
    humanize.valueText = grooveValueText(kKnobDefs[2], g.humanize);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], g.dynamics);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], g.length);
    intensitySl.valueText = grooveValueText(kKnobDefs[5], g.intensity);
    repaint();
}

void EffectsInspector::setEdit(const ClipEdit& e, juce::NotificationType)
{
    edit = e;
    octaveStepper.setValue(e.octave, juce::dontSendNotification);
    pitchRow.stepper.setValue(e.pitchShift, juce::dontSendNotification);
    keyPopup.setIndex(e.root >= 0 ? e.root : 0, juce::dontSendNotification);
    modePopup.setIndex((int) e.mode, juce::dontSendNotification);
    fitSwitch.setToggleState(e.fitScale, juce::dontSendNotification);
    mapSwitch.setToggleState(e.mapToRoot, juce::dontSendNotification);
    refreshPitchAnnotation();
}

void EffectsInspector::notifyGroove()
{
    swing.valueText = grooveValueText(kKnobDefs[0], groove.swing);
    pocket.valueText = grooveValueText(kKnobDefs[1], groove.pocket);
    humanize.valueText = grooveValueText(kKnobDefs[2], groove.humanize);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], groove.dynamics);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], groove.length);
    intensitySl.valueText = grooveValueText(kKnobDefs[5], groove.intensity);
    if (onGrooveChanged) onGrooveChanged(groove);
}

void EffectsInspector::notifyEdit()
{
    if (onEditChanged) onEditChanged(edit);
}

void EffectsInspector::layoutSections()
{
    const int w = juce::jmax(1, body.getWidth());
    int y = 0;
    for (auto* sec : { &playback, &timing, &performance, &pitchSec })
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
    auto header = r.removeFromTop(kHeaderBarH).reduced(10, 4);
    btnEffectsLock.setBounds(header.removeFromRight(22).withSizeKeepingCentre(20, 20));
    header.removeFromRight(4);
    btnReset.setBounds(header.removeFromRight(22).withSizeKeepingCentre(20, 20));

    viewport.setBounds(r);
    body.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()), body.getHeight());

    tempoToggle.setSize(tempoToggle.idealWidth(), kRowMinH);
    tempoRow.setControlWidth(tempoToggle.idealWidth());
    trimRow.setControlWidth(btnTrim.idealWidth());
    swingStylePopup.setSize(swingStylePopup.idealWidth(), kRowMinH);
    swingStyleRow.setControlWidth(swingStylePopup.idealWidth());
    octaveStepper.setSize(octaveStepper.idealWidth(), kRowMinH);
    octaveRow.setControlWidth(octaveStepper.idealWidth());
    pitchRow.stepper.setSize(pitchRow.stepper.idealWidth(), kRowMinH);
    fitRow.setControlWidth(fitSwitch.idealWidth());
    mapRow.setControlWidth(mapSwitch.idealWidth());
    keyModeRow.setSize(keyPopup.idealWidth() + modePopup.idealWidth() + 7, kRowMinH);

    layoutSections();
}

void EffectsInspector::paint(juce::Graphics& g)
{
    const auto& t = inspectorTokens();
    g.fillAll(t.panelBg);
    g.setColour(t.divider);
    g.fillRect(getLocalBounds().removeFromLeft(1));

    auto header = getLocalBounds().removeFromTop(kHeaderBarH);
    g.setColour(t.divider);
    g.fillRect(header.getX(), header.getBottom() - 1, header.getWidth(), 1);

    g.setColour(t.headerText);
    g.setFont(inspectorFont(true));
    g.drawText("Effects", 14, 8, 120, 18, juce::Justification::centredLeft);
}

} // namespace pflow
