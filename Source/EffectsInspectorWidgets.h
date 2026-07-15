#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "UiAtoms.h"
#include "GrooveEngine.h"

namespace pflow {
namespace fx {

// Caps B2 canvas metrics (fixed px — see effects-photos-caps-b2.canvas.tsx)
constexpr float kFontPt = 12.0f;
constexpr float kAnnotPt = 11.0f;
constexpr float kPopupFontPt = 12.0f;
constexpr float kSectionTitlePt = 14.0f;
constexpr int kSectionPadH = 12;       // shell horizontal pad
constexpr int kRowMinH = 26;
constexpr int kControlH = 26;
constexpr int kSectionHeaderH = 30;
constexpr int kSectionRowInset = 22;   // INDENT past padded edge (chevron column)
constexpr int kSliderRowH = 26;
constexpr int kSelectW = 96;
constexpr int kHeaderIconW = 20; // match library sidebar glyph hit target
constexpr float kTallRadius = 4.0f;
constexpr float kTallPadX = 10.0f;

/** Compact / Comfortable spacing between Toolkit rows and section chrome. */
inline int toolkitRowGap()
{
    return metrics::scaled(currentDensity() == Density::Comfortable ? 10 : 6);
}
inline int toolkitSectionPadT()
{
    return metrics::scaled(currentDensity() == Density::Comfortable ? 10 : 6);
}
inline int toolkitSectionPadB()
{
    return metrics::scaled(currentDensity() == Density::Comfortable ? 6 : 3);
}
inline int toolkitTitleGap()
{
    return metrics::scaled(currentDensity() == Density::Comfortable ? 10 : 6);
}
/** Bottom inset under the last Toolkit section — a bit more than section top pad. */
inline int toolkitBodyPadB()
{
    return metrics::scaled(currentDensity() == Density::Comfortable ? 18 : 14);
}

inline juce::Font inspectorFont(bool semibold = false) { return uiFontFixed(kFontPt, semibold); }
inline juce::Font inspectorMono(bool semibold = false) { return monoFontFixed(kFontPt, semibold); }
inline juce::Font popupFont(bool semibold = false) { return uiFontFixed(kPopupFontPt, semibold); }
inline juce::Font sectionTitleFont() { return uiFontFixed(kSectionTitlePt, true); }

inline void fillTallWell(juce::Graphics& g, juce::Rectangle<float> r, bool hot)
{
    const auto& t = inspectorTokens();
    g.setColour(hot ? t.tallWellHot : t.tallWell);
    g.fillRoundedRectangle(r, kTallRadius);
    if (hot)
    {
        g.setColour(t.controlHairline);
        g.drawRoundedRectangle(r.reduced(0.5f), kTallRadius, 1.0f);
    }
}

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
        // Caps B2 library / Photos: 28×15 track.
        auto track = getLocalBounds().toFloat().withSizeKeepingCentre(28.0f, 15.0f);
        g.setColour(on ? t.accent : t.switchOffTrack);
        g.fillRoundedRectangle(track, 7.5f);
        const float kx = on ? track.getX() + 14.0f : track.getX() + 1.5f;
        g.setColour(on ? juce::Colours::white : (t.dark ? juce::Colour(0xffd4d4d8) : juce::Colours::white));
        g.fillEllipse(kx, track.getCentreY() - 6.0f, 12.0f, 12.0f);
        if (!t.dark && !on)
        {
            g.setColour(t.controlHairline);
            g.drawEllipse(kx, track.getCentreY() - 6.0f, 12.0f, 12.0f, 0.5f);
        }
    }

    int idealWidth() const { return 28; }
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
        auto font = popupFont();
        float maxW = 0.0f;
        for (const auto& s : labels)
            maxW = juce::jmax(maxW, font.getStringWidthFloat(s));
        // pad + caret column; never narrower than the Caps select.
        return juce::jmax(kSelectW, (int) std::ceil((double) maxW) + 28);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        fillTallWell(g, r, isMouseOver());

        auto textArea = r.reduced(8.0f, 0.0f);
        textArea.removeFromRight(14.0f);

