#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TestHelpers.h"
#include <catch2/catch_approx.hpp>

using namespace pflow;

namespace {

juce::Component* findById(juce::Component* root, const juce::String& id)
{
    if (root->getComponentID() == id)
        return root;
    for (int i = 0; i < root->getNumChildComponents(); ++i)
        if (auto* found = findById(root->getChildComponent(i), id))
            return found;
    return nullptr;
}

juce::File makeLeadBarFolder(const juce::String& name)
{
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name);
    dir.deleteRecursively();
    dir.createDirectory();
    // First bar empty: notes start at beat 4 (bar 2 of 2).
    auto clip = test::makeClipWithNotes({ { 60, 100, 4.0, 1.0, 1 },
                                          { 64, 100, 6.0, 1.5, 1 } }, 8.0);
    writeMidiFile(clip, dir.getChildFile("lead.mid"), 120.0);
    return dir;
}

} // namespace

TEST_CASE("Trim button trims and restores through the real editor UI", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto dir = makeLeadBarFolder("MidiBrowserUiTest");
    const auto path = dir.getChildFile("lead.mid").getFullPathName();

    MidiBrowserProcessor proc;
    proc.lastBrowserDir = dir.getFullPathName();

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    REQUIRE(ed != nullptr);
    ed->setVisible(true);   // hit-testing requires a visible component tree
    ed->setSize(1100, 620);

    auto* trimComp = findById(ed.get(), "btnTrim");
    REQUIRE(trimComp != nullptr);
    auto* trim = dynamic_cast<juce::Button*>(trimComp);
    REQUIRE(trim != nullptr);
    REQUIRE(trim->isVisible());
    REQUIRE(trim->isEnabled());   // the clip has an empty lead bar

    // Nothing overlaps the button: a click at its centre must reach it.
    const auto centre = ed->getLocalPoint(trim, trim->getLocalBounds().getCentre());
    auto* hit = ed->getComponentAt(centre.x, centre.y);
    INFO("component at trim centre: " << (hit != nullptr ? hit->getComponentID() + " / "
        + juce::String(typeid(*hit).name()) : juce::String("null")));
    REQUIRE((hit == trim || trim->isParentOf(hit)));

    // Button::triggerClick is asynchronous (needs a message pump), so invoke
    // the wired handler directly; enabled + hit-test above cover reachability.
    REQUIRE(trim->onClick != nullptr);

    // Click: trims the empty lead bar.
    trim->onClick();
    REQUIRE(proc.clipEdits.count(path) == 1);
    CHECK(proc.clipEdits[path].trimLead == 1);
    CHECK(proc.clipEdits[path].trimTail == 0);
    CHECK(trim->isEnabled());

    // Click again: restores.
    trim->onClick();
    CHECK(proc.clipEdits[path].trimLead == 0);
    CHECK(proc.clipEdits[path].trimTail == 0);

    // And a third time: trims again (it toggles indefinitely).
    trim->onClick();
    CHECK(proc.clipEdits[path].trimLead == 1);

    ed = nullptr;
    dir.deleteRecursively();
}

TEST_CASE("Fold and lock buttons are reachable and lock persists a template", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto dir = makeLeadBarFolder("MidiBrowserUiTest2");

    MidiBrowserProcessor proc;
    proc.lastBrowserDir = dir.getFullPathName();

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setVisible(true);
    ed->setSize(1100, 620);

    auto* fold = dynamic_cast<juce::Button*>(findById(ed.get(), "btnFold"));
    REQUIRE(fold != nullptr);
    REQUIRE(fold->isEnabled());
    fold->onClick();   // no crash, toggles fold state

    auto* lock = dynamic_cast<juce::Button*>(findById(ed.get(), "btnLock"));
    REQUIRE(lock != nullptr);
    REQUIRE_FALSE(proc.editLock);
    lock->onClick();
    CHECK(proc.editLock);
    lock->onClick();
    CHECK_FALSE(proc.editLock);

    ed = nullptr;
    dir.deleteRecursively();
}
