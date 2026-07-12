#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "GrooveEngine.h"

namespace pflow {
namespace fx {

constexpr float kFontPt = 11.5f;
constexpr float kAnnotPt = 10.5f;
constexpr int kSectionPadT = 9;
constexpr int kSectionPadH = 14;
constexpr int kSectionPadB = 11;
constexpr int kRowMinH = 25;
constexpr int kRowGap = 3;
constexpr int kSectionHeaderH = 18;
constexpr int kSliderRowH = 30;

inline juce::Font inspectorFont(bool semibold = false) { return uiFont(kFontPt, semibold); }
inline juce::Font inspectorMono(bool semibold = false) { return monoFont(kFontPt, semibold); }

inline void drawCaretDown(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    juce::Path p;
    const float cx = area.getCentreX(), cy = area.getCentreY();
    const float s = 4.5f;
    p.addTriangle(cx - s, cy - s * 0.35f, cx + s, cy - s * 0.35f, cx, cy + s * 0.65f);
    g.setColour(colour);
    g.fillPath(p);
}

inline void drawCaretUpDown(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    const float cx = area.getCentreX(), cy = area.getCentreY();
    g.setColour(colour);
    juce::Path up, dn;
    up.addTriangle(cx - 3.5f, cy + 1.5f, cx + 3.5f, cy + 1.5f, cx, cy - 2.5f);
    dn.addTriangle(cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
    g.fillPath(up);
    g.fillPath(dn);
}

inline juce::String signedIntText(int v)
{
    if (v > 0) return "+" + juce::String(v);
    return juce::String(v);
}

class FlatTextButton : public juce::Button
{
public:
    explicit FlatTextButton(const juce::String& text) : juce::Button(text), label(text) {}

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        if (active)
        {
            auto fill = t.accent;
            if (down) fill = fill.darker(0.08f);
            else if (over) fill = fill.brighter(0.06f);
            g.setColour(fill);
            g.fillRoundedRectangle(r, metrics::controlRadius);
            g.setColour(juce::Colours::white);
        }
        else
        {
            drawInspectorControlSurface(g, r, over, down);
            g.setColour(t.rowLabel);
        }
        g.setFont(inspectorFont());
        g.drawFittedText(label, getLocalBounds().reduced(12, 0), juce::Justification::centred, 1);
        if (!isEnabled())
        {
            g.setColour(t.panelBg.withAlpha(0.45f));
            g.fillRoundedRectangle(r, metrics::controlRadius);
        }
    }

    int idealWidth() const
    {
        return (int) std::ceil(juce::GlyphArrangement::getStringWidth(inspectorFont(), label)) + 24;
    }

    juce::String label;
    bool active = false;
};

// ── FlatSwitch ───────────────────────────────────────────────────────────────

class FlatSwitch : public juce::Button
{
public:
    FlatSwitch() : juce::Button({}) { setClickingTogglesState(true); }

    void paintButton(juce::Graphics& g, bool, bool) override
    {
        const auto& t = inspectorTokens();
        const bool on = getToggleState();
        auto track = getLocalBounds().toFloat().withSizeKeepingCentre(26.0f, 15.0f);
        g.setColour(on ? t.accent : t.switchOffTrack);
        g.fillRoundedRectangle(track, 7.5f);
        const float kx = on ? track.getX() + 12.0f : track.getX() + 1.0f;
        g.setColour(on ? juce::Colours::white : (t.dark ? juce::Colour(0xffd4d4d8) : juce::Colours::white));
        g.fillEllipse(kx, track.getCentreY() - 6.5f, 13.0f, 13.0f);
        if (!t.dark && !on)
        {
            g.setColour(t.controlHairline);
            g.drawEllipse(kx, track.getCentreY() - 6.5f, 13.0f, 13.0f, 0.5f);
        }
    }

    int idealWidth() const { return 26; }
};

// ── FlatPopup ────────────────────────────────────────────────────────────────

class FlatPopup : public juce::Component
{
public:
    std::function<void(int)> onChange;

    void setItems(const juce::StringArray& items, int selected)
    {
        labels = items;
        index = juce::jlimit(0, juce::jmax(0, labels.size() - 1), selected);
        repaint();
    }

    int getIndex() const { return index; }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (labels.isEmpty()) return;
        juce::PopupMenu m;
        for (int i = 0; i < labels.size(); ++i)
            m.addItem(i + 1, labels[i], true, i == index);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                        [this](int r)
                        {
                            if (r > 0)
                                setIndex(r - 1, juce::sendNotification);
                        });
    }