        g.setColour(t.headerText);
        g.setFont(popupFont());
        const juce::String val = labels.size() > index ? labels[index] : juce::String();
        g.drawText(val, textArea.toNearestInt(), juce::Justification::centredLeft, false);

        drawCaretUpDown(g, r.removeFromRight(14.0f), t.chevron);
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

    bool bare = false; // no chrome — plain − value + (Pitch row)

    int idealWidth() const { return bare ? 56 : kSelectW; }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        if (!bare)
            fillTallWell(g, r, isMouseOver());

        const float seg = r.getWidth() / 3.0f;
        if (!bare)
        {
            g.setColour(t.controlSeparator);
            g.fillRect(r.getX() + seg, r.getY() + 3.0f, 0.5f, r.getHeight() - 6.0f);
            g.fillRect(r.getX() + seg * 2.0f, r.getY() + 3.0f, 0.5f, r.getHeight() - 6.0f);
        }

        g.setColour(t.rowLabel);
        g.setFont(inspectorFont());
        g.drawFittedText("-", r.withWidth(seg).toNearestInt(), juce::Justification::centred, 1);
        g.drawFittedText("+", r.withTrimmedLeft(seg * 2.0f).toNearestInt(), juce::Justification::centred, 1);

        const juce::String text = format ? format(value) : signedIntText(value);
        g.setFont(inspectorMono(true));
        g.setColour(t.headerText);
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
        fillTallWell(g, r, isMouseOver());
        g.setColour(t.controlSeparator);
        g.fillRect(r.getCentreX() - 0.25f, r.getY() + 4.0f, 0.5f, r.getHeight() - 8.0f);

        auto left = r.withTrimmedRight(r.getWidth() * 0.5f);
        auto right = r.withTrimmedLeft(r.getWidth() * 0.5f);
        if (selected == Sel::Half)
        {
            g.setColour(t.accent);
            g.fillRoundedRectangle(left.reduced(1.5f, 2.0f), 3.0f);
        }
        if (selected == Sel::Double)
        {
            g.setColour(t.accent);
            g.fillRoundedRectangle(right.reduced(1.5f, 2.0f), 3.0f);
        }

        g.setFont(inspectorFont());
        auto textCol = [&](Sel s)
        {
            return selected == s ? juce::Colours::white : t.rowLabel;
        };
        g.setColour(textCol(Sel::Half));
        g.drawText("Half", left.toNearestInt(), juce::Justification::centred, false);
        g.setColour(textCol(Sel::Double));
        g.drawText("Double", right.toNearestInt(), juce::Justification::centred, false);
    }

private:
    Sel selected = Sel::None;
};

// ── FlatSliderRow (tall Photos Adjust: label + value inside, hover highlight) ─

class FlatSliderRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatSliderRow(const juce::String& l, int mn, int mx, int d, bool bi)
        : label(l), minV(mn), maxV(mx), defV(d), value(d), bipolar(bi)
    {
        setWantsKeyboardFocus(false);
        setRepaintsOnMouseActivity(true);
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

    void mouseDown(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent&) override { setValue(defV); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        const bool hot = isMouseOverOrDragging();
        // Neutral well; value fill is accent — outline only while interacting.
        fillTallWell(g, r, false);

        const float norm = (float) (value - minV) / (float) juce::jmax(1, maxV - minV);
        juce::Rectangle<float> fill;
        if (bipolar)
        {
            const float mid = r.getCentreX();
            const float x = r.getX() + norm * r.getWidth();
            fill = juce::Rectangle<float>::leftTopRightBottom(
                juce::jmin(mid, x), r.getY(), juce::jmax(mid, x), r.getBottom());
        }
        else
        {
            fill = r.withWidth(juce::jmax(0.0f, r.getWidth() * norm));
        }
        g.setColour(t.accent);
        g.fillRoundedRectangle(fill, kTallRadius);

        if (hot)
        {
            g.setColour(t.accent);
            g.drawRoundedRectangle(r.reduced(0.5f), kTallRadius, 1.2f);
        }

        auto pad = r.reduced(kTallPadX, 0.0f).toNearestInt();
        const auto text = valueText.isNotEmpty() ? valueText
                      : grooveValueText({ label.toRawUTF8(), label.toRawUTF8(), minV, maxV, defV }, value);

        auto drawTexts = [&](juce::Colour labelCol, juce::Colour valueCol)
        {
            g.setFont(inspectorFont());
            g.setColour(labelCol);
            g.drawText(label, pad, juce::Justification::centredLeft, true);
            g.setFont(inspectorMono());
            g.setColour(valueCol);
            g.drawText(text, pad, juce::Justification::centredRight, true);
        };
        // Base colours on the well; white wherever text sits on the accent fill.
        drawTexts(hot ? t.headerText : t.rowLabel, t.headerText);
        if (fill.getWidth() > 0.5f)
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(fill.getSmallestIntegerContainer());
            drawTexts(juce::Colours::white, juce::Colours::white);
        }
    }

