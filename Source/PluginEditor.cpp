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
    e.bars = clip.bars;
    e.edited = edited;
    e.starred = starred;
    return e;
}

bool clipMatches(const StepClip& clip, const juce::File& file, const BrowserSearch& s)
{
    if (s.query.isNotEmpty()
        && !file.getFileNameWithoutExtension().containsIgnoreCase(s.query))
        return false;
    if (s.bpm > 0.0 && std::abs(clip.bpm - s.bpm) > 0.5)
        return false;
    if (s.keyRoot >= 0 && clip.root != s.keyRoot)
        return false;
    if (s.bars > 0 && clip.bars != s.bars)
        return false;
    return true;
}

void scanMidiFiles(const juce::File& dir, bool subdirs,
                   std::vector<StepClip>& out,
                   const std::function<bool(const StepClip&, const juce::File&)>& matches)
{
    if (!dir.isDirectory()) return;

    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.mid;*.midi");
    files.sort();
    for (const auto& f : files)
    {
        auto clip = makeStepClip(parseMidiFile(f));
        if (matches(clip, f))
            out.push_back(std::move(clip));
    }

    if (!subdirs) return;
    auto dirs = dir.findChildFiles(juce::File::findDirectories, false, "*");
    dirs.sort();
    for (const auto& d : dirs)
    {
        if (d.getFileName().startsWithChar('.')) continue;
        scanMidiFiles(d, true, out, matches);
    }
}

} // namespace

