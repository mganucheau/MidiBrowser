#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace pflow {

/** One selected time range on a take lane (half-open [startBeat, endBeat)). */
struct TakeCompSegment
{
    int    laneIndex  = 0;
    double startBeat  = 0.0;
    double endBeat    = 0.0;
};

/** Named comp: mutually exclusive segments across lanes in time. */
struct TakeComp
{
    juce::String                name;
    std::vector<TakeCompSegment> segments;
};

namespace comping {

/** Remove [removeStart, removeEnd) from [a, b), half-open; append 0–2 pieces to out. */
void subtractHalfOpen(double a, double b, double removeStart, double removeEnd,
                      std::vector<std::pair<double, double>>& out);

/**
 * Logic-style swipe: remove [s,e) from every lane's segments, then add [s,e) on laneIdx
 * and merge contiguous same-lane pieces.
 */
void applySwipe(TakeComp& comp, int laneIdx, double s, double e);

std::vector<TakeCompSegment> sortedSegments(const TakeComp& c);

void mergeAdjacentSameLane(std::vector<TakeCompSegment>& segs);

/** After deleting lane \a removedIndex, drop its segments and shift higher indices down. */
void remapLanesAfterDelete(std::vector<TakeComp>& comps, int removedIndex);

/** Undo helper: after inserting a lane at \a insertIndex, shift segment lane indices at or above. */
void remapLanesAfterInsert(std::vector<TakeComp>& comps, int insertIndex);

} // namespace comping

} // namespace pflow