private:
    void setFromX(float x)
    {
        const float w = (float) juce::jmax(1, getWidth());
        const float t = juce::jlimit(0.0f, 1.0f, x / w);
        setValue(minV + (int) std::lround(t * (float) (maxV - minV)));
    }

    juce::String label;
    int minV, maxV, defV, value;
    bool bipolar = false;
};

// ── FlatRangeSliderRow (dual-thumb velocity / similar ranges) ─────────────────

class FlatRangeSliderRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatRangeSliderRow(const juce::String& l, int mn, int mx, int defLo, int defHi)
        : label(l), minV(mn), maxV(mx), lo(defLo), hi(defHi), defLoV(defLo), defHiV(defHi)
    {
        setWantsKeyboardFocus(false);
        setRepaintsOnMouseActivity(true);
    }

    std::function<void(int, int)> onChange;
    juce::String valueText;
    /** Library search: white label/value on accent fill (toolkit uses accent fill too). */
    bool accentFill = false;

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

    void mouseDown(const juce::MouseEvent& e) override { dragThumb = hitThumb(e.position.x); setFromX(e.position.x); }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent&) override { setRange(defLoV, defHiV); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        const bool hot = isMouseOverOrDragging();
        fillTallWell(g, r, false);

        const float span = (float) juce::jmax(1, maxV - minV);
        const float xLo = r.getX() + r.getWidth() * ((float) (lo - minV) / span);
        const float xHi = r.getX() + r.getWidth() * ((float) (hi - minV) / span);
        auto fill = juce::Rectangle<float>::leftTopRightBottom(
            xLo, r.getY(), xHi, r.getBottom());
        g.setColour(t.accent);
        g.fillRoundedRectangle(fill, kTallRadius);

        if (hot)
        {
            g.setColour(t.accent);
            g.drawRoundedRectangle(r.reduced(0.5f), kTallRadius, 1.2f);
        }

        auto pad = r.reduced(accentFill ? 8.0f : kTallPadX, 0.0f).toNearestInt();
        // Library search filters use compact 11pt (Caps B2 FILTER_PT).
        const auto text = valueText.isNotEmpty() ? valueText
                      : (juce::String(lo) + "-" + juce::String(hi));
        auto drawTexts = [&](juce::Colour labelCol, juce::Colour valueCol)
        {
            g.setFont(accentFill ? uiFontFixed(kAnnotPt) : inspectorFont());
            g.setColour(labelCol);
            g.drawText(label, pad, juce::Justification::centredLeft, true);
            g.setFont(accentFill ? monoFontFixed(kAnnotPt) : inspectorMono());
            g.setColour(valueCol);
            g.drawText(text, pad, juce::Justification::centredRight, true);
        };
        drawTexts(hot ? t.headerText : t.rowLabel, t.headerText);
        if (fill.getWidth() > 0.5f)
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(fill.getSmallestIntegerContainer());
            drawTexts(juce::Colours::white, juce::Colours::white);
        }
    }

