#include "UndoActions.h"
#include "PluginProcessor.h"

namespace pflow {

// ── AddRegionAction ─────────────────────────────────────────────────────────

bool AddRegionAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
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
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
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
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    savedRegion = lane.regions[regionIdx];
    savedClipIdx = savedRegion.clipIndex;
    if (savedClipIdx >= 0 && savedClipIdx < (int)lane.clips.size())
        savedClip = lane.clips[savedClipIdx];
    lane.removeRegion(regionIdx);
    return true;
}

bool RemoveRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
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
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    lane.regions[regionIdx].startBeat = newStartBeat;
    lane.regions[regionIdx].endBeat = newEndBeat;
    return true;
}

bool MoveRegionAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    lane.regions[regionIdx].startBeat = oldStartBeat;
    lane.regions[regionIdx].endBeat = oldEndBeat;
    return true;
}

// ── ToggleMuteAction ────────────────────────────────────────────────────────

bool ToggleMuteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
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
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
    lane.clips[clipIdx].notes.push_back(newNote);
    return true;
}

bool AddNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[clipIdx].notes;
    if (!notes.empty()) notes.pop_back();
    return true;
}

// ── DeleteNoteAction ────────────────────────────────────────────────────────

bool DeleteNoteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[clipIdx].notes;
    if (noteIndex < 0 || noteIndex >= (int)notes.size()) return false;
    savedNote = notes[noteIndex];
    notes.erase(notes.begin() + noteIndex);
    return true;
}

bool DeleteNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[laneIdx];
    if (regionIdx < 0 || regionIdx >= (int)lane.regions.size()) return false;
    int clipIdx = lane.regions[regionIdx].clipIndex;
    if (clipIdx < 0 || clipIdx >= (int)lane.clips.size()) return false;
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

} // namespace pflow
