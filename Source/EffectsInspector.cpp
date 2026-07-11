#include "EffectsInspector.h"

namespace pflow {

namespace {

int tempoStepFromMultiplier(double m)
{
    if (m < 0.75) return 0;
    if (m > 1.5) return 2;
    return 1;
}

double multiplierFromTempoStep(int step)
{
    switch (step)
    {
        case 0: return 0.5;
        case 2: return 2.0;
        default: return 1.0;
    }
}
} // namespace

// ── ParamSlider ──────────────────────────────────────────────────────────────

EffectsInspector::ParamSlider::ParamSlider(const juce::String& l, int mn, int mx, int d, bool bi)
    : label(l), minV(mn), maxV(mx), defV(d), value(d), bipolar(bi)
{
}

void EffectsInspector::ParamSlider::setValue(int v, juce::NotificationType notify)
{
    v = juce::jlimit(minV, maxV, v);
    if (v == value) return;
    value = v;
    repaint();
    if (notify != juce::dontSendNotification && onChange)
        onChange(value);
}

void EffectsInspector::ParamSlider::resized()
{
    auto r = getLocalBounds().toFloat().reduced(0.0f, 6.0f);
    track = r.withTrimmedTop(18.0f).withHeight(4.0f);
    track = track.withX(track.getX() + 2.0f).withWidth(track.getWidth() - 4.0f);
}

void EffectsInspector::ParamSlider::setFromX(float x)
{
    const float t = juce::jlimit(0.0f, 1.0f, (x - track.getX()) / juce::jmax(1.0f, track.getWidth()));
    setValue(minV + (int) std::lround(t * (float) (maxV - minV)));
}

void EffectsInspector::ParamSlider::mouseDown(const juce::MouseEvent& e) { setFromX(e.position.x); }
void EffectsInspector::ParamSlider::mouseDrag(const juce::MouseEvent& e) { setFromX(e.position.x); }
void EffectsInspector::ParamSlider::mouseDoubleClick(const juce::MouseEvent&) { setValue(defV); }

void EffectsInspector::ParamSlider::paint(juce::Graphics& g)
{
    auto top = getLocalBounds().removeFromTop(16);
    g.setFont(uiFont(11.0f, false));
    g.setColour(colours::text2());
    g.drawText(label, top, juce::Justification::centredLeft);

    g.setFont(monoFont(11.0f, true));
    g.setColour(colours::text());
    const auto text = valueText.isNotEmpty() ? valueText
                      : grooveValueText({ label.toRawUTF8(), label.toRawUTF8(), minV, maxV, defV }, value);
    g.drawText(text, top, juce::Justification::centredRight);

    g.setColour(colours::knobTrack());
    g.fillRoundedRectangle(track, 2.0f);

    const float t = (float) (value - minV) / (float) juce::jmax(1, maxV - minV);
    const float thumbX = track.getX() + t * track.getWidth();

    if (bipolar)
    {
        const float mid = track.getCentreX();
        g.setColour(colours::accent());
        g.fillRoundedRectangle(juce::Rectangle<float>::leftTopRightBottom(
                                   juce::jmin(mid, thumbX), track.getY(),
                                   juce::jmax(mid, thumbX), track.getBottom()), 2.0f);
        g.setColour(colours::lineStrong().withAlpha(0.5f));
        g.fillRect(mid - 0.5f, track.getY() - 2.0f, 1.0f, track.getHeight() + 4.0f);
    }
    else
    {
        g.setColour(colours::accent());
        g.fillRoundedRectangle(track.withWidth(juce::jmax(0.0f, thumbX - track.getX())), 2.0f);
    }

    g.setColour(juce::Colours::white);
    g.fillEllipse(thumbX - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.drawEllipse(thumbX - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f, 0.8f);
}

// ── DiscreteSlider ───────────────────────────────────────────────────────────

EffectsInspector::DiscreteSlider::DiscreteSlider(const juce::String& l, int steps, int def)
    : label(l), numSteps(steps), defStep(def), step(def)
{
}

void EffectsInspector::DiscreteSlider::setLabels(const juce::StringArray& ls)
{
    labels = ls;
    repaint();
}

void EffectsInspector::DiscreteSlider::setStep(int s, juce::NotificationType notify)
{
    s = juce::jlimit(0, numSteps - 1, s);
    if (s == step) return;
    step = s;
    repaint();
    if (notify != juce::dontSendNotification && onChange)
        onChange(step);
}

void EffectsInspector::DiscreteSlider::resized()
{
    auto r = getLocalBounds().toFloat().reduced(0.0f, 6.0f);
    track = r.withTrimmedTop(18.0f).withHeight(4.0f);
    track = track.withX(track.getX() + 2.0f).withWidth(track.getWidth() - 4.0f);
}

void EffectsInspector::DiscreteSlider::setFromX(float x)
{
    const float t = juce::jlimit(0.0f, 1.0f, (x - track.getX()) / juce::jmax(1.0f, track.getWidth()));
    setStep((int) std::lround(t * (float) (numSteps - 1)));
}

void EffectsInspector::DiscreteSlider::mouseDown(const juce::MouseEvent& e) { setFromX(e.position.x); }
void EffectsInspector::DiscreteSlider::mouseDrag(const juce::MouseEvent& e) { setFromX(e.position.x); }
void EffectsInspector::DiscreteSlider::mouseDoubleClick(const juce::MouseEvent&) { setStep(defStep); }

void EffectsInspector::DiscreteSlider::paint(juce::Graphics& g)
{
    auto top = getLocalBounds().removeFromTop(16);
    g.setFont(uiFont(11.0f, false));
    g.setColour(colours::text2());
    g.drawText(label, top, juce::Justification::centredLeft);

    const juce::String val = labels.size() > step ? labels[step] : juce::String(step);
    g.setFont(monoFont(11.0f, true));
    g.setColour(colours::text());
    g.drawText(val, top, juce::Justification::centredRight);

    g.setColour(colours::knobTrack());
    g.fillRoundedRectangle(track, 2.0f);

    const float t = numSteps > 1 ? (float) step / (float) (numSteps - 1) : 0.0f;
    const float thumbX = track.getX() + t * track.getWidth();
    const float mid = track.getCentreX();

    if (defStep >= 0 && defStep < numSteps)
    {
        g.setColour(colours::lineStrong().withAlpha(0.45f));
        g.fillRect(mid - 0.5f, track.getY() - 2.0f, 1.0f, track.getHeight() + 4.0f);
    }

    g.setColour(colours::accent());
    if (thumbX < mid)
        g.fillRoundedRectangle(juce::Rectangle<float>(thumbX, track.getY(), mid - thumbX, track.getHeight()), 2.0f);
    else if (thumbX > mid)
        g.fillRoundedRectangle(juce::Rectangle<float>(mid, track.getY(), thumbX - mid, track.getHeight()), 2.0f);

    g.setColour(juce::Colours::white);
    g.fillEllipse(thumbX - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.drawEllipse(thumbX - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f, 0.8f);
}

// ── LabeledRow ───────────────────────────────────────────────────────────────

EffectsInspector::LabeledRow::LabeledRow(const juce::String& cap, juce::Component& c)
    : caption(cap), control(c)
{
    addAndMakeVisible(control);
}

void EffectsInspector::LabeledRow::resized()
{
    auto r = getLocalBounds();
    control.setBounds(r.withTrimmedTop(16).reduced(0, 2));
}

void EffectsInspector::LabeledRow::paint(juce::Graphics& g)
{
    g.setFont(uiFont(11.0f, false));
    g.setColour(colours::text2());
    g.drawText(caption, getLocalBounds().removeFromTop(14), juce::Justification::centredLeft);
}

// ── Section ──────────────────────────────────────────────────────────────────

EffectsInspector::Section::Section(const juce::String& t) : title(t) {}

void EffectsInspector::Section::setOpen(bool o)
{
    if (open == o) return;
    open = o;
    for (auto* c : rows)
        c->setVisible(open);
    if (onToggle) onToggle();
    repaint();
}

int EffectsInspector::Section::idealHeight() const
{
    return open ? kHeaderH + (int) rows.size() * kRowH : kHeaderH;
}

void EffectsInspector::Section::resized()
{
    auto r = getLocalBounds().withTrimmedTop(kHeaderH).reduced(kPadH, 0);
    for (auto* c : rows)
    {
        if (!open) { c->setBounds({}); continue; }
        c->setBounds(r.removeFromTop(kRowH));
    }
}

void EffectsInspector::Section::mouseDown(const juce::MouseEvent& e)
{
    if (e.y <= kHeaderH)
        setOpen(!open);
}

void EffectsInspector::Section::paint(juce::Graphics& g)
{
    g.setColour(colours::text());
    g.setFont(uiFont(11.0f, true));
    g.drawText(title, kPadH + 10, 6, getWidth() - kPadH * 2, 20, juce::Justification::centredLeft);

    juce::Path caret;
    const float cx = (float) kPadH + 2.0f, cy = 16.0f;
    if (open)
        caret.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 4.0f);
    else
        caret.addTriangle(cx - 2.0f, cy - 4.0f, cx + 4.0f, cy, cx - 2.0f, cy + 4.0f);
    g.setColour(colours::text3());
    g.fillPath(caret);
}

// ── EffectsInspector ─────────────────────────────────────────────────────────

EffectsInspector::EffectsInspector()
{
    viewport.setViewedComponent(&body, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    btnLock.setComponentID("btnEffectsLock");
    btnLock.onClick = [this]
    {
        effectsLocked = !effectsLocked;
        setEffectsLocked(effectsLocked);
        if (onEffectsLockToggled) onEffectsLockToggled(effectsLocked);
    };
    addAndMakeVisible(btnLock);

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

    btnPitchLock.setComponentID("btnLock");
    btnPitchLock.onClick = [this]
    {
        pitchLocked = !pitchLocked;
        setPitchLocked(pitchLocked);
        if (onPitchLockToggled) onPitchLockToggled(pitchLocked);
    };
    addAndMakeVisible(btnPitchLock);

    auto wireSlider = [this](ParamSlider& s, int knobIdx)
    {
        s.onChange = [this, knobIdx](int v)
        {
            groove.set(knobIdx, v);
            notifyGroove();
        };
        body.addAndMakeVisible(s);
    };
    wireSlider(swing, 0);
    wireSlider(pocket, 1);
    wireSlider(humanize, 2);
    wireSlider(dynamicsSl, 3);
    wireSlider(lengthSl, 4);
    wireSlider(intensity, 5);

    swingGrid.setLabels(juce::StringArray({ "1/16", "1/8", "1/4", "1/2", "1", "2" }));
    swingGrid.onChange = [this](int idx)
    {
        groove.swingGridIndex = idx;
        groove.swingBase = idx == 0 ? SwingBase::Sixteenth : SwingBase::Eighth;
        notifyGroove();
    };
    body.addAndMakeVisible(swingGrid);

    tempo.setLabels(juce::StringArray({ "/2", "1×", "×2" }));
    tempo.onChange = [this](int idx)
    {
        bpmMultiplier = multiplierFromTempoStep(idx);
        if (onBpmMultiplierChanged) onBpmMultiplierChanged(bpmMultiplier);
    };
    body.addAndMakeVisible(tempo);

    timing.rows = { &swing, &swingGrid, &tempo, &pocket, &humanize };
    dynamics.rows = { &dynamicsSl, &intensity };
    lengthSec.rows = { &lengthSl };

    for (int o = 3; o >= -3; --o)
        octavePicker.addItem((o > 0 ? "+" : "") + juce::String(o), o + 4);
    octavePicker.onChange = [this]
    {
        edit.octave = octavePicker.getSelectedId() - 4;
        notifyEdit();
    };
    body.addAndMakeVisible(octaveRow);

    rootPicker.addItem("—", 1);
    for (int i = 0; i < 12; ++i)
        rootPicker.addItem(kNoteNames[(size_t) i], i + 2);
    rootPicker.onChange = [this]
    {
        const int id = rootPicker.getSelectedId();
        edit.root = id <= 1 ? -1 : id - 2;
        notifyEdit();
    };
    body.addAndMakeVisible(keyRow);

    for (int i = 0; i < kNumModes; ++i)
        modePicker.addItem(modeName((Mode) i), i + 1);
    modePicker.onChange = [this]
    {
        edit.mode = (Mode) juce::jmax(0, modePicker.getSelectedId() - 1);
        notifyEdit();
    };
    body.addAndMakeVisible(modeRow);

    fitSwitch.onClick = [this]
    {
        edit.fitScale = fitSwitch.getToggleState();
        notifyEdit();
    };
    body.addAndMakeVisible(fitRow);

    mapSwitch.onClick = [this]
    {
        edit.mapToRoot = mapSwitch.getToggleState();
        notifyEdit();
    };
    body.addAndMakeVisible(mapRow);

    pitch.rows = { &octaveRow, &keyRow, &modeRow, &fitRow, &mapRow };

    for (auto* sec : { &timing, &dynamics, &lengthSec, &pitch })
    {
        sec->setOpen(false);
        sec->onToggle = [this] { layoutSections(); };
        body.addAndMakeVisible(*sec);
    }

    setGroove(GrooveParams{}, juce::dontSendNotification);
    setEdit(ClipEdit{}, juce::dontSendNotification);
    setBpmMultiplier(1.0, juce::dontSendNotification);
}

void EffectsInspector::setEffectsLocked(bool locked)
{
    effectsLocked = locked;
    btnLock.icon = locked ? icons::lockClosed : icons::lockOpen;
    btnLock.active = locked;
    btnLock.repaint();
    repaint();
}

void EffectsInspector::setPitchLocked(bool locked)
{
    pitchLocked = locked;
    btnPitchLock.icon = locked ? icons::lockClosed : icons::lockOpen;
    btnPitchLock.active = locked;
    btnPitchLock.repaint();
}

void EffectsInspector::setHasClip(bool has)
{
    hasClip = has;
    repaint();
}

void EffectsInspector::setBpmMultiplier(double mult, juce::NotificationType)
{
    bpmMultiplier = juce::jlimit(0.25, 4.0, mult);
    tempo.setStep(tempoStepFromMultiplier(bpmMultiplier), juce::dontSendNotification);
    repaint();
}

void EffectsInspector::setGroove(const GrooveParams& g, juce::NotificationType)
{
    groove = g;
    swing.setValue(g.swing, juce::dontSendNotification);
    pocket.setValue(g.pocket, juce::dontSendNotification);
    humanize.setValue(g.humanize, juce::dontSendNotification);
    dynamicsSl.setValue(g.dynamics, juce::dontSendNotification);
    lengthSl.setValue(g.length, juce::dontSendNotification);
    intensity.setValue(g.intensity, juce::dontSendNotification);
    swingGrid.setStep(juce::jlimit(0, 5, g.swingGridIndex), juce::dontSendNotification);
    swing.valueText = grooveValueText(kKnobDefs[0], g.swing);
    pocket.valueText = grooveValueText(kKnobDefs[1], g.pocket);
    humanize.valueText = grooveValueText(kKnobDefs[2], g.humanize);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], g.dynamics);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], g.length);
    intensity.valueText = grooveValueText(kKnobDefs[5], g.intensity);
    repaint();
}

