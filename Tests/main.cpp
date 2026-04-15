#include <catch2/catch_test_macros.hpp>
#include "MidiFileData.h"
#include "PluginProcessor.h"
#include "UndoActions.h"
#include "Theme.h"
#include <juce_core/juce_core.h>

using namespace pflow;

// ── MidiFileData tests ───────────────────────────────────────────────────────

TEST_CASE("parseMidiFile returns empty clip for non-existent file", "[MidiFileData]")
{
    juce::File nonexistent("/nonexistent/path/file.mid");
    auto clip = parseMidiFile(nonexistent);
    REQUIRE(clip.notes.empty());
}

TEST_CASE("writeMidiFile and parseMidiFile round-trip", "[MidiFileData]")
{
    MidiClip original;
    original.name = "TestClip";
    original.lengthBeats = 4.0;
    original.notes.push_back({ 60, 100, 0.0, 1.0, 1 });
    original.notes.push_back({ 64, 80, 1.0, 0.5, 1 });
    original.rootNoteOffset = 0;

    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("PatternFlow_test_roundtrip.mid");
    tempFile.deleteFile();

    REQUIRE(writeMidiFile(original, tempFile, 120.0));
    auto parsed = parseMidiFile(tempFile);
    tempFile.deleteFile();

    REQUIRE(parsed.notes.size() == 2);
    REQUIRE(parsed.notes[0].noteNumber == 60);
    REQUIRE(parsed.notes[0].velocity == 100);
    REQUIRE(std::abs(parsed.notes[0].startBeat - 0.0) < 0.01);
    REQUIRE(std::abs(parsed.notes[0].lengthBeats - 1.0) < 0.01);
    REQUIRE(parsed.notes[1].noteNumber == 64);
}

TEST_CASE("writeMidiFile applies rootNoteOffset", "[MidiFileData]")
{
    MidiClip clip;
    clip.name = "Transposed";
    clip.lengthBeats = 2.0;
    clip.notes.push_back({ 60, 100, 0.0, 1.0, 1 });
    clip.rootNoteOffset = 12;  // transpose up octave

    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("PatternFlow_test_offset.mid");
    tempFile.deleteFile();

    REQUIRE(writeMidiFile(clip, tempFile, 120.0));
    auto parsed = parseMidiFile(tempFile);
    tempFile.deleteFile();

    REQUIRE(parsed.notes.size() == 1);
    REQUIRE(parsed.notes[0].noteNumber == 72);  // 60 + 12
}

TEST_CASE("scaleIntervals returns correct intervals for Major", "[MidiFileData]")
{
    auto intervals = scaleIntervals(ScaleType::Major);
    REQUIRE(intervals.size() == 7);
    REQUIRE(intervals[0] == 0);
    REQUIRE(intervals[1] == 2);
    REQUIRE(intervals[2] == 4);
    REQUIRE(intervals[3] == 5);
    REQUIRE(intervals[4] == 7);
    REQUIRE(intervals[5] == 9);
    REQUIRE(intervals[6] == 11);
}

TEST_CASE("quantiseToScale maps to nearest scale degree", "[MidiFileData]")
{
    // C major (root 0): 0,2,4,5,7,9,11 -> C,D,E,F,G,A,B
    REQUIRE(quantiseToScale(60, 0, ScaleType::Major) == 60);   // C stays C
    REQUIRE(quantiseToScale(62, 0, ScaleType::Major) == 62);   // D stays D
    REQUIRE(quantiseToScale(64, 0, ScaleType::Major) == 64);   // E stays E
    REQUIRE(quantiseToScale(65, 0, ScaleType::Major) == 65);   // F stays F
}

// ── PluginProcessor snapBeat tests ────────────────────────────────────────────

TEST_CASE("snapBeat with GridSize::Off snaps to quarter beats", "[PluginProcessor]")
{
    PatternFlowProcessor proc;
    proc.gridSnap.store((int)PatternFlowProcessor::GridSize::Off);
    REQUIRE(proc.snapBeat(3.7) == 3.75);
    REQUIRE(proc.snapBeat(3.62) == 3.5);
}

TEST_CASE("snapBeat with GridSize::Beat snaps to whole beats", "[PluginProcessor]")
{
    PatternFlowProcessor proc;
    proc.gridSnap.store((int)PatternFlowProcessor::GridSize::Beat);
    REQUIRE(proc.snapBeat(2.3) == 2.0);
    REQUIRE(proc.snapBeat(2.6) == 3.0);
}

TEST_CASE("snapBeat with GridSize::Eighth snaps correctly", "[PluginProcessor]")
{
    PatternFlowProcessor proc;
    proc.gridSnap.store((int)PatternFlowProcessor::GridSize::Eighth);
    double div = PatternFlowProcessor::getGridDivision(PatternFlowProcessor::GridSize::Eighth);
    REQUIRE(div == 0.125);
    REQUIRE(std::abs(proc.snapBeat(1.0) - 1.0) < 0.001);
    REQUIRE(std::abs(proc.snapBeat(1.07) - 1.125) < 0.001);  // 1.07 rounds to 1.125
}

