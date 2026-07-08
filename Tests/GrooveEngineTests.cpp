#include "GrooveEngine.h"
#include "TestHelpers.h"
#include <catch2/catch_approx.hpp>

using namespace pflow;
using Catch::Approx;

// Golden values in this file were computed by running the verbatim
// Prototype/mbd/knob.jsx functions in Node (Math.imul semantics).

namespace {

std::vector<RollNote> testNotes()
{
    return {
        { 0, 60, 0.0, 4.0 },
        { 1, 62, 2.0, 2.0 },
        { 2, 64, 5.0, 1.0 },
        { 3, 65, 6.0, 2.0 },
        { 4, 67, 14.0, 2.0 },
        { 5, 69, 31.0, 3.0 },
    };
}

} // namespace

TEST_CASE("knob defs match the spec table", "[groove]")
{
    REQUIRE(kKnobDefs.size() == 6);
    CHECK(juce::String(kKnobDefs[0].key) == "swing");
    CHECK(kKnobDefs[0].min == 0);   CHECK(kKnobDefs[0].max == 75);  CHECK(kKnobDefs[0].def == 0);
    CHECK(juce::String(kKnobDefs[1].key) == "pocket");
    CHECK(kKnobDefs[1].min == -100); CHECK(kKnobDefs[1].max == 100); CHECK(kKnobDefs[1].def == 0);
    CHECK(kKnobDefs[1].bipolar());
    CHECK_FALSE(kKnobDefs[0].bipolar());
    CHECK(juce::String(kKnobDefs[4].key) == "length");
    CHECK(kKnobDefs[4].min == 25);  CHECK(kKnobDefs[4].max == 200); CHECK(kKnobDefs[4].def == 100);
    CHECK(juce::String(kKnobDefs[5].key) == "intensity");
    CHECK(kKnobDefs[5].def == 80);
}

TEST_CASE("GrooveParams defaults and activeCount", "[groove]")
{
    GrooveParams k;
    CHECK(k.isDefault());
    CHECK(k.activeCount() == 0);

    k.swing = 30;
    k.intensity = 60;
    CHECK(k.activeCount() == 2);
    CHECK_FALSE(k.isDefault());

    k.set(0, 999);   // clamps to knob max
    CHECK(k.swing == 75);
    k.set(1, -999);
    CHECK(k.pocket == -100);
}

TEST_CASE("baseVel matches prototype hash", "[groove]")
{
    CHECK(baseVel(0) == Approx(0.92));
    CHECK(baseVel(1) == Approx(0.69));
    CHECK(baseVel(2) == Approx(0.70));
    CHECK(baseVel(3) == Approx(0.71));
    CHECK(baseVel(4) == Approx(0.48));
    CHECK(baseVel(5) == Approx(0.49));
    CHECK(baseVel(17) == Approx(0.53));
    CHECK(baseVel(100) == Approx(0.64));
}

TEST_CASE("swing delays only offbeat 8ths", "[groove]")
{
    GrooveParams k;
    k.swing = 50;
    const auto g = applyGroove(testNotes(), k);
    CHECK(g[0].start == Approx(0.0));    // eighth 0 (even)
    CHECK(g[1].start == Approx(2.5));    // eighth 1 (odd) -> +0.5
    CHECK(g[2].start == Approx(5.0));    // eighth 2
    CHECK(g[3].start == Approx(6.5));    // eighth 3
    CHECK(g[4].start == Approx(14.5));   // eighth 7
    CHECK(g[5].start == Approx(31.5));   // eighth 15
}

TEST_CASE("pocket shifts every note, clamped at 0", "[groove]")
{
    GrooveParams k;
    k.pocket = -75;
    const auto g = applyGroove(testNotes(), k);
    CHECK(g[0].start == Approx(0.0));    // clamped
    CHECK(g[1].start == Approx(0.8));
    CHECK(g[2].start == Approx(3.8));
    CHECK(g[3].start == Approx(4.8));
    CHECK(g[4].start == Approx(12.8));
    CHECK(g[5].start == Approx(29.8));
}

TEST_CASE("humanize applies deterministic per-id jitter", "[groove]")
{
    GrooveParams k;
    k.humanize = 60;
    const auto g = applyGroove(testNotes(), k);
    CHECK(g[0].start == Approx(0.28188));
    CHECK(g[1].start == Approx(1.70408));
    CHECK(g[2].start == Approx(5.52596));
    CHECK(g[3].start == Approx(5.94816));
    CHECK(g[4].start == Approx(14.45036));
    CHECK(g[5].start == Approx(31.19224));

    // Deterministic: same input, same output.
    const auto g2 = applyGroove(testNotes(), k);
    for (size_t i = 0; i < g.size(); ++i)
        CHECK(g[i].start == Approx(g2[i].start));
}