MidiBrowserEditor::MidiBrowserEditor(MidiBrowserProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setLookAndFeel(&lnf);
    lnf.refreshColours();
    juce::Desktop::getInstance().addDarkModeSettingListener(this);

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
    transport.onToggleEditor = [this] { toggleEditorFold(); };
    transport.onToggleEffects = [this] { toggleEffectsFold(); };
    transport.onDragToDaw = [this] { startDragExport(); };
    transport.onToggleAppearance = [this]
    {
        auto& t = tweaks();
        t.appearance.store(usesDarkAppearance() ? (int) Appearance::Light
                                                : (int) Appearance::Dark);
        lnf.refreshColours();
        sendLookAndFeelChange();
        transport.refreshAppearanceIcon();
        resized();
        repaint();
    };
    transport.setSynced(processorRef.syncToHost.load());
    transport.setFreeBpm(processorRef.freeBpm.load());
    transport.setBpmMultiplier(processorRef.bpmMultiplier.load());
    transport.setHostBpm(processorRef.hostBpm.load());
    transport.setEditorOpen(processorRef.editorOpen);
    transport.setEffectsOpen(processorRef.effectsOpen);
    content.addAndMakeVisible(transport);

    // ── Sidebar ──
    sidebar.onOpenFolder = [this] { chooseFolder(); };
    sidebar.onPickDir = [this](const juce::String& path)
    {
        const juce::File dir(path);
        if (dir.isDirectory())
        {
            setBrowseMode(0);
            setRootDirectory(dir);
        }
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
    sidebar.onShowStarred = [this]
    {
        setBrowseMode(1);
        loadStarredClips();
    };
    sidebar.onShowSearch = [this] { sidebar.setSearchFormOpen(!sidebar.isSearchFormOpen()); };
    sidebar.onRunSearch = [this](const BrowserSearch& s) { runSearchAsync(s); };
    sidebar.onPickSavedSearch = [this](int idx)
    {
        if (!juce::isPositiveAndBelow(idx, (int) processorRef.savedSearches.size()))
            return;
        activeSavedSearchIdx = idx;
        activeSearch = processorRef.savedSearches[(size_t) idx].search;
        setBrowseMode(2);
        runSearchAsync(activeSearch);
        refreshSidebar();
    };
    sidebar.onRemoveSavedSearch = [this](int idx)
    {
        processorRef.removeSavedSearch(idx);
        if (activeSavedSearchIdx == idx)
            activeSavedSearchIdx = -1;
        else if (activeSavedSearchIdx > idx)
            --activeSavedSearchIdx;
        refreshSidebar();
    };
    sidebar.onSaveCurrentSearch = [this] { saveCurrentSearch(); };
    sidebar.onOpenTweaks = [this] { showTweaksMenu(); };
    sidebar.onCollapsedChanged = [this]
    {
        processorRef.sidebarCollapsed = sidebar.isCollapsed();
        applyLayoutState();
    };
    sidebar.setCollapsed(processorRef.sidebarCollapsed);
    content.addAndMakeVisible(sidebar);

    // ── File list ──
    fileList.onSelect = [this](int displayIdx)
    {
        if (!juce::isPositiveAndBelow(displayIdx, (int) displayRows.size()))
            return;
        const auto& row = displayRows[(size_t) displayIdx];
        if (row.isDirectory)
            return;   // highlight only; enter via Right / click
        selectIndex(row.clipIndex);
    };
    fileList.onPlayRow = [this](int displayIdx)
    {
        if (!juce::isPositiveAndBelow(displayIdx, (int) displayRows.size()))
            return;
        const auto& row = displayRows[(size_t) displayIdx];
        if (row.isDirectory) return;
        selectIndex(row.clipIndex);
        processorRef.previewArmed.store(true);
    };
    fileList.onToggleStar = [this](int displayIdx)
    {
        if (!juce::isPositiveAndBelow(displayIdx, (int) displayRows.size()))
            return;
        const auto& row = displayRows[(size_t) displayIdx];
        if (row.isDirectory || row.clipIndex < 0) return;
        processorRef.toggleStarred(clips[(size_t) row.clipIndex].filePath);
        rebuildEntries();
        if (const int d = displayForClip(selectedIdx); d >= 0)
            fileList.setSelectedIndex(d, juce::dontSendNotification);
    };
    fileList.onEnterParent = [this]
    {
        if (browseMode == 0)
            enterParentFolder();
    };
    fileList.onEnterFolder = [this](int displayIdx) { enterFolderAtDisplay(displayIdx); };
    fileList.onDragFile = [this](const juce::File& f) { startDragOriginalFile(f); };
    content.addAndMakeVisible(fileList);

    content.addAndMakeVisible(previewHeader);
    content.addAndMakeVisible(miniRoll);

    // ── Piano-roll editor ──
    rollEditor.onEditChanged = [this](const ClipEdit& e)
    {
        if (const auto* clip = selectedClip())
        {
            processorRef.editFor(clip->filePath) = e;
            if (processorRef.editLock)
            {
                applyPitchLock(e, processorRef.lockedEdit);
                processorRef.lockAutoTrim = e.hasTrim();
            }
            refreshEntryMeta(selectedIdx);
            pushPreviewToProcessor();
            syncEffectsInspector();
        }
    };
    rollEditor.onTrimStateChanged = [this](bool active, bool enabled, const juce::String& label)
    {
        effectsInspector.setTrimState(active, enabled, label);
    };
    rollEditor.onLoopChanged = [this](double startStep, double endStep)
    {
        const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        processorRef.previewLoopStartBeat.store(startStep * stretch / 4.0);
        processorRef.previewLoopEndBeat.store(endStep * stretch / 4.0);
    };
    applyTimeStretchFromMultiplier();
    content.addAndMakeVisible(rollEditor);

    // ── Effects inspector ──
    effectsInspector.onGrooveChanged = [this](const GrooveParams& k)
    {
        if (const auto* clip = selectedClip())
        {
            processorRef.grooveFor(clip->filePath) = k;
            if (processorRef.effectsLock)
                processorRef.lockedGroove = k;
            rollEditor.setClip(*clip, selectedEdit(), k);
            pushPreviewToProcessor();
            updateMiniPreview();
        }
    };
    effectsInspector.onEditChanged = [this](const ClipEdit& e)
    {
        if (const auto* clip = selectedClip())
        {
            processorRef.editFor(clip->filePath) = e;
            if (processorRef.editLock)
                applyPitchLock(e, processorRef.lockedEdit);
            rollEditor.setClip(*clip, e, selectedGroove());
            refreshEntryMeta(selectedIdx);
            pushPreviewToProcessor();
            updateMiniPreview();
        }
    };
    effectsInspector.onEffectsLockToggled = [this](bool locked)
    {
        processorRef.effectsLock = locked;
        if (locked)
        {
            processorRef.lockedGroove = selectedGroove();
        }
        else
        {
            processorRef.lockedGroove = GrooveParams();
            processorRef.clipGrooves.clear();
            if (const auto* clip = selectedClip())
            {
                rollEditor.setClip(*clip, selectedEdit(), GrooveParams());
                pushPreviewToProcessor();
                updateMiniPreview();
            }
            syncEffectsInspector();
        }
    };
    effectsInspector.onPitchLockToggled = [this](bool locked)
    {
        processorRef.editLock = locked;
        if (locked)
        {
            applyPitchLock(selectedEdit(), processorRef.lockedEdit);
            processorRef.lockAutoTrim = selectedEdit().hasTrim();
        }
    };
    effectsInspector.onTrimClicked = [this] { rollEditor.toggleTrim(); };
    effectsInspector.onResetGroove = [this]
    {
        if (const auto* clip = selectedClip())
        {
            processorRef.clipGrooves.erase(clip->filePath);
            processorRef.lockedGroove = GrooveParams();
            rollEditor.setClip(*clip, selectedEdit(), GrooveParams());
            pushPreviewToProcessor();
            updateMiniPreview();
        }
    };
    effectsInspector.onBpmMultiplierChanged = [this](double m)
    {
        processorRef.bpmMultiplier.store(m);
        transport.setBpmMultiplier(m);
        applyTimeStretchFromMultiplier();
        updateMiniPreview();
    };
    effectsInspector.setEffectsLocked(processorRef.effectsLock);
    juce::ignoreUnused(processorRef.editLock);
    effectsInspector.setBpmMultiplier(processorRef.bpmMultiplier.load());
    content.addAndMakeVisible(effectsInspector);

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
    updateMiniPreview();
}

MidiBrowserEditor::~MidiBrowserEditor()
{
    juce::Desktop::getInstance().removeDarkModeSettingListener(this);
    stopTimer();
    setLookAndFeel(nullptr);
}

void MidiBrowserEditor::darkModeSettingChanged()
{
    if (currentAppearance() != Appearance::System)
        return;
    lnf.refreshColours();
    sendLookAndFeelChange();
    transport.refreshAppearanceIcon();
    repaint();
}

// ── data ─────────────────────────────────────────────────────────────────────

void MidiBrowserEditor::setRootDirectory(const juce::File& dir, bool keepSelection)
{
    browseMode = 0;
    rootDir = dir;
    processorRef.lastBrowserDir = dir.getFullPathName();
    rescanFolder(keepSelection);
    refreshSidebar();
    fileList.grabBrowseFocus();
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

    rebuildEntries();

    int nextSel = clips.empty() ? -1 : 0;
    if (previousPath.isNotEmpty())
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath)
                nextSel = i;

    // Prefer a visible (filtered) selection when possible.
    if (nextSel >= 0 && displayForClip(nextSel) < 0)
    {
        nextSel = -1;
        for (const auto& row : displayRows)
            if (!row.isDirectory) { nextSel = row.clipIndex; break; }
    }

    selectedIdx = -1;
    if (nextSel >= 0)
        selectIndex(nextSel);
    else
    {
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
    }
}