    void setIndex(int i, juce::NotificationType notify)
    {
        i = juce::jlimit(0, juce::jmax(0, labels.size() - 1), i);
        if (i == index) return;
        index = i;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(index);
    }

    int idealWidth() const
    {
        float w = 0.0f;
        for (const auto& l : labels)
            w = juce::jmax(w, juce::GlyphArrangement::getStringWidth(inspectorFont(), l));
        const auto& t = inspectorTokens();
        return (int) std::ceil(w) + (t.dark ? 22 : 30);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        drawInspectorControlSurface(g, r, isMouseOver(), false);

        auto textArea = r.reduced(t.dark ? 8.0f : 8.0f, 2.0f);
        if (!t.dark)
            textArea.removeFromRight(16.0f);
        else
            textArea.removeFromRight(14.0f);

        g.setColour(t.rowLabel);
        g.setFont(inspectorFont());
        const juce::String val = labels.size() > index ? labels[index] : juce::String();
        g.drawFittedText(val, textArea.toNearestInt(), juce::Justification::centredLeft, 1);

        if (!t.dark)
        {
            auto cap = r.removeFromRight(15.0f).reduced(1.0f, 3.0f);
            g.setColour(t.accent);
            g.fillRoundedRectangle(cap, 3.5f);
            drawCaretUpDown(g, cap, juce::Colours::white);
        }
        else
        {
            drawCaretUpDown(g, r.removeFromRight(14.0f), t.chevron);
        }
    }

private:
    juce::StringArray labels;
    int index = 0;
};

// ── FlatStepper ──────────────────────────────────────────────────────────────

class FlatStepper : public juce::Component
{
public:
    int minV = -3, maxV = 3, value = 0;
    std::function<void(int)> onChange;
    std::function<juce::String(int)> format;

    void setValue(int v, juce::NotificationType notify = juce::sendNotification)
    {
        v = juce::jlimit(minV, maxV, v);
        if (v == value) return;
        value = v;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(value);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat();
        const float seg = r.getWidth() / 3.0f;
        if (e.x < seg) setValue(value - 1);
        else if (e.x > seg * 2.0f) setValue(value + 1);
    }

    int idealWidth() const { return 72; }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        drawInspectorControlSurface(g, r, isMouseOver(), false);

        const float seg = r.getWidth() / 3.0f;
        g.setColour(t.controlSeparator);
        g.fillRect(r.getX() + seg, r.getY() + 2.0f, 0.5f, r.getHeight() - 4.0f);
        g.fillRect(r.getX() + seg * 2.0f, r.getY() + 2.0f, 0.5f, r.getHeight() - 4.0f);

        g.setColour(t.rowLabel);
        g.setFont(inspectorFont());
        g.drawFittedText("−", r.withWidth(seg).toNearestInt(), juce::Justification::centred, 1);
        g.drawFittedText("+", r.withTrimmedLeft(seg * 2.0f).toNearestInt(), juce::Justification::centred, 1);

        const juce::String text = format ? format(value) : signedIntText(value);
        g.setFont(inspectorMono(true));
        g.drawFittedText(text, r.withTrimmedLeft(seg).withTrimmedRight(seg).toNearestInt(),
                         juce::Justification::centred, 1);
    }
};

// ── TempoToggle ──────────────────────────────────────────────────────────────

class TempoToggle : public juce::Component
{
public:
    enum class Sel { None, Half, Double };
    std::function<void(double)> onChange;

    void setMultiplier(double m, juce::NotificationType notify = juce::dontSendNotification)
    {
        Sel next = Sel::None;
        if (m < 0.75) next = Sel::Half;
        else if (m > 1.5) next = Sel::Double;
        if (next == selected) return;
        selected = next;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(multiplier());
    }

    double multiplier() const
    {
        switch (selected)
        {
            case Sel::Half: return 0.5;
            case Sel::Double: return 2.0;
            case Sel::None: break;
        }
        return 1.0;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat();
        const bool left = e.x < r.getCentreX();
        const Sel seg = left ? Sel::Half : Sel::Double;
        selected = (selected == seg) ? Sel::None : seg;
        repaint();
        if (onChange) onChange(multiplier());
    }

