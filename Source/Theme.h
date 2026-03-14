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
// ── Core colour theme ────────────────────────────────────────────────────────
//
//  LIGHT MODE                          DARK MODE
//  ──────────────────────────          ──────────────────────────
//  Background:                         Background:
//    bg           #EAEEF2                bg           #1A1D21
//    bgLight      #F5F7FA                bgLight      #24272C
//    bgLighter    #DDE3E9                bgLighter    #32373E
//    panel        #F0F2F5                panel        #1F2227
//    panelBorder  #BFC6CE                panelBorder  #3E444C
//
//  Accent (blue):                      Accent (blue):
//    accent       #2563EB                accent       #5B9BD5
//    accentDim    #1D4ED8                accentDim    #3D7AB8
//    accentBright #3B82F6                accentBright #7EC8FF
//
//  Text:                               Text:
//    text         #1E293B                text         #E0E4E8
//    textDim      #4B5563                textDim      #9CA3AF
//    textBright   #0F172A                textBright   #FFFFFF
//
//  Knob:                               Knob:
//    knobTrack    #C3CAD2                knobTrack    #484D54
//
//  Piano roll:                         Piano roll:
//    pianoWhite   #EDF0F3                pianoWhite   #353535
//    pianoBlack   #D0D6DD                pianoBlack   #222222
//    pianoGrid    #A8B0B9                pianoGrid    #555555
//    noteBlock    #2563EB                noteBlock    #5B9BD5
//    selection    #2563EB 40%            selection    #5B9BD5 40%
//    playhead     #DC2626                playhead     #FFFFFF
//
namespace colours {
    // Backgrounds
    inline juce::Colour bg()            { return darkModeEnabled() ? juce::Colour(0xff1a1d21) : juce::Colour(0xffeaeef2); }
    inline juce::Colour bgLight()       { return darkModeEnabled() ? juce::Colour(0xff24272c) : juce::Colour(0xfff5f7fa); }
    inline juce::Colour bgLighter()     { return darkModeEnabled() ? juce::Colour(0xff32373e) : juce::Colour(0xffdde3e9); }
    inline juce::Colour panel()         { return darkModeEnabled() ? juce::Colour(0xff1f2227) : juce::Colour(0xfff0f2f5); }
    inline juce::Colour panelBorder()   { return darkModeEnabled() ? juce::Colour(0xff3e444c) : juce::Colour(0xffbfc6ce); }

    // Accent (blue)
    inline juce::Colour accent()        { return darkModeEnabled() ? juce::Colour(0xff5b9bd5) : juce::Colour(0xff2563eb); }
    inline juce::Colour accentDim()     { return darkModeEnabled() ? juce::Colour(0xff3d7ab8) : juce::Colour(0xff1d4ed8); }
    inline juce::Colour accentBright()  { return darkModeEnabled() ? juce::Colour(0xff7ec8ff) : juce::Colour(0xff3b82f6); }

    // Text
    inline juce::Colour text()          { return darkModeEnabled() ? juce::Colour(0xffe0e4e8) : juce::Colour(0xff1e293b); }
    inline juce::Colour textDim()       { return darkModeEnabled() ? juce::Colour(0xff9ca3af) : juce::Colour(0xff4b5563); }
    inline juce::Colour textBright()    { return darkModeEnabled() ? juce::Colour(0xffffffff) : juce::Colour(0xff0f172a); }

    // Knob
    inline juce::Colour knobTrack()     { return darkModeEnabled() ? juce::Colour(0xff484d54) : juce::Colour(0xffc3cad2); }

    // Lane colours
    inline juce::Colour laneA()         { return juce::Colour(0xff2563eb); }
    inline juce::Colour laneB()         { return juce::Colour(0xff0891b2); }
    inline juce::Colour laneC()         { return juce::Colour(0xffdc2626); }
    inline juce::Colour laneD()         { return juce::Colour(0xff16a34a); }
    inline juce::Colour laneE()         { return juce::Colour(0xffeab308); }

    // Piano roll
    inline juce::Colour pianoWhiteKey() { return darkModeEnabled() ? juce::Colour(0xff353535) : juce::Colour(0xffedf0f3); }
    inline juce::Colour pianoBlackKey() { return darkModeEnabled() ? juce::Colour(0xff222222) : juce::Colour(0xffd0d6dd); }
    inline juce::Colour pianoGrid()     { return darkModeEnabled() ? juce::Colour(0xff555555) : juce::Colour(0xffa8b0b9); }
    inline juce::Colour noteBlock()     { return darkModeEnabled() ? juce::Colour(0xff5b9bd5) : juce::Colour(0xff2563eb); }
    inline juce::Colour selection()     { return darkModeEnabled() ? juce::Colour(0x665b9bd5) : juce::Colour(0x662563eb); }
    inline juce::Colour playhead()      { return darkModeEnabled() ? juce::Colour(0xffffffff) : juce::Colour(0xffdc2626); }
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
    constexpr const char* number = "0.4.0";
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
    constexpr int controlPanelH     = 90;
    constexpr int pianoRollH        = 280;
    constexpr int laneHeaderW       = 120;
    constexpr int laneHeight        = 64;
    constexpr int compLaneHeight    = 40;
    constexpr int knobSize          = 56;
    constexpr int knobLabelH        = 16;
    constexpr int knobSpacing       = 72;
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
    void refreshColours();

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