void MidiBrowserEditor::setBrowseMode(int mode)
{
    browseMode = juce::jlimit(0, 2, mode);
    sidebar.setBrowseMode(browseMode);
}

void MidiBrowserEditor::loadStarredClips()
{
    const juce::String previousPath =
        (selectedIdx >= 0 && selectedIdx < (int) clips.size())
            ? clips[(size_t) selectedIdx].filePath : juce::String();

    clips.clear();
    for (const auto& path : processorRef.starredFiles)
    {
        const juce::File f(path);
        if (f.existsAsFile())
            clips.push_back(makeStepClip(parseMidiFile(f)));
    }

    fileList.setFolderName("Favorites");
    rebuildEntries();

    int nextSel = clips.empty() ? -1 : 0;
    if (previousPath.isNotEmpty())
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath)
                nextSel = i;

    selectedIdx = -1;
    if (nextSel >= 0)
        selectIndex(nextSel);
    else
    {
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
    }
    fileList.grabBrowseFocus();
}

bool MidiBrowserEditor::clipMatchesSearch(const StepClip& clip, const juce::File& file,
                                          const BrowserSearch& criteria) const
{
    return clipMatches(clip, file, criteria);
}

void MidiBrowserEditor::runSearch(const BrowserSearch& criteria)
{
    const juce::String previousPath =
        (selectedIdx >= 0 && selectedIdx < (int) clips.size())
            ? clips[(size_t) selectedIdx].filePath : juce::String();

    activeSearch = criteria;
    clips.clear();

    const juce::File searchRoot = rootDir.isDirectory() ? rootDir
        : juce::File(processorRef.lastBrowserDir);
    if (searchRoot.isDirectory())
    {
        const auto matcher = [this, criteria](const StepClip& clip, const juce::File& f)
        {
            return clipMatchesSearch(clip, f, criteria);
        };
        scanMidiFiles(searchRoot, criteria.subdirs, clips, matcher);
    }

    juce::String title = "Search";
    if (criteria.query.isNotEmpty())
        title = criteria.query;
    else if (criteria.bpm > 0.0)
        title = juce::String((int) std::lround(criteria.bpm)) + " BPM";
    fileList.setFolderName(title);
    rebuildEntries();

    int nextSel = clips.empty() ? -1 : 0;
    if (previousPath.isNotEmpty())
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath)
                nextSel = i;

    selectedIdx = -1;
    if (nextSel >= 0)
        selectIndex(nextSel);
    else
    {
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
    }
    fileList.grabBrowseFocus();
}

