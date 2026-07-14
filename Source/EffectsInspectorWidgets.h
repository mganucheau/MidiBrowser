#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "GrooveEngine.h"

namespace pflow {
namespace fx {

constexpr float kFontPt = 11.5f;
constexpr float kAnnotPt = 10.5f;
constexpr float kPopupFontPt = 10.5f;
constexpr int kSectionPadT = 6;
constexpr int kSectionPadH = 8;
constexpr int kSectionPadB = 6;
constexpr int kRowMinH = 18;
constexpr int kControlH = 16;
constexpr int kRowGap = 3;
constexpr int kSectionHeaderH = 18;
constexpr int kSectionTitleGap = 4;    // space between title and first row
constexpr int kSectionRowInset = 10;   // align row labels with title text (past chevron)
constexpr int kSliderRowH = 18;        // single-line: title | track | value
constexpr int kSliderLabelW = 62;      // fixed title column
constexpr int kSliderTrackW = 88;      // uniform track width across all rows
constexpr int kSliderValueW = 32;      // readout after the track (right-aligned)
constexpr int kSliderGap = 4;          // gaps: label|track and track|value
constexpr float kSliderThumbR = 4.5f;
constexpr float kSliderTrackH = 3.0f;

inline juce::Font inspectorFont(bool semibold = false) { return uiFont(kFontPt, semibold); }
inline juce::Font inspectorMono(bool semibold = false) { return monoFont(kFontPt, semibold); }
inline juce::Font popupFont(bool semibold = false) { return uiFont(kPopupFontPt, semibold); }

inline void drawCaretDown(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    juce::Path p;
    const float cx = area.getCentreX(), cy = area.getCentreY();
    const float s = 4.0f;
    p.addTriangle(cx - s, cy - s * 0.55f, cx + s, cy - s * 0.55f, cx, cy + s * 0.7f);
    g.setColour(colour);
    g.fillPath(p);
}

inline void drawCaretRight(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    juce::Path p;
    const float cx = area.getCentreX(), cy = area.getCentreY();
    const float s = 4.0f;
    p.addTriangle(cx - s * 0.55f, cy - s, cx + s * 0.7f, cy, cx - s * 0.55f, cy + s);
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
    explicit FlatTextButton(const juce::String& text) : juce::Button(text), label(text)
    {
        setWantsKeyboardFocus(false);
    }

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
        g.drawFittedText(label, getLocalBounds().reduced(8, 0), juce::Justification::centred, 1);
        if (!isEnabled())
        {
            g.setColour(t.panelBg.withAlpha(0.45f));
            g.fillRoundedRectangle(r, metrics::controlRadius);
        }
    }

    int idealWidth() const
    {
        return (int) std::ceil(juce::GlyphArrangement::getStringWidth(inspectorFont(), label)) + 16;
    }

    juce::String label;
    bool active = false;
};

// ── FlatSwitch ───────────────────────────────────────────────────────────────

class FlatSwitch : public juce::Button
{
public:
    FlatSwitch() : juce::Button({})
    {
        setClickingTogglesState(true);
        setWantsKeyboardFocus(false);
    }

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

class FlatPopup : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatPopup() { setWantsKeyboardFocus(false); }
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
        showMenu();
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
            w = juce::jmax(w, juce::GlyphArrangement::getStringWidth(popupFont(), l));
        const auto& t = inspectorTokens();
        // Compact chevron + side padding (longest labels e.g. "Alternate", "1/16").
        return (int) std::ceil(w) + (t.dark ? 20 : 26);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        drawInspectorControlSurface(g, r, isMouseOver(), false);

        auto textArea = r.reduced(6.0f, 1.0f);
        if (!t.dark)
            textArea.removeFromRight(13.0f);
        else
            textArea.removeFromRight(12.0f);

        g.setColour(t.rowLabel);
        g.setFont(popupFont());
        const juce::String val = labels.size() > index ? labels[index] : juce::String();
        g.drawFittedText(val, textArea.toNearestInt(), juce::Justification::centredLeft, 1, 1.0f);

