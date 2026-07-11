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
//   Pocket    — tight↔loose feel (− push / + laid-back); strong beats move less
//   Humanize  — deterministic per-note timing jitter (Ableton Random)
//   Dynamics  — metric accent contrast; bipolar (− inverts, like Ableton Velocity)
//   Length    — note duration scale; 100 = unchanged (center default)
//   Intensity — velocity scale; 100 = file velocities unchanged (center default)

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

constexpr int kNumKnobs = 6;
extern const std::array<KnobDef, kNumKnobs> kKnobDefs;

struct GrooveParams
{
    int swing     = 0;      // 0..100     offbeat delay amount
    int pocket    = 0;      // -100..100  − push / + lay back (metric-weighted)
    int humanize  = 0;      // 0..100     timing jitter
    int dynamics  = 0;      // -100..100  metric accent (− inverts)
    int length    = 100;    // 25..200    duration % (100 = original)
    int intensity = 100;    // 0..200     velocity % (100 = original)

    SwingBase swingBase = SwingBase::Eighth;

    int  get(int knobIndex) const;
    void set(int knobIndex, int value);
    bool isDefault() const;
    int  activeCount() const;   // knobs/toggles off default (badge count)
};

/** Timing + length transform. Returns new notes; never mutates. */
std::vector<RollNote> applyGroove(const std::vector<RollNote>& notes, const GrooveParams& k);

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
