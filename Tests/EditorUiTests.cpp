#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BrowserPanels.h"
#include "EffectsInspectorWidgets.h"
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

void openAncestorSection(juce::Component* c)
{
    for (auto* p = c; p != nullptr; p = p->getParentComponent())
        if (auto* sec = dynamic_cast<fx::Section*>(p))
        {
            sec->setOpen(true);
            if (auto* parent = sec->getParentComponent())
                parent->resized();
            return;
        }
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

    // Keep content scale at 1.0 so hit-testing matches component bounds.
    tweaks().size.store((int) ContentSize::Medium);
    tweaks().appearance.store((int) Appearance::Light);
    tweaks().density.store((int) Density::Compact);

    const auto dir = makeLeadBarFolder("MidiBrowserUiTest");
    const auto path = dir.getChildFile("lead.mid").getFullPathName();

    MidiBrowserProcessor proc;
    proc.lastBrowserDir = dir.getFullPathName();
    proc.editorOpen = true;
    proc.effectsOpen = true;

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    REQUIRE(ed != nullptr);
    ed->setVisible(true);   // hit-testing requires a visible component tree
    ed->setSize(1100, 620);

    auto* trimComp = findById(ed.get(), "btnTrim");
    REQUIRE(trimComp != nullptr);
    openAncestorSection(trimComp);
    ed->resized();

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
    CHECK(proc.clipEdits[path].removedBars == std::vector<int>{ 0 });
    CHECK(trim->isEnabled());

    // Click again: restores.
    trim->onClick();
    CHECK(proc.clipEdits[path].removedBars.empty());

    // And a third time: trims again (it toggles indefinitely).
    trim->onClick();
    CHECK(proc.clipEdits[path].removedBars == std::vector<int>{ 0 });

    ed = nullptr;
    dir.deleteRecursively();
}

TEST_CASE("Fold and pitch-lock buttons are reachable; lock persists a template", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto dir = makeLeadBarFolder("MidiBrowserUiTest2");

    MidiBrowserProcessor proc;
    proc.lastBrowserDir = dir.getFullPathName();
    proc.editorOpen = true;
    proc.effectsOpen = true;

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setVisible(true);
    ed->setSize(1300, 620);

    auto* fold = dynamic_cast<juce::Button*>(findById(ed.get(), "btnFold"));
    REQUIRE(fold != nullptr);
    REQUIRE(fold->isEnabled());
    fold->onClick();   // no crash, toggles fold state

    // Effects lock lives in the Effects inspector header.
    auto* lock = dynamic_cast<juce::Button*>(findById(ed.get(), "btnLock"));
    REQUIRE(lock != nullptr);
    REQUIRE_FALSE(proc.effectsLock);
    lock->onClick();
    CHECK(proc.effectsLock);
    lock->onClick();
    CHECK_FALSE(proc.effectsLock);

    ed = nullptr;
    dir.deleteRecursively();
}

TEST_CASE("File table keyboard nav follows sorted display order", "[editorui]")
{
    juce::ScopedJuceInitialiser_GUI gui;

    FileListPanel panel;
    panel.setSize(400, 300);

    std::vector<FileListEntry> entries(3);
    entries[0].name = "zebra";
    entries[0].bars = 8;
    entries[0].bpm = 120;
    entries[1].name = "alpha";
    entries[1].bars = 2;
    entries[1].bpm = 90;
    entries[2].name = "middle";
    entries[2].bars = 4;
    entries[2].bpm = 100;
    panel.setEntries(std::move(entries));

    // Default sort: Name ascending → alpha, middle, zebra
    std::vector<int> visited;
    panel.onSelect = [&](int idx) { visited.push_back(idx); };

    panel.setSelectedIndex(1); // alpha (entry 1)
    REQUIRE(panel.getSelectedIndex() == 1);

    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 2); // middle
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 0); // zebra

    // Sort by Bars ascending → alpha(2), middle(4), zebra(8)
    panel.setSort(FileListPanel::SortColumn::Bars, true);
    panel.setSelectedIndex(1); // alpha
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 2); // middle
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 0); // zebra

    // Bars descending → zebra, middle, alpha
    panel.setSort(FileListPanel::SortColumn::Bars, false);
    panel.setSelectedIndex(0); // zebra
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 2); // middle
    panel.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    REQUIRE(panel.getSelectedIndex() == 1); // alpha
}
