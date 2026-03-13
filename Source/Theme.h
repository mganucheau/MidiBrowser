#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace pflow {

// ── Dark mode toggle (false = light mode default) ───────────────────────────
inline std::atomic<bool>& darkModeEnabled()
{
    static std::atomic<bool> enabled { false };
    return enabled;
}

// ── Colour palette ───────────────────────────────────────────────────────────
// Light mode: clean white/gray. Dark mode: standard dark gray palette.
namespace colours {
    // Backgrounds
    inline juce::Colour bg()            { return darkModeEnabled() ? juce::Colour(0xff1e1e1e) : juce::Colour(0xfff5f5f5); }
    inline juce::Colour bgLight()       { return darkModeEnabled() ? juce::Colour(0xff2d2d2d) : juce::Colour(0xffffffff); }
    inline juce::Colour bgLighter()     { return darkModeEnabled() ? juce::Colour(0xff383838) : juce::Colour(0xffe8e8e8); }
    inline juce::Colour panel()         { return darkModeEnabled() ? juce::Colour(0xff252525) : juce::Colour(0xfffafafa); }
    inline juce::Colour panelBorder()   { return darkModeEnabled() ? juce::Colour(0xff444444) : juce::Colour(0xffd0d0d0); }

    // Accent (blue)
    inline juce::Colour accent()        { return darkModeEnabled() ? juce::Colour(0xff5b9bd5) : juce::Colour(0xff2979ff); }
    inline juce::Colour accentDim()     { return darkModeEnabled() ? juce::Colour(0xff3d7ab8) : juce::Colour(0xff1565c0); }
    inline juce::Colour accentBright()  { return darkModeEnabled() ? juce::Colour(0xff7ec8ff) : juce::Colour(0xff448aff); }

    // Text
    inline juce::Colour text()          { return darkModeEnabled() ? juce::Colour(0xffe0e0e0) : juce::Colour(0xff212121); }
    inline juce::Colour textDim()       { return darkModeEnabled() ? juce::Colour(0xff999999) : juce::Colour(0xff757575); }
    inline juce::Colour textBright()    { return darkModeEnabled() ? juce::Colour(0xffffffff) : juce::Colour(0xff000000); }

    // Knob
    inline juce::Colour knobTrack()     { return darkModeEnabled() ? juce::Colour(0xff484848) : juce::Colour(0xffc8c8c8); }

    // Lane colours
    inline juce::Colour laneA()         { return juce::Colour(0xff2979ff); }
    inline juce::Colour laneB()         { return juce::Colour(0xff00bcd4); }
    inline juce::Colour laneC()         { return juce::Colour(0xffef5350); }
    inline juce::Colour laneD()         { return juce::Colour(0xff66bb6a); }
    inline juce::Colour laneE()         { return juce::Colour(0xfffdd835); }

    // Piano roll
    inline juce::Colour pianoWhiteKey() { return darkModeEnabled() ? juce::Colour(0xff353535) : juce::Colour(0xfff0f0f0); }
    inline juce::Colour pianoBlackKey() { return darkModeEnabled() ? juce::Colour(0xff222222) : juce::Colour(0xffd8d8d8); }
    inline juce::Colour pianoGrid()     { return darkModeEnabled() ? juce::Colour(0xff555555) : juce::Colour(0xffbbbbbb); }
    inline juce::Colour noteBlock()     { return darkModeEnabled() ? juce::Colour(0xff5b9bd5) : juce::Colour(0xff2979ff); }
    inline juce::Colour selection()     { return darkModeEnabled() ? juce::Colour(0x665b9bd5) : juce::Colour(0x662979ff); }
    inline juce::Colour playhead()      { return darkModeEnabled() ? juce::Colour(0xffffffff) : juce::Colour(0xffff1744); }
}

// ── Clip colour presets ──────────────────────────────────────────────────────
inline std::vector<juce::Colour> getClipColourPresets()
{
    return {
        juce::Colour(0xff2979ff), juce::Colour(0xff00bcd4),
        juce::Colour(0xffef5350), juce::Colour(0xff66bb6a),
        juce::Colour(0xfffdd835), juce::Colour(0xffab47bc),
        juce::Colour(0xffff7043), juce::Colour(0xff26a69a),
        juce::Colour(0xffec407a), juce::Colour(0xff5c6bc0),
    };
}

// ── Version ─────────────────────────────────────────────────────────────────
namespace version {
    constexpr const char* number = "0.3.0";
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

// ── Metrics ──────────────────────────────────────────────────────────────────
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