void MidiBrowserEditor::runSearchAsync(const BrowserSearch& criteria)
{
    sidebar.setSearchFormOpen(false);

    const juce::File searchRoot = rootDir.isDirectory() ? rootDir
        : juce::File(processorRef.lastBrowserDir);
    if (!searchRoot.isDirectory())
        return;

    fileList.setSearching(true);
    activeSearch = criteria;
    activeSavedSearchIdx = -1;
    setBrowseMode(2);

    juce::Thread::launch([this, criteria, searchRoot]
    {
        std::vector<StepClip> found;
        const auto matcher = [criteria](const StepClip& clip, const juce::File& f)
        {
            return clipMatches(clip, f, criteria);
        };
        scanMidiFiles(searchRoot, criteria.subdirs, found, matcher);

        juce::MessageManager::callAsync([this, results = std::move(found), criteria]() mutable
        {
            clips = std::move(results);
            juce::String title = "Search";
            if (criteria.query.isNotEmpty())
                title = criteria.query;
            else if (criteria.bpm > 0.0)
                title = juce::String((int) std::lround(criteria.bpm)) + " BPM";
            fileList.setFolderName(title);
            rebuildEntries();
            fileList.setSearching(false);
            refreshSidebar();

            selectedIdx = -1;
            if (!clips.empty())
                selectIndex(0);
            else
            {
                rollEditor.clearClip();
                syncEffectsInspector();
                processorRef.setPreviewState({}, false, false, false);
            }
            fileList.grabBrowseFocus();
        });
    });
}

void MidiBrowserEditor::saveCurrentSearch()
{
    if (browseMode != 2)
        return;

    auto dialog = std::make_shared<juce::AlertWindow>("Save search",
        "Name this search:", juce::AlertWindow::NoIcon);
    dialog->addTextEditor("name",
        activeSearch.query.isNotEmpty() ? activeSearch.query : "Search", "Name");
    dialog->addButton("Save", 1);
    dialog->addButton("Cancel", 0);

    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dialog](int result)
        {
            if (result != 1) return;
            SavedSearchEntry entry;
            entry.name = dialog->getTextEditorContents("name").trim();
            if (entry.name.isEmpty())
                entry.name = "Search";
            entry.search = activeSearch;
            processorRef.addSavedSearch(entry);
            activeSavedSearchIdx = (int) processorRef.savedSearches.size() - 1;
            refreshSidebar();
        }));
}

