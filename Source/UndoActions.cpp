#include "UndoActions.h"
#include "PluginProcessor.h"
#include <cmath>
#include "Theme.h"

namespace pflow {

// ── AddRegionAction ─────────────────────────────────────────────────────────

bool AddRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    addedClipIdx = (int)lane.clips.size();
    lane.clips.push_back(newClip);
    newRegion.clipIndex = addedClipIdx;
    addedRegionIdx = (int)lane.regions.size();
    lane.regions.push_back(newRegion);
    return true;
}

bool AddRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (addedRegionIdx >= 0 && addedRegionIdx < (int)lane.regions.size())
        lane.regions.erase(lane.regions.begin() + addedRegionIdx);
    if (addedClipIdx >= 0 && addedClipIdx < (int)lane.clips.size())
        lane.clips.erase(lane.clips.begin() + addedClipIdx);
    return true;
}

// ── RemoveRegionAction ──────────────────────────────────────────────────────

bool RemoveRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    savedRegion = lane.regions[regionIdx];
    savedClipIdx = savedRegion.clipIndex;
    if (savedClipIdx >= 0 && savedClipIdx < (int)lane.clips.size())
        savedClip = lane.clips[savedClipIdx];
    savedLaneName = lane.name;
    savedLaneColour = lane.colour;
    lane.removeRegion(regionIdx);
    if (lane.regions.empty())
    {
        laneWasDeleted = true;
        proc.lanes.erase(proc.lanes.begin() + laneIdx);
        proc.remapLanesAfterDeleteLocked(laneIdx);
    }
    return true;
}

bool RemoveRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneWasDeleted)
    {
        CompLane restoredLane;
        restoredLane.name = savedLaneName;
        restoredLane.colour = savedLaneColour;
        int clipIdx = 0;
        restoredLane.clips.push_back(savedClip);
        CompRegion restored = savedRegion;
        restored.clipIndex = clipIdx;
        restoredLane.regions.push_back(restored);
        int insertAt = juce::jlimit(0, (int)proc.lanes.size(), laneIdx);
        proc.lanes.insert(proc.lanes.begin() + insertAt, restoredLane);
        proc.remapLanesAfterInsertLocked(insertAt);
    }
    else
    {
        if (!proc.isValidLane(laneIdx)) return false;
        auto& lane = proc.lanes[laneIdx];
        int clipIdx = (int)lane.clips.size();
        lane.clips.push_back(savedClip);
        CompRegion restored = savedRegion;
        restored.clipIndex = clipIdx;
        if (regionIdx <= (int)lane.regions.size())
            lane.regions.insert(lane.regions.begin() + regionIdx, restored);
        else
            lane.regions.push_back(restored);
    }
    return true;
}

// ── MoveRegionAction ────────────────────────────────────────────────────────

bool MoveRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    auto& r = lane.regions[regionIdx];
    r.startBeat = newStartBeat;
    r.endBeat = newEndBeat;
    r.ensureSelectionInBounds();
    return true;
}

bool MoveRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    auto& r = lane.regions[regionIdx];
    r.startBeat = oldStartBeat;
    r.endBeat = oldEndBeat;
    r.ensureSelectionInBounds();
    return true;
}

// ── ToggleMuteAction ────────────────────────────────────────────────────────

bool ToggleMuteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    lane.regions[regionIdx].muted = !lane.regions[regionIdx].muted;
    return true;
}

bool ToggleMuteAction::undo()
{
    return perform(); // Toggle is self-inverse
}

// ── AddNoteAction ───────────────────────────────────────────────────────────

bool AddNoteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (!proc.isValidClipIndex(laneIdx, clipIdx)) return false;
    lane.clips[clipIdx].notes.push_back(newNote);
    return true;
}

bool AddNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (!proc.isValidClipIndex(laneIdx, clipIdx)) return false;
    auto& notes = lane.clips[clipIdx].notes;
    if (!notes.empty()) notes.pop_back();
    return true;
}

// ── DeleteNoteAction ────────────────────────────────────────────────────────

bool DeleteNoteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (!proc.isValidClipIndex(laneIdx, clipIdx)) return false;
    auto& notes = lane.clips[clipIdx].notes;
    if (noteIndex < 0 || noteIndex >= (int)notes.size()) return false;
    savedNote = notes[noteIndex];
    notes.erase(notes.begin() + noteIndex);
    return true;
}

bool DeleteNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (!proc.isValidClipIndex(laneIdx, clipIdx)) return false;
    auto& notes = lane.clips[clipIdx].notes;
    if (noteIndex <= (int)notes.size())
        notes.insert(notes.begin() + noteIndex, savedNote);
    else
        notes.push_back(savedNote);
    return true;
}

