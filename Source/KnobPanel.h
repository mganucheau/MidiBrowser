#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "GrooveEngine.h"
#include "UiAtoms.h"

namespace pflow {

// ── Rotary groove knob ───────────────────────────────────────────────────────
// ~270° sweep. Drag vertically (or arrow keys) to change; double-click resets
// to default. Bipolar knobs (min < 0) fill their arc from the center.

class GrooveKnob : public juce::Component
{
public:
    explicit GrooveKnob(const KnobDef& def);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

    void setValue(int v, juce::NotificationType notify = juce::sendNotification);
    int getValue() const { return value; }
    const KnobDef& definition() const { return def; }

    std::function<void(int)> onChange;

    static constexpr int knobSize = 48;
    static constexpr int totalW = 64;
    static constexpr int totalH = 86;   // knob + label + readout

private:
    KnobDef def;
    int value;
    int dragStartValue = 0;
    float dragStartY = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrooveKnob)
};

// ── Folding "Groove" footer panel ────────────────────────────────────────────
// Collapsed: single 32px row naming the six knobs. Expanded: wrapping knob row.

class KnobsPanel : public juce::Component
{
public:
    KnobsPanel();

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

    void setParams(const GrooveParams&, juce::NotificationType notify = juce::dontSendNotification);
    GrooveParams getParams() const { return params; }
    bool isOpen() const { return open; }
    void setOpen(bool shouldOpen);
    int idealHeight() const;

    std::function<void(const GrooveParams&)> onParamsChanged;
    std::function<void()> onOpenChanged;

    static constexpr int headerH = 32;

private:
    void pushParams();

    GrooveParams params;
    bool open = true;
    std::array<std::unique_ptr<GrooveKnob>, kNumKnobs> knobs;
    IconBtn btnReset { icons::undo, "Reset groove" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobsPanel)
};

} // namespace pflow
