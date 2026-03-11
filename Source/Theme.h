#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace pflow {

// ── Colour palette ───────────────────────────────────────────────────────────
namespace colours {
    const juce::Colour bg            { 0xff1a1a2e };
    const juce::Colour bgLight       { 0xff222240 };
    const juce::Colour bgLighter     { 0xff2a2a50 };
    const juce::Colour panel         { 0xff16162b };
    const juce::Colour panelBorder   { 0xff333360 };
    const juce::Colour accent        { 0xff6c63ff };
    const juce::Colour accentDim     { 0xff4e47b3 };
    const juce::Colour accentBright  { 0xff8a83ff };
    const juce::Colour text          { 0xffe0e0e8 };
    const juce::Colour textDim       { 0xff8888a0 };
    const juce::Colour textBright    { 0xffffffff };
    const juce::Colour knobTrack     { 0xff3a3a60 };
    const juce::Colour laneA         { 0xff3a7bd5 };
    const juce::Colour laneB         { 0xff00d2ff };
    const juce::Colour laneC         { 0xffff6b6b };
    const juce::Colour laneD         { 0xff51cf66 };
    const juce::Colour laneE         { 0xfffcc419 };
    const juce::Colour pianoWhiteKey { 0xff2c2c4a };
    const juce::Colour pianoBlackKey { 0xff1a1a30 };
    const juce::Colour pianoGrid     { 0xff333355 };
    const juce::Colour noteBlock     { 0xff6c63ff };
    const juce::Colour selection     { 0x446c63ff };
    const juce::Colour playhead      { 0xffffffff };
}

// ── Clip colour presets ──────────────────────────────────────────────────────
inline std::vector<juce::Colour> getClipColourPresets()
{
    return {
        juce::Colour(0xff3a7bd5), juce::Colour(0xff00d2ff),
        juce::Colour(0xffff6b6b), juce::Colour(0xff51cf66),
        juce::Colour(0xfffcc419), juce::Colour(0xffcc5de8),
        juce::Colour(0xffff922b), juce::Colour(0xff20c997),
        juce::Colour(0xfff06595), juce::Colour(0xff748ffc),
    };
}

// ── Metrics ──────────────────────────────────────────────────────────────────
namespace metrics {
    constexpr int browserWidth      = 220;
    constexpr int controlPanelH     = 52;
    constexpr int pianoRollH        = 260;
    constexpr int laneHeaderW       = 100;
    constexpr int laneHeight        = 48;
    constexpr int compLaneHeight    = 32;
    constexpr int knobSize          = 36;
    constexpr int knobLabelH        = 14;
    constexpr int knobSpacing       = 52;
    constexpr float cornerRadius    = 4.0f;
    constexpr float clipCorner      = 3.0f;
    constexpr int scrollbarW        = 8;
    constexpr int buttonH           = 24;
    constexpr int padding           = 6;
}

// ── Custom LookAndFeel ───────────────────────────────────────────────────────
class PatternFlowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PatternFlowLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& bg,
                              bool highlighted, bool down) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;

    juce::Font getLabelFont(juce::Label&) override;
};

} // namespace pflow
