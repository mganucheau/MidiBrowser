#pragma once
#include <array>
#include <vector>

#include "EditModel.h"

namespace pflow {

// ── Groove engine ────────────────────────────────────────────────────────────
// Non-destructive timing/velocity transform over resolveClip() output.
// Modelled on Ableton Groove Pool + Logic Q-Swing / Humanize conventions:
//
//   Swing     — delay offbeats at a chosen base (1/8 or 1/16); 0 = straight
//   Pocket    — tight<->loose feel (- push / + laid-back); strong beats move less
//   Humanize  — deterministic per-note timing jitter (Ableton Random)
//   Dynamics  — metric accent contrast; bipolar (- inverts, like Ableton Velocity)
//   Length    — note duration scale; 100 = unchanged (center default)
//   Intensity — velocity scale; 100 = file velocities unchanged (center default)
//   Quantize  — snap starts toward a grid (incl. triplets); strength 0 = off
//   Articulation — phrasing shape (length/velocity); strength 0 = off
//   Sustain   — pedal automation for export/preview

struct KnobDef
{
    const char* key;
    const char* label;
    int min, max, def;
    /** Arc fills from 0 (bipolar knobs). */
    bool bipolar() const { return min < 0; }
    /** Arc fills from def when def sits mid-range (Length, Intensity). */
    bool fillFromDefault() const { return !bipolar() && def > min && def < max; }
};

enum class SwingBase { Eighth = 0, Sixteenth = 1 };

/** Quantize grid labels: 1/4, 1/4t, 1/8, 1/8t, 1/16, 1/16t, 1/32, 1/32t. */
enum class QuantizeGrid : int
{
    Quarter = 0,
    QuarterT,
    Eighth,
    EighthT,
    Sixteenth,
    SixteenthT,
    ThirtySecond,
    ThirtySecondT,
    Count
};

enum class Articulation : int
{
    Legato = 0,
    Staccato,
    Tenuto,
    Pianissimo,
    Sforzando,
    Slur,
    Count
};

enum class SustainPedalMode : int
{
    Off = 0,
    StartOfClip,
    Auto,
    Count
};

constexpr int kNumKnobs = 6;
extern const std::array<KnobDef, kNumKnobs> kKnobDefs;

inline const char* quantizeGridLabel(QuantizeGrid g)
{
    switch (g)
    {
        case QuantizeGrid::Quarter:       return "1/4";
        case QuantizeGrid::QuarterT:      return "1/4t";
        case QuantizeGrid::Eighth:        return "1/8";
        case QuantizeGrid::EighthT:       return "1/8t";
        case QuantizeGrid::Sixteenth:     return "1/16";
        case QuantizeGrid::SixteenthT:    return "1/16t";
        case QuantizeGrid::ThirtySecond:  return "1/32";
        case QuantizeGrid::ThirtySecondT: return "1/32t";
        case QuantizeGrid::Count:         break;
    }
    return "1/8";
}

inline const char* articulationLabel(Articulation a)
{
    switch (a)
    {
        case Articulation::Legato:     return "Legato";
        case Articulation::Staccato:   return "Staccato";
        case Articulation::Tenuto:     return "Tenuto";
        case Articulation::Pianissimo: return "Pianissimo";
        case Articulation::Sforzando:  return "Sforzando";
        case Articulation::Slur:       return "Slur";
        case Articulation::Count:      break;
    }
    return "Legato";
}

inline const char* sustainPedalLabel(SustainPedalMode m)
{
    switch (m)
    {
        case SustainPedalMode::Off:         return "Off";
        case SustainPedalMode::StartOfClip: return "Start of Clip";
        case SustainPedalMode::Auto:        return "Auto";
        case SustainPedalMode::Count:       break;
    }
    return "Off";
}

struct GrooveParams
{
    int swing     = 0;      // 0..100     offbeat delay amount
    int pocket    = 0;      // -100..100  - push / + lay back (metric-weighted)
    int humanize  = 0;      // 0..100     timing jitter
    int dynamics  = 0;      // -100..100  metric accent (- inverts)
    int length    = 100;    // 25..200    duration % (100 = original)
    int intensity = 100;    // 0..200     velocity % (100 = original)

    SwingBase swingBase = SwingBase::Eighth;
    /** Swing grid: 0=1/16 ... 5=2 (see kSwingGridLabels). */
    int swingGridIndex = 1;

    int quantizeGridIndex = (int) QuantizeGrid::Eighth;
    int quantizeStrength = 0;       // 0..100
    int articulationIndex = (int) Articulation::Legato;
    int articulationStrength = 0;   // 0..100
    int sustainPedalMode = (int) SustainPedalMode::Off;

    int  get(int knobIndex) const;
    void set(int knobIndex, int value);
    bool isDefault() const;
    int  activeCount() const;   // knobs/toggles off default (badge count)

    QuantizeGrid quantizeGrid() const
    {
        return (QuantizeGrid) juce::jlimit(0, (int) QuantizeGrid::Count - 1, quantizeGridIndex);
    }
    Articulation articulation() const
    {
        return (Articulation) juce::jlimit(0, (int) Articulation::Count - 1, articulationIndex);
    }
    SustainPedalMode sustainMode() const
    {
        return (SustainPedalMode) juce::jlimit(0, (int) SustainPedalMode::Count - 1, sustainPedalMode);
    }
};

/** Timing + length transform. Returns new notes; never mutates. */
std::vector<RollNote> applyGroove(const std::vector<RollNote>& notes, const GrooveParams& k);

/** Build CC64 sustain points (beat domain) for export/preview. Empty when Off. */
std::vector<AutomationPoint> buildSustainPedalAutomation(const std::vector<RollNote>& notes,
                                                         double lengthBeats,
                                                         const GrooveParams& k);

/** Deterministic per-note base velocity, 0.45–1.0 (hashed from note id). */
double baseVel(int id);

/** Effective velocity 0.04–1.0. Intensity scales the whole performance;
    Dynamics widens (or inverts) contrast by metric position. */
double noteVelocity(const RollNote& n, const GrooveParams& k);

/** noteVelocity with an explicit base — used for real MIDI files so the
    file's own velocities survive as the base performance. */
double noteVelocityWithBase(double base, const RollNote& n, const GrooveParams& k);

/** Formatted knob readout (e.g. "50%", "+12", "100%"). */
juce::String grooveValueText(const KnobDef& def, int value);

} // namespace pflow
