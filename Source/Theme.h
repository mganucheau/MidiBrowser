#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace pflow {

// ── High contrast mode toggle ────────────────────────────────────────────────
inline std::atomic<bool>& highContrastEnabled()
{
    static std::atomic<bool> enabled { false };
    return enabled;
}

// ── Colour palette ───────────────────────────────────────────────────────────
// High contrast palette: black background, bright saturated colours, strong borders
namespace colours {
    inline juce::Colour bg()            { return highContrastEnabled() ? juce::Colour(0xff000000) : juce::Colour(0xff1a1a2e); }
    inline juce::Colour bgLight()       { return highContrastEnabled() ? juce::Colour(0xff111111) : juce::Colour(0xff222240); }
    inline juce::Colour bgLighter()     { return highContrastEnabled() ? juce::Colour(0xff1a1a1a) : juce::Colour(0xff2a2a50); }
    inline juce::Colour panel()         { return highContrastEnabled() ? juce::Colour(0xff0a0a0a) : juce::Colour(0xff16162b); }
    inline juce::Colour panelBorder()   { return highContrastEnabled() ? juce::Colour(0xffaaaaaa) : juce::Colour(0xff333360); }
    inline juce::Colour accent()        { return highContrastEnabled() ? juce::Colour(0xff44aaff) : juce::Colour(0xff6c63ff); }
    inline juce::Colour accentDim()     { return highContrastEnabled() ? juce::Colour(0xff2288dd) : juce::Colour(0xff4e47b3); }
    inline juce::Colour accentBright()  { return highContrastEnabled() ? juce::Colour(0xff66ccff) : juce::Colour(0xff8a83ff); }
    inline juce::Colour text()          { return highContrastEnabled() ? juce::Colour(0xffffffff) : juce::Colour(0xffe0e0e8); }
    inline juce::Colour textDim()       { return highContrastEnabled() ? juce::Colour(0xffcccccc) : juce::Colour(0xffa0a0bb); }
    inline juce::Colour textBright()    { return juce::Colour(0xffffffff); }
    inline juce::Colour knobTrack()     { return highContrastEnabled() ? juce::Colour(0xff555555) : juce::Colour(0xff3a3a60); }
    inline juce::Colour laneA()         { return juce::Colour(0xff3a7bd5); }
    inline juce::Colour laneB()         { return juce::Colour(0xff00d2ff); }
    inline juce::Colour laneC()         { return juce::Colour(0xffff6b6b); }
    inline juce::Colour laneD()         { return juce::Colour(0xff51cf66); }
    inline juce::Colour laneE()         { return juce::Colour(0xfffcc419); }
    inline juce::Colour pianoWhiteKey() { return highContrastEnabled() ? juce::Colour(0xff222222) : juce::Colour(0xff2c2c4a); }
    inline juce::Colour pianoBlackKey() { return highContrastEnabled() ? juce::Colour(0xff0d0d0d) : juce::Colour(0xff1a1a30); }
    inline juce::Colour pianoGrid()     { return highContrastEnabled() ? juce::Colour(0xff666666) : juce::Colour(0xff333355); }
    inline juce::Colour noteBlock()     { return highContrastEnabled() ? juce::Colour(0xff44aaff) : juce::Colour(0xff6c63ff); }
    inline juce::Colour selection()     { return highContrastEnabled() ? juce::Colour(0x6644aaff) : juce::Colour(0x446c63ff); }
    inline juce::Colour playhead()      { return highContrastEnabled() ? juce::Colour(0xffffff00) : juce::Colour(0xffffffff); }
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
// ── Version ─────────────────────────────────────────────────────────────────
namespace version {
    constexpr const char* number = "0.2.0";
    constexpr const char* name   = "PatternFlow";
    constexpr const char* desc   = "A MIDI composition and comping tool for creative producers. "
                                   "Arrange, layer, and reshape MIDI clips with per-note comping, "
                                   "scale quantisation, humanisation, and flexible routing.";
    constexpr const char* license =
        "Commercial License\n\n"
        "Copyright (c) 2024-2026 PatternFlow. All rights reserved.\n\n"
        "This software is licensed, not sold. You are granted a non-exclusive, "
        "non-transferable license to use this software for personal and commercial "
        "music production. Redistribution, reverse engineering, or modification of "
        "the software is prohibited without prior written consent from the author.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND.";
}

namespace metrics {
    constexpr int browserWidth      = 240;
    constexpr int controlPanelH     = 60;
    constexpr int pianoRollH        = 280;
    constexpr int laneHeaderW       = 120;
    constexpr int laneHeight        = 64;
    constexpr int compLaneHeight    = 40;
    constexpr int knobSize          = 42;
    constexpr int knobLabelH        = 16;
    constexpr int knobSpacing       = 58;
    constexpr float cornerRadius    = 4.0f;
    constexpr float clipCorner      = 3.0f;
    constexpr int scrollbarW        = 8;
    constexpr int buttonH           = 28;
    constexpr int padding           = 8;
    constexpr float browserFontSize = 11.0f;
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
