#include "PluginEditor.h"
#include "BuildInfo.h"

namespace pflow {

namespace {

FileListEntry entryForClip(const StepClip& clip, const juce::File& f,
                           bool edited, bool starred)
{
    FileListEntry e;
    e.file = f;
    e.name = f.getFileNameWithoutExtension();
    e.kind = clip.kind;
    e.rootName = clip.root >= 0 ? juce::String(kNoteNames[(size_t) clip.root]) : juce::String();
    e.bpm = clip.bpm;
    e.edited = edited;
    e.starred = starred;
    return e;
}

bool anyStarredIn(const MidiBrowserProcessor& p, const std::vector<StepClip>& clips)
{
    for (const auto& c : clips)
        if (p.isStarred(c.filePath))
            return true;
    return false;
}

} // namespace

MidiBrowserEditor::MidiBrowserEditor(MidiBrowserProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setLookAndFeel(&lnf);
    lnf.refreshColours();

    content.onLayout = [this] { layoutContent(); };
    addAndMakeVisible(content);

    // ── Transport ──
    transport.onPlayPause = [this]
    {
        const bool arm = !processorRef.previewArmed.load();
        if (arm && !processorRef.syncToHost.load())
            processorRef.freerunBeat.store(processorRef.freerunBeat.load());
        processorRef.previewArmed.store(arm);
    };
    transport.onStop = [this]
    {
        processorRef.previewArmed.store(false);
        processorRef.freerunBeat.store(0.0);
    };
    transport.onSyncChanged = [this](bool synced)
    {
        processorRef.syncToHost.store(synced);
        processorRef.freerunBeat.store(0.0);
    };
    transport.onFreeBpmChanged = [this](double bpm) { processorRef.freeBpm.store(bpm); };
    transport.onMultiplierChanged = [this](double m)
    {
        processorRef.bpmMultiplier.store(m);
    };
    transport.onToggleEditor = [this] { toggleEditorFold(); };
    transport.onDragToDaw = [this] { startDragExport(); };
    transport.setSynced(processorRef.syncToHost.load());
    transport.setFreeBpm(processorRef.freeBpm.load());
    transport.setBpmMultiplier(processorRef.bpmMultiplier.load());
    transport.setHostBpm(processorRef.hostBpm.load());
    content.addAndMakeVisible(transport);

    // ── Sidebar ──
    sidebar.setCollapsed(processorRef.sidebarCollapsed);
    sidebar.onCollapsedChanged = [this]
    {
        processorRef.sidebarCollapsed = sidebar.isCollapsed();
        resized();
    };
    sidebar.onPickDir = [this](const juce::String& path)
    {
        const juce::File dir(path);
        if (dir.isDirectory())
            setRootDirectory(dir);
    };
    sidebar.onAddCurrent = [this]
    {
        if (rootDir.isDirectory())
        {
            processorRef.addSavedBrowserDir(rootDir.getFullPathName());
            refreshSidebar();
        }
    };
    sidebar.onRemoveDir = [this](const juce::String& path)
    {
        processorRef.removeSavedBrowserDir(path);
        refreshSidebar();
    };
    sidebar.onOpenTweaks = [this] { showTweaksMenu(); };
    content.addAndMakeVisible(sidebar);

    // ── File list ──
    fileList.onOpenFolder = [this] { chooseFolder(); };
    fileList.onSelect = [this](int displayIdx)
    {
        if (juce::isPositiveAndBelow(displayIdx, (int) shown.size()))
            selectIndex(shown[(size_t) displayIdx]);
    };
    fileList.onPlayRow = [this](int displayIdx)
    {
        if (juce::isPositiveAndBelow(displayIdx, (int) shown.size()))
        {
            selectIndex(shown[(size_t) displayIdx]);
            processorRef.previewArmed.store(true);
        }
    };
    fileList.onToggleStar = [this](int displayIdx)
    {
        if (!juce::isPositiveAndBelow(displayIdx, (int) shown.size()))
            return;
        const int clipIdx = shown[(size_t) displayIdx];
        processorRef.toggleStarred(clips[(size_t) clipIdx].filePath);
        rebuildEntries();
        // Keep selection visible after filter changes.
        if (const int d = displayForClip(selectedIdx); d >= 0)
            fileList.setSelectedIndex(d, juce::dontSendNotification);
    };
    fileList.onToggleStarFilter = [this]
    {
        starFilterOn = !starFilterOn;
        rebuildEntries();
        if (const int d = displayForClip(selectedIdx); d >= 0)
            fileList.setSelectedIndex(d, juce::dontSendNotification);
        else if (!shown.empty())
            selectIndex(shown.front());
    };
    content.addAndMakeVisible(fileList);

    // ── Piano-roll editor ──
    rollEditor.onEditChanged = [this](const ClipEdit& e)
    {
        if (const auto* clip = selectedClip())
        {
            processorRef.editFor(clip->filePath) = e;
            if (processorRef.editLock)
                applyPitchLock(e, processorRef.lockedEdit);   // keep the template current
            refreshEntryMeta(selectedIdx);
            pushPreviewToProcessor();
            miniRoll.setNotes(applyGroove(resolveClip(*clip, e).notes, selectedGroove()),
                              resolveClip(*clip, e).bars, selectedGroove());
        }
    };
    rollEditor.onGrooveChanged = [this](const GrooveParams& k)
    {
        if (const auto* clip = selectedClip())
        {
            processorRef.grooveFor(clip->filePath) = k;
            pushPreviewToProcessor();
            if (const auto* c = selectedClip())
            {
                const auto resolved = resolveClip(*c, selectedEdit());
                miniRoll.setNotes(applyGroove(resolved.notes, k), resolved.bars, k);
            }
        }
    };
    rollEditor.onLockToggled = [this](bool locked)
    {
        processorRef.editLock = locked;
        if (locked)
            applyPitchLock(selectedEdit(), processorRef.lockedEdit);
        rollEditor.setLockActive(locked);
    };
    rollEditor.setLockActive(processorRef.editLock);
    content.addAndMakeVisible(rollEditor);

    // ── Folded mini preview ──
    content.addChildComponent(miniHeader);
    content.addChildComponent(miniRoll);

    if (processorRef.lastBrowserDir.isNotEmpty())
    {
        const juce::File dir(processorRef.lastBrowserDir);
        if (dir.isDirectory())
            setRootDirectory(dir);
    }
    refreshSidebar();

    setResizable(true, true);
    applyLayoutState();
    startTimerHz(30);
    setWantsKeyboardFocus(true);
}

MidiBrowserEditor::~MidiBrowserEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ── data ─────────────────────────────────────────────────────────────────────

void MidiBrowserEditor::setRootDirectory(const juce::File& dir, bool keepSelection)
{
    rootDir = dir;
    processorRef.lastBrowserDir = dir.getFullPathName();
    rescanFolder(keepSelection);
    refreshSidebar();
}

void MidiBrowserEditor::rescanFolder(bool keepSelection)
{
    const juce::String previousPath =
        (keepSelection && selectedIdx >= 0 && selectedIdx < (int) clips.size())
            ? clips[(size_t) selectedIdx].filePath : juce::String();

    auto files = rootDir.findChildFiles(juce::File::findFiles, false, "*.mid;*.midi");
    files.sort();

    clips.clear();
    for (const auto& f : files)
        clips.push_back(makeStepClip(parseMidiFile(f)));

    fileList.setFolderName(rootDir.isDirectory() ? rootDir.getFileName() : "Select a folder");
    transport.setFolderPath(rootDir.isDirectory() ? rootDir.getFullPathName() : juce::String());

    rebuildEntries();

    int nextSel = clips.empty() ? -1 : 0;
    if (previousPath.isNotEmpty())
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath)
                nextSel = i;