private:
    enum class Thumb { None, Lo, Hi };
    Thumb dragThumb = Thumb::None;

    Thumb hitThumb(float x) const
    {
        const float span = (float) juce::jmax(1, maxV - minV);
        const float w = (float) juce::jmax(1, getWidth());
        const float xLo = w * ((float) (lo - minV) / span);
        const float xHi = w * ((float) (hi - minV) / span);
        const float dLo = std::abs(x - xLo);
        const float dHi = std::abs(x - xHi);
        if (dLo <= dHi) return Thumb::Lo;
        return Thumb::Hi;
    }

    void setFromX(float x)
    {
        const float t = juce::jlimit(0.0f, 1.0f, x / (float) juce::jmax(1, getWidth()));
        const int v = minV + (int) std::lround(t * (float) (maxV - minV));
        if (dragThumb == Thumb::Lo)
            setRange(juce::jmin(v, hi), hi);
        else
            setRange(lo, juce::jmax(v, lo));
    }

    juce::String label;
    int minV, maxV, lo, hi, defLoV, defHiV;
};

// ── InlineRow (title left · control right-aligned at kSelectW) ───────────────

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
        // Caps B2: always fixed select width, right-aligned.
        const int useW = controlW > 0 ? controlW : kSelectW;
        auto ctrl = r.removeFromRight(useW);
        control.setBounds(ctrl.withSizeKeepingCentre(useW, kControlH));
        labelBounds = r.withTrimmedRight(8);
    }

    void setControlWidth(int w)
    {
        controlW = w > 0 ? w : kSelectW;
        resized();
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText(label, labelBounds, juce::Justification::centredLeft, true);
    }

    juce::Component& control;
    int controlW = 0;

private:
    juce::String label;
    juce::Rectangle<int> labelBounds;
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
        modePopup.setBounds(r.removeFromRight(kSelectW).withSizeKeepingCentre(kSelectW, kControlH));
        r.removeFromRight(6);
        keyPopup.setBounds(r.removeFromRight(kSelectW).withSizeKeepingCentre(kSelectW, kControlH));
        labelBounds = r.withTrimmedRight(8);
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText("Key", labelBounds, juce::Justification::centredLeft, true);
    }

    juce::Rectangle<int> labelBounds;

private:
    FlatPopup& keyPopup;
    FlatPopup& modePopup;
};

// ── PitchRow (plain text + note tight against bare stepper) ──────────────────

class PitchRow : public juce::Component
{
public:
    FlatStepper stepper;
    juce::String annotation;

    PitchRow()
    {
        stepper.minV = -12;
        stepper.maxV = 12;
        stepper.bare = true;
        stepper.format = [](int v) { return signedIntText(v); };
        addAndMakeVisible(stepper);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int stepW = stepper.idealWidth();
        stepper.setBounds(r.removeFromRight(stepW).withSizeKeepingCentre(stepW, kControlH));
        r.removeFromRight(4);
        r.removeFromLeft(42);
        annotBounds = r;
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText("Pitch", getLocalBounds().withWidth(42),
                   juce::Justification::centredLeft, true);

        g.setFont(uiFontFixed(kAnnotPt, false));
        g.setColour(inspectorTokens().valueText);
        g.drawFittedText(annotation, annotBounds, juce::Justification::centredRight, 1);
    }

    juce::Rectangle<int> annotBounds;
};

// ── NoteFilterBlock (Filter label + 1-oct keyboard; type is a separate row) ──

class NoteFilterBlock : public juce::Component
{
public:
    NoteFilterBlock()
    {
        setWantsKeyboardFocus(false);
    }

    std::function<void()> onChange;

    void setState(uint16_t mask, juce::NotificationType notify)
    {
        mask = (uint16_t) (mask & 0x0FFF);
        if (mask == 0) mask = 0x0FFF;
        const bool changed = mask != noteFilterMask;
        noteFilterMask = mask;
        if (changed) repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange();
    }

    uint16_t getMask() const { return noteFilterMask; }

    int idealHeight() const
    {
        return kLabelH + toolkitRowGap() + kKeysH;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop(kLabelH + toolkitRowGap());
        keysBounds = r.removeFromTop(kKeysH);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!keysBounds.contains(e.getPosition()))
            return;
        const int pc = hitPitchClass(e.x - keysBounds.getX(), e.y - keysBounds.getY(),
                                     keysBounds.getWidth(), keysBounds.getHeight());
        if (pc < 0) return;
        const bool on = (noteFilterMask & (uint16_t) (1u << pc)) != 0;
        uint16_t next = noteFilterMask;
        if (on)
        {
            next = (uint16_t) (next & (uint16_t) ~(1u << pc) & 0x0FFF);
            if (next == 0) return; // keep at least one
        }
        else
        {
            next = (uint16_t) ((next | (uint16_t) (1u << pc)) & 0x0FFF);
        }
        noteFilterMask = next;
        repaint(keysBounds);
        if (onChange) onChange();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto label = getLocalBounds().removeFromTop(kLabelH);
        g.setFont(inspectorFont());
        g.setColour(t.rowLabel);
        g.drawText("Filter", label, juce::Justification::centredLeft, true);