        if (!t.dark)
        {
            auto cap = r.removeFromRight(12.0f).reduced(1.0f, 2.5f);
            g.setColour(t.accent);
            g.fillRoundedRectangle(cap, 3.0f);
            drawCaretUpDown(g, cap, juce::Colours::white);
        }
        else
        {
            drawCaretUpDown(g, r.removeFromRight(12.0f), t.chevron);
        }
    }

private:
    class MenuList : public juce::Component
    {
    public:
        MenuList(FlatPopup& o, juce::StringArray items, int selected, int maxH)
            : owner(o), labels(std::move(items)), index(selected)
        {
            const int rowH = 24;
            float textW = 0.0f;
            for (const auto& l : labels)
                textW = juce::jmax(textW, juce::GlyphArrangement::getStringWidth(inspectorFont(), l));
            const int menuW = juce::jmax(o.getWidth(), (int) std::ceil(textW) + 28);
            const int fullH = labels.size() * rowH + 8;
            setSize(menuW, juce::jmin(fullH, juce::jmax(rowH * 8 + 8, maxH)));
        }

        void paint(juce::Graphics& g) override
        {
            const auto& t = inspectorTokens();
            auto bounds = getLocalBounds().toFloat();
            g.setColour(t.panelBg);
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour(t.controlHairline);
            g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 0.5f);

            const int rowH = 24;
            auto body = getLocalBounds().reduced(4);
            const int first = juce::jlimit(0, juce::jmax(0, labels.size() - 1), scroll);
            const int visible = juce::jmax(1, (body.getHeight()) / rowH);
            for (int i = first; i < labels.size() && i < first + visible; ++i)
            {
                auto row = body.removeFromTop(rowH).toFloat();
                const bool hi = (i == hover);
                const bool sel = (i == index);
                if (hi || sel)
                {
                    g.setColour(hi ? t.accent : t.accent.withAlpha(0.18f));
                    g.fillRoundedRectangle(row, 4.0f);
                }
                g.setFont(inspectorFont());
                g.setColour(hi ? juce::Colours::white : t.rowLabel);
                g.drawText(labels[i], row.reduced(8.0f, 0.0f).toNearestInt(),
                           juce::Justification::centredLeft, true);
            }
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            const int rowH = 24;
            const int visibleIdx = (e.y - 4) / rowH;
            const int h = scroll + visibleIdx;
            const int clamped = juce::isPositiveAndBelow(h, labels.size()) ? h : -1;
            if (clamped != hover) { hover = clamped; repaint(); }
        }