void MidiBrowserEditor::rebuildEntries()
{
    displayRows.clear();
    std::vector<FileListEntry> entries;

    if (browseMode == 0 && rootDir.isDirectory())
    {
        auto dirs = rootDir.findChildFiles(juce::File::findDirectories, false, "*");
        dirs.sort();
        for (const auto& d : dirs)
        {
            if (d.getFileName().startsWithChar('.')) continue;
            FileListEntry e;
            e.file = d;
            e.name = d.getFileName();
            e.isDirectory = true;
            entries.push_back(e);
            displayRows.push_back({ true, -1, d });
        }
    }

    for (int i = 0; i < (int) clips.size(); ++i)
    {
        const auto& clip = clips[(size_t) i];
        const bool starred = processorRef.isStarred(clip.filePath);
        const bool edited = processorRef.editLock
                                && !editIsClean(processorRef.editFor(clip.filePath));
        entries.push_back(entryForClip(clip, juce::File(clip.filePath), edited, starred));
        displayRows.push_back({ false, i, juce::File(clip.filePath) });
    }

    fileList.setEntries(std::move(entries));

    if (const int d = displayForClip(selectedIdx); d >= 0)
        fileList.setSelectedIndex(d, juce::dontSendNotification);
}

int MidiBrowserEditor::displayForClip(int clipIdx) const
{
    for (int i = 0; i < (int) displayRows.size(); ++i)
        if (!displayRows[(size_t) i].isDirectory && displayRows[(size_t) i].clipIndex == clipIdx)
            return i;
    return -1;
}

void MidiBrowserEditor::enterFolderAtDisplay(int displayIdx)
{
    if (browseMode != 0)
        return;
    if (!juce::isPositiveAndBelow(displayIdx, (int) displayRows.size()))
        return;
    const auto& row = displayRows[(size_t) displayIdx];
    if (!row.isDirectory || !row.file.isDirectory())
        return;
    setRootDirectory(row.file);
}

void MidiBrowserEditor::enterParentFolder()
{
    if (!rootDir.isDirectory())
        return;
    const auto parent = rootDir.getParentDirectory();
    if (parent != rootDir && parent.isDirectory())
        setRootDirectory(parent);
}

void MidiBrowserEditor::applyTimeStretchFromMultiplier()
{
    const double mult = juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
    const double stretch = 1.0 / mult;
    rollEditor.setTimeStretch(stretch);
    if (selectedClip() != nullptr)
        pushPreviewToProcessor();
}