// ── EditNoteAction ──────────────────────────────────────────────────────────

bool EditNoteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (!proc.isValidClipIndex(laneIdx, clipIdx)) return false;
    auto& notes = lane.clips[clipIdx].notes;
    if (noteIndex < 0 || noteIndex >= (int)notes.size()) return false;
    notes[noteIndex] = newNoteState;
    return true;
}

bool EditNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (!proc.isValidClipIndex(laneIdx, clipIdx)) return false;
    auto& notes = lane.clips[clipIdx].notes;
    if (noteIndex < 0 || noteIndex >= (int)notes.size()) return false;
    notes[noteIndex] = oldNoteState;
    return true;
}

// ── SetAutomationAction ─────────────────────────────────────────────────────

bool SetAutomationAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (!proc.isValidClipIndex(laneIdx, clipIdx)) return false;
    lane.clips[clipIdx].automation[cc] = newPoints;
    return true;
}

bool SetAutomationAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (!proc.isValidClipIndex(laneIdx, clipIdx)) return false;
    lane.clips[clipIdx].automation[cc] = oldPoints;
    return true;
}

// ── DuplicateRegionAction ──────────────────────────────────────────────────

bool DuplicateRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    auto& srcRegion = lane.regions[regionIdx];
    double len = srcRegion.endBeat - srcRegion.startBeat;
    CompRegion dup = srcRegion;
    dup.startBeat = srcRegion.endBeat;
    dup.endBeat = dup.startBeat + len;
    // Comping removed: no selection duplication
    addedRegionIdx = (int)lane.regions.size();
    lane.regions.push_back(dup);
    return true;
}

bool DuplicateRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (addedRegionIdx >= 0 && addedRegionIdx < (int)lane.regions.size())
        lane.regions.erase(lane.regions.begin() + addedRegionIdx);
    return true;
}

// ── SplitRegionAction ──────────────────────────────────────────────────────

bool SplitRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    if (!proc.isValidRegion(laneIdx, regionIdx)) return false;
    origRegion = lane.regions[regionIdx];
    if (splitAtBeat <= origRegion.startBeat || splitAtBeat >= origRegion.endBeat) return false;
    // Create second half
    CompRegion secondHalf = origRegion;
    secondHalf.startBeat = splitAtBeat;
    // Trim first half
    lane.regions[regionIdx].endBeat = splitAtBeat;
    addedRegionIdx = (int)lane.regions.size();
    lane.regions.push_back(secondHalf);
    return true;
}

bool SplitRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(laneIdx)) return false;
    auto& lane = proc.lanes[laneIdx];
    // Remove the second half
    if (addedRegionIdx >= 0 && addedRegionIdx < (int)lane.regions.size())
        lane.regions.erase(lane.regions.begin() + addedRegionIdx);
    // Restore original region
    if (proc.isValidRegion(laneIdx, regionIdx))
        lane.regions[regionIdx] = origRegion;
    return true;
}

// ── MoveRegionToLaneAction ───────────────────────────────────────────────────

bool MoveRegionToLaneAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (!proc.isValidLane(srcLaneIdx)) return false;
    auto& srcLane = proc.lanes[srcLaneIdx];
    if (!proc.isValidRegion(srcLaneIdx, srcRegionIdx)) return false;

    savedRegion = srcLane.regions[srcRegionIdx];
    savedClipIdx = savedRegion.clipIndex;
    if (proc.isValidClipIndex(srcLaneIdx, savedClipIdx))
        savedClip = srcLane.clips[savedClipIdx];
    savedSrcLaneName = srcLane.name;
    savedSrcLaneColour = srcLane.colour;

    // Create new lanes if needed (create enough to reach dstLaneIdx)
    // Must save srcLane data above - push_back can reallocate and invalidate srcLane ref
    int numLanesBefore = (int)proc.lanes.size();
    if (needsNewLane && dstLaneIdx >= numLanesBefore)
    {
        int lanesToCreate = dstLaneIdx - numLanesBefore + 1;
        auto presets = getClipColourPresets();
        for (int i = 0; i < lanesToCreate; ++i)
        {
            CompLane newLane;
            newLane.name = "Lane " + juce::String(proc.lanes.size() + 1);
            newLane.colour = presets[proc.lanes.size() % presets.size()];
            proc.lanes.push_back(newLane);
        }
    }

    if (!proc.isValidLane(dstLaneIdx)) return false;
    auto& dstLane = proc.lanes[dstLaneIdx];

    // Add clip to destination lane
    addedClipIdx = (int)dstLane.clips.size();
    dstLane.clips.push_back(savedClip);

    // Add region to destination lane with updated beat positions
    CompRegion newRegion = savedRegion;
    double delta = newStartBeat - savedRegion.startBeat;
    newRegion.startBeat = newStartBeat;
    newRegion.endBeat = newEndBeat;
    (void)delta;
    newRegion.ensureSelectionInBounds();
    newRegion.clipIndex = addedClipIdx;
    addedRegionIdx = (int)dstLane.regions.size();
    dstLane.regions.push_back(newRegion);

    // Remove from source lane (re-fetch srcLane - may have been invalidated by push_back)
    auto& srcLaneNow = proc.lanes[srcLaneIdx];
    srcLaneNow.removeRegion(srcRegionIdx);
    if (srcLaneNow.regions.empty())
    {
        srcLaneWasDeleted = true;
        proc.lanes.erase(proc.lanes.begin() + srcLaneIdx);
    }

    proc.removeEmptyLanesExceptFirst();
    return true;
}

