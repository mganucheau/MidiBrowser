#include "GrooveEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pflow {

const std::array<KnobDef, kNumKnobs> kKnobDefs {{
    { "swing",     "Swing",      0,  100,   0 },   // 0 = straight (Ableton Timing)
    { "pocket",    "Pocket",  -100,  100,   0 },   // bipolar: - push / + laid-back loose
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
        && swingGridIndex == 1
        && quantizeStrength == 0
        && articulationStrength == 0
        && sustainPedalMode == (int) SustainPedalMode::Off;
}

int GrooveParams::activeCount() const
{
    int n = 0;
    for (int i = 0; i < kNumKnobs; ++i)
        if (get(i) != kKnobDefs[(size_t) i].def) ++n;
    if (swingGridIndex != 1) ++n;
    if (quantizeStrength > 0) ++n;
    if (articulationStrength > 0) ++n;
    if (sustainPedalMode != (int) SustainPedalMode::Off) ++n;
    return n;
}

juce::String grooveValueText(const KnobDef& def, int value)
{
    if (def.bipolar())
        return (value > 0 ? "+" : "") + juce::String(value);
    return juce::String(value) + "%";
}

namespace {

uint32_t knuthHash(int a)
{
    return (uint32_t) a * 2654435761u;
}

double hashSigned(int id, int salt)
{
    return (double) (knuthHash(id + salt) % 1000u) / 1000.0 * 2.0 - 1.0;
}

int swingPeriodSteps(int gridIndex)
{
    static const int periods[] = { 1, 2, 4, 8, 16, 32 };
    return periods[juce::jlimit(0, 5, gridIndex)];
}

/** Quantize period in 16th-steps (triplets use 2/3 of the straight value). */
double quantizePeriodSteps(QuantizeGrid g)
{
    switch (g)
    {
        case QuantizeGrid::Quarter:       return 4.0;
        case QuantizeGrid::QuarterT:      return 8.0 / 3.0;
        case QuantizeGrid::Eighth:        return 2.0;
        case QuantizeGrid::EighthT:       return 4.0 / 3.0;
        case QuantizeGrid::Sixteenth:     return 1.0;
        case QuantizeGrid::SixteenthT:    return 2.0 / 3.0;
        case QuantizeGrid::ThirtySecond:  return 0.5;
        case QuantizeGrid::ThirtySecondT: return 1.0 / 3.0;
        default:                          return 2.0;
    }
}

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
    const double swingMax = (double) period * 0.5;
    const double pocketAmt = (k.pocket / 100.0) * 1.8;
    const double humanAmt = (k.humanize / 100.0) * 0.9;
    const double lenMul = std::max(0.05, k.length / 100.0);
    const double qPeriod = quantizePeriodSteps(k.quantizeGrid());
    const double qAmt = juce::jlimit(0.0, 1.0, k.quantizeStrength / 100.0);
    const double artAmt = juce::jlimit(0.0, 1.0, k.articulationStrength / 100.0);
    const auto art = k.articulation();

    // Sort by start for legato/slur gap filling.
    std::vector<RollNote> sorted = notes;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const RollNote& a, const RollNote& b) { return a.start < b.start; });

    std::vector<RollNote> out;
    out.reserve(sorted.size());

    for (size_t i = 0; i < sorted.size(); ++i)
    {
        const auto& n = sorted[i];
        double start = n.start;

        // 1. Quantize — pull toward grid (before swing so swing still feels musical).
        if (qAmt > 0.0 && qPeriod > 1.0e-6)
        {
            const double snapped = std::round(start / qPeriod) * qPeriod;
            start += (snapped - start) * qAmt;
        }

        // 2. Swing
        if (k.swing > 0)
        {
            const int slot = (int) std::floor(start / (double) period);
            if (((slot % 2) + 2) % 2 == 1)
                start += (k.swing / 100.0) * swingMax;
        }

        // 3. Pocket
        if (k.pocket != 0)
        {
            const double w = metricWeight(n.start);
            const double looseness = 1.0 - w;
            start += pocketAmt * (0.15 + 0.85 * looseness);
        }

        // 4. Humanize
        if (k.humanize > 0)
            start += hashSigned(n.id, 1) * humanAmt;

        double len = std::max(0.5, n.len * lenMul);

        // 5. Articulation length shaping
        if (artAmt > 0.0)
        {
            double nextStart = start + 64.0;
            for (size_t j = i + 1; j < sorted.size(); ++j)
                if (sorted[j].start > n.start + 1.0e-6)
                {
                    nextStart = sorted[j].start;
                    break;
                }
            const double gap = std::max(0.0, nextStart - start);

            switch (art)
            {
                case Articulation::Staccato:
                    len *= (1.0 - 0.65 * artAmt);
                    break;
                case Articulation::Tenuto:
                    len *= (1.0 + 0.12 * artAmt);
                    break;
                case Articulation::Legato:
                case Articulation::Slur:
                {
                    const double target = std::max(len, gap * (art == Articulation::Slur ? 1.02 : 0.98));
                    len += (target - len) * artAmt;
                    break;
                }
                case Articulation::Pianissimo:
                case Articulation::Sforzando:
                default:
                    break;
            }
            len = std::max(0.35, len);
        }

        RollNote g = n;
        g.start = std::max(-2.0, start);
        g.len = len;
        out.push_back(g);
    }
    return out;
}

