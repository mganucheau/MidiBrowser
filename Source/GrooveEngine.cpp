#include "GrooveEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pflow {

const std::array<KnobDef, kNumKnobs> kKnobDefs {{
    { "swing",     "Swing",     0,    75,  0   },
    { "pocket",    "Pocket",    -100, 100, 0   },
    { "humanize",  "Humanize",  0,    100, 0   },
    { "dynamics",  "Dynamics",  0,    100, 0   },
    { "length",    "Length",    25,   200, 100 },
    { "intensity", "Intensity", 0,    100, 80  },
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
        default: return intensity;
    }
}

void GrooveParams::set(int i, int value)
{
    const auto& d = kKnobDefs[(size_t) std::clamp(i, 0, kNumKnobs - 1)];
    value = std::clamp(value, d.min, d.max);
    switch (i)
    {
        case 0: swing = value; break;
        case 1: pocket = value; break;
        case 2: humanize = value; break;
        case 3: dynamics = value; break;
        case 4: length = value; break;
        default: intensity = value; break;
    }
}

bool GrooveParams::isDefault() const { return activeCount() == 0; }

int GrooveParams::activeCount() const
{
    int count = 0;
    for (int i = 0; i < kNumKnobs; ++i)
        if (get(i) != kKnobDefs[(size_t) i].def)
            ++count;
    return count;
}

namespace {

// Math.imul(a, 2654435761) >>> 0 — 32-bit wrapping multiply, read unsigned.
uint32_t knuthHash(int a)
{
    return (uint32_t) a * 2654435761u;
}

} // namespace

std::vector<RollNote> applyGroove(const std::vector<RollNote>& notes, const GrooveParams& k)
{
    std::vector<RollNote> out;
    out.reserve(notes.size());

    for (const auto& n : notes)
    {
        RollNote g = n;
        double start = n.start;

        if (k.swing != 0)
        {
            const auto eighth = (long long) std::floor(n.start / 2.0);
            if (((eighth % 2) + 2) % 2 == 1)
                start += (k.swing / 100.0) * 1.0;   // delay offbeat 8ths
        }
        if (k.pocket != 0)
            start += (k.pocket / 100.0) * 1.6;      // − ahead of grid, + laid back
        if (k.humanize != 0)
        {
            const double r = (double) (knuthHash(n.id + 1) % 1000u) / 1000.0 * 2.0 - 1.0;
            start += r * (k.humanize / 100.0) * 0.9;
        }

        g.len = std::max(0.5, n.len * (k.length / 100.0));
        g.start = std::max(0.0, start);
        out.push_back(g);
    }
    return out;
}

double baseVel(int id)
{
    return 0.45 + (double) (knuthHash(id + 7) % 56u) / 100.0;
}

double noteVelocityWithBase(double base, const RollNote& n, const GrooveParams& k)
{
    double v = base;
    if (k.dynamics != 0)
    {
        const int pos = ((int) std::lround(n.start) % kStepsPerBar + kStepsPerBar) % kStepsPerBar;
        double accent;
        if (pos == 0)          accent = 1.0;    // bar downbeat
        else if (pos % 4 == 0) accent = 0.45;   // beats 2 / 3 / 4
        else if (pos % 2 == 0) accent = -0.2;   // 8th-note offbeats
        else                   accent = -0.6;   // in-between 16ths
        v += accent * (k.dynamics / 100.0) * 0.55;
    }
    v *= (k.intensity / 100.0);
    return std::clamp(v, 0.04, 1.0);
}

double noteVelocity(const RollNote& n, const GrooveParams& k)
{
    return noteVelocityWithBase(baseVel(n.id), n, k);
}

} // namespace pflow
