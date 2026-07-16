#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "Theme.h"
#include "UiAtoms.h"
#include "GrooveEngine.h"
#include "EditModel.h"

namespace pflow {
namespace fx {

// Caps B2 canvas metrics (fixed px — see effects-photos-caps-b2.canvas.tsx)
constexpr float kFontPt = 12.0f;
constexpr float kAnnotPt = 11.0f;
constexpr float kPopupFontPt = 12.0f;
constexpr float kSectionTitlePt = 14.0f;
constexpr int kSectionPadH = 12;       // shell horizontal pad (section header chrome)
constexpr int kRowMinH = 26;
constexpr int kControlH = 26;
constexpr int kSectionHeaderH = 30;
constexpr int kSectionRowInset = 22;   // INDENT past padded edge (chevron column)
/** Content inset from pane edge ≈ left-edge→title (excludes chevron). */
constexpr int kContentPadX = kSectionPadH + kSectionRowInset; // 34
constexpr int kSliderRowH = 26;
constexpr int kSelectW = 48;       // 1/4 content column (Extend, Quantize, …)
constexpr int kSelectHalfW = 96;   // 1/2 content column (Half/Double)
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
    g.setColour(hot ? (usesDarkAppearance() ? ds::ctl().brighter(0.08f) : ds::ctl().darker(0.03f))
                    : ds::ctl());
    g.fillRoundedRectangle(r, kTallRadius);
    g.setColour(ds::ctlb());
    g.drawRoundedRectangle(r.reduced(0.5f), kTallRadius, 1.0f);
    ds::shadow::control(g, r, kTallRadius);
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
            g.setColour(juce::Colours::black);
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
        setWantsKeyboardFocus(true);
    }

    void paintButton(juce::Graphics& g, bool, bool) override
    {
        const auto& t = inspectorTokens();
        const bool on = getToggleState();
        // Source-list / Photos: 26×16 track.
        auto track = getLocalBounds().toFloat().withSizeKeepingCentre(26.0f, 16.0f);
        g.setColour(on ? t.accent : t.switchOffTrack);
        g.fillRoundedRectangle(track, 8.0f);
        const float kx = on ? track.getX() + 11.0f : track.getX() + 1.5f;
        g.setColour(juce::Colours::white);
        g.fillEllipse(kx, track.getCentreY() - 6.5f, 13.0f, 13.0f);
        if (!t.dark && !on)
        {
            g.setColour(t.controlHairline);
            g.drawEllipse(kx, track.getCentreY() - 6.5f, 13.0f, 13.0f, 0.5f);
        }
        if (hasKeyboardFocus(true))
            drawFocusRing(g, getLocalBounds().toFloat(), 4.0f);
    }

    int idealWidth() const { return 26; }
};

// ── FlatPopup ────────────────────────────────────────────────────────────────