TEST_CASE("getGridDivision returns correct values", "[PluginProcessor]")
{
    using GS = PatternFlowProcessor::GridSize;
    REQUIRE(PatternFlowProcessor::getGridDivision(GS::Bar) == 4.0);
    REQUIRE(PatternFlowProcessor::getGridDivision(GS::Beat) == 1.0);
    REQUIRE(PatternFlowProcessor::getGridDivision(GS::HalfBeat) == 0.5);
    REQUIRE(PatternFlowProcessor::getGridDivision(GS::QuarterBeat) == 0.25);
    REQUIRE(PatternFlowProcessor::getGridDivision(GS::Eighth) == 0.125);
    REQUIRE(PatternFlowProcessor::getGridDivision(GS::Sixteenth) == 0.0625);
}

// ── Validation helpers ───────────────────────────────────────────────────────

TEST_CASE("isValidLane validates lane indices", "[PluginProcessor]")
{
    PatternFlowProcessor proc;
    // Processor starts with 0 lanes by default
    REQUIRE_FALSE(proc.isValidLane(0));
    REQUIRE_FALSE(proc.isValidLane(-1));
    REQUIRE_FALSE(proc.isValidLane(1));
    // Create a lane and verify
    {
        juce::ScopedLock sl(proc.laneLock);
        proc.lanes.push_back(CompLane());
    }
    REQUIRE(proc.isValidLane(0));
    REQUIRE_FALSE(proc.isValidLane(-1));
    REQUIRE_FALSE(proc.isValidLane(1));
}

TEST_CASE("isValidRegion validates region indices", "[PluginProcessor]")
{
    PatternFlowProcessor proc;
    REQUIRE_FALSE(proc.isValidRegion(0, 0));  // no lanes
    REQUIRE_FALSE(proc.isValidRegion(-1, 0));
    REQUIRE_FALSE(proc.isValidRegion(1, 0));
    // Create empty lane 0
    {
        juce::ScopedLock sl(proc.laneLock);
        proc.lanes.push_back(CompLane());
    }
    REQUIRE_FALSE(proc.isValidRegion(0, 0));  // lane 0 has no regions
    REQUIRE_FALSE(proc.isValidRegion(-1, 0));
    REQUIRE_FALSE(proc.isValidRegion(1, 0));
}

// ── Undo actions ──────────────────────────────────────────────────────────────

TEST_CASE("AddRegionAction perform and undo", "[UndoActions]")
{
    PatternFlowProcessor proc;
    // Create lane 0 (processor starts with 0 lanes)
    {
        juce::ScopedLock sl(proc.laneLock);
        proc.lanes.push_back(CompLane());
    }

    MidiClip clip;
    clip.name = "Test";
    clip.lengthBeats = 4.0;
    clip.notes.push_back({ 60, 100, 0.0, 1.0, 1 });

    CompRegion region;
    region.startBeat = 0.0;
    region.endBeat = 4.0;
    region.clipIndex = 0;

    AddRegionAction action(proc, 0, region, clip);
    REQUIRE(action.perform());
    {
        juce::ScopedLock sl(proc.laneLock);
        REQUIRE(proc.lanes[0].regions.size() == 1);
        REQUIRE(proc.lanes[0].clips.size() == 1);
    }
    REQUIRE(action.undo());
    {
        juce::ScopedLock sl(proc.laneLock);
        REQUIRE(proc.lanes[0].regions.empty());
        REQUIRE(proc.lanes[0].clips.empty());
    }
}

TEST_CASE("MoveRegionAction perform and undo", "[UndoActions]")
{
    PatternFlowProcessor proc;
    {
        juce::ScopedLock sl(proc.laneLock);
        proc.lanes.push_back(CompLane());
        proc.lanes[0].clips.push_back({});
        CompRegion r;
        r.startBeat = 0.0;
        r.endBeat = 4.0;
        r.clipIndex = 0;
        proc.lanes[0].regions.push_back(r);
    }

    MoveRegionAction action(proc, 0, 0, 0.0, 4.0, 4.0, 8.0);
    REQUIRE(action.perform());
    {
        juce::ScopedLock sl(proc.laneLock);
        REQUIRE(proc.lanes[0].regions[0].startBeat == 4.0);
        REQUIRE(proc.lanes[0].regions[0].endBeat == 8.0);
    }
    REQUIRE(action.undo());
    {
        juce::ScopedLock sl(proc.laneLock);
        REQUIRE(proc.lanes[0].regions[0].startBeat == 0.0);
        REQUIRE(proc.lanes[0].regions[0].endBeat == 4.0);
    }
}