std::vector<AutomationPoint> buildSustainPedalAutomation(const std::vector<RollNote>& notes,
                                                         double lengthBeats,
                                                         const GrooveParams& k)
{
    std::vector<AutomationPoint> pts;
    const auto mode = k.sustainMode();
    if (mode == SustainPedalMode::Off || lengthBeats <= 0.0)
        return pts;

    if (mode == SustainPedalMode::StartOfClip)
    {
        pts.push_back({ 0.0, 127 });
        pts.push_back({ std::max(0.05, lengthBeats - 0.02), 0 });
        return pts;
    }

    // Auto: phrase-aware pedal — down at phrase starts, lift in gaps, never robotic.
    if (notes.empty())
    {
        pts.push_back({ 0.0, 100 });
        pts.push_back({ std::max(0.05, lengthBeats - 0.02), 0 });
        return pts;
    }

    struct Ev { double t; bool on; };
    std::vector<Ev> evs;
    evs.reserve(notes.size() * 2);
    for (const auto& n : notes)
    {
        evs.push_back({ n.start / 4.0, true });
        evs.push_back({ (n.start + n.len) / 4.0, false });
    }
    std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b)
    {
        if (std::abs(a.t - b.t) < 1.0e-9) return a.on && !b.on;
        return a.t < b.t;
    });

    int active = 0;
    double lastSoundEnd = 0.0;
    bool pedalDown = false;
    constexpr double kGapLift = 0.35; // beats of silence before a lift
    int phraseSalt = 0;

    auto push = [&](double beat, int value)
    {
        // Slight humanize so Auto isn't a grid of identical edges.
        const double jitter = hashSigned(phraseSalt++, 9) * 0.03;
        pts.push_back({ std::max(0.0, beat + jitter), value });
    };

    for (const auto& e : evs)
    {
        if (e.on)
        {
            if (active == 0)
            {
                const double gap = e.t - lastSoundEnd;
                if (!pedalDown || gap >= kGapLift)
                {
                    if (pedalDown && gap >= kGapLift)
                        push(std::max(0.0, lastSoundEnd + 0.04), 0);
                    // Soft down (not always 127) for a natural performer feel.
                    const int depth = 95 + (int) std::lround(hashSigned(phraseSalt, 3) * 20.0);
                    push(e.t, juce::jlimit(70, 127, depth));
                    pedalDown = true;
                }
            }
            ++active;
        }
        else
        {
            active = std::max(0, active - 1);
            if (active == 0)
                lastSoundEnd = e.t;
        }
    }

    if (pedalDown)
        push(std::max(0.05, lengthBeats - 0.02), 0);

    // Deduplicate near-identical consecutive points.
    std::vector<AutomationPoint> cleaned;
    for (const auto& p : pts)
    {
        if (!cleaned.empty()
            && std::abs(cleaned.back().beat - p.beat) < 0.01
            && cleaned.back().value == p.value)
            continue;
        cleaned.push_back(p);
    }
    return cleaned;
}

double baseVel(int id)
{
    return 0.45 + (double) (knuthHash(id + 7) % 56u) / 100.0;
}

double noteVelocity(const RollNote& n, const GrooveParams& k)
{
    return noteVelocityWithBase(baseVel(n.id), n, k);
}

double noteVelocityWithBase(double base, const RollNote& n, const GrooveParams& k)
{
    double v = base * (k.intensity / 100.0);

    if (k.dynamics != 0)
    {
        const double w = metricWeight(n.start);
        const double amt = std::abs(k.dynamics) / 100.0;
        if (k.dynamics > 0)
        {
            const double shaped = 0.35 + w * 0.65;
            v *= (1.0 - amt) + amt * shaped;
        }
        else
        {
            const double shaped = 1.0 - w * 0.55;
            v *= (1.0 - amt) + amt * shaped;
        }
    }

    const double artAmt = juce::jlimit(0.0, 1.0, k.articulationStrength / 100.0);
    if (artAmt > 0.0)
    {
        switch (k.articulation())
        {
            case Articulation::Pianissimo:
                v *= (1.0 - 0.55 * artAmt);
                break;
            case Articulation::Sforzando:
                v *= (1.0 + 0.55 * artAmt);
                break;
            case Articulation::Staccato:
                v *= (1.0 + 0.08 * artAmt); // slight bite
                break;
            default:
                break;
        }
    }

    return std::clamp(v, 0.04, 1.0);
}

} // namespace pflow