        paintKeyboard(g, keysBounds.toFloat());
    }

private:
    static constexpr int kLabelH = 18;
    static constexpr int kKeysH = 40;
    /** Solid dark gray for filtered-off keys (deactivated look). */
    static juce::Colour offKeyFill() { return juce::Colour(0xff3a3a3a); }

    void paintKeyboard(juce::Graphics& g, juce::Rectangle<float> area) const
    {
        if (area.getWidth() < 8.0f || area.getHeight() < 8.0f)
            return;

        // White keys: C D E F G A B
        static constexpr int kWhitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr int kBlackPc[5] = { 1, 3, 6, 8, 10 };
        // Black sits after white index: C#, D#, F#, G#, A#
        static constexpr int kBlackAfterWhite[5] = { 0, 1, 3, 4, 5 };

        const float whiteW = area.getWidth() / 7.0f;
        const float whiteH = area.getHeight();
        const float blackW = whiteW * 0.62f;
        const float blackH = whiteH * 0.58f;

        auto enabled = [this](int pc)
        {
            return (noteFilterMask & (uint16_t) (1u << pc)) != 0;
        };

        for (int i = 0; i < 7; ++i)
        {
            const int pc = kWhitePc[i];
            auto key = juce::Rectangle<float>(area.getX() + whiteW * (float) i,
                                              area.getY(), whiteW - 1.0f, whiteH);
            const bool on = enabled(pc);
            g.setColour(on ? colours::kbWhite() : offKeyFill());
            g.fillRoundedRectangle(key, 2.0f);
            g.setColour(colours::line().withAlpha(on ? 0.55f : 0.35f));
            g.drawRoundedRectangle(key.reduced(0.5f), 2.0f, 0.8f);
        }

        for (int i = 0; i < 5; ++i)
        {
            const int pc = kBlackPc[i];
            const float cx = area.getX() + whiteW * ((float) kBlackAfterWhite[i] + 1.0f);
            auto key = juce::Rectangle<float>(cx - blackW * 0.5f, area.getY(), blackW, blackH);
            const bool on = enabled(pc);
            g.setColour(on ? colours::kbBlack() : offKeyFill().darker(0.15f));
            g.fillRoundedRectangle(key, 2.0f);
            g.setColour(colours::line().withAlpha(on ? 0.0f : 0.30f));
            if (!on)
                g.drawRoundedRectangle(key.reduced(0.5f), 2.0f, 0.8f);
        }
    }

    static int hitPitchClass(int x, int y, int width, int height)
    {
        if (width <= 0 || height <= 0) return -1;
        static constexpr int kWhitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr int kBlackPc[5] = { 1, 3, 6, 8, 10 };
        static constexpr int kBlackAfterWhite[5] = { 0, 1, 3, 4, 5 };
        const float whiteW = (float) width / 7.0f;
        const float blackW = whiteW * 0.62f;
        const float blackH = (float) height * 0.58f;
        if ((float) y <= blackH)
        {
            for (int i = 0; i < 5; ++i)
            {
                const float cx = whiteW * ((float) kBlackAfterWhite[i] + 1.0f);
                if (std::abs((float) x - cx) <= blackW * 0.5f)
                    return kBlackPc[i];
            }
        }
        const int wi = juce::jlimit(0, 6, (int) ((float) x / whiteW));
        return kWhitePc[wi];
    }

    uint16_t noteFilterMask = 0x0FFF;
    juce::Rectangle<int> keysBounds;
};

// ── Section (fold chevron + large title + refresh + lock) ────────────────────

class Section : public juce::Component
{
public:
    explicit Section(const juce::String& t) : title(t) {}