class FlatPopup : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatPopup() { setWantsKeyboardFocus(true); }
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
        if (!hasKeyboardFocus(true))
            grabKeyboardFocus();
        showMenu();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (labels.isEmpty()) return false;
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        {
            showMenu();
            return true;
        }
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::leftKey)
        {
            setIndex(juce::jmax(0, index - 1), juce::sendNotification);
            return true;
        }
        if (key == juce::KeyPress::downKey || key == juce::KeyPress::rightKey)
        {
            setIndex(juce::jmin(labels.size() - 1, index + 1), juce::sendNotification);
            return true;
        }
        return false;
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
        auto r = getLocalBounds().toFloat();
        fillTallWell(g, r, isMouseOver() || hasKeyboardFocus(true));

        // §6 popup: accent chevron capsule 11×15, r3, white ▲▼.
        auto capsule = r.removeFromRight(15.0f)
                           .withSizeKeepingCentre(11.0f, juce::jmin(15.0f, r.getHeight() - 4.0f));
        g.setColour(ds::acc());
        g.fillRoundedRectangle(capsule, 3.0f);
        drawCaretUpDown(g, capsule, juce::Colours::white);

        auto textArea = r.reduced(8.0f, 0.0f);
        g.setColour(ds::tx());
        g.setFont(popupFont());
        const juce::String val = labels.size() > index ? labels[index] : juce::String();
        g.drawText(val, textArea.toNearestInt(), juce::Justification::centredLeft, false);

        if (hasKeyboardFocus(true))
            drawFocusRing(g, getLocalBounds().toFloat(), kTallRadius);
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
                g.setColour(hi ? juce::Colours::black : t.rowLabel);
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
        // Fit Half|Double into half a toolkit row (same as former select width).
        return kSelectHalfW;
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
            return selected == s ? juce::Colours::black : t.rowLabel;
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
        // Black text on accent fill for contrast.
        drawTexts(hot ? t.headerText : t.rowLabel, t.headerText);
        if (fill.getWidth() > 0.5f)
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(fill.getSmallestIntegerContainer());
            drawTexts(juce::Colours::black, juce::Colours::black);
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
        setWantsKeyboardFocus(true);
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

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!hasKeyboardFocus(true))
            grabKeyboardFocus();
        dragThumb = hitThumb(e.position.x);
        setFromX(e.position.x);
    }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent&) override { setRange(defLoV, defHiV); }

    bool keyPressed(const juce::KeyPress& key) override
    {
        const int step = key.getModifiers().isShiftDown() ? 5 : 1;
        if (key == juce::KeyPress::leftKey)
        {
            setRange(lo - step, hi - step);
            return true;
        }
        if (key == juce::KeyPress::rightKey)
        {
            setRange(lo + step, hi + step);
            return true;
        }
        if (key == juce::KeyPress::downKey)
        {
            setRange(lo, hi - step);
            return true;
        }
        if (key == juce::KeyPress::upKey)
        {
            setRange(lo, hi + step);
            return true;
        }
        return false;
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        const bool hot = isMouseOverOrDragging() || hasKeyboardFocus(true);
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
        if (hasKeyboardFocus(true))
            drawFocusRing(g, r, kTallRadius);

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
        // Black text on accent fill — higher contrast than white-on-blue.
        drawTexts(hot ? t.headerText : t.rowLabel, t.headerText);
        if (fill.getWidth() > 0.5f)
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(fill.getSmallestIntegerContainer());
            drawTexts(juce::Colours::black, juce::Colours::black);
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

// ── ChipGrid (2×N key / articulation / mode buttons) ─────────────────────────

class ChipGrid : public juce::Component
{
public:
    std::function<void()> onChange;
    bool multiSelect = false;
    int columns = 6;
    /** When true, chip width follows label text and rows wrap to available width. */
    bool fitContent = false;

    void setItems(juce::StringArray labelsIn)
    {
        labels = std::move(labelsIn);
        selected.assign((size_t) labels.size(), false);
        candidateMask = 0;
        layoutDirty = true;
        repaint();
        resized();
    }

    void setSelectedIndex(int idx, juce::NotificationType notify = juce::dontSendNotification)
    {
        if (labels.isEmpty()) return;
        selected.assign((size_t) labels.size(), false);
        if (juce::isPositiveAndBelow(idx, labels.size()))
            selected[(size_t) idx] = true;
        repaint();
        if (notify != juce::dontSendNotification && onChange) onChange();
    }

    void setSelectedMask(uint16_t mask, juce::NotificationType notify = juce::dontSendNotification)
    {
        selected.assign((size_t) labels.size(), false);
        for (int i = 0; i < labels.size() && i < 16; ++i)
            selected[(size_t) i] = (mask & (uint16_t) (1u << i)) != 0;
        repaint();
        if (notify != juce::dontSendNotification && onChange) onChange();
    }

    /** Soft-highlight chips that match clip analysis (does not change selection). */
    void setCandidateMask(uint16_t mask)
    {
        if (candidateMask == mask) return;
        candidateMask = mask;
        repaint();
    }

    int getSelectedIndex() const
    {
        for (int i = 0; i < (int) selected.size(); ++i)
            if (selected[(size_t) i]) return i;
        return -1;
    }

    uint16_t getSelectedMask() const
    {
        uint16_t m = 0;
        for (int i = 0; i < (int) selected.size() && i < 16; ++i)
            if (selected[(size_t) i]) m = (uint16_t) (m | (uint16_t) (1u << i));
        return m;
    }

    juce::String selectedSummary() const
    {
        juce::StringArray parts;
        for (int i = 0; i < labels.size(); ++i)
            if (i < (int) selected.size() && selected[(size_t) i])
                parts.add(labels[i]);
        if (!parts.isEmpty())
            return parts.joinIntoString(" ");
        // Fall back to candidate names when nothing is exclusively selected.
        for (int i = 0; i < labels.size() && i < 16; ++i)
            if ((candidateMask & (uint16_t) (1u << i)) != 0)
                parts.add(labels[i]);
        return parts.isEmpty() ? "Any" : parts.joinIntoString(" ");
    }

    int idealHeight() const
    {
        if (labels.isEmpty()) return kLabelH;
        if (!fitContent)
        {
            const int rows = (labels.size() + columns - 1) / juce::jmax(1, columns);
            return kLabelH + toolkitRowGap() + rows * kChipH + juce::jmax(0, rows - 1) * kChipGap;
        }
        // Prefer laid-out width; else estimate Toolkit content column width.
        const int w = juce::jmax(120, getWidth() > 10 ? getWidth()
                                    : metrics::effectsPaneWidth() - 2 * kContentPadX);
        return kLabelH + toolkitRowGap() + measureWrappedHeight(w);
    }

    juce::String title;

    void resized() override
    {
        layoutDirty = true;
        rebuildChipBounds();
    }

    void paint(juce::Graphics& g) override
    {
        rebuildChipBounds();
        const auto& t = inspectorTokens();
        auto header = getLocalBounds().removeFromTop(kLabelH);
        g.setFont(inspectorFont(true));
        g.setColour(t.rowLabel);
        g.drawText(title, header.removeFromLeft(header.getWidth() / 2),
                   juce::Justification::centredLeft, true);
        g.setFont(inspectorFont(true));
        g.setColour(t.accent);
        g.drawText(selectedSummary(), header, juce::Justification::centredRight, true);

        for (int i = 0; i < labels.size(); ++i)
        {
            if (i >= (int) chipBounds.size()) break;
            auto cell = chipBounds[(size_t) i].toFloat();
            const bool on = i < (int) selected.size() && selected[(size_t) i];
            const bool cand = !on && i < 16
                && (candidateMask & (uint16_t) (1u << i)) != 0;
            if (on)
                g.setColour(t.accent);
            else if (cand)
                g.setColour(t.accent.withAlpha(0.22f));
            else
                g.setColour(t.tallWell);
            g.fillRoundedRectangle(cell, 5.0f);
            if (cand)
            {
                g.setColour(t.accent.withAlpha(0.85f));
                g.drawRoundedRectangle(cell.reduced(0.5f), 5.0f, 1.4f);
            }
            else if (!on)
            {
                g.setColour(t.controlHairline);
                g.drawRoundedRectangle(cell.reduced(0.5f), 5.0f, 1.0f);
            }
            g.setFont(uiFontFixed(kAnnotPt, on));
            g.setColour(on ? juce::Colours::black
                           : (cand ? t.accent.brighter(0.15f) : t.headerText));
            g.drawText(labels[i], cell.toNearestInt(), juce::Justification::centred, false);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        rebuildChipBounds();
        const int idx = hitIndex(e.getPosition());
        if (idx < 0) return;
        if (multiSelect)
        {
            selected[(size_t) idx] = !selected[(size_t) idx];
        }
        else
        {
            selected.assign((size_t) labels.size(), false);
            selected[(size_t) idx] = true;
        }
        repaint();
        if (onChange) onChange();
    }

private:
    static constexpr int kLabelH = 18;
    static constexpr int kChipH = 26;
    static constexpr int kChipGap = 4;
    static constexpr int kChipPadX = 10;

    int chipWidthFor(int idx) const
    {
        const auto f = uiFontFixed(kAnnotPt, false);
        const float tw = juce::GlyphArrangement::getStringWidth(f, labels[idx]);
        return juce::jmax(28, (int) std::ceil(tw) + kChipPadX * 2);
    }

    int measureWrappedHeight(int width) const
    {
        if (labels.isEmpty() || width <= 0) return kChipH;
        int x = 0, rows = 1;
        for (int i = 0; i < labels.size(); ++i)
        {
            const int cw = chipWidthFor(i);
            if (x > 0 && x + cw > width)
            {
                ++rows;
                x = 0;
            }
            x += cw + kChipGap;
        }
        return rows * kChipH + juce::jmax(0, rows - 1) * kChipGap;
    }

    void rebuildChipBounds() const
    {
        if (!layoutDirty && (int) chipBounds.size() == labels.size())
            return;
        layoutDirty = false;
        chipBounds.assign((size_t) labels.size(), {});
        auto body = getLocalBounds().withTrimmedTop(kLabelH + toolkitRowGap());
        if (body.getWidth() <= 0 || labels.isEmpty())
            return;

        if (!fitContent)
        {
            const int cols = juce::jmax(1, columns);
            const float gap = (float) kChipGap;
            const float cellW = (body.getWidth() - gap * (float) (cols - 1)) / (float) cols;
            const float cellH = (float) kChipH;
            for (int i = 0; i < labels.size(); ++i)
            {
                const int r = i / cols, c = i % cols;
                chipBounds[(size_t) i] = juce::Rectangle<int>(
                    body.getX() + (int) ((float) c * (cellW + gap)),
                    body.getY() + r * (kChipH + kChipGap),
                    (int) cellW, (int) cellH);
            }
            return;
        }

        int x = body.getX(), y = body.getY();
        for (int i = 0; i < labels.size(); ++i)
        {
            const int cw = chipWidthFor(i);
            if (x > body.getX() && x + cw > body.getRight())
            {
                x = body.getX();
                y += kChipH + kChipGap;
            }
            chipBounds[(size_t) i] = { x, y, cw, kChipH };
            x += cw + kChipGap;
        }
    }

    int hitIndex(juce::Point<int> pos) const
    {
        for (int i = 0; i < (int) chipBounds.size(); ++i)
            if (chipBounds[(size_t) i].contains(pos))
                return i;
        return -1;
    }

    juce::StringArray labels;
    std::vector<bool> selected;
    uint16_t candidateMask = 0;
    mutable std::vector<juce::Rectangle<int>> chipBounds;
    mutable bool layoutDirty = true;
};

// ── SegmentedSelector (single-row pill group: octave / bars-style) ────────────

class SegmentedSelector : public juce::Component
{
public:
    std::function<void(int)> onChange;
    juce::String title;

    void setItems(juce::StringArray labelsIn, int selectedIdx = 0)
    {
        labels = std::move(labelsIn);
        index = juce::jlimit(0, juce::jmax(0, labels.size() - 1), selectedIdx);
        repaint();
    }

    void setIndex(int i, juce::NotificationType notify = juce::dontSendNotification)
    {
        i = juce::jlimit(0, juce::jmax(0, labels.size() - 1), i);
        if (i == index) return;
        index = i;
        repaint();
        if (notify != juce::dontSendNotification && onChange) onChange(index);
    }

    int getIndex() const { return index; }

    int idealHeight() const { return kLabelH + toolkitRowGap() + kTrackH; }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds();
        auto header = r.removeFromTop(kLabelH);
        g.setFont(inspectorFont(true));
        g.setColour(t.rowLabel);
        g.drawText(title, header, juce::Justification::centredLeft, true);

        r.removeFromTop(toolkitRowGap());
        auto track = r.removeFromTop(kTrackH).toFloat();
        g.setColour(t.tallWell);
        g.fillRoundedRectangle(track, track.getHeight() * 0.5f);
        g.setColour(t.controlHairline);
        g.drawRoundedRectangle(track.reduced(0.5f), track.getHeight() * 0.5f, 1.0f);

        if (labels.isEmpty()) return;
        const float cellW = track.getWidth() / (float) labels.size();
        for (int i = 0; i < labels.size(); ++i)
        {
            auto cell = juce::Rectangle<float>(track.getX() + cellW * (float) i,
                                               track.getY(), cellW, track.getHeight())
                            .reduced(2.0f, 2.0f);
            if (i == index)
            {
                g.setColour(t.accent);
                g.fillRoundedRectangle(cell, cell.getHeight() * 0.5f);
            }
            g.setFont(uiFontFixed(kAnnotPt, i == index));
            g.setColour(i == index ? juce::Colours::black : t.headerText);
            g.drawText(labels[i], cell.toNearestInt(), juce::Justification::centred, false);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto track = getLocalBounds().withTrimmedTop(kLabelH + toolkitRowGap()).removeFromTop(kTrackH);
        if (!track.contains(e.getPosition()) || labels.isEmpty()) return;
        const int i = juce::jlimit(0, labels.size() - 1,
            (int) ((float) (e.x - track.getX()) / (float) track.getWidth() * (float) labels.size()));
        setIndex(i, juce::sendNotification);
    }

private:
    static constexpr int kLabelH = 18;
    static constexpr int kTrackH = 28;
    juce::StringArray labels;
    int index = 0;
};

// ── HistRangeSlider (TEMPO-style histogram + dual thumbs) ─────────────────────

class HistRangeSlider : public juce::Component, public juce::SettableTooltipClient
{
public:
    HistRangeSlider(const juce::String& l, int mn, int mx)
        : label(l), minV(mn), maxV(mx), lo(mn), hi(mx), defLo(mn), defHi(mx)
    {
        setWantsKeyboardFocus(true);
        setRepaintsOnMouseActivity(true);
        bins.assign(16, 0);
    }

    std::function<void(int, int)> onChange;

    /** Visual histogram bins spanning [minV, maxV] evenly (any length). */
    void setHistogram(const std::vector<int>& counts)
    {
        bins = counts;
        if (bins.empty())
            bins.assign(16, 0);
        repaint();
    }

    /** Build ~`numBins` bars from integer samples in [minV, maxV]. */
    void setHistogramFromSamples(const std::vector<int>& samples, int numBins = 16)
    {
        numBins = juce::jlimit(4, 48, numBins);
        bins.assign((size_t) numBins, 0);
        const float span = (float) juce::jmax(1, maxV - minV);
        for (int s : samples)
        {
            const int v = juce::jlimit(minV, maxV, s);
            int idx = (int) ((float) (v - minV) / span * (float) numBins);
            idx = juce::jlimit(0, numBins - 1, idx);
            bins[(size_t) idx] += 1;
        }
        repaint();
    }

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
    bool isFullRange() const { return lo <= minV && hi >= maxV; }

    int idealHeight() const { return kLabelH + toolkitRowGap() + kHistH + 4 + kTrackH; }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!hasKeyboardFocus(true)) grabKeyboardFocus();
        dragThumb = hitThumb(e.position.x);
        setFromX(e.position.x);
    }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent&) override { setRange(defLo, defHi); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds();
        auto header = r.removeFromTop(kLabelH);
        g.setFont(inspectorFont(true));
        g.setColour(t.rowLabel);
        g.drawText(label, header.removeFromLeft(header.getWidth() / 2),
                   juce::Justification::centredLeft, true);
        g.setColour(t.accent);
        g.setFont(inspectorMono(true));
        const juce::String val = isFullRange() ? "Any"
            : (juce::String(lo) + juce::String::charToString((juce::juce_wchar) 0x2013) + juce::String(hi));
        g.drawText(val, header, juce::Justification::centredRight, true);

        r.removeFromTop(toolkitRowGap());
        auto hist = r.removeFromTop(kHistH).toFloat();
        const int n = juce::jmax(1, (int) bins.size());
        int peak = 1;
        for (int c : bins) peak = juce::jmax(peak, c);
        const float gap = 1.5f;
        const float barW = juce::jmax(2.0f, (hist.getWidth() - gap * (float) (n - 1)) / (float) n);
        const float span = (float) juce::jmax(1, maxV - minV);
        for (int i = 0; i < n; ++i)
        {
            const int binLo = minV + (int) std::floor((float) i / (float) n * span);
            const int binHi = minV + (int) std::floor((float) (i + 1) / (float) n * span) - 1;
            const bool inRange = binHi >= lo && binLo <= hi;
            const float h = hist.getHeight() * ((float) bins[(size_t) i] / (float) peak);
            auto bar = juce::Rectangle<float>(
                hist.getX() + (float) i * (barW + gap),
                hist.getBottom() - juce::jmax(2.0f, h),
                barW, juce::jmax(2.0f, h));
            g.setColour(inRange ? t.accent : t.tallFill);
            g.fillRoundedRectangle(bar, 1.5f);
        }

        r.removeFromTop(4);
        auto track = r.removeFromTop(kTrackH).toFloat().reduced(0.0f, 6.0f);
        g.setColour(t.tallFill);
        g.fillRoundedRectangle(track, track.getHeight() * 0.5f);
        const float trackSpan = (float) juce::jmax(1, maxV - minV);
        const float xLo = track.getX() + track.getWidth() * ((float) (lo - minV) / trackSpan);
        const float xHi = track.getX() + track.getWidth() * ((float) (hi - minV) / trackSpan);
        auto sel = juce::Rectangle<float>::leftTopRightBottom(xLo, track.getY(), xHi, track.getBottom());
        g.setColour(t.accent);
        g.fillRoundedRectangle(sel, track.getHeight() * 0.5f);
        const float th = track.getHeight() + 6.0f;
        g.setColour(juce::Colours::white);
        g.fillEllipse(xLo - th * 0.5f, track.getCentreY() - th * 0.5f, th, th);
        g.fillEllipse(xHi - th * 0.5f, track.getCentreY() - th * 0.5f, th, th);
        g.setColour(t.controlHairline);
        g.drawEllipse(xLo - th * 0.5f, track.getCentreY() - th * 0.5f, th, th, 0.8f);
        g.drawEllipse(xHi - th * 0.5f, track.getCentreY() - th * 0.5f, th, th, 0.8f);
    }