    // Prefer a visible (filtered) selection when possible.
    if (nextSel >= 0 && displayForClip(nextSel) < 0 && !shown.empty())
        nextSel = shown.front();

    selectedIdx = -1;
    if (nextSel >= 0)
        selectIndex(nextSel);
    else
    {
        rollEditor.clearClip();
        processorRef.setPreviewState({}, false, false, false);
    }
}

void MidiBrowserEditor::rebuildEntries()
{
    shown.clear();
    std::vector<FileListEntry> entries;

    for (int i = 0; i < (int) clips.size(); ++i)
    {
        const auto& clip = clips[(size_t) i];
        const bool starred = processorRef.isStarred(clip.filePath);
        if (starFilterOn && !starred)
            continue;

        shown.push_back(i);
        const bool edited = processorRef.editLock
                                && !editIsClean(processorRef.editFor(clip.filePath));
        entries.push_back(entryForClip(clip, juce::File(clip.filePath), edited, starred));
    }

    fileList.setEntries(std::move(entries));
    fileList.setStarFilter(starFilterOn, anyStarredIn(processorRef, clips));

    if (const int d = displayForClip(selectedIdx); d >= 0)
        fileList.setSelectedIndex(d, juce::dontSendNotification);
}

int MidiBrowserEditor::displayForClip(int clipIdx) const
{
    for (int i = 0; i < (int) shown.size(); ++i)
        if (shown[(size_t) i] == clipIdx)
            return i;
    return -1;
}

