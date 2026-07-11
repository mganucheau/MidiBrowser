#include "EffectsInspector.h"

namespace pflow {

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
    track = getLocalBounds().toFloat().reduced(0, 14.0f).withTrimmedTop(4.0f);
    track = track.withHeight(4.0f).withY(getHeight() - 12.0f);
}

void EffectsInspector::ParamSlider::setFromX(float x)
{
    const float t = juce::jlimit(0.0f, 1.0f, (x - track.getX()) / juce::jmax(1.0f, track.getWidth()));
    setValue(minV + (int) std::lround(t * (float) (maxV - minV)));
}

void EffectsInspector::ParamSlider::mouseDown(const juce::MouseEvent& e) { setFromX(e.position.x); }
void EffectsInspector::ParamSlider::mouseDrag(const juce::MouseEvent& e) { setFromX(e.position.x); }
void EffectsInspector::ParamSlider::mouseDoubleClick(const juce::MouseEvent&)
{
    setValue(defV);
}

void EffectsInspector::ParamSlider::paint(juce::Graphics& g)
{
    g.setFont(uiFont(11.0f, false));
    g.setColour(colours::text2());
    g.drawText(label, getLocalBounds().removeFromTop(14).reduced(0, 0),
               juce::Justification::centredLeft);

    g.setFont(monoFont(11.0f, true));
    g.setColour(colours::text());
    const auto text = valueText.isNotEmpty() ? valueText
                      : grooveValueText({ label.toRawUTF8(), label.toRawUTF8(), minV, maxV, defV }, value);
    g.drawText(text, getLocalBounds().removeFromTop(14), juce::Justification::centredRight);

    // Track
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

    // Thumb
    g.setColour(juce::Colours::white);
    g.fillEllipse(thumbX - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.drawEllipse(thumbX - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f, 0.8f);
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
    return open ? 28 + (int) rows.size() * 36 : 28;
}

void EffectsInspector::Section::resized()
{
    auto r = getLocalBounds().withTrimmedTop(28);
    for (auto* c : rows)
    {
        if (!open) { c->setBounds({}); continue; }
        c->setBounds(r.removeFromTop(34).reduced(8, 2));
        r.removeFromTop(2);
    }
}

void EffectsInspector::Section::mouseDown(const juce::MouseEvent& e)
{
    if (e.y <= 28)
        setOpen(!open);
}

void EffectsInspector::Section::paint(juce::Graphics& g)
{
    g.setColour(colours::text());
    g.setFont(uiFont(11.0f, true));
    g.drawText(title, 20, 4, getWidth() - 28, 20, juce::Justification::centredLeft);

    // Disclosure caret
    juce::Path caret;
    const float cx = 10.0f, cy = 14.0f;
    if (open)
    {
        caret.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 4.0f);
    }
    else
    {
        caret.addTriangle(cx - 2.0f, cy - 4.0f, cx + 4.0f, cy, cx - 2.0f, cy + 4.0f);
    }
    g.setColour(colours::text3());
    g.fillPath(caret);
}

// ── Swing base row (1/8 · 1/16) ─────────────────────────────────────────────

EffectsInspector::SwingBaseRow::SwingBaseRow(ChipBtn& eighth, ChipBtn& sixteenth)
    : eight(eighth), sixteenth(sixteenth)
{
    addAndMakeVisible(eight);
    addAndMakeVisible(sixteenth);
}

