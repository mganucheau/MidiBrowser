#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "MidiFileData.h"
#include "TestHelpers.h"

using namespace pflow;
using Catch::Approx;

TEST_CASE("parseMidiFile returns empty clip for missing and invalid inputs", "[MidiFileData][qa]")
{
    REQUIRE(parseMidiFile(juce::File("/nonexistent/path/file.mid")).notes.empty());

    const auto emptyFile = test::tempMidiFile("empty.mid");
    emptyFile.getParentDirectory().createDirectory();
    emptyFile.replaceWithText("not midi");
    REQUIRE(parseMidiFile(emptyFile).notes.empty());
    test::removeTempMidiFile(emptyFile);
}

TEST_CASE("writeMidiFile and parseMidiFile round-trip preserves note data", "[MidiFileData][qa]")
{
    auto original = test::makeClipWithNotes({
        { 60, 100, 0.0, 1.0, 1 },
        { 64, 80, 1.0, 0.5, 2 },
    });
    original.name = "RoundTrip";

    const auto tempFile = test::tempMidiFile("roundtrip.mid");
    tempFile.getParentDirectory().createDirectory();
    tempFile.deleteFile();

    REQUIRE(writeMidiFile(original, tempFile, 120.0));
    const auto parsed = parseMidiFile(tempFile);
    test::removeTempMidiFile(tempFile);

    REQUIRE(parsed.notes.size() == 2);
    REQUIRE(parsed.notes[0].noteNumber == 60);
    REQUIRE(parsed.notes[0].velocity == 100);
    REQUIRE(parsed.notes[0].channel == 1);
    REQUIRE(parsed.notes[0].startBeat == Approx(0.0).margin(0.02));
    REQUIRE(parsed.notes[0].lengthBeats == Approx(1.0).margin(0.02));
    REQUIRE(parsed.notes[1].noteNumber == 64);
    REQUIRE(parsed.notes[1].channel == 2);
}

TEST_CASE("writeMidiFile rejects empty clips", "[MidiFileData][qa]")
{
    MidiClip empty;
    const auto tempFile = test::tempMidiFile("reject_empty.mid");
    tempFile.getParentDirectory().createDirectory();
    tempFile.deleteFile();
    REQUIRE_FALSE(writeMidiFile(empty, tempFile, 120.0));
    test::removeTempMidiFile(tempFile);
}

TEST_CASE("writeMidiFile applies rootNoteOffset on export", "[MidiFileData][qa]")
{
    auto clip = test::makeClipWithNotes({ { 60, 100, 0.0, 1.0, 1 } });
    clip.rootNoteOffset = 12;

    const auto tempFile = test::tempMidiFile("offset.mid");
    tempFile.getParentDirectory().createDirectory();
    tempFile.deleteFile();

    REQUIRE(writeMidiFile(clip, tempFile, 120.0));
    const auto parsed = parseMidiFile(tempFile);
    test::removeTempMidiFile(tempFile);

    REQUIRE(parsed.notes.size() == 1);
    REQUIRE(parsed.notes[0].noteNumber == 72);
}

TEST_CASE("writeMidiFile maxLengthBeats truncates exported notes", "[MidiFileData][qa]")
{
    auto clip = test::makeClipWithNotes({
        { 60, 100, 0.0, 1.0, 1 },
        { 62, 90, 2.0, 1.0, 1 },
        { 64, 80, 4.0, 1.0, 1 },
    });

    const auto tempFile = test::tempMidiFile("maxlen.mid");
    tempFile.getParentDirectory().createDirectory();
    tempFile.deleteFile();

    REQUIRE(writeMidiFile(clip, tempFile, 120.0, 3.0));
    const auto parsed = parseMidiFile(tempFile);
    test::removeTempMidiFile(tempFile);

    REQUIRE(parsed.notes.size() == 2);
    for (const auto& n : parsed.notes)
        REQUIRE(n.startBeat < 3.0);
}

TEST_CASE("scaleIntervals cover named scales", "[MidiFileData][qa]")
{
    const auto major = scaleIntervals(ScaleType::Major);
    REQUIRE(major.size() == 7);
    REQUIRE(major == std::vector<int>({ 0, 2, 4, 5, 7, 9, 11 }));

    const auto pent = scaleIntervals(ScaleType::PentatonicMinor);
    REQUIRE(pent.size() == 5);
    REQUIRE(pent == std::vector<int>({ 0, 3, 5, 7, 10 }));

    const auto chrom = scaleIntervals(ScaleType::Chromatic);
    REQUIRE(chrom.size() == 12);
}

TEST_CASE("scaleTypeName returns readable labels", "[MidiFileData][qa]")
{
    REQUIRE(scaleTypeName(ScaleType::Major) == "Major");
    REQUIRE(scaleTypeName(ScaleType::NaturalMinor) == "Natural Minor");
    REQUIRE(scaleTypeName(ScaleType::Blues) == "Blues");
}

TEST_CASE("quantiseToScale maps to nearest scale degree", "[MidiFileData][qa]")
{
    REQUIRE(quantiseToScale(60, 0, ScaleType::Major) == 60);
    REQUIRE(quantiseToScale(61, 0, ScaleType::Major) == 60);
    REQUIRE(quantiseToScale(62, 0, ScaleType::Major) == 62);
    REQUIRE(quantiseToScale(60, 0, ScaleType::Chromatic) == 60);
    REQUIRE(quantiseToScale(61, 0, ScaleType::Chromatic) == 61);
}

TEST_CASE("estimatePitchClassFromNotes weights by duration", "[MidiFileData][qa]")
{
    std::vector<NoteEvent> notes {
        { 60, 100, 0.0, 0.1, 1 },
        { 67, 100, 1.0, 4.0, 1 },
    };
    REQUIRE(estimatePitchClassFromNotes(notes) == 7);
    REQUIRE(estimatePitchClassFromNotes({}) == 0);
}

TEST_CASE("MidiClip pitch helpers and sequence export", "[MidiClip][qa]")
{
    auto clip = test::makeClipWithNotes({
        { 60, 100, 0.0, 1.0, 1 },
        { 60, 90, 2.0, 0.5, 1 },
        { 64, 80, 1.0, 1.0, 1 },
    });

    const auto c60 = clip.getNotesForPitch(60);
    REQUIRE(c60.size() == 2);
    const auto pitches = clip.getDistinctPitches();
    REQUIRE(pitches.size() == 2);
    REQUIRE(pitches[0] == 60);
    REQUIRE(pitches[1] == 64);

    const auto seq = clip.toMidiSequence(120.0);
    REQUIRE(seq.getNumEvents() >= 4);
}

TEST_CASE("CompLane region helpers", "[CompLane][qa]")
{
    CompLane lane;
    auto clip = test::makeClipWithNotes({ { 60, 100, 0.0, 1.0, 1 } }, 4.0);
    lane.addClipAtPosition(clip, 8.0);

    REQUIRE(lane.clips.size() == 1);
    REQUIRE(lane.regions.size() == 1);
    REQUIRE(lane.regions[0].startBeat == Approx(8.0));
    REQUIRE(lane.regions[0].endBeat == Approx(12.0));
    REQUIRE(lane.regions[0].clipIndex == 0);

    lane.removeRegion(0);
    REQUIRE(lane.regions.empty());

    CompRegion region;
    region.startBeat = -4.0;
    region.endBeat = 1.0;
    region.ensureSelectionInBounds();
    REQUIRE(region.startBeat == Approx(0.0));
    REQUIRE(region.endBeat > region.startBeat);
}