void MidiBrowserEditor::syncEffectsInspector()
{
    const auto* clip = selectedClip();
    effectsInspector.setHasClip(clip != nullptr);
    effectsInspector.setClipRoot(clip != nullptr ? clip->root : -1);
    effectsInspector.setEdit(selectedEdit(), juce::dontSendNotification);
    effectsInspector.setGroove(selectedGroove(), juce::dontSendNotification);
    effectsInspector.setBpmMultiplier(processorRef.bpmMultiplier.load(), juce::dontSendNotification);
    effectsInspector.setEffectsLocked(processorRef.effectsLock);
    juce::ignoreUnused(processorRef.editLock);
    updateMiniPreview();
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

    // Unlocked pitch edits are ephemeral when leaving a file.
    if (selectedIdx >= 0 && selectedIdx != index && !processorRef.editLock
        && juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
    {
        const auto& prev = clips[(size_t) selectedIdx];
        processorRef.clipEdits.erase(prev.filePath);
        refreshEntryMeta(selectedIdx);
    }
    // Unlocked effects: discard per-file groove so browsing auditions raw clips.
    if (selectedIdx >= 0 && selectedIdx != index && !processorRef.effectsLock
        && juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
    {
        const auto& prev = clips[(size_t) selectedIdx];
        processorRef.clipGrooves.erase(prev.filePath);
    }

    selectedIdx = index;

    // Always release held notes before the new clip starts sounding.
    processorRef.requestNoteFlush();

    const auto& clip = clips[(size_t) index];

    if (processorRef.editLock)
    {
        auto& e = processorRef.editFor(clip.filePath);
        applyPitchLock(processorRef.lockedEdit, e);
        if (processorRef.lockAutoTrim)
        {
            ClipEdit probe = e;
            probe.clearTrim();
            const auto preTrim = resolveClip(clip, probe);
            e.clearTrim();
            e.removedBars = emptyBars(preTrim.notes, clip.bars);
        }
        refreshEntryMeta(index);
    }

    if (processorRef.effectsLock)
        processorRef.grooveFor(clip.filePath) = processorRef.lockedGroove;

    const auto edit = selectedEdit();
    const auto groove = selectedGroove();
    const auto resolved = resolveClip(clip, edit);

    // Keep freerun phase continuous across clip changes so browsing mid-playback
    // feels seamless. Wrap into the new clip length; synced mode already follows
    // the host and does not restart.
    {
        const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        const double newLen = juce::jmax(0.25, (double) (resolved.bars * kStepsPerBar) / 4.0 * stretch);
        double beat = processorRef.freerunBeat.load();
        beat = std::fmod(beat, newLen);
        if (beat < 0.0) beat += newLen;
        processorRef.freerunBeat.store(beat);
    }

    transport.setClipBpm(clip.bpm);
    rollEditor.setClip(clip, edit, groove);
    applyTimeStretchFromMultiplier();
    syncEffectsInspector();

    if (const int d = displayForClip(index); d >= 0)
        fileList.setSelectedIndex(d, juce::dontSendNotification);

    pushPreviewToProcessor();
    updateMiniPreview();
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
    // Non-destructive pipeline: resolveClip → applyGroove → velocities.
    // bpmMultiplier stretches timing so /2 exports half-time MIDI at session tempo.
    MidiClip out;
    const auto* clip = selectedClip();
    if (clip == nullptr)
        return out;

    const auto edit = selectedEdit();
    const auto groove = selectedGroove();
    const auto resolved = resolveClip(*clip, edit);
    const auto notes = applyGroove(resolved.notes, groove);
    const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());

    out.name = clip->name;
    out.bpm = clip->bpm;
    out.lengthBeats = (double) (resolved.bars * kStepsPerBar) / 4.0 * stretch;
    for (const auto& n : notes)
    {
        NoteEvent ev;
        ev.noteNumber = juce::jlimit(0, 127, n.pitch);
        ev.velocity = juce::jlimit(1, 127, (int) std::lround(effectiveVelocity(n, groove) * 127.0));
        ev.startBeat = n.start / 4.0 * stretch;
        ev.lengthBeats = juce::jmax(0.05, n.len / 4.0 * stretch);
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

void MidiBrowserEditor::startDragOriginalFile(const juce::File& file)
{
    if (!file.getFullPathName().isNotEmpty() || !file.existsAsFile())
        return;
    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        juce::StringArray(file.getFullPathName()), true, &fileList);
}

void MidiBrowserEditor::updateMiniPreview()
{
    const auto* clip = selectedClip();
    if (clip == nullptr)
    {
        miniRoll.setNotes({}, 1, {});
        previewHeader.repaint();
        transport.setHasClip(false);
        return;
    }

    transport.setHasClip(true);
    const auto edit = selectedEdit();
    const auto groove = selectedGroove();
    const auto resolved = resolveClip(*clip, edit);
    miniRoll.setNotes(applyGroove(resolved.notes, groove), resolved.bars, groove);
    const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
    miniRoll.setTimeStretch(stretch);
    previewHeader.repaint();
}

// ── Preview header ───────────────────────────────────────────────────────────

void MidiBrowserEditor::PreviewHeader::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromTop(1));

    auto r = getLocalBounds().reduced(10, 0);
    auto caret = r.removeFromLeft(12).toFloat().withSizeKeepingCentre(9.0f, 9.0f);
    drawIcon(g, owner.processorRef.previewOpen ? icons::caretDown : icons::caretUp,
             caret, colours::text3(), 1.5f);
    r.removeFromLeft(6);

    g.setColour(colours::text2());
    g.setFont(uiFont(13.0f, true));
    g.drawText("Preview", r, juce::Justification::centredLeft, true);
}

