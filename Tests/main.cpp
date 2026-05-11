#include <catch2/catch_test_macros.hpp>
#include "MidiFileData.h"
#include "Theme.h"
#include <juce_core/juce_core.h>

using namespace pflow;

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
        .getChildFile("MidiBrowser_test_roundtrip.mid");
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
    clip.rootNoteOffset = 12;

    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("MidiBrowser_test_offset.mid");
    tempFile.deleteFile();

    REQUIRE(writeMidiFile(clip, tempFile, 120.0));
    auto parsed = parseMidiFile(tempFile);
    tempFile.deleteFile();

    REQUIRE(parsed.notes.size() == 1);
    REQUIRE(parsed.notes[0].noteNumber == 72);
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
    REQUIRE(quantiseToScale(60, 0, ScaleType::Major) == 60);
    REQUIRE(quantiseToScale(62, 0, ScaleType::Major) == 62);
    REQUIRE(quantiseToScale(64, 0, ScaleType::Major) == 64);
    REQUIRE(quantiseToScale(65, 0, ScaleType::Major) == 65);
}

TEST_CASE("trimEmptyMeasuresInClip removes bar with no notes", "[MidiFileData]")
{
    MidiClip clip;
    clip.lengthBeats = 16.0;
    clip.notes.push_back({ 60, 100, 0.0, 1.0, 1 });
    clip.notes.push_back({ 62, 90, 8.0, 1.0, 1 });

    trimEmptyMeasuresInClip(clip, 4.0);

    REQUIRE(clip.notes.size() == 2);
    REQUIRE(std::abs(clip.notes[0].startBeat - 0.0) < 1e-9);
    REQUIRE(std::abs(clip.notes[1].startBeat - 4.0) < 1e-9);
}