TEST_CASE("combined groove matches prototype golden values", "[groove]")
{
    GrooveParams k;
    k.swing = 75;
    k.pocket = 40;
    k.humanize = 100;
    k.length = 25;
    const auto g = applyGroove(testNotes(), k);
    CHECK(g[0].start == Approx(1.1098));
    CHECK(g[0].len == Approx(1.0));
    CHECK(g[1].start == Approx(2.8968));
    CHECK(g[1].len == Approx(0.5));      // floor at 0.5
    CHECK(g[2].start == Approx(6.5166));
    CHECK(g[3].start == Approx(7.3036));
    CHECK(g[4].start == Approx(16.1406));
    CHECK(g[5].start == Approx(32.7104));
    CHECK(g[5].len == Approx(0.75));
}

TEST_CASE("applyGroove never mutates its input", "[groove]")
{
    const auto notes = testNotes();
    GrooveParams k;
    k.swing = 75;
    k.pocket = 100;
    k.humanize = 100;
    k.length = 25;
    (void) applyGroove(notes, k);
    CHECK(notes[1].start == Approx(2.0));
    CHECK(notes[1].len == Approx(2.0));
}

TEST_CASE("noteVelocity: intensity is a flat scale", "[groove]")
{
    GrooveParams k;   // defaults: dynamics 0, intensity 80
    const auto notes = testNotes();
    CHECK(noteVelocity(notes[0], k) == Approx(0.736));
    CHECK(noteVelocity(notes[1], k) == Approx(0.552));
    CHECK(noteVelocity(notes[2], k) == Approx(0.560));
    CHECK(noteVelocity(notes[3], k) == Approx(0.568));
    CHECK(noteVelocity(notes[4], k) == Approx(0.384));
    CHECK(noteVelocity(notes[5], k) == Approx(0.392));
}

TEST_CASE("noteVelocity: dynamics adds metric-position contrast", "[groove]")
{
    GrooveParams k;
    k.dynamics = 100;
    k.intensity = 100;
    const auto notes = testNotes();
    CHECK(noteVelocity(notes[0], k) == Approx(1.0));     // downbeat, clamped
    CHECK(noteVelocity(notes[1], k) == Approx(0.58));    // 8th offbeat
    CHECK(noteVelocity(notes[2], k) == Approx(0.37));    // in-between 16th
    CHECK(noteVelocity(notes[3], k) == Approx(0.60));    // 8th offbeat
    CHECK(noteVelocity(notes[4], k) == Approx(0.37));    // pos 14
    CHECK(noteVelocity(notes[5], k) == Approx(0.16));    // pos 15 (16th)
}

TEST_CASE("noteVelocity: dynamics and intensity compose", "[groove]")
{
    GrooveParams k;
    k.dynamics = 65;
    k.intensity = 40;
    const auto notes = testNotes();
    CHECK(noteVelocity(notes[0], k) == Approx(0.511));
    CHECK(noteVelocity(notes[1], k) == Approx(0.2474));
    CHECK(noteVelocity(notes[2], k) == Approx(0.1942));
    CHECK(noteVelocity(notes[3], k) == Approx(0.2554));
    CHECK(noteVelocity(notes[4], k) == Approx(0.1634));
    CHECK(noteVelocity(notes[5], k) == Approx(0.1102));
}

TEST_CASE("noteVelocity clamps to [0.04, 1]", "[groove]")
{
    GrooveParams k;
    k.dynamics = 100;
    k.intensity = 100;
    RollNote ghost { 4, 60, 15.0, 1.0 };   // baseVel(4)=0.48, 16th accent -0.6
    CHECK(noteVelocity(ghost, k) == Approx(0.48 - 0.6 * 0.55));

    k.intensity = 1;
    CHECK(noteVelocity(ghost, k) == Approx(0.04));   // floor

    RollNote loud { 0, 60, 0.0, 1.0 };     // baseVel(0)=0.92 + 0.55
    k.intensity = 100;
    CHECK(noteVelocity(loud, k) == Approx(1.0));     // ceiling
}

TEST_CASE("noteVelocityWithBase uses file velocities as the base", "[groove]")
{
    GrooveParams k;
    k.intensity = 100;
    RollNote n { 42, 60, 0.0, 1.0 };
    CHECK(noteVelocityWithBase(0.5, n, k) == Approx(0.5));

    k.dynamics = 100;
    CHECK(noteVelocityWithBase(0.5, n, k) == Approx(1.0));   // 0.5 + 0.55 clamps

    n.start = 15.0;   // in-between 16th: 0.5 - 0.6*0.55
    CHECK(noteVelocityWithBase(0.5, n, k) == Approx(0.17));
}
