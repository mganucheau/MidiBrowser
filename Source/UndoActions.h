#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include "MidiFileData.h"

namespace pflow {

class PatternFlowProcessor;

// ── Add clip to lane ────────────────────────────────────────────────────────
class AddClipAction : public juce::UndoableAction
{
public:
    AddClipAction(PatternFlowProcessor& p, int lane, MidiClip clip, double beatPos)
        : proc(p), laneIdx(lane), newClip(std::move(clip)), clipBeatPos(beatPos) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx;
    MidiClip newClip;
    double clipBeatPos;
    int addedClipIdx = -1;
};

// ── Remove clip from lane ──────────────────────────────────────────────────
class RemoveClipAction : public juce::UndoableAction
{
public:
    RemoveClipAction(PatternFlowProcessor& p, int lane, int clipIdx)
        : proc(p), laneIdx(lane), clipIndex(clipIdx) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx;
    int clipIndex;
    MidiClip savedClip;
    double savedBeatPos = 0.0;
};

// ── Move clip (change beat position) ───────────────────────────────────────
class MoveClipAction : public juce::UndoableAction
{
public:
    MoveClipAction(PatternFlowProcessor& p, int lane, int clipIdx,
                   double oldBeatPos, double newBeatPos)
        : proc(p), laneIdx(lane), clipIndex(clipIdx),
          oldPos(oldBeatPos), newPos(newBeatPos) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, clipIndex;
    double oldPos, newPos;
};

// ── Move clip to a different lane ──────────────────────────────────────────
class MoveClipToLaneAction : public juce::UndoableAction
{
public:
    MoveClipToLaneAction(PatternFlowProcessor& p, int srcLane, int clipIdx,
                         int dstLane, double newBeatPos, bool createNewLane = false)
        : proc(p), srcLaneIdx(srcLane), srcClipIdx(clipIdx),
          dstLaneIdx(dstLane), newPos(newBeatPos),
          needsNewLane(createNewLane) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int srcLaneIdx, srcClipIdx, dstLaneIdx;
    double newPos;
    bool needsNewLane;
    MidiClip savedClip;
    double savedBeatPos = 0.0;
    int addedClipIdx = -1;
};

// ── Add note in piano roll ─────────────────────────────────────────────────
class AddNoteAction : public juce::UndoableAction
{
public:
    AddNoteAction(PatternFlowProcessor& p, int lane, int clipIdx, NoteEvent note)
        : proc(p), laneIdx(lane), clipIndex(clipIdx), newNote(std::move(note)) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, clipIndex;
    NoteEvent newNote;
};

// ── Delete note in piano roll ──────────────────────────────────────────────
class DeleteNoteAction : public juce::UndoableAction
{
public:
    DeleteNoteAction(PatternFlowProcessor& p, int lane, int clipIdx, int noteIdx)
        : proc(p), laneIdx(lane), clipIndex(clipIdx), noteIndex(noteIdx) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, clipIndex, noteIndex;
    NoteEvent savedNote;
};

// ── Move/resize note in piano roll ─────────────────────────────────────────
class EditNoteAction : public juce::UndoableAction
{
public:
    EditNoteAction(PatternFlowProcessor& p, int lane, int clipIdx, int noteIdx,
                   NoteEvent oldNote, NoteEvent newNote)
        : proc(p), laneIdx(lane), clipIndex(clipIdx), noteIndex(noteIdx),
          oldNoteState(std::move(oldNote)), newNoteState(std::move(newNote)) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, clipIndex, noteIndex;
    NoteEvent oldNoteState, newNoteState;
};

// ── Duplicate clip ─────────────────────────────────────────────────────────
class DuplicateClipAction : public juce::UndoableAction
{
public:
    DuplicateClipAction(PatternFlowProcessor& p, int lane, int clipIdx)
        : proc(p), laneIdx(lane), clipIndex(clipIdx) {}

    bool perform() override;
    bool undo() override;

private:
    PatternFlowProcessor& proc;
    int laneIdx, clipIndex;
    int addedClipIdx = -1;
};

} // namespace pflow