void MidiBrowserEditor::PreviewHeader::mouseDown(const juce::MouseEvent&)
{
    owner.processorRef.previewOpen = !owner.processorRef.previewOpen;
    owner.applyLayoutState();
    owner.repaint();
}

// ── layout ───────────────────────────────────────────────────────────────────

void MidiBrowserEditor::toggleEditorFold()
{
    processorRef.editorOpen = !processorRef.editorOpen;
    applyLayoutState();
}

void MidiBrowserEditor::toggleEffectsFold()
{
    processorRef.effectsOpen = !processorRef.effectsOpen;
    applyLayoutState();
}

void MidiBrowserEditor::applyLayoutState()
{
    const bool edOpen = processorRef.editorOpen;
    const bool fxOpen = processorRef.effectsOpen;
    const float s = contentScale();
    transport.setEditorOpen(edOpen);
    transport.setEffectsOpen(fxOpen);
    if (getHeight() > 0)
        lastWindowH = getHeight();

    const int side = sidebar.idealWidth();
    const int table = metrics::fileTableW;
    const int logicalW = side + table
                         + (edOpen ? metrics::editorPaneW : 0)
                         + (fxOpen ? metrics::effectsPaneW : 0);
    const int w = juce::roundToInt((float) logicalW * s);
    const int minW = juce::roundToInt((float) (side + metrics::browserMinWidth
                                               + (edOpen ? metrics::openRollMinW : 0)
                                               + (fxOpen ? metrics::effectsPaneW : 0)) * s);
    setResizeLimits(minW, juce::roundToInt(420 * s), 2400, 2000);

    const int previewExtra = processorRef.previewOpen ? metrics::miniRollH() + 34 : 0;
    const int targetH = juce::jmax(juce::roundToInt((460 + previewExtra) * s), lastWindowH);

    layoutAnimFromW = getWidth() > 0 ? getWidth() : w;
    layoutTargetW = w;
    layoutAnimStartMs = juce::Time::getMillisecondCounterHiRes();
    if (std::abs(layoutAnimFromW - layoutTargetW) < 2)
    {
        setSize(w, targetH);
        resized();
    }
}

void MidiBrowserEditor::resized()
{
    const float s = contentScale();
    content.setTransform({});
    content.setBounds(0, 0, juce::roundToInt((float) getWidth() / s),
                      juce::roundToInt((float) getHeight() / s));
    if (std::abs(s - 1.0f) > 1.0e-4f)
        content.setTransform(juce::AffineTransform::scale(s));
    layoutContent();
}

void MidiBrowserEditor::layoutContent()
{
    auto r = content.getLocalBounds();
    const bool edOpen = processorRef.editorOpen;
    const bool fxOpen = processorRef.effectsOpen;
    const bool previewOpen = processorRef.previewOpen;

    transport.setBounds(r.removeFromTop(metrics::transportH()));

    rollEditor.setVisible(edOpen);
    effectsInspector.setVisible(fxOpen);
    const bool showPreview = !edOpen;
    previewHeader.setVisible(showPreview);
    miniRoll.setVisible(showPreview && previewOpen);

    auto row = r;
    sidebar.setBounds(row.removeFromLeft(sidebar.idealWidth()));

    const int browserW = metrics::fileTableW;
    auto browserCol = row.removeFromLeft(browserW);

    int paneX = sidebar.getWidth() + browserW;
    const int paneY = row.getY();
    const int paneH = row.getHeight();
    if (edOpen)
    {
        // Prototype: editor sits in a padded rounded card.
        auto card = juce::Rectangle<int>(paneX, paneY, metrics::editorPaneW, paneH)
                        .reduced(8, 8);
        rollEditor.setBounds(card);
        paneX += metrics::editorPaneW;
    }
    if (fxOpen)
        effectsInspector.setBounds(paneX, paneY, metrics::effectsPaneW, paneH);
    const int previewHeaderH = 26;

    if (showPreview && previewOpen)
    {
        auto preview = browserCol.removeFromBottom(metrics::miniRollH());
        previewHeader.setBounds(browserCol.removeFromBottom(previewHeaderH));
        miniRoll.setBounds(preview);
    }
    else if (showPreview)
    {
        previewHeader.setBounds(browserCol.removeFromBottom(previewHeaderH));
    }

    fileList.setBounds(browserCol);
    browserColW = fileList.getWidth();
}