        void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }

        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            const int rowH = 24;
            const int visible = juce::jmax(1, (getHeight() - 8) / rowH);
            const int maxScroll = juce::jmax(0, labels.size() - visible);
            scroll = juce::jlimit(0, maxScroll, scroll - (int) std::lround(wheel.deltaY * 3.0f));
            repaint();
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            const int rowH = 24;
            const int i = scroll + (e.y - 4) / rowH;
            if (juce::isPositiveAndBelow(i, labels.size()))
                owner.setIndex(i, juce::sendNotification);
            if (auto* overlay = getParentComponent())
            {
                if (auto* host = overlay->getParentComponent())
                    host->removeChildComponent(overlay);
                delete overlay;
            }
        }

        FlatPopup& owner;
        juce::StringArray labels;
        int index = 0;
        int hover = -1;
        int scroll = 0;
    };

    void showMenu()
    {
        auto* host = getTopLevelComponent();
        if (host == nullptr) return;

        if (auto* existing = host->findChildWithID("flatPopupMenuHost"))
        {
            host->removeChildComponent(existing);
            delete existing;
        }

        struct Veil : juce::Component
        {
            void mouseDown(const juce::MouseEvent&) override
            {
                if (auto* p = getParentComponent())
                    p->removeChildComponent(this);
                delete this;
            }
        };

        auto* overlay = new Veil();
        overlay->setComponentID("flatPopupMenuHost");
        overlay->setBounds(host->getLocalBounds());
        host->addAndMakeVisible(overlay);
        overlay->toFront(true);

        const int maxH = juce::jmax(200, host->getHeight() - 40);
        auto* menu = new MenuList(*this, labels, index, maxH);
        // Start scrolled so the selected item is visible.
        {
            const int rowH = 24;
            const int visible = juce::jmax(1, (menu->getHeight() - 8) / rowH);
            menu->scroll = juce::jlimit(0, juce::jmax(0, labels.size() - visible),
                                        index - visible / 2);
        }
        const auto screen = localAreaToGlobal(getLocalBounds());
        const auto hostScreen = host->getScreenBounds();
        int x = screen.getX() - hostScreen.getX();
        int y = screen.getBottom() - hostScreen.getY() + 2;
        if (y + menu->getHeight() > host->getHeight())
            y = screen.getY() - hostScreen.getY() - menu->getHeight() - 2;
        y = juce::jlimit(4, host->getHeight() - menu->getHeight() - 4, y);
        x = juce::jlimit(4, host->getWidth() - menu->getWidth() - 4, x);
        menu->setBounds(x, y, menu->getWidth(), menu->getHeight());
        overlay->addAndMakeVisible(menu);
        menu->toFront(false);
    }

    juce::StringArray labels;
    int index = 0;
};

// ── FlatStepper ──────────────────────────────────────────────────────────────

class FlatStepper : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatStepper() { setWantsKeyboardFocus(false); }
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

    int idealWidth() const { return 64; }

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
        g.drawFittedText("-", r.withWidth(seg).toNearestInt(), juce::Justification::centred, 1);
        g.drawFittedText("+", r.withTrimmedLeft(seg * 2.0f).toNearestInt(), juce::Justification::centred, 1);

        const juce::String text = format ? format(value) : signedIntText(value);
        g.setFont(inspectorMono(true));
        g.drawFittedText(text, r.withTrimmedLeft(seg).withTrimmedRight(seg).toNearestInt(),
                         juce::Justification::centred, 1);
    }
};

// ── TempoToggle ──────────────────────────────────────────────────────────────

class TempoToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    TempoToggle()
    {
        setWantsKeyboardFocus(false);
        setTooltip("Halve or double playback tempo vs the clip / host");
    }
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

    int idealWidth() const
    {
        const auto f = inspectorFont();
        // Size each segment for the longer label so neither word is condensed.
        const float labelW = juce::jmax(juce::GlyphArrangement::getStringWidth(f, "Half"),
                                        juce::GlyphArrangement::getStringWidth(f, "Double"));
        const float segW = labelW + 28.0f;
        return (int) std::ceil(segW * 2.0f);
    }

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
        // drawText (not fitted) — never horizontally compress the labels.
        g.setColour(textCol(Sel::Half));
        g.drawText("Half", left.toNearestInt(), juce::Justification::centred, false);
        g.setColour(textCol(Sel::Double));
        g.drawText("Double", right.toNearestInt(), juce::Justification::centred, false);
    }

private:
    Sel selected = Sel::None;
};

// ── FlatSliderRow ────────────────────────────────────────────────────────────

class FlatSliderRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatSliderRow(const juce::String& l, int mn, int mx, int d, bool bi)
        : label(l), minV(mn), maxV(mx), defV(d), value(d), bipolar(bi)
    {
        setWantsKeyboardFocus(false);
    }

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
    void setDefault(int d) { defV = juce::jlimit(minV, maxV, d); }

    void resized() override
    {
        // Label left; track + value hug the right so they share an edge with dropdowns.
        auto r = getLocalBounds().toFloat();
        labelBounds = r.removeFromLeft((float) kSliderLabelW);
        valueBounds = r.removeFromRight((float) kSliderValueW);
        r.removeFromRight((float) kSliderGap);
        r.removeFromLeft((float) kSliderGap);
        track = r.removeFromRight((float) kSliderTrackW)
                    .withSizeKeepingCentre((float) kSliderTrackW, kSliderTrackH);
    }

    void mouseDown(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent&) override { setValue(defV); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        g.setFont(inspectorFont());
        g.setColour(t.rowLabel);
        g.drawText(label, labelBounds.toNearestInt(), juce::Justification::centredLeft, true);

        const auto text = valueText.isNotEmpty() ? valueText
                      : grooveValueText({ label.toRawUTF8(), label.toRawUTF8(), minV, maxV, defV }, value);
        g.setFont(inspectorMono());
        g.setColour(t.valueText);
        g.drawText(text, valueBounds.toNearestInt(), juce::Justification::centredRight, true);

        const float radius = kSliderTrackH * 0.5f;
        g.setColour(t.sliderTrack);
        g.fillRoundedRectangle(track, radius);

        const float norm = (float) (value - minV) / (float) juce::jmax(1, maxV - minV);
        const float thumbX = track.getX() + norm * track.getWidth();
        const float mid = track.getCentreX();

        g.setColour(t.accent);
        if (bipolar)
        {
            g.fillRoundedRectangle(juce::Rectangle<float>::leftTopRightBottom(
                                       juce::jmin(mid, thumbX), track.getY(),
                                       juce::jmax(mid, thumbX), track.getBottom()), radius);
        }
        else
        {
            g.fillRoundedRectangle(track.withWidth(juce::jmax(0.0f, thumbX - track.getX())), radius);
        }

        const float d = kSliderThumbR * 2.0f;
        g.setColour(t.sliderKnob);
        g.fillEllipse(thumbX - kSliderThumbR, track.getCentreY() - kSliderThumbR, d, d);
        g.setColour(t.dark ? t.accent.withAlpha(0.55f) : t.controlHairline);
        g.drawEllipse(thumbX - kSliderThumbR, track.getCentreY() - kSliderThumbR, d, d,
                      t.dark ? 1.0f : 0.75f);
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
    juce::Rectangle<float> track, labelBounds, valueBounds;
};

// ── FlatRangeSliderRow (dual-thumb velocity / similar ranges) ─────────────────

class FlatRangeSliderRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatRangeSliderRow(const juce::String& l, int mn, int mx, int defLo, int defHi)
        : label(l), minV(mn), maxV(mx), lo(defLo), hi(defHi), defLoV(defLo), defHiV(defHi)
    {
        setWantsKeyboardFocus(false);
    }

    std::function<void(int, int)> onChange;
    juce::String valueText;

    void setRange(int newLo, int newHi, juce::NotificationType notify = juce::sendNotification)
    {
        newLo = juce::jlimit(minV, maxV, newLo);
        newHi = juce::jlimit(minV, maxV, newHi);
        if (newHi < newLo) std::swap(newLo, newHi);
        if (newLo == lo && newHi == hi) return;
        lo = newLo;
        hi = newHi;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(lo, hi);
    }

    int getLo() const { return lo; }
    int getHi() const { return hi; }

    void resized() override
    {
        auto r = getLocalBounds().toFloat();
        labelBounds = r.removeFromLeft((float) kSliderLabelW);
        valueBounds = r.removeFromRight((float) kSliderValueW);
        r.removeFromRight((float) kSliderGap);
        r.removeFromLeft((float) kSliderGap);
        track = r.removeFromRight((float) kSliderTrackW)
                    .withSizeKeepingCentre((float) kSliderTrackW, kSliderTrackH);
    }

    void mouseDown(const juce::MouseEvent& e) override { dragThumb = hitThumb(e.position.x); setFromX(e.position.x); }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        setRange(defLoV, defHiV);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        g.setFont(inspectorFont());
        g.setColour(t.rowLabel);
        g.drawText(label, labelBounds.toNearestInt(), juce::Justification::centredLeft, true);

        g.setFont(inspectorMono());
        g.setColour(t.valueText);
        const auto text = valueText.isNotEmpty() ? valueText
                      : (juce::String(lo) + "-" + juce::String(hi));
        g.drawText(text, valueBounds.toNearestInt(), juce::Justification::centredRight, true);

        const float radius = kSliderTrackH * 0.5f;
        g.setColour(t.sliderTrack);
        g.fillRoundedRectangle(track, radius);

        const float span = juce::jmax(1, maxV - minV);
        const float xLo = track.getX() + track.getWidth() * ((float) (lo - minV) / span);
        const float xHi = track.getX() + track.getWidth() * ((float) (hi - minV) / span);
        g.setColour(t.accent.withAlpha(t.dark ? 0.85f : 0.75f));
        g.fillRoundedRectangle(juce::Rectangle<float>::leftTopRightBottom(
                                   xLo, track.getY(), xHi, track.getBottom()), radius);

        const float d = kSliderThumbR * 2.0f;
        auto drawThumb = [&](float x)
        {
            g.setColour(t.sliderKnob);
            g.fillEllipse(x - kSliderThumbR, track.getCentreY() - kSliderThumbR, d, d);
            g.setColour(t.dark ? t.accent.withAlpha(0.55f) : t.controlHairline);
            g.drawEllipse(x - kSliderThumbR, track.getCentreY() - kSliderThumbR, d, d,
                          t.dark ? 1.0f : 0.75f);
        };
        drawThumb(xLo);
        drawThumb(xHi);
    }

