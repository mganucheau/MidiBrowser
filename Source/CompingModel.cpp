#include "CompingModel.h"
#include <algorithm>
#include <cmath>

namespace pflow {
namespace comping {

namespace {

constexpr double kEps = 1.0e-9;

} // namespace

void subtractHalfOpen(double a, double b, double removeStart, double removeEnd,
                      std::vector<std::pair<double, double>>& out)
{
    if (b <= a + kEps) return;
    if (removeEnd <= removeStart + kEps)
    {
        out.emplace_back(a, b);
        return;
    }
    const double rs = std::max(removeStart, a);
    const double re = std::min(removeEnd, b);
    if (re <= rs + kEps)
    {
        out.emplace_back(a, b);
        return;
    }
    if (rs > a + kEps) out.emplace_back(a, rs);
    if (b > re + kEps) out.emplace_back(re, b);
}

void mergeAdjacentSameLane(std::vector<TakeCompSegment>& segs)
{
    if (segs.empty()) return;
    std::sort(segs.begin(), segs.end(),
              [](const TakeCompSegment& x, const TakeCompSegment& y)
              {
                  if (std::abs(x.startBeat - y.startBeat) > kEps) return x.startBeat < y.startBeat;
                  return x.laneIndex < y.laneIndex;
              });
    std::vector<TakeCompSegment> out;
    for (auto& s : segs)
    {
        if (s.endBeat <= s.startBeat + kEps) continue;
        if (!out.empty() && out.back().laneIndex == s.laneIndex
            && std::abs(out.back().endBeat - s.startBeat) < 1.0e-6)
            out.back().endBeat = s.endBeat;
        else
            out.push_back(s);
    }
    segs.swap(out);
}

void applySwipe(TakeComp& comp, int laneIdx, double s, double e)
{
    if (s > e) std::swap(s, e);
    if (e <= s + kEps) return;

    std::vector<TakeCompSegment> next;
    next.reserve(comp.segments.size() + 2);

    for (const auto& seg : comp.segments)
    {
        std::vector<std::pair<double, double>> parts;
        subtractHalfOpen(seg.startBeat, seg.endBeat, s, e, parts);
        for (auto& p : parts)
        {
            if (p.second <= p.first + kEps) continue;
            next.push_back({ seg.laneIndex, p.first, p.second });
        }
    }
    next.push_back({ laneIdx, s, e });
    mergeAdjacentSameLane(next);
    comp.segments = std::move(next);
}

std::vector<TakeCompSegment> sortedSegments(const TakeComp& c)
{
    auto out = c.segments;
    std::sort(out.begin(), out.end(),
              [](const TakeCompSegment& a, const TakeCompSegment& b)
              {
                  return a.startBeat < b.startBeat;
              });
    return out;
}

void remapLanesAfterDelete(std::vector<TakeComp>& comps, int removedIndex)
{
    if (removedIndex < 0) return;
    for (auto& comp : comps)
    {
        std::vector<TakeCompSegment> kept;
        kept.reserve(comp.segments.size());
        for (auto& seg : comp.segments)
        {
            if (seg.laneIndex == removedIndex) continue;
            if (seg.laneIndex > removedIndex) --seg.laneIndex;
            kept.push_back(seg);
        }
        comp.segments = std::move(kept);
    }
}

void remapLanesAfterInsert(std::vector<TakeComp>& comps, int insertIndex)
{
    if (insertIndex < 0) return;
    for (auto& comp : comps)
        for (auto& seg : comp.segments)
            if (seg.laneIndex >= insertIndex)
                ++seg.laneIndex;
}

} // namespace comping
} // namespace pflow