void EffectsInspector::setEdit(const ClipEdit& e, juce::NotificationType)
{
    edit = e;
    octavePicker.setSelectedId(e.octave + 4, juce::dontSendNotification);
    rootPicker.setSelectedId(e.root >= 0 ? e.root + 2 : 1, juce::dontSendNotification);
    modePicker.setSelectedId((int) e.mode + 1, juce::dontSendNotification);
    fitSwitch.setToggleState(e.fitScale, juce::dontSendNotification);
    mapSwitch.setToggleState(e.mapToRoot, juce::dontSendNotification);
}

void EffectsInspector::notifyGroove()
{
    swing.valueText = grooveValueText(kKnobDefs[0], groove.swing);
    pocket.valueText = grooveValueText(kKnobDefs[1], groove.pocket);
    humanize.valueText = grooveValueText(kKnobDefs[2], groove.humanize);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], groove.dynamics);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], groove.length);
    intensity.valueText = grooveValueText(kKnobDefs[5], groove.intensity);
    if (onGrooveChanged) onGrooveChanged(groove);
}

void EffectsInspector::notifyEdit()
{
    if (onEditChanged) onEditChanged(edit);
}

void EffectsInspector::layoutSections()
{
    const int w = juce::jmax(1, body.getWidth() - kBodyPadH * 2);
    int y = 0;
    for (auto* sec : { &timing, &dynamics, &lengthSec, &pitch })
    {
        const int h = sec->idealHeight();
        sec->setBounds(kBodyPadH, y, w, h);
        sec->resized();
        y += h + 6;
    }
    body.setSize(juce::jmax(1, (int) viewport.getMaximumVisibleWidth()),
                 juce::jmax(y, viewport.getMaximumVisibleHeight()));
}

void EffectsInspector::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop(headerH).reduced(kBodyPadH, 4);
    btnPitchLock.setBounds(header.removeFromRight(24).withSizeKeepingCentre(22, 22));
    header.removeFromRight(4);
    btnReset.setBounds(header.removeFromRight(24).withSizeKeepingCentre(22, 22));
    header.removeFromRight(4);
    btnLock.setBounds(header.removeFromRight(24).withSizeKeepingCentre(22, 22));

    viewport.setBounds(r);
    body.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()), body.getHeight());
    layoutSections();
}

void EffectsInspector::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromLeft(1));

    g.setColour(colours::text());
    g.setFont(uiFont(12.5f, true));
    g.drawText("Effects", kBodyPadH, 8, 100, 20, juce::Justification::centredLeft);
}

} // namespace pflow