void MidiBrowserEditor::chooseFolder()
{
    auto chooser = std::make_shared<juce::FileChooser>("Select MIDI folder", rootDir, "");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc)
        {
            if (fc.getResult().isDirectory())
                setRootDirectory(fc.getResult());
        });
}

const StepClip* MidiBrowserEditor::selectedClip() const
{
    if (juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
        return &clips[(size_t) selectedIdx];
    return nullptr;
}

ClipEdit MidiBrowserEditor::selectedEdit() const
{
    if (const auto* c = selectedClip())
        return processorRef.clipEdits.count(c->filePath) > 0
                   ? processorRef.clipEdits.at(c->filePath) : ClipEdit();
    return {};
}

GrooveParams MidiBrowserEditor::selectedGroove() const
{
    if (const auto* c = selectedClip())
        return processorRef.clipGrooves.count(c->filePath) > 0
                   ? processorRef.clipGrooves.at(c->filePath) : GrooveParams();
    return {};
}

void MidiBrowserEditor::selectIndex(int index)
{
    if (!juce::isPositiveAndBelow(index, (int) clips.size()))
        return;

    // Unlocked edits are ephemeral: leaving a file discards its edits/groove
    // so returning plays the original MIDI.
    if (selectedIdx >= 0 && selectedIdx != index && !processorRef.editLock
        && juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
    {
        const auto& prev = clips[(size_t) selectedIdx];
        processorRef.clipEdits.erase(prev.filePath);
        processorRef.clipGrooves.erase(prev.filePath);
        refreshEntryMeta(selectedIdx);
    }

    selectedIdx = index;

    // Always release held notes before the new clip starts sounding.
    processorRef.requestNoteFlush();

    // Reset playhead to 0 on selection change (spec).
    processorRef.freerunBeat.store(0.0);

    const auto& clip = clips[(size_t) index];

    // Browse-lock: stamp the locked pitch edits onto whatever clip we land on.
    if (processorRef.editLock)
    {
        applyPitchLock(processorRef.lockedEdit, processorRef.editFor(clip.filePath));
        refreshEntryMeta(index);
    }
    const auto edit = selectedEdit();
    const auto groove = selectedGroove();

    transport.setClipBpm(clip.bpm);
    rollEditor.setClip(clip, edit, groove);

    const auto resolved = resolveClip(clip, edit);
    miniRoll.setNotes(applyGroove(resolved.notes, groove), resolved.bars, groove);

    if (const int d = displayForClip(index); d >= 0)
        fileList.setSelectedIndex(d, juce::dontSendNotification);

    pushPreviewToProcessor();
}

void MidiBrowserEditor::refreshEntryMeta(int index)
{
    if (!juce::isPositiveAndBelow(index, (int) clips.size()))
        return;
    const int displayIdx = displayForClip(index);
    if (displayIdx < 0)
        return;

    const auto& clip = clips[(size_t) index];
    // Only show the edited dot for locked (persisted) pitch edits.
    const bool edited = processorRef.editLock
                            && !editIsClean(processorRef.editFor(clip.filePath));
    fileList.updateEntry(displayIdx,
                         entryForClip(clip, juce::File(clip.filePath), edited,
                                      processorRef.isStarred(clip.filePath)));
}

MidiClip MidiBrowserEditor::buildRenderedClip() const
{
    // Non-destructive pipeline: resolveClip → applyGroove → velocities. The
    // source clip and file stay untouched.
    MidiClip out;
    const auto* clip = selectedClip();
    if (clip == nullptr)
        return out;

    const auto edit = selectedEdit();
    const auto groove = selectedGroove();
    const auto resolved = resolveClip(*clip, edit);
    const auto notes = applyGroove(resolved.notes, groove);

    out.name = clip->name;
    out.bpm = clip->bpm;
    out.lengthBeats = (double) (resolved.bars * kStepsPerBar) / 4.0;
    for (const auto& n : notes)
    {
        NoteEvent ev;
        ev.noteNumber = juce::jlimit(0, 127, n.pitch);
        ev.velocity = juce::jlimit(1, 127, (int) std::lround(effectiveVelocity(n, groove) * 127.0));
        ev.startBeat = n.start / 4.0;
        ev.lengthBeats = juce::jmax(0.05, n.len / 4.0);
        ev.channel = juce::jlimit(1, 16, n.channel);
        out.notes.push_back(ev);
    }
    return out;
}

void MidiBrowserEditor::pushPreviewToProcessor()
{
    const auto preview = buildRenderedClip();
    processorRef.setPreviewState(preview, !preview.notes.empty(), false, false);
}

void MidiBrowserEditor::startDragExport()
{
    const auto* clip = selectedClip();
    if (clip == nullptr)
        return;
    const auto rendered = buildRenderedClip();
    if (rendered.notes.empty())
        return;

    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("MidiBrowser Drag");
    dir.createDirectory();
    const auto file = dir.getChildFile(
        juce::File::createLegalFileName(clip->name + " (edited).mid"));
    file.deleteFile();
    if (!writeMidiFile(rendered, file, rendered.bpm))
        return;

    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        juce::StringArray(file.getFullPathName()), false, &transport);
}

// ── layout ───────────────────────────────────────────────────────────────────

void MidiBrowserEditor::toggleEditorFold()
{
    processorRef.editorOpen = !processorRef.editorOpen;
    applyLayoutState();
}

void MidiBrowserEditor::applyLayoutState()
{
    const bool open = processorRef.editorOpen;
    const float s = contentScale();
    transport.setEditorOpen(open);
    if (getHeight() > 0)
        lastWindowH = getHeight();

    const int w = juce::roundToInt((float) (open ? metrics::openWindowW
                                                 : metrics::foldedWindowW) * s);
    setResizeLimits(juce::roundToInt((open ? 640 : 280) * s),
                    juce::roundToInt(420 * s), 1920, 2000);
    setSize(w, juce::jmax(juce::roundToInt(460 * s), lastWindowH));
    resized();
}

void MidiBrowserEditor::resized()
{
    // Content-size tweak: scale the whole UI with one transform; children lay
    // out in logical (unscaled) coordinates inside `content`.
    const float s = contentScale();
    content.setTransform(juce::AffineTransform::scale(s));
    content.setBounds(0, 0, juce::roundToInt((float) getWidth() / s),
                      juce::roundToInt((float) getHeight() / s));
    layoutContent();
}

void MidiBrowserEditor::layoutContent()
{
    auto r = content.getLocalBounds();
    const bool open = processorRef.editorOpen;

    transport.setBounds(r.removeFromTop(metrics::transportH()));

    rollEditor.setVisible(open);
    miniHeader.setVisible(!open);

    if (open)
    {
        miniRoll.setVisible(false);
        auto row = r;
        sidebar.setBounds(row.removeFromLeft(sidebar.idealWidth()));
        fileList.setBounds(row.removeFromLeft(metrics::browserWidth));
        rollEditor.setBounds(row);
    }
    else
    {
        // Vertical stack: browser row above a full-width preview panel.
        const int headerH = 26;
        const int previewH = processorRef.miniOpen ? headerH + metrics::miniRollH() + 8 : headerH;
        auto preview = r.removeFromBottom(previewH);

        auto row = r;
        sidebar.setBounds(row.removeFromLeft(sidebar.idealWidth()));
        fileList.setBounds(row);

        miniHeader.setBounds(preview.removeFromTop(headerH));
        miniRoll.setVisible(processorRef.miniOpen);
        if (processorRef.miniOpen)
            miniRoll.setBounds(preview.reduced(8, 2));
    }
}

void MidiBrowserEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::bg());
}

