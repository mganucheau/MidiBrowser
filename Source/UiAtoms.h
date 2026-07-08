#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace pflow {

// ── Icon glyphs (Phosphor-style, drawn as paths) ─────────────────────────────

namespace icons {
    inline constexpr const char* play          = "play";
    inline constexpr const char* pause         = "pause";
    inline constexpr const char* stop          = "stop";
    inline constexpr const char* caretUp       = "caret-up";
    inline constexpr const char* caretDown     = "caret-down";
    inline constexpr const char* caretLeft     = "caret-left";
    inline constexpr const char* caretRight    = "caret-right";
    inline constexpr const char* arrowsIn      = "arrows-in-line-horizontal";
    inline constexpr const char* arrowsOut     = "arrows-out-line-horizontal";
    inline constexpr const char* scissors      = "scissors";
    inline constexpr const char* sliders       = "sliders";
    inline constexpr const char* folder        = "folder";
    inline constexpr const char* folderOpen    = "folder-open";
    inline constexpr const char* star          = "star";
    inline constexpr const char* plus          = "plus";
    inline constexpr const char* minus         = "minus";
    inline constexpr const char* x             = "x";
    inline constexpr const char* undo          = "arrow-counter-clockwise";
    inline constexpr const char* zoomIn        = "zoom-in";
    inline constexpr const char* zoomOut       = "zoom-out";
    inline constexpr const char* noteBass      = "wave";        // bass clips
    inline constexpr const char* noteKeys      = "piano-keys";  // keys clips
    inline constexpr const char* noteDrums     = "drum";        // drum clips
    inline constexpr const char* sidebar       = "sidebar";
    inline constexpr const char* gear          = "gear";
    inline constexpr const char* lockOpen      = "lock-open";
    inline constexpr const char* lockClosed    = "lock-closed";
    inline constexpr const char* foldRows      = "fold-rows";
}

/** Stroke-drawn icon centered in bounds. `px` is the stroke width. */
void drawIcon(juce::Graphics& g, const juce::String& name,
              juce::Rectangle<float> bounds, juce::Colour colour, float px = 1.6f);

// ── Small controls ───────────────────────────────────────────────────────────

/** Square icon button. Optional `active` accent state. */
class IconBtn : public juce::Button
{
public:
    explicit IconBtn(const juce::String& iconName, const juce::String& tip = {});
    void paintButton(juce::Graphics&, bool over, bool down) override;

    juce::String icon;
    bool active = false;       // accent fill
    bool ghost = true;         // no background when idle
    float iconScale = 1.0f;
};

/** Rounded chip with optional leading icon; used for Trim, badges, Editor btn. */
class ChipBtn : public juce::Button
{
public:
    explicit ChipBtn(const juce::String& text, const juce::String& iconName = {});
    void paintButton(juce::Graphics&, bool over, bool down) override;
    int idealWidth() const;

    juce::String label;
    juce::String icon;         // leading icon
    juce::String trailingIcon; // e.g. removable “x”
    bool active = false;       // accent style
    bool accentText = false;   // accent-tinted text/icon when inactive
    bool mono = false;
};

/** Pill toggle with a status dot (DAW-sync). */
class PillToggle : public juce::Button
{
public:
    PillToggle(const juce::String& onText, const juce::String& offText);
    void paintButton(juce::Graphics&, bool over, bool down) override;
    int idealWidth() const;

    juce::String onLabel, offLabel;
};

/** Labelled mini switch (Fit to scale / Map to root): caption · track · value
    laid out on one row. */
class MiniSwitch : public juce::Button
{
public:
    explicit MiniSwitch(const juce::String& caption);
    void paintButton(juce::Graphics&, bool over, bool down) override;
    int idealWidth() const;

    juce::String caption;          // control name, left of the track
    juce::String onText { "on" };  // value text when on
    juce::String offText { "off" };
};

/** − value + stepper (octave). */
class Stepper : public juce::Component
{
public:
    Stepper();
    void resized() override;
    void paint(juce::Graphics&) override;

    void setValue(int v, juce::NotificationType notify = juce::sendNotification);
    int getValue() const { return value; }
    int minValue = -3, maxValue = 3;
    std::function<void(int)> onChange;
    std::function<juce::String(int)> format;

private:
    int value = 0;
    IconBtn btnDown { icons::minus }, btnUp { icons::plus };
};

} // namespace pflow
