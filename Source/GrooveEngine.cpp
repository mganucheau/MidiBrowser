#include "GrooveEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pflow {

const std::array<KnobDef, kNumKnobs> kKnobDefs {{
    { "swing",     "Swing",      0,  100,   0 },   // 0 = straight (Ableton Timing)
    { "pocket",    "Pocket",  -100,  100,   0 },   // bipolar: − push / + laid-back loose
    { "humanize",  "Humanize",   0,  100,   0 },   // 0 = locked (Ableton Random)
    { "dynamics",  "Dynamics",-100,  100,   0 },   // bipolar: Ableton Velocity
    { "length",    "Length",    25,  200, 100 },   // 100% = original (center)
    { "intensity", "Intensity",  0,  200, 100 },   // 100% = original (center)
}};

int GrooveParams::get(int i) const
{
    switch (i)
    {
        case 0: return swing;
        case 1: return pocket;
        case 2: return humanize;
        case 3: return dynamics;
        case 4: return length;
        case 5: return intensity;
        default: return 0;
    }
}

void GrooveParams::set(int i, int v)
{
    if (i < 0 || i >= kNumKnobs) return;
    const auto& d = kKnobDefs[(size_t) i];
    v = std::clamp(v, d.min, d.max);
    switch (i)
    {
        case 0: swing = v; break;
        case 1: pocket = v; break;
        case 2: humanize = v; break;
        case 3: dynamics = v; break;
        case 4: length = v; break;
        case 5: intensity = v; break;
        default: break;
    }
}

bool GrooveParams::isDefault() const
{
    return swing == 0 && pocket == 0 && humanize == 0 && dynamics == 0
        && length == 100 && intensity == 100
        && swingGridIndex == 1;
}

int GrooveParams::activeCount() const
{
    int n = 0;
    for (int i = 0; i < kNumKnobs; ++i)
        if (get(i) != kKnobDefs[(size_t) i].def) ++n;
    if (swingGridIndex != 1) ++n;
    return n;
}

juce::String grooveValueText(const KnobDef& def, int value)
{
    if (def.bipolar())
        return (value > 0 ? "+" : "") + juce::String(value);
    return juce::String(value) + "%";
}

namespace {

// Math.imul(a, 2654435761) >>> 0 — 32-bit wrapping multiply (prototype hash).
uint32_t knuthHash(int a)
{
    return (uint32_t) a * 2654435761u;
}

/** Deterministic −1…+1 from note id + salt (stable across sessions). */
double hashSigned(int id, int salt)
{
    return (double) (knuthHash(id + salt) % 1000u) / 1000.0 * 2.0 - 1.0;
}

/** Swing grid period in 16th-steps for index 0=1/16 … 5=2. */
int swingPeriodSteps(int gridIndex)
{
    static const int periods[] = { 1, 2, 4, 8, 16, 32 };
    return periods[juce::jlimit(0, 5, gridIndex)];
}

/** Metric accent weight by sixteenth in the bar (Ableton-style).
    Downbeat strongest → beat 3 → beats 2/4 → 8th offbeats → 16ths. */
double metricWeight(double start)
{
    const int s = ((int) std::lround(start) % kStepsPerBar + kStepsPerBar) % kStepsPerBar;
    if (s == 0)  return 1.00;
    if (s == 8)  return 0.88;
    if (s == 4 || s == 12) return 0.72;
    if (s % 2 == 0) return 0.55;
    return 0.38;
}

} // namespace

std::vector<RollNote> applyGroove(const std::vector<RollNote>& notes, const GrooveParams& k)
{
    if (k.isDefault())
        return notes;

    const int period = swingPeriodSteps(k.swingGridIndex);
    // Max swing delay = half a period (Ableton Timing toward the next grid).
    const double swingMax = (double) period * 0.5;
    // Pocket: magnitude = looseness, sign = direction. Strong beats stay
    // tighter; weak/offbeats drift more (tight vs loose pocket feel).
    const double pocketAmt = (k.pocket / 100.0) * 1.8;
    // Humanize: ±0.9 sixteenths at 100% (Ableton Random feel).
    const double humanAmt = (k.humanize / 100.0) * 0.9;
    const double lenMul = std::max(0.05, k.length / 100.0);

    std::vector<RollNote> out;
    out.reserve(notes.size());

    for (const auto& n : notes)
    {
        double start = n.start;

        // 1. Swing — delay notes whose floor grid index is odd (Ableton Timing /
        //    prototype: every other 8th or 16th slot, not only exact grid hits).
        if (k.swing > 0)
        {
            const int slot = (int) std::floor(n.start / (double) period);
            if (((slot % 2) + 2) % 2 == 1)
                start += (k.swing / 100.0) * swingMax;
        }

        // 2. Pocket — metric-weighted push/pull (not a flat translate).
        if (k.pocket != 0)
        {
            const double w = metricWeight(n.start);          // 1.0 downbeat … 0.38 16ths
            const double looseness = 1.0 - w;                // weak beats drift more
            start += pocketAmt * (0.15 + 0.85 * looseness);
        }

        // 3. Humanize — deterministic per-id timing jitter.
        if (k.humanize > 0)
            start += hashSigned(n.id, 1) * humanAmt;

        RollNote g = n;
        // Allow slight negative start for pocket-ahead; clamp hard floor.
        g.start = std::max(-2.0, start);
        g.len   = std::max(0.5, n.len * lenMul);
        out.push_back(g);
    }
    return out;
}

double baseVel(int id)
{
    // Keep prototype hash so existing velocity-lane look stays familiar.
    return 0.45 + (double) (knuthHash(id + 7) % 56u) / 100.0;
}

double noteVelocity(const RollNote& n, const GrooveParams& k)
{
    return noteVelocityWithBase(baseVel(n.id), n, k);
}

double noteVelocityWithBase(double base, const RollNote& n, const GrooveParams& k)
{
    // Intensity: 100% = unchanged. Range 0–200% so you can push hotter than the file.
    double v = base * (k.intensity / 100.0);

    // Dynamics: Ableton Velocity (−100…+100).
    //   + widens accents (stronger on-beats, softer offbeats)
    //   − inverts (ghost on-beats, punch offbeats)
    //   0 leaves relative contrast alone
    if (k.dynamics != 0)
    {
        const double w = metricWeight(n.start);
        const double amt = std::abs(k.dynamics) / 100.0;
        if (k.dynamics > 0)
        {
            const double shaped = 0.35 + w * 0.65;   // 0.35…1.0
            v *= (1.0 - amt) + amt * shaped;
        }
        else
        {
            const double shaped = 1.0 - w * 0.55;    // ~1.0…0.45
            v *= (1.0 - amt) + amt * shaped;
        }
    }

    return std::clamp(v, 0.04, 1.0);
}

} // namespace pflow