// ── mini header ──────────────────────────────────────────────────────────────

void MidiBrowserEditor::MiniHeader::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromTop(1));

    auto r = getLocalBounds().reduced(10, 0);
    auto caret = r.removeFromLeft(12).toFloat().withSizeKeepingCentre(9.0f, 9.0f);
    drawIcon(g, owner.processorRef.miniOpen ? icons::caretDown : icons::caretUp,
             caret, colours::text3(), 1.5f);
    r.removeFromLeft(6);

    const auto* clip = owner.selectedClip();
    g.setColour(colours::text2());
    g.setFont(uiFont(13.0f, true));
    g.drawText(clip != nullptr ? clip->name : "Preview",
               r.withTrimmedRight(52), juce::Justification::centredLeft, true);

    if (clip != nullptr)
    {
        const auto resolved = resolveClip(*clip, owner.selectedEdit());
        g.setColour(colours::text3());
        g.setFont(monoFont(12.0f, false));
        g.drawText(juce::String(resolved.bars) + " bars", r, juce::Justification::centredRight);
    }
}

void MidiBrowserEditor::MiniHeader::mouseDown(const juce::MouseEvent&)
{
    owner.processorRef.miniOpen = !owner.processorRef.miniOpen;
    owner.resized();
    repaint();
}