private:
    enum class Thumb { None, Lo, Hi };
    Thumb dragThumb = Thumb::None;

    Thumb hitThumb(float x) const
    {
        const float span = juce::jmax(1, maxV - minV);
        const float xLo = track.getX() + track.getWidth() * ((float) (lo - minV) / span);
        const float xHi = track.getX() + track.getWidth() * ((float) (hi - minV) / span);
        const float dLo = std::abs(x - xLo);
        const float dHi = std::abs(x - xHi);
        if (dLo <= dHi && dLo < kSliderThumbR * 2.5f) return Thumb::Lo;
        if (dHi < kSliderThumbR * 2.5f) return Thumb::Hi;
        return (x < (xLo + xHi) * 0.5f) ? Thumb::Lo : Thumb::Hi;
    }

    void setFromX(float x)
    {
        const float t = juce::jlimit(0.0f, 1.0f, (x - track.getX()) / juce::jmax(1.0f, track.getWidth()));
        const int v = minV + (int) std::lround(t * (float) (maxV - minV));
        if (dragThumb == Thumb::Lo)
            setRange(juce::jmin(v, hi), hi);
        else
            setRange(lo, juce::jmax(v, lo));
    }

    juce::String label;
    int minV, maxV, lo, hi, defLoV, defHiV;
    juce::Rectangle<float> track, labelBounds, valueBounds;
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
        r.removeFromLeft(kSliderLabelW);
        r.removeFromLeft(kSliderGap);
        const int w = controlW > 0 ? controlW
                    : (control.getWidth() > 0 ? control.getWidth() : 72);
        // Cap width so long labels (e.g. articulation) don't blow past the value column.
        const int maxW = juce::jmax(40, r.getWidth());
        const int useW = juce::jmin(w, maxW);
        auto ctrl = r.removeFromRight(useW);
        control.setBounds(ctrl.withSizeKeepingCentre(useW, kControlH));
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
        g.drawText(label, getLocalBounds().withWidth(kSliderLabelW),
                   juce::Justification::centredLeft, true);
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
        modePopup.setBounds(r.removeFromRight(modeW).withSizeKeepingCentre(modeW, kControlH));
        r.removeFromRight(7);
        keyPopup.setBounds(r.removeFromRight(keyW).withSizeKeepingCentre(keyW, kControlH));
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText("Key", getLocalBounds().withWidth(kSliderLabelW),
                   juce::Justification::centredLeft, true);
    }

private:
    FlatPopup& keyPopup;
    FlatPopup& modePopup;
};

