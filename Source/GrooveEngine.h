#pragma once
#include <array>
#include <vector>

#include "EditModel.h"

namespace pflow {

// ── Groove engine ────────────────────────────────────────────────────────────
// Port of Prototype/mbd/knob.jsx. Non-destructive: applied as a render /
// playback transform over resolveClip()'s output, never written back.

struct KnobDef
{
    const char* key;
    const char* label;
    int min, max, def;
    bool bipolar() const { return min < 0; }   // bipolar knobs fill the arc from center
};

constexpr int kNumKnobs = 6;
extern const std::array<KnobDef, kNumKnobs> kKnobDefs;

struct GrooveParams
{
    int swing     = 0;     // 0..75      delays offbeat 8ths
    int pocket    = 0;     // -100..100  − ahead of grid (tight), + behind (loose)
    int humanize  = 0;     // 0..100     deterministic per-note timing jitter
    int dynamics  = 0;     // 0..100     accent contrast by metric position
    int length    = 100;   // 25..200    note length scale
    int intensity = 80;    // 0..100     flat velocity scale

    int  get(int knobIndex) const;
    void set(int knobIndex, int value);
    bool isDefault() const;
    int  activeCount() const;   // knobs off their default (drives the Groove badge)
};

/** Timing + length transform. Returns new notes; never mutates. */
std::vector<RollNote> applyGroove(const std::vector<RollNote>& notes, const GrooveParams& k);

/** Deterministic per-note base velocity, 0.45–1.0 (hashed from note id). */
double baseVel(int id);

/** Effective velocity 0.04–1.0. Intensity scales the whole performance;
    Dynamics widens contrast by metric position (downbeats punch, in-between
    16ths duck). */
double noteVelocity(const RollNote& n, const GrooveParams& k);

/** noteVelocity with an explicit base — used for real MIDI files so the
    file's own velocities survive as the base performance. */
double noteVelocityWithBase(double base, const RollNote& n, const GrooveParams& k);

} // namespace pflow
