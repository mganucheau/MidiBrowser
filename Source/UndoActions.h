#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include "MidiFileData.h"

namespace pflow {

class PatternFlowProcessor;

// ── Add region to lane ───────────────────────────────────────────────────────
class AddRegionAction : public juce::UndoableAction
{
public:
    AddRegionAction(PatternFlowProcessor& p, int lane, CompRegion region, MidiClip clip)
        : proc(p), laneIdx(lane), newRegion(std::move(region)), newClip(std::move(clip)) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx;
    CompRegion newRegion;
    MidiClip newClip;
    int addedRegionIdx = -1;
    int addedClipIdx = -1;
};

// ── Remove region from lane ─────────────────────────────────────────────────
class RemoveRegionAction : public juce::UndoableAction
{
public:
    RemoveRegionAction(PatternFlowProcessor& p, int lane, int region)
        : proc(p), laneIdx(lane), regionIdx(region) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx;
    int regionIdx;
    CompRegion savedRegion;
    MidiClip savedClip;
    int savedClipIdx = -1;
};

// ── Move region ─────────────────────────────────────────────────────────────
class MoveRegionAction : public juce::UndoableAction
{
public:
    MoveRegionAction(PatternFlowProcessor& p, int lane, int region,
                     double oldStart, double oldEnd, double newStart, double newEnd)
        : proc(p), laneIdx(lane), regionIdx(region),
          oldStartBeat(oldStart), oldEndBeat(oldEnd),
          newStartBeat(newStart), newEndBeat(newEnd) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, regionIdx;
    double oldStartBeat, oldEndBeat;
    double newStartBeat, newEndBeat;
};

// ── Toggle mute ─────────────────────────────────────────────────────────────
class ToggleMuteAction : public juce::UndoableAction
{
public:
    ToggleMuteAction(PatternFlowProcessor& p, int lane, int region)
        : proc(p), laneIdx(lane), regionIdx(region) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, regionIdx;
};

// ── Add note in piano roll ──────────────────────────────────────────────────
class AddNoteAction : public juce::UndoableAction
{
public:
    AddNoteAction(PatternFlowProcessor& p, int lane, int region, NoteEvent note)
        : proc(p), laneIdx(lane), regionIdx(region), newNote(std::move(note)) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, regionIdx;
    NoteEvent newNote;
};

// ── Delete note in piano roll ───────────────────────────────────────────────
class DeleteNoteAction : public juce::UndoableAction
{
public:
    DeleteNoteAction(PatternFlowProcessor& p, int lane, int region, int noteIdx)
        : proc(p), laneIdx(lane), regionIdx(region), noteIndex(noteIdx) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, regionIdx, noteIndex;
    NoteEvent savedNote;
};

// ── Move/resize note in piano roll ──────────────────────────────────────────
class EditNoteAction : public juce::UndoableAction
{
public:
    EditNoteAction(PatternFlowProcessor& p, int lane, int region, int noteIdx,
                   NoteEvent oldNote, NoteEvent newNote)
        : proc(p), laneIdx(lane), regionIdx(region), noteIndex(noteIdx),
          oldNoteState(std::move(oldNote)), newNoteState(std::move(newNote)) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, regionIdx, noteIndex;
    NoteEvent oldNoteState, newNoteState;
};

// ── Duplicate region ────────────────────────────────────────────────────────
class DuplicateRegionAction : public juce::UndoableAction
{
public:
    DuplicateRegionAction(PatternFlowProcessor& p, int lane, int region)
        : proc(p), laneIdx(lane), regionIdx(region) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, regionIdx;
    int addedRegionIdx = -1;
};

// ── Split region at beat ────────────────────────────────────────────────────
class SplitRegionAction : public juce::UndoableAction
{
public:
    SplitRegionAction(PatternFlowProcessor& p, int lane, int region, double splitBeat)
        : proc(p), laneIdx(lane), regionIdx(region), splitAtBeat(splitBeat) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, regionIdx;
    double splitAtBeat;
    CompRegion origRegion;
    int addedRegionIdx = -1;
};

// ── Move region to a different lane ──────────────────────────────────────────
class MoveRegionToLaneAction : public juce::UndoableAction
{
public:
    MoveRegionToLaneAction(PatternFlowProcessor& p, int srcLane, int regionIdx,
                           int dstLane, double newStart, double newEnd,
                           bool createNewLane = false)
        : proc(p), srcLaneIdx(srcLane), srcRegionIdx(regionIdx),
          dstLaneIdx(dstLane), newStartBeat(newStart), newEndBeat(newEnd),
          needsNewLane(createNewLane) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int srcLaneIdx, srcRegionIdx, dstLaneIdx;
    double newStartBeat, newEndBeat;
    bool needsNewLane;
    CompRegion savedRegion;
    MidiClip savedClip;
    int savedClipIdx = -1;
    int addedRegionIdx = -1;
    int addedClipIdx = -1;
};

} // namespace pflow