// ── RangeRow (Min / Max note pickers) ────────────────────────────────────────

class RangeRow : public juce::Component
{
public:
    RangeRow(FlatPopup& minP, FlatPopup& maxP) : minPopup(minP), maxPopup(maxP)
    {
        addAndMakeVisible(minPopup);
        addAndMakeVisible(maxPopup);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int maxW = maxPopup.idealWidth();
        const int minW = minPopup.idealWidth();
        maxPopup.setBounds(r.removeFromRight(maxW).withSizeKeepingCentre(maxW, kControlH));
        r.removeFromRight(7);
        minPopup.setBounds(r.removeFromRight(minW).withSizeKeepingCentre(minW, kControlH));
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText("Range", getLocalBounds().withWidth(kSliderLabelW),
                   juce::Justification::centredLeft, true);
    }

private:
    FlatPopup& minPopup;
    FlatPopup& maxPopup;
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
        stepper.setBounds(r.removeFromRight(stepW).withSizeKeepingCentre(stepW, kControlH));
        r.removeFromRight(kSliderGap);
        r.removeFromLeft(kSliderLabelW);
        annotBounds = r;
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText("Pitch", getLocalBounds().withWidth(kSliderLabelW),
                   juce::Justification::centredLeft, true);

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

    void addRow(juce::Component* c, int h = kRowMinH, std::function<bool()> dirtyFn = {})
    {
        rows.push_back({ c, h, std::move(dirtyFn) });
        addChildComponent(c);
        refreshRowVisibility();
    }

    void setOpen(bool o)
    {
        if (open == o) return;
        open = o;
        refreshRowVisibility();
        if (onToggle) onToggle();
        repaint();
    }

    /** Re-evaluate dirty-row visibility (call after param changes). */
    void refreshDirtyRows()
    {
        refreshRowVisibility();
        resized();
        repaint();
    }

    int idealHeight() const
    {
        int h = kSectionPadT + kSectionHeaderH + kSectionPadB;
        int shown = 0;
        int content = 0;
        for (const auto& r : rows)
        {
            if (!isRowShowing(r)) continue;
            if (shown > 0) content += kRowGap;
            content += r.h;
            ++shown;
        }
        if (shown > 0)
            h += kSectionTitleGap + content;
        return h;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop(kSectionPadT + kSectionHeaderH + kSectionTitleGap);
        r.removeFromBottom(kSectionPadB);
        // Keep the right edge; inset left so row labels line up with the title text
        // (title sits past the chevron).
        r.removeFromLeft(kSectionPadH + kSectionRowInset);
        r.removeFromRight(kSectionPadH);
        bool first = true;
        for (auto& row : rows)
        {
            if (!isRowShowing(row))
            {
                row.comp->setBounds({});
                continue;
            }
            if (!first) r.removeFromTop(kRowGap);
            first = false;
            row.comp->setBounds(r.removeFromTop(row.h));
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

        auto chev = header.removeFromLeft(14).toFloat().withSizeKeepingCentre(12.0f, 12.0f);
        header.removeFromLeft(4);
        if (open)
            drawCaretDown(g, chev, t.chevron);
        else
            drawCaretRight(g, chev, t.chevron);

        g.setFont(inspectorFont(true));
        g.setColour(t.headerText);
        g.drawText(title, header, juce::Justification::centredLeft);

        drawInspectorDivider(g, getLocalBounds());
    }

    juce::String title;
    bool open = false;
    std::function<void()> onToggle;

    struct Row
    {
        juce::Component* comp = nullptr;
        int h = kRowMinH;
        std::function<bool()> dirty;
    };
    std::vector<Row> rows;

private:
    bool isRowShowing(const Row& r) const
    {
        if (open) return true;
        return r.dirty && r.dirty();
    }

    void refreshRowVisibility()
    {
        for (auto& r : rows)
            r.comp->setVisible(isRowShowing(r));
    }
};
} // namespace fx
} // namespace pflow