private:
    static constexpr int kLabelH = 18;
    static constexpr int kHistH = 16;
    static constexpr int kTrackH = 20;
    enum class Thumb { None, Lo, Hi };
    Thumb dragThumb = Thumb::None;

    Thumb hitThumb(float x) const
    {
        const float span = (float) juce::jmax(1, maxV - minV);
        const float w = (float) juce::jmax(1, getWidth());
        const float xLo = w * ((float) (lo - minV) / span);
        const float xHi = w * ((float) (hi - minV) / span);
        return std::abs(x - xLo) <= std::abs(x - xHi) ? Thumb::Lo : Thumb::Hi;
    }

    void setFromX(float x)
    {
        const float t = juce::jlimit(0.0f, 1.0f, x / (float) juce::jmax(1, getWidth()));
        const int v = minV + (int) std::lround(t * (float) (maxV - minV));
        if (dragThumb == Thumb::Lo) setRange(juce::jmin(v, hi), hi);
        else setRange(lo, juce::jmax(v, lo));
    }

    juce::String label;
    int minV, maxV, lo, hi, defLo, defHi;
    std::vector<int> bins;
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

    void setScaleContext(int rootPc, Mode mode)
    {
        scaleRoot = rootPc;
        scaleMode = mode;
        repaint();
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
    /**
     * Three clearly separated states (ds tokens only):
     * 1) on + in scale  — kw/kb + accent wash
     * 2) on + out of scale — tx3 / trk
     * 3) off — panel/ctl + slash
     */
    static juce::Colour activeWhite()   { return ds::kw(); }
    static juce::Colour activeBlack()   { return ds::kb(); }
    static juce::Colour outScaleWhite() { return ds::tx3(); }
    static juce::Colour outScaleBlack() { return ds::trk(); }
    static juce::Colour offWhiteKey()   { return ds::panel(); }
    static juce::Colour offBlackKey()   { return ds::ctl(); }

    static void paintOffSlash(juce::Graphics& g, juce::Rectangle<float> key)
    {
        g.setColour(ds::tx3().withAlpha(0.55f));
        const float inset = juce::jmin(4.0f, key.getWidth() * 0.18f);
        g.drawLine(key.getX() + inset, key.getBottom() - inset,
                   key.getRight() - inset, key.getY() + inset, 1.4f);
    }

    void paintKeyboard(juce::Graphics& g, juce::Rectangle<float> area) const
    {
        if (area.getWidth() < 8.0f || area.getHeight() < 8.0f)
            return;

        static constexpr int kWhitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr int kBlackPc[5] = { 1, 3, 6, 8, 10 };
        static constexpr int kBlackAfterWhite[5] = { 0, 1, 3, 4, 5 };

        const float whiteW = area.getWidth() / 7.0f;
        const float whiteH = area.getHeight();
        const float blackW = whiteW * 0.62f;
        const float blackH = whiteH * 0.58f;
        const auto& t = inspectorTokens();

        auto enabled = [this](int pc)
        {
            return (noteFilterMask & (uint16_t) (1u << pc)) != 0;
        };
        auto inScale = [this](int pc)
        {
            if (scaleRoot < 0) return true;
            return pitchInScale(pc, scaleRoot, scaleMode);
        };

        for (int i = 0; i < 7; ++i)
        {
            const int pc = kWhitePc[i];
            auto key = juce::Rectangle<float>(area.getX() + whiteW * (float) i,
                                              area.getY(), whiteW - 1.0f, whiteH);
            const bool on = enabled(pc);
            const bool scale = inScale(pc);
            if (!on)
                g.setColour(offWhiteKey());
            else if (scale)
                g.setColour(activeWhite());
            else
                g.setColour(outScaleWhite());
            g.fillRoundedRectangle(key, 2.0f);
            if (on && scale)
            {
                g.setColour(t.accent.withAlpha(0.45f));
                g.fillRoundedRectangle(key.reduced(1.0f), 2.0f);
            }
            if (!on)
                paintOffSlash(g, key);
            g.setColour(on ? (scale ? t.accent.withAlpha(0.55f)
                                    : ds::tx3().withAlpha(0.70f))
                           : ds::trk());
            g.drawRoundedRectangle(key.reduced(0.5f), 2.0f, on ? 1.0f : 1.2f);
        }

        for (int i = 0; i < 5; ++i)
        {
            const int pc = kBlackPc[i];
            const float cx = area.getX() + whiteW * ((float) kBlackAfterWhite[i] + 1.0f);
            auto key = juce::Rectangle<float>(cx - blackW * 0.5f, area.getY(), blackW, blackH);
            const bool on = enabled(pc);
            const bool scale = inScale(pc);
            if (!on)
                g.setColour(offBlackKey());
            else if (scale)
                g.setColour(activeBlack());
            else
                g.setColour(outScaleBlack());
            g.fillRoundedRectangle(key, 2.0f);
            if (on && scale)
            {
                g.setColour(t.accent.withAlpha(0.55f));
                g.fillRoundedRectangle(key.reduced(1.0f), 2.0f);
            }
            if (!on)
                paintOffSlash(g, key);
            g.setColour(on ? (scale ? t.accent.withAlpha(0.65f)
                                    : ds::tx2().withAlpha(0.45f))
                           : ds::tx3().withAlpha(0.70f));
            g.drawRoundedRectangle(key.reduced(0.5f), 2.0f, on ? 1.0f : 1.2f);
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
    int scaleRoot = -1;
    Mode scaleMode = Mode::Ionian;
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
        r.removeFromLeft(kContentPadX);
        r.removeFromRight(kContentPadX);
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