    void addRow(juce::Component* c, int h = kRowMinH, std::function<bool()> dirtyFn = {})
    {
        rows.push_back({ c, h, {}, std::move(dirtyFn) });
        addChildComponent(c);
        refreshRowVisibility();
    }

    void addRow(juce::Component* c, std::function<int()> heightFn, std::function<bool()> dirtyFn = {})
    {
        rows.push_back({ c, kRowMinH, std::move(heightFn), std::move(dirtyFn) });
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

    void setLocked(bool l)
    {
        if (locked == l) return;
        locked = l;
        repaint();
    }

    bool isLocked() const { return locked; }

    /** Re-evaluate dirty-row visibility (call after param changes). */
    void refreshDirtyRows()
    {
        refreshRowVisibility();
        resized();
        repaint();
    }

    int idealHeight() const
    {
        const int padT = toolkitSectionPadT();
        const int padB = toolkitSectionPadB();
        const int rowGap = toolkitRowGap();
        const int titleGap = toolkitTitleGap();
        int h = padT + kSectionHeaderH + padB;
        int shown = 0;
        int content = 0;
        for (const auto& r : rows)
        {
            if (!isRowShowing(r)) continue;
            if (shown > 0) content += rowGap;
            content += r.height();
            ++shown;
        }
        if (shown > 0)
            h += titleGap + content;
        return h;
    }

    void resized() override
    {
        const int padT = toolkitSectionPadT();
        const int padB = toolkitSectionPadB();
        const int rowGap = toolkitRowGap();
        const int titleGap = toolkitTitleGap();
        auto r = getLocalBounds();
        r.removeFromTop(padT + kSectionHeaderH + titleGap);
        r.removeFromBottom(padB);
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
            if (!first) r.removeFromTop(rowGap);
            first = false;
            row.comp->setBounds(r.removeFromTop(row.height()));
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int padT = toolkitSectionPadT();
        if (e.y > padT + kSectionHeaderH) return;

        auto header = getLocalBounds().withTrimmedTop(padT).removeFromTop(kSectionHeaderH)
                          .reduced(kSectionPadH, 0);
        auto lockR = header.removeFromRight(kHeaderIconW);
        auto resetR = header.removeFromRight(kHeaderIconW);

        if (lockR.contains(e.getPosition()) && onLockToggle)
        {
            onLockToggle();
            return;
        }
        if (resetR.contains(e.getPosition()) && onReset)
        {
            onReset();
            return;
        }
        setOpen(!open);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto header = getLocalBounds().withTrimmedTop(toolkitSectionPadT()).removeFromTop(kSectionHeaderH);
        header = header.reduced(kSectionPadH, 0);

        auto lockR = header.removeFromRight(kHeaderIconW).toFloat()
                         .withSizeKeepingCentre(16.0f, 16.0f);
        auto resetR = header.removeFromRight(kHeaderIconW).toFloat()
                          .withSizeKeepingCentre(16.0f, 16.0f);

        auto chev = header.removeFromLeft(14).toFloat().withSizeKeepingCentre(12.0f, 12.0f);
        header.removeFromLeft(6);
        if (open)
            drawCaretDown(g, chev, t.chevron);
        else
            drawCaretRight(g, chev, t.chevron);

        g.setFont(sectionTitleFont());
        g.setColour(t.headerText);
        g.drawText(title, header, juce::Justification::centredLeft, true);

        drawIcon(g, icons::undo, resetR, t.chevron, 1.4f);
        drawIcon(g, locked ? icons::lockClosed : icons::lockOpen, lockR,
                 locked ? t.accent : t.chevron, 1.4f);
    }

    juce::String title;
    /** Expanded by default; when folded, dirty (changed) rows stay visible. */
    bool open = true;
    bool locked = false;
    std::function<void()> onToggle;
    std::function<void()> onReset;
    std::function<void()> onLockToggle;

    struct Row
    {
        juce::Component* comp = nullptr;
        int h = kRowMinH;
        std::function<int()> heightFn;
        std::function<bool()> dirty;
        int height() const { return heightFn ? juce::jmax(1, heightFn()) : h; }
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
