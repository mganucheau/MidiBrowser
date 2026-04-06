#include "UndoActions.h"
#include "PluginProcessor.h"
#include "Theme.h"

namespace pflow {

// ── AddClipAction ──────────────────────────────────────────────────────────

bool AddClipAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    addedClipIdx = (int)lane.clips.size();
    lane.addClip(newClip, clipBeatPos);
    return true;
}

bool AddClipAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (addedClipIdx >= 0 && addedClipIdx < (int)lane.clips.size())
        lane.removeClip(addedClipIdx);
    return true;
}

// ── RemoveClipAction ───────────────────────────────────────────────────────

bool RemoveClipAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (clipIndex < 0 || clipIndex >= (int)lane.clips.size()) return false;
    savedClip = lane.clips[static_cast<size_t>(clipIndex)];
    savedBeatPos = lane.clipStarts[static_cast<size_t>(clipIndex)];
    lane.removeClip(clipIndex);
    return true;
}

bool RemoveClipAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    // Re-insert clip at original position
    if (clipIndex <= (int)lane.clips.size())
    {
        lane.clips.insert(lane.clips.begin() + clipIndex, savedClip);
        lane.clipStarts.insert(lane.clipStarts.begin() + clipIndex, savedBeatPos);
    }
    else
    {
        lane.addClip(savedClip, savedBeatPos);
    }
    return true;
}

// ── MoveClipAction ─────────────────────────────────────────────────────────

bool MoveClipAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (clipIndex < 0 || clipIndex >= (int)lane.clipStarts.size()) return false;
    lane.clipStarts[static_cast<size_t>(clipIndex)] = newPos;
    return true;
}

bool MoveClipAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (clipIndex < 0 || clipIndex >= (int)lane.clipStarts.size()) return false;
    lane.clipStarts[static_cast<size_t>(clipIndex)] = oldPos;
    return true;
}

// ── MoveClipToLaneAction ───────────────────────────────────────────────────

bool MoveClipToLaneAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (srcLaneIdx < 0 || srcLaneIdx >= (int)proc.lanes.size()) return false;
    auto& srcLane = proc.lanes[static_cast<size_t>(srcLaneIdx)];
    if (srcClipIdx < 0 || srcClipIdx >= (int)srcLane.clips.size()) return false;

    savedClip = srcLane.clips[static_cast<size_t>(srcClipIdx)];
    savedBeatPos = srcLane.clipStarts[static_cast<size_t>(srcClipIdx)];

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
    auto& dstLane = proc.lanes[static_cast<size_t>(dstLaneIdx)];

    addedClipIdx = (int)dstLane.clips.size();
    dstLane.addClip(savedClip, newPos);

    // Remove from source lane
    srcLane.removeClip(srcClipIdx);

    return true;
}

bool MoveClipToLaneAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);

    // Remove from destination
    if (dstLaneIdx >= 0 && dstLaneIdx < (int)proc.lanes.size())
    {
        auto& dstLane = proc.lanes[static_cast<size_t>(dstLaneIdx)];
        if (addedClipIdx >= 0 && addedClipIdx < (int)dstLane.clips.size())
            dstLane.removeClip(addedClipIdx);
    }

    // Remove created lane if we made one
    if (needsNewLane && dstLaneIdx < (int)proc.lanes.size())
        proc.lanes.erase(proc.lanes.begin() + dstLaneIdx);

    // Restore to source lane
    if (srcLaneIdx >= 0 && srcLaneIdx < (int)proc.lanes.size())
    {
        auto& srcLane = proc.lanes[static_cast<size_t>(srcLaneIdx)];
        if (srcClipIdx <= (int)srcLane.clips.size())
        {
            srcLane.clips.insert(srcLane.clips.begin() + srcClipIdx, savedClip);
            srcLane.clipStarts.insert(srcLane.clipStarts.begin() + srcClipIdx, savedBeatPos);
        }
        else
        {
            srcLane.addClip(savedClip, savedBeatPos);
        }
    }

    return true;
}

// ── AddNoteAction ──────────────────────────────────────────────────────────

bool AddNoteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (clipIndex < 0 || clipIndex >= (int)lane.clips.size()) return false;
    lane.clips[static_cast<size_t>(clipIndex)].notes.push_back(newNote);
    return true;
}

bool AddNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (clipIndex < 0 || clipIndex >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[static_cast<size_t>(clipIndex)].notes;
    if (!notes.empty()) notes.pop_back();
    return true;
}

// ── DeleteNoteAction ───────────────────────────────────────────────────────

bool DeleteNoteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (clipIndex < 0 || clipIndex >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[static_cast<size_t>(clipIndex)].notes;
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
    if (clipIndex < 0 || clipIndex >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[static_cast<size_t>(clipIndex)].notes;
    if (noteIndex <= (int)notes.size())
        notes.insert(notes.begin() + noteIndex, savedNote);
    else
        notes.push_back(savedNote);
    return true;
}

// ── EditNoteAction ─────────────────────────────────────────────────────────

bool EditNoteAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (clipIndex < 0 || clipIndex >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[static_cast<size_t>(clipIndex)].notes;
    if (noteIndex < 0 || noteIndex >= (int)notes.size()) return false;
    notes[static_cast<size_t>(noteIndex)] = newNoteState;
    return true;
}

bool EditNoteAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (clipIndex < 0 || clipIndex >= (int)lane.clips.size()) return false;
    auto& notes = lane.clips[static_cast<size_t>(clipIndex)].notes;
    if (noteIndex < 0 || noteIndex >= (int)notes.size()) return false;
    notes[static_cast<size_t>(noteIndex)] = oldNoteState;
    return true;
}

// ── DuplicateClipAction ───────────────────────────────────────────────────

bool DuplicateClipAction::perform()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (clipIndex < 0 || clipIndex >= (int)lane.clips.size()) return false;
    auto& srcClip = lane.clips[static_cast<size_t>(clipIndex)];
    double srcStart = lane.clipStarts[static_cast<size_t>(clipIndex)];
    double newStart = srcStart + srcClip.lengthBeats;
    addedClipIdx = (int)lane.clips.size();
    lane.addClip(srcClip, newStart);
    return true;
}

bool DuplicateClipAction::undo()
{
    juce::ScopedLock sl(proc.laneLock);
    if (laneIdx < 0 || laneIdx >= (int)proc.lanes.size()) return false;
    auto& lane = proc.lanes[static_cast<size_t>(laneIdx)];
    if (addedClipIdx >= 0 && addedClipIdx < (int)lane.clips.size())
        lane.removeClip(addedClipIdx);
    return true;
}

} // namespace pflow