bool MoveRegionToLaneAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);

    // When we created new lanes, removeEmptyLanesExceptFirst() ran, so our clip is in the last lane
    int effectiveDstIdx = needsNewLane && !proc.lanes.empty()
        ? (int)proc.lanes.size() - 1
        : (srcLaneWasDeleted && srcLaneIdx < dstLaneIdx ? dstLaneIdx - 1 : dstLaneIdx);

    // Remove from destination
    if (effectiveDstIdx >= 0 && effectiveDstIdx < (int)proc.lanes.size())
    {
        auto& dstLane = proc.lanes[effectiveDstIdx];
        if (addedRegionIdx >= 0 && addedRegionIdx < (int)dstLane.regions.size())
            dstLane.regions.erase(dstLane.regions.begin() + addedRegionIdx);
        if (addedClipIdx >= 0 && addedClipIdx < (int)dstLane.clips.size())
            dstLane.clips.erase(dstLane.clips.begin() + addedClipIdx);
    }

    // Remove created lane(s) if we made any
    if (needsNewLane && effectiveDstIdx >= 0 && effectiveDstIdx < (int)proc.lanes.size())
        proc.lanes.erase(proc.lanes.begin() + effectiveDstIdx);

    // Restore to source lane
    if (srcLaneWasDeleted)
    {
        CompLane restoredLane;
        restoredLane.name = savedSrcLaneName;
        restoredLane.colour = savedSrcLaneColour;
        restoredLane.clips.push_back(savedClip);
        CompRegion restored = savedRegion;
        restored.clipIndex = 0;
        restored.startBeat = savedRegion.startBeat;
        restored.endBeat = savedRegion.endBeat;
        restoredLane.regions.push_back(restored);
        int insertAt = juce::jlimit(0, (int)proc.lanes.size(), srcLaneIdx);
        proc.lanes.insert(proc.lanes.begin() + insertAt, restoredLane);
    }
    else if (srcLaneIdx >= 0 && srcLaneIdx < (int)proc.lanes.size())
    {
        auto& srcLane = proc.lanes[srcLaneIdx];
        int clipIdx = (int)srcLane.clips.size();
        srcLane.clips.push_back(savedClip);
        CompRegion restored = savedRegion;
        restored.clipIndex = clipIdx;
        if (srcRegionIdx <= (int)srcLane.regions.size())
            srcLane.regions.insert(srcLane.regions.begin() + srcRegionIdx, restored);
        else
            srcLane.regions.push_back(restored);
    }

    return true;
}

// ── Take comp segments ───────────────────────────────────────────────────────

bool takeCompSegmentsEquivalent(const std::vector<TakeCompSegment>& a,
                                const std::vector<TakeCompSegment>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i].laneIndex != b[i].laneIndex) return false;
        if (std::abs(a[i].startBeat - b[i].startBeat) > 1.0e-6) return false;
        if (std::abs(a[i].endBeat - b[i].endBeat) > 1.0e-6) return false;
    }
    return true;
}

bool SetTakeCompSegmentsAction::perform()
{
    {
        juce::ScopedLock sl(proc.laneLock);
        proc.ensureDefaultTakeComp();
        proc.takeComps[0].segments = afterSegs;
    }
    proc.rebuildMasterClip();
    proc.scheduleCompBoundaryParamSync();
    return true;
}

bool SetTakeCompSegmentsAction::undo()
{
    {
        juce::ScopedLock sl(proc.laneLock);
        proc.ensureDefaultTakeComp();
        proc.takeComps[0].segments = beforeSegs;
    }
    proc.rebuildMasterClip();
    proc.scheduleCompBoundaryParamSync();
    return true;
}

} // namespace pflow