// ── tweaks ───────────────────────────────────────────────────────────────────

void MidiBrowserEditor::showTweaksMenu()
{
    juce::PopupMenu menu;
    auto& tw = tweaks();

    juce::PopupMenu themeMenu;
    const char* themeNames[] = { "Charcoal", "Graphite", "Ink" };
    for (int i = 0; i < kNumThemes; ++i)
        themeMenu.addItem(100 + i, themeNames[i], true, tw.theme.load() == i);
    menu.addSubMenu("Theme", themeMenu);

    juce::PopupMenu accentMenu;
    const char* accentNames[] = { "Blue", "Amber", "Mint" };
    for (int i = 0; i < kNumAccents; ++i)
        accentMenu.addItem(200 + i, accentNames[i], true, tw.accent.load() == i);
    menu.addSubMenu("Accent", accentMenu);

    juce::PopupMenu densityMenu;
    densityMenu.addItem(300, "Compact", true, tw.density.load() == 0);
    densityMenu.addItem(301, "Comfortable", true, tw.density.load() == 1);
    menu.addSubMenu("Spacing", densityMenu);

    juce::PopupMenu gridMenu;
    const char* gridNames[] = { "Lanes", "Minimal", "Blueprint" };
    for (int i = 0; i < kNumGridStyles; ++i)
        gridMenu.addItem(400 + i, gridNames[i], true, tw.grid.load() == i);
    menu.addSubMenu("Note grid", gridMenu);

    juce::PopupMenu sizeMenu;
    const char* sizeNames[] = { "Small", "Medium", "Large" };
    for (int i = 0; i < kNumContentSizes; ++i)
        sizeMenu.addItem(500 + i, sizeNames[i], true, tw.size.load() == i);
    menu.addSubMenu("Content size", sizeMenu);

    menu.addSeparator();
    const juce::String stamp = juce::String("Build ")
        + build_info::kVersion + " · "
        + juce::String(build_info::kGitHash).substring(0, 7) + " · "
        + build_info::kGitDateIso;
    menu.addItem(-1, stamp, false);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sidebar),
        [this](int result)
        {
            if (result == 0 || result < 0) return;
            auto& t = tweaks();
            bool sizeChanged = false;
            if (result >= 500)      { t.size.store(result - 500); sizeChanged = true; }
            else if (result >= 400) t.grid.store(result - 400);
            else if (result >= 300) t.density.store(result - 300);
            else if (result >= 200) t.accent.store(result - 200);
            else if (result >= 100) t.theme.store(result - 100);
            lnf.refreshColours();
            sendLookAndFeelChange();
            if (sizeChanged)
                applyLayoutState();   // rescale the window to match
            resized();
            repaint();
        });
}

void MidiBrowserEditor::refreshSidebar()
{
    sidebar.setSavedDirs(processorRef.savedBrowserDirs,
                         rootDir.isDirectory() ? rootDir.getFullPathName() : juce::String());
}