void MidiBrowserEditor::paint(juce::Graphics& g)
{
    // Flat window fill — matches prototype (no desktop chrome behind content).
    g.fillAll(colours::bg());
}

// ── tweaks ───────────────────────────────────────────────────────────────────

void MidiBrowserEditor::showTweaksMenu()
{
    juce::PopupMenu menu;
    auto& tw = tweaks();

    juce::PopupMenu appearanceMenu;
    appearanceMenu.addItem(400, "System", true, tw.appearance.load() == (int) Appearance::System);
    appearanceMenu.addItem(401, "Light", true, tw.appearance.load() == (int) Appearance::Light);
    appearanceMenu.addItem(402, "Dark", true, tw.appearance.load() == (int) Appearance::Dark);
    menu.addSubMenu("Appearance", appearanceMenu);

    juce::PopupMenu densityMenu;
    densityMenu.addItem(300, "Compact", true, tw.density.load() == 0);
    densityMenu.addItem(301, "Comfortable", true, tw.density.load() == 1);
    menu.addSubMenu("Spacing", densityMenu);

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
            if (result >= 500) { t.size.store(result - 500); sizeChanged = true; }
            else if (result >= 400) t.appearance.store(result - 400);
            else if (result >= 300) t.density.store(result - 300);
            lnf.refreshColours();
            sendLookAndFeelChange();
            transport.refreshAppearanceIcon();
            if (sizeChanged)
                applyLayoutState();
            resized();
            repaint();
        });
}

void MidiBrowserEditor::refreshSidebar()
{
    sidebar.setSavedDirs(processorRef.savedBrowserDirs,
                         rootDir.isDirectory() ? rootDir.getFullPathName() : juce::String());
    sidebar.setSavedSearches(processorRef.savedSearches, activeSavedSearchIdx);
    sidebar.setBrowseMode(browseMode);
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

    // E toggles the editor pane; F toggles effects.
    if (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E')
    {
        toggleEditorFold();
        return true;
    }
    if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
    {
        toggleEffectsFold();
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
        || key == juce::KeyPress::pageUpKey || key == juce::KeyPress::pageDownKey
        || key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey
        || key == juce::KeyPress::returnKey)
    {
        if (!fileList.hasKeyboardFocus(true))
            fileList.grabBrowseFocus();
        return fileList.keyPressed(key);
    }

    if (!fileList.hasKeyboardFocus(true))
        fileList.grabBrowseFocus();
    return fileList.keyPressed(key);
}

void MidiBrowserEditor::timerCallback()
{
    if (layoutTargetW > 0 && std::abs(getWidth() - layoutTargetW) > 1)
    {
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - layoutAnimStartMs;
        const float t = juce::jlimit(0.0f, 1.0f, (float) (elapsed / kLayoutAnimMs));
        // Ease-out cubic
        const float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        const int w = juce::roundToInt((float) layoutAnimFromW
                                       + e * (float) (layoutTargetW - layoutAnimFromW));
        setSize(w, juce::jmax(getHeight(), lastWindowH));
        if (t >= 1.0f)
            layoutTargetW = 0;
    }

    // Transport state is read from processor atomics (updated in processBlock).
    // Do not call getPlayHead()->getPosition() here — AU hosts can crash
    // when the editor timer invokes a stale host callback.

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
    miniRoll.setPlayheadStep(step, sounding);
}

} // namespace pflow