void EffectsInspector::SwingBaseRow::resized()
{
    auto r = getLocalBounds();
    eight.setBounds(r.removeFromLeft(38).withSizeKeepingCentre(36, 22));
    r.removeFromLeft(4);
    sixteenth.setBounds(r.removeFromLeft(38).withSizeKeepingCentre(36, 22));
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
        if (onResetGroove) onResetGroove();
        notifyGroove();
    };
    addAndMakeVisible(btnReset);

    btnPitchLock.setComponentID("btnLock");
    btnPitchLock.onClick = [this]
    {
        pitchLocked = !pitchLocked;
        setPitchLocked(pitchLocked);
        if (onPitchLockToggled) onPitchLockToggled(pitchLocked);
    };

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

    btnSwing8.mono = true;
    btnSwing16.mono = true;
    btnSwing8.setTooltip("Swing base: delay offbeat 8ths");
    btnSwing16.setTooltip("Swing base: delay offbeat 16ths");
    btnSwing8.onClick = [this]
    {
        groove.swingBase = SwingBase::Eighth;
        syncSwingBaseButtons();
        notifyGroove();
    };
    btnSwing16.onClick = [this]
    {
        groove.swingBase = SwingBase::Sixteenth;
        syncSwingBaseButtons();
        notifyGroove();
    };
    body.addAndMakeVisible(swingBaseRow);

    timing.rows = { &swing, &swingBaseRow, &pocket, &humanize };
    dynamics.rows = { &dynamicsSl, &intensity };
    lengthSec.rows = { &lengthSl };

    octaveStepper.minValue = -3;
    octaveStepper.maxValue = 3;
    octaveStepper.onChange = [this](int v)
    {
        edit.octave = v;
        notifyEdit();
    };
    body.addAndMakeVisible(octaveStepper);

    rootPicker.addItem("—", 1);
    for (int i = 0; i < 12; ++i)
        rootPicker.addItem(kNoteNames[(size_t) i], i + 2);
    rootPicker.onChange = [this]
    {
        const int id = rootPicker.getSelectedId();
        edit.root = id <= 1 ? -1 : id - 2;
        notifyEdit();
    };
    body.addAndMakeVisible(rootPicker);

    for (int i = 0; i < kNumModes; ++i)
        modePicker.addItem(modeName((Mode) i), i + 1);
    modePicker.onChange = [this]
    {
        edit.mode = (Mode) juce::jmax(0, modePicker.getSelectedId() - 1);
        notifyEdit();
    };
    body.addAndMakeVisible(modePicker);

    fitSwitch.onClick = [this]
    {
        edit.fitScale = fitSwitch.getToggleState();
        notifyEdit();
    };
    body.addAndMakeVisible(fitSwitch);

    mapSwitch.onClick = [this]
    {
        edit.mapToRoot = mapSwitch.getToggleState();
        notifyEdit();
    };
    body.addAndMakeVisible(mapSwitch);
    body.addAndMakeVisible(btnPitchLock);

    pitch.rows = { &octaveStepper, &rootPicker, &modePicker, &fitSwitch, &mapSwitch, &btnPitchLock };

    for (auto* sec : { &timing, &dynamics, &lengthSec, &pitch })
    {
        sec->onToggle = [this] { layoutSections(); };
        body.addAndMakeVisible(*sec);
    }

    setGroove(GrooveParams{}, juce::dontSendNotification);
    setEdit(ClipEdit{}, juce::dontSendNotification);
}

void EffectsInspector::setEffectsLocked(bool locked)
{
    effectsLocked = locked;
    btnLock.icon = locked ? icons::lockClosed : icons::lockOpen;
    btnLock.active = locked;
    btnLock.repaint();
    resized();
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

void EffectsInspector::syncSwingBaseButtons()
{
    btnSwing8.active = (groove.swingBase == SwingBase::Eighth);
    btnSwing16.active = (groove.swingBase == SwingBase::Sixteenth);
    btnSwing8.repaint();
    btnSwing16.repaint();
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
    swing.valueText = grooveValueText(kKnobDefs[0], g.swing);
    pocket.valueText = grooveValueText(kKnobDefs[1], g.pocket);
    humanize.valueText = grooveValueText(kKnobDefs[2], g.humanize);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], g.dynamics);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], g.length);
    intensity.valueText = grooveValueText(kKnobDefs[5], g.intensity);
    syncSwingBaseButtons();
    repaint();
}

void EffectsInspector::setEdit(const ClipEdit& e, juce::NotificationType)
{
    edit = e;
    octaveStepper.setValue(e.octave, juce::dontSendNotification);
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
    auto r = juce::Rectangle<int>(0, 0, juce::jmax(1, body.getWidth()), 0);
    int y = 0;
    for (auto* sec : { &timing, &dynamics, &lengthSec, &pitch })
    {
        const int h = sec->idealHeight();
        sec->setBounds(0, y, r.getWidth(), h);
        sec->resized();
        y += h + 4;
    }
    body.setSize(juce::jmax(1, (int) viewport.getMaximumVisibleWidth()), juce::jmax(y, viewport.getMaximumVisibleHeight()));
}

void EffectsInspector::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop(headerH).reduced(8, 4);
    btnReset.setBounds(header.removeFromRight(24).withSizeKeepingCentre(22, 22));
    header.removeFromRight(4);
    btnLock.setBounds(header.removeFromRight(24).withSizeKeepingCentre(22, 22));

    if (effectsLocked)
        r.removeFromTop(bannerH);

    viewport.setBounds(r);
    layoutSections();
}

void EffectsInspector::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromLeft(1));

    g.setColour(colours::text());
    g.setFont(uiFont(12.5f, true));
    g.drawText("Effects", 12, 8, 100, 20, juce::Justification::centredLeft);

    if (effectsLocked)
    {
        auto banner = juce::Rectangle<float>(8.0f, (float) headerH,
                                             (float) getWidth() - 16.0f, (float) bannerH - 4.0f);
        g.setColour(colours::accentSoft());
        g.fillRoundedRectangle(banner, 5.0f);
        g.setColour(colours::accent());
        g.setFont(uiFont(10.5f, false));
        g.drawText("Locked: settings persist while browsing",
                   banner.toNearestInt().reduced(8, 0), juce::Justification::centredLeft);
    }
}

} // namespace pflow