    int idealWidth() const { return 76; }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        drawInspectorControlSurface(g, r, isMouseOver(), false);
        g.setColour(t.controlSeparator);
        g.fillRect(r.getCentreX() - 0.25f, r.getY() + 2.0f, 0.5f, r.getHeight() - 4.0f);

        auto left = r.withTrimmedRight(r.getWidth() * 0.5f);
        auto right = r.withTrimmedLeft(r.getWidth() * 0.5f);
        if (selected == Sel::Half)
        {
            g.setColour(t.accent);
            g.fillRoundedRectangle(left.reduced(1.0f), 4.0f);
        }
        if (selected == Sel::Double)
        {
            g.setColour(t.accent);
            g.fillRoundedRectangle(right.reduced(1.0f), 4.0f);
        }

        g.setFont(inspectorFont());
        auto textCol = [&](Sel s)
        {
            return selected == s ? juce::Colours::white : t.segmentText;
        };
        g.setColour(textCol(Sel::Half));
        g.drawFittedText("/2", left.toNearestInt(), juce::Justification::centred, 1);
        g.setColour(textCol(Sel::Double));
        g.drawFittedText("×2", right.toNearestInt(), juce::Justification::centred, 1);
    }

private:
    Sel selected = Sel::None;
};

// ── FlatSliderRow ────────────────────────────────────────────────────────────

class FlatSliderRow : public juce::Component
{
public:
    FlatSliderRow(const juce::String& l, int mn, int mx, int d, bool bi)
        : label(l), minV(mn), maxV(mx), defV(d), value(d), bipolar(bi) {}

    std::function<void(int)> onChange;
    juce::String valueText;

    void setValue(int v, juce::NotificationType notify = juce::sendNotification)
    {
        v = juce::jlimit(minV, maxV, v);
        if (v == value) return;
        value = v;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(value);
    }

    int getValue() const { return value; }

    void resized() override
    {
        track = getLocalBounds().toFloat().withTrimmedTop(16.0f).withHeight(4.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent&) override { setValue(defV); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto top = getLocalBounds().removeFromTop(14);
        g.setFont(inspectorFont());
        g.setColour(t.rowLabel);
        g.drawText(label, top, juce::Justification::centredLeft);

        g.setFont(inspectorMono());
        g.setColour(t.valueText);
        const auto text = valueText.isNotEmpty() ? valueText
                      : grooveValueText({ label.toRawUTF8(), label.toRawUTF8(), minV, maxV, defV }, value);
        g.drawText(text, top, juce::Justification::centredRight);

        g.setColour(t.sliderTrack);
        g.fillRoundedRectangle(track, 2.0f);

        const float norm = (float) (value - minV) / (float) juce::jmax(1, maxV - minV);
        const float thumbX = track.getX() + norm * track.getWidth();
        const float mid = track.getCentreX();

        g.setColour(t.accent);
        if (bipolar)
        {
            g.fillRoundedRectangle(juce::Rectangle<float>::leftTopRightBottom(
                                       juce::jmin(mid, thumbX), track.getY(),
                                       juce::jmax(mid, thumbX), track.getBottom()), 2.0f);
        }
        else
        {
            g.fillRoundedRectangle(track.withWidth(juce::jmax(0.0f, thumbX - track.getX())), 2.0f);
        }

        g.setColour(t.sliderKnob);
        g.fillEllipse(thumbX - 6.5f, track.getCentreY() - 6.5f, 13.0f, 13.0f);
        if (!t.dark)
        {
            g.setColour(t.controlHairline);
            g.drawEllipse(thumbX - 6.5f, track.getCentreY() - 6.5f, 13.0f, 13.0f, 0.5f);
        }
    }

private:
    void setFromX(float x)
    {
        const float t = juce::jlimit(0.0f, 1.0f, (x - track.getX()) / juce::jmax(1.0f, track.getWidth()));
        setValue(minV + (int) std::lround(t * (float) (maxV - minV)));
    }

    juce::String label;
    int minV, maxV, defV, value;
    bool bipolar = false;
    juce::Rectangle<float> track;
};

// ── InlineRow ────────────────────────────────────────────────────────────────

class InlineRow : public juce::Component
{
public:
    InlineRow(const juce::String& l, juce::Component& c) : control(c), label(l)
    {
        addAndMakeVisible(control);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int w = controlW > 0 ? controlW
                    : (control.getWidth() > 0 ? control.getWidth() : 80);
        auto ctrl = r.removeFromRight(w);
        control.setBounds(ctrl.withSizeKeepingCentre(w, kRowMinH));
    }