// ── ticking ──────────────────────────────────────────────────────────────────

bool MidiBrowserEditor::keyPressed(const juce::KeyPress& key)
{
    // Space: toggle preview arm. When synced, that follows the DAW playhead
    // (plugin playhead moves with the host). Most VST3 hosts including Live
    // refuse to let plugins start/stop the DAW transport itself.
    if (key == juce::KeyPress::spaceKey)
    {
        if (processorRef.syncToHost.load())
        {
            // Synced: arm/disarm so the plugin follows (or ignores) the host.
            // If the host is already playing, arming starts preview immediately.
            const bool arm = !processorRef.previewArmed.load();
            processorRef.previewArmed.store(arm);
            if (!arm)
                processorRef.freerunBeat.store(0.0);
        }
        else
        {
            const bool arm = !processorRef.previewArmed.load();
            processorRef.previewArmed.store(arm);
            if (!arm)
                processorRef.freerunBeat.store(0.0);
        }
        return true;
    }

    if (rollEditor.isVisible() && rollEditor.hasSelection()
        && (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey))
    {
        rollEditor.deleteSelectedNotes();
        return true;
    }

    if (rollEditor.isVisible() && rollEditor.keyPressed(key))
        return true;

    // Map page/arrow keys through the display list → clip indices.
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey
        || key == juce::KeyPress::pageUpKey || key == juce::KeyPress::pageDownKey)
    {
        const int curDisplay = displayForClip(selectedIdx);
        const int pageRows = juce::jmax(1, fileList.getHeight() / metrics::listRowH());
        int delta = 0;
        if (key == juce::KeyPress::upKey)        delta = -1;
        else if (key == juce::KeyPress::downKey)  delta = 1;
        else if (key == juce::KeyPress::pageUpKey)   delta = -pageRows;
        else if (key == juce::KeyPress::pageDownKey) delta = pageRows;

        if (!shown.empty())
        {
            const int nextDisplay = juce::jlimit(0, (int) shown.size() - 1,
                                                 (curDisplay >= 0 ? curDisplay : 0) + delta);
            selectIndex(shown[(size_t) nextDisplay]);
            return true;
        }
    }

    return fileList.keyPressed(key);
}

void MidiBrowserEditor::timerCallback()
{
    // Hosts often skip processBlock while stopped — pull tempo/position here
    // so the BPM readout stays live even when the transport isn't running.
    if (auto* playHead = processorRef.getPlayHead())
    {
        if (const auto posInfo = playHead->getPosition())
        {
            if (auto bpm = posInfo->getBpm(); bpm.hasValue())
                processorRef.hostBpm.store(*bpm);
            if (auto ppq = posInfo->getPpqPosition(); ppq.hasValue())
                processorRef.hostBeatPos.store(*ppq);
            processorRef.hostPlaying.store(posInfo->getIsPlaying());
        }
    }

    const bool sounding = processorRef.isPreviewSounding();
    transport.setPlaying(sounding);
    fileList.setPlaying(sounding);

    // Keep the BPM readout live with the host (× multiplier when synced).
    transport.setHostBpm(processorRef.hostBpm.load());
    transport.setBpmMultiplier(processorRef.bpmMultiplier.load());

    double step = 0.0;
    if (const auto* clip = selectedClip())
    {
        const auto resolved = resolveClip(*clip, selectedEdit());
        const double lenBeats = juce::jmax(0.25, (double) (resolved.bars * kStepsPerBar) / 4.0);
        const double mult = juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        double beat = 0.0;
        if (processorRef.syncToHost.load())
        {
            // Match processor: scaled host position wraps over the clip.
            const double scaled = processorRef.hostBeatPos.load() * mult;
            beat = std::fmod(scaled, lenBeats);
            if (beat < 0.0) beat += lenBeats;
        }
        else
        {
            beat = std::fmod(processorRef.freerunBeat.load(), lenBeats);
        }
        step = beat * 4.0;
    }
    rollEditor.setPlayheadStep(step, sounding);
    if (miniRoll.isVisible())
        miniRoll.setPlayheadStep(step, sounding);
}

} // namespace pflow
