#include "UndoActions.h"
#include "PluginProcessor.h"
#include "Theme.h"

namespace pflow {

// ── AddRegionAction ─────────────────────────────────────────────────────────

bool AddRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
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
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
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
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    savedRegion = lane.regions[static_cast<size_t>(regionIdx)];
    savedClipIdx = savedRegion.clipIndex;
    if (savedClipIdx >= 0 && savedClipIdx < (int)lane.clips.size())
        savedClip = lane.clips[static_cast<size_t>(savedClipIdx)];
    lane.removeRegion(regionIdx);
    return true;
}

bool RemoveRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    // Re-insert clip and region
    int clipIdx = (int)lane.clips.size();
    lane.clips.push_back(savedClip);
    CompRegion restored = savedRegion;
    restored.clipIndex = clipIdx;
    if (regionIdx <= (int)lane.regions.size())
        lane.regions.insert(lane.regions.begin() + regionIdx, restored);
    else
        lane.regions.push_back(restored);
    return true;
}

// ── MoveRegionAction ────────────────────────────────────────────────────────

bool MoveRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    lane.regions[static_cast<size_t>(regionIdx)].startBeat = newStartBeat;
    lane.regions[static_cast<size_t>(regionIdx)].endBeat = newEndBeat;
    return true;
}

bool MoveRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    lane.regions[static_cast<size_t>(regionIdx)].startBeat = oldStartBeat;
    lane.regions[static_cast<size_t>(regionIdx)].endBeat = oldEndBeat;
    return true;
}

// ── ToggleMuteAction ────────────────────────────────────────────────────────

bool ToggleMuteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    lane.regions[static_cast<size_t>(regionIdx)].muted = !lane.regions[static_cast<size_t>(regionIdx)].muted;
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
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[static_cast<size_t>(regionIdx)].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
    lane.clips[static_cast<size_t>(clipIdx)].notes.push_back(newNote);
    return true;
}

bool AddNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[static_cast<size_t>(regionIdx)].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[static_cast<size_t>(clipIdx)].notes;
    if (!notes.empty()) notes.pop_back();
    return true;
}

// ── DeleteNoteAction ────────────────────────────────────────────────────────

bool DeleteNoteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[static_cast<size_t>(regionIdx)].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[static_cast<size_t>(clipIdx)].notes;
    if (noteIndex < 0 || noteIndex >= (int)notes.size()) return false;
    savedNote = notes[static_cast<size_t>(noteIndex)];
    notes.erase(notes.begin() + noteIndex);
    return true;
}

bool DeleteNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[static_cast<size_t>(regionIdx)].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[static_cast<size_t>(clipIdx)].notes;
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
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[clipIdx].notes;
    if (noteIndex < 0 || noteIndex >= (int)notes.size()) return false;
    notes[noteIndex] = newNoteState;
    return true;
}

bool EditNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[clipIdx].notes;
    if (noteIndex < 0 || noteIndex >= (int)notes.size()) return false;
    notes[noteIndex] = oldNoteState;
    return true;
}

// ── DuplicateRegionAction ──────────────────────────────────────────────────

bool DuplicateRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    auto& srcRegion = lane.regions[regionIdx];
    double len = srcRegion.endBeat - srcRegion.startBeat;
    CompRegion dup = srcRegion;
    dup.startBeat = srcRegion.endBeat;
    dup.endBeat = dup.startBeat + len;
    addedRegionIdx = (int)lane.regions.size();
    lane.regions.push_back(dup);
    return true;
}

bool DuplicateRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (addedRegionIdx >= 0 && addedRegionIdx < (int)lane.regions.size())
        lane.regions.erase(lane.regions.begin() + addedRegionIdx);
    return true;
}

// ── SplitRegionAction ──────────────────────────────────────────────────────

bool SplitRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
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
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    // Remove the second half
    if (addedRegionIdx >= 0 && addedRegionIdx < (int)lane.regions.size())
        lane.regions.erase(lane.regions.begin() + addedRegionIdx);
    // Restore original region
    if (regionIdx >= 0 && regionIdx < (int)lane.regions.size())
        lane.regions[regionIdx] = origRegion;
    return true;
}

// ── MoveRegionToLaneAction ──────────────────────────────────────────────────

bool MoveRegionToLaneAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (srcLaneIdx < 0 || srcLaneIdx >= (int)proc.lanes.size()) return false;
    auto& srcLane = proc.lanes[srcLaneIdx];
    if (srcRegionIdx < 0 || srcRegionIdx >= (int)srcLane.regions.size()) return false;

    savedRegion = srcLane.regions[srcRegionIdx];
    savedClipIdx = savedRegion.clipIndex;
    if (savedClipIdx >= 0 && savedClipIdx < (int)srcLane.clips.size())
        savedClip = srcLane.clips[savedClipIdx];

    // Create new lane if needed
    if (needsNewLane)
    {
        CompLane newLane;
        newLane.name = "Lane " + juce::String(proc.lanes.size() + 1);
        auto presets = getClipColourPresets();
        newLane.colour = presets[proc.lanes.size() % presets.size()];
        proc.lanes.push_back(newLane);
    }

    if (dstLaneIdx < 0 || dstLaneIdx >= (int)proc.lanes.size()) return false;
    auto& dstLane = proc.lanes[dstLaneIdx];

    // Add clip to destination lane
    addedClipIdx = (int)dstLane.clips.size();
    dstLane.clips.push_back(savedClip);

    // Add region to destination lane with updated beat positions
    CompRegion newRegion = savedRegion;
    newRegion.startBeat = newStartBeat;
    newRegion.endBeat = newEndBeat;
    newRegion.clipIndex = addedClipIdx;
    addedRegionIdx = (int)dstLane.regions.size();
    dstLane.regions.push_back(newRegion);

    // Remove from source lane
    srcLane.removeRegion(srcRegionIdx);

    return true;
}

bool MoveRegionToLaneAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);

    // Remove from destination
    if (dstLaneIdx >= 0 && dstLaneIdx < (int)proc.lanes.size())
    {
        auto& dstLane = proc.lanes[dstLaneIdx];
        if (addedRegionIdx >= 0 && addedRegionIdx < (int)dstLane.regions.size())
            dstLane.regions.erase(dstLane.regions.begin() + addedRegionIdx);
        if (addedClipIdx >= 0 && addedClipIdx < (int)dstLane.clips.size())
            dstLane.clips.erase(dstLane.clips.begin() + addedClipIdx);
    }

    // Remove created lane if we made one
    if (needsNewLane && dstLaneIdx < (int)proc.lanes.size())
        proc.lanes.erase(proc.lanes.begin() + dstLaneIdx);

    // Restore to source lane
    if (srcLaneIdx >= 0 && srcLaneIdx < (int)proc.lanes.size())
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

} // namespace pflow