    void setControlWidth(int w)
    {
        controlW = w;
        resized();
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText(label, getLocalBounds(), juce::Justification::centredLeft);
    }

    juce::Component& control;
    int controlW = 0;

private:
    juce::String label;
};

// ── KeyModeRow ───────────────────────────────────────────────────────────────

class KeyModeRow : public juce::Component
{
public:
    KeyModeRow(FlatPopup& key, FlatPopup& mode) : keyPopup(key), modePopup(mode)
    {
        addAndMakeVisible(keyPopup);
        addAndMakeVisible(modePopup);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int modeW = modePopup.idealWidth();
        const int keyW = keyPopup.idealWidth();
        modePopup.setBounds(r.removeFromRight(modeW).withSizeKeepingCentre(modeW, kRowMinH));
        r.removeFromRight(7);
        keyPopup.setBounds(r.removeFromRight(keyW).withSizeKeepingCentre(keyW, kRowMinH));
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText("Key", getLocalBounds(), juce::Justification::centredLeft);
    }

private:
    FlatPopup& keyPopup;
    FlatPopup& modePopup;
};

// ── PitchRow ─────────────────────────────────────────────────────────────────

class PitchRow : public juce::Component
{
public:
    FlatStepper stepper;
    juce::String annotation;

    PitchRow()
    {
        stepper.minV = -12;
        stepper.maxV = 12;
        stepper.format = [](int v) { return signedIntText(v); };
        addAndMakeVisible(stepper);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int stepW = stepper.idealWidth();
        stepper.setBounds(r.removeFromRight(stepW).withSizeKeepingCentre(stepW, kRowMinH));
        r.removeFromRight(8);
        // Leave room for the "Pitch" label on the left.
        r.removeFromLeft(42);
        annotBounds = r;
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText("Pitch", getLocalBounds().withWidth(42), juce::Justification::centredLeft);

        g.setFont(uiFont(kAnnotPt, false));
        g.setColour(inspectorTokens().valueText);
        g.drawFittedText(annotation, annotBounds, juce::Justification::centredRight, 1);
    }

    juce::Rectangle<int> annotBounds;
};

// ── Section ──────────────────────────────────────────────────────────────────

class Section : public juce::Component
{
public:
    explicit Section(const juce::String& t) : title(t) {}

    void addRow(juce::Component* c, int h = kRowMinH)
    {
        rows.push_back({ c, h });
        addAndMakeVisible(c);
    }

    void setOpen(bool o)
    {
        if (open == o) return;
        open = o;
        for (auto& r : rows)
            r.comp->setVisible(open);
        if (onToggle) onToggle();
        repaint();
    }

    int idealHeight() const
    {
        int h = kSectionPadT + kSectionHeaderH + kSectionPadB;
        if (!open) return h;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (i > 0) h += kRowGap;
            h += rows[i].h;
        }
        return h;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop(kSectionPadT + kSectionHeaderH);
        r.removeFromBottom(kSectionPadB);
        r = r.reduced(kSectionPadH, 0);
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (!open) { rows[i].comp->setBounds({}); continue; }
            if (i > 0) r.removeFromTop(kRowGap);
            rows[i].comp->setBounds(r.removeFromTop(rows[i].h));
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.y <= kSectionPadT + kSectionHeaderH)
            setOpen(!open);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto header = getLocalBounds().withTrimmedTop(kSectionPadT).removeFromTop(kSectionHeaderH);
        header = header.reduced(kSectionPadH, 0);

        g.setFont(inspectorFont(true));
        g.setColour(t.headerText);
        g.drawText(title, header, juce::Justification::centredLeft);

        auto chev = header.removeFromRight(12).toFloat();
        if (open)
            drawCaretDown(g, chev, t.chevron);
        else
        {
            juce::Path p;
            const float cx = chev.getCentreX(), cy = chev.getCentreY();
            p.addTriangle(cx - 3.5f, cy - 2.0f, cx + 3.5f, cy, cx - 3.5f, cy + 2.0f);
            g.setColour(t.chevron);
            g.fillPath(p);
        }

        drawInspectorDivider(g, getLocalBounds());
    }

    juce::String title;
    bool open = true;
    std::function<void()> onToggle;

    struct Row { juce::Component* comp; int h; };
    std::vector<Row> rows;
};
} // namespace fx
} // namespace pflow
