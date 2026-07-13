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
    e.timeSigNum = clip.timeSigNum;
    e.timeSigDen = clip.timeSigDen;
    e.noteCount = clip.noteCount;
    e.difNotes = clip.difNotes;
    e.complexity = clip.complexity;
    e.edited = edited;
    e.starred = starred;
    return e;
}

bool clipMatches(const StepClip& clip, const juce::File& file, const BrowserSearch& s)
{
    if (s.query.isNotEmpty()
        && !file.getFileNameWithoutExtension().containsIgnoreCase(s.query))
        return false;

    if (s.bpmMin > 0.0 && clip.bpm < s.bpmMin - 0.5)
        return false;
    if (s.bpmMax > 0.0 && clip.bpm > s.bpmMax + 0.5)
        return false;

    if (s.keyRoot >= 0 && clip.root != s.keyRoot)
        return false;

    if (s.barsMin > 0)
    {
        if (s.barsMin >= 64)
        {
            if (clip.bars < 64)
                return false;
        }
        else if (clip.bars < s.barsMin)
        {
            return false;
        }
    }
    if (s.barsMax > 0 && s.barsMax < 64 && clip.bars > s.barsMax)
        return false;

    return true;
}

juce::String searchBpmTitle(const BrowserSearch& criteria)
{
    if (criteria.bpmMin > 0.0 && criteria.bpmMax > 0.0)
    {
        if (std::abs(criteria.bpmMin - criteria.bpmMax) < 0.5)
            return juce::String((int) std::lround(criteria.bpmMin)) + " BPM";
        return juce::String((int) std::lround(criteria.bpmMin)) + "-"
             + juce::String((int) std::lround(criteria.bpmMax)) + " BPM";
    }
    if (criteria.bpmMin > 0.0)
        return juce::String((int) std::lround(criteria.bpmMin)) + "+ BPM";
    if (criteria.bpmMax > 0.0)
        return "<=" + juce::String((int) std::lround(criteria.bpmMax)) + " BPM";
    return {};
}

juce::String formatBarsBound(int v)
{
    if (v <= 0) return {};
    if (v >= 64) return "64+";
    return juce::String(v);
}

juce::String searchBarsLabel(const BrowserSearch& criteria)
{
    const auto lo = formatBarsBound(criteria.barsMin);
    const auto hi = formatBarsBound(criteria.barsMax);
    if (lo.isNotEmpty() && hi.isNotEmpty())
    {
        if (lo == hi) return lo + " bars";
        return lo + "-" + hi + " bars";
    }
    if (lo.isNotEmpty()) return lo + "+ bars";
    if (hi.isNotEmpty()) return "<=" + hi + " bars";
    return {};
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
        // Enabling sync while the DAW is already playing should start preview.
        if (synced && processorRef.hostPlaying.load())
            processorRef.previewArmed.store(true);
    };
    transport.onFreeBpmChanged = [this](double bpm) { processorRef.freeBpm.store(bpm); };
    transport.onToggleEditor = [this] { toggleEditorFold(); };
    transport.onToggleEffects = [this] { toggleEffectsFold(); };
    transport.onDragToDaw = [this] { startDragExport(); };
    transport.onCopyToFolder = [this] { copyRenderedClipToFolder(); };
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
        juce::File dir = rootDir;
        if (!dir.isDirectory() && processorRef.lastBrowserDir.isNotEmpty())
            dir = juce::File(processorRef.lastBrowserDir);
        if (!dir.isDirectory())
            return;
        processorRef.addSavedBrowserDir(dir.getFullPathName());
        refreshSidebar();
        // Keep the saved list visible.
        if (sidebar.isCollapsed())
            sidebar.setCollapsed(false);
    };
    sidebar.onRemoveDir = [this](const juce::String& path)
    {
        processorRef.removeSavedBrowserDir(path);
        refreshSidebar();
    };
    sidebar.onShowStarred = [this]
    {
        if (browseMode == 1)
        {
            // Leave the global starred library; return to the folder browser.
            starredFilter = false;
            sidebar.setStarredFilter(false);
            setBrowseMode(0);
            if (rootDir.isDirectory())
                rescanFolder(true);
            else
            {
                clips.clear();
                rebuildEntries();
            }
            return;
        }

        starredFilter = false;
        sidebar.setStarredFilter(true); // highlights the Starred row
        setBrowseMode(1);
        loadStarredClips();
        refreshSidebar();
    };
    sidebar.onShowSearch = [this]
    {
        const bool open = !sidebar.isSearchFormOpen();
        sidebar.setSearchFormOpen(open);
    };
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
    sidebar.onCopyStarredToFolder = [this] { copyStarredToFolder(); };
    sidebar.onOpenSettings = [this] { showTweaksMenu(); };
    sidebar.onCollapsedChanged = [this]
    {
        processorRef.sidebarCollapsed = sidebar.isCollapsed();
        applyLayoutState();
    };
    sidebar.onWidthChanged = [this]
    {
        // Live-resize without animating the window — file list absorbs the slack.
        layoutContent();
    };
    sidebar.setCollapsed(processorRef.sidebarCollapsed);
    content.addAndMakeVisible(sidebar);

    // ── File list ──
    fileList.onSelect = [this](int entryIdx)
    {
        if (!juce::isPositiveAndBelow(entryIdx, (int) displayRows.size()))
            return;
        const auto& row = displayRows[(size_t) entryIdx];
        if (row.isDirectory)
            return;   // highlight only; enter via Right / click
        selectIndex(row.clipIndex);
    };
    fileList.onPlayRow = [this](int entryIdx)
    {
        if (!juce::isPositiveAndBelow(entryIdx, (int) displayRows.size()))
            return;
        const auto& row = displayRows[(size_t) entryIdx];
        if (row.isDirectory) return;
        selectIndex(row.clipIndex);
        processorRef.previewArmed.store(true);
    };
    fileList.onToggleStar = [this](int entryIdx)
    {
        if (!juce::isPositiveAndBelow(entryIdx, (int) displayRows.size()))
            return;
        const auto& row = displayRows[(size_t) entryIdx];
        if (row.isDirectory || row.clipIndex < 0) return;
        processorRef.toggleStarred(clips[(size_t) row.clipIndex].filePath);
        if (browseMode == 1)
            loadStarredClips(); // drop unstarred from the global list
        else
            rebuildEntries();
        if (const int d = displayForClip(selectedIdx); d >= 0)
            fileList.setSelectedIndex(d, juce::dontSendNotification);
    };
    fileList.onEnterParent = [this]
    {
        if (browseMode == 0)
            enterParentFolder();
    };
    fileList.onEnterFolder = [this](int entryIdx) { enterFolderAtDisplay(entryIdx); };
    fileList.onDragFile = [this](const juce::File& f) { startDragOriginalFile(f); };
    fileList.onEmptyOpenFolder = [this] { chooseFolder(); };
    fileList.onActivated = [this] { claimKeyNav(KeyNavTarget::Browser); };
    fileList.onRevealFile = [](const juce::File& f) { f.revealToUser(); };
    fileList.onCopyFileToFolder = [this](const juce::File& f) { copyFileToFolder(f); };
    fileList.setColumnVisibility(processorRef.columnVisibility);
    fileList.onColumnVisibilityChanged = [this](const BrowserColumnVisibility& v)
    {
        processorRef.columnVisibility = v;
        applyLayoutState();
    };
    content.addAndMakeVisible(fileList);

    content.addAndMakeVisible(previewHeader);
    content.addAndMakeVisible(miniRoll);

    // ── Piano-roll editor ──
    rollEditor.onEditChanged = [this](const ClipEdit& e)
    {
        if (const auto* clip = selectedClip())
        {
            processorRef.editFor(clip->filePath) = e;
            if (processorRef.effectsLock || processorRef.editLock)
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
        const double startBeat = startStep * stretch / 4.0;
        const double endBeat = endStep * stretch / 4.0;
        processorRef.previewLoopStartBeat.store(startBeat);
        processorRef.previewLoopEndBeat.store(endBeat);
        // Keep the free-run playhead inside the new region immediately.
        double beat = processorRef.freerunBeat.load();
        const double loopLen = juce::jmax(0.25, endBeat - startBeat);
        if (beat < startBeat || beat >= endBeat)
        {
            beat = startBeat + std::fmod(std::max(0.0, beat - startBeat), loopLen);
            if (beat < startBeat) beat += loopLen;
            processorRef.freerunBeat.store(beat);
        }
    };
    rollEditor.onActivated = [this] { claimKeyNav(KeyNavTarget::Editor); };
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
            if (processorRef.effectsLock || processorRef.editLock)
            {
                applyPitchLock(e, processorRef.lockedEdit);
                processorRef.lockAutoTrim = e.hasTrim();
            }
            rollEditor.setClip(*clip, e, selectedGroove());
            refreshEntryMeta(selectedIdx);
            pushPreviewToProcessor();
            updateMiniPreview();
        }
    };
    effectsInspector.onEffectsLockToggled = [this](bool locked)
    {
        processorRef.effectsLock = locked;
        processorRef.editLock = locked;
        if (locked)
        {
            processorRef.lockedGroove = selectedGroove();
            applyPitchLock(selectedEdit(), processorRef.lockedEdit);
            processorRef.lockAutoTrim = selectedEdit().hasTrim();
        }
        else
        {
            processorRef.lockedGroove = GrooveParams();
            processorRef.lockedEdit = ClipEdit();
            processorRef.lockAutoTrim = false;
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
    effectsInspector.onActivated = [this] { claimKeyNav(KeyNavTarget::Effects); };
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
    repaint();
}

// ── data ─────────────────────────────────────────────────────────────────────

void MidiBrowserEditor::setRootDirectory(const juce::File& dir, bool keepSelection)
{
    browseMode = 0;
    starredFilter = false;
    sidebar.setStarredFilter(false);
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

    fileList.setFolderName("Starred");
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
    else if (const auto bpmTitle = searchBpmTitle(criteria); bpmTitle.isNotEmpty())
        title = bpmTitle;
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
    starredFilter = false;
    sidebar.setStarredFilter(false);
    setBrowseMode(2);

    // Fast path: reuse cached paths from a previous identical search.
    if (const auto* cached = processorRef.library().findSearchCache(searchRoot, criteria))
    {
        std::vector<StepClip> found;
        found.reserve((size_t) cached->resultPaths.size());
        bool missing = false;
        for (const auto& path : cached->resultPaths)
        {
            const juce::File f(path);
            if (!f.existsAsFile())
            {
                missing = true;
                break;
            }
            found.push_back(makeStepClip(parseMidiFile(f)));
        }

        if (!missing)
        {
            clips = std::move(found);
            juce::String title = "Search";
            if (criteria.query.isNotEmpty())
                title = criteria.query;
            else if (const auto bpmTitle = searchBpmTitle(criteria); bpmTitle.isNotEmpty())
                title = bpmTitle;
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
            return;
        }
    }

    juce::Thread::launch([this, criteria, searchRoot]
    {
        std::vector<StepClip> found;
        const auto matcher = [criteria](const StepClip& clip, const juce::File& f)
        {
            return clipMatches(clip, f, criteria);
        };
        scanMidiFiles(searchRoot, criteria.subdirs, found, matcher);

        juce::StringArray resultPaths;
        for (const auto& c : found)
            resultPaths.add(c.filePath);

        juce::MessageManager::callAsync([this, results = std::move(found), criteria, searchRoot,
                                         paths = std::move(resultPaths)]() mutable
        {
            processorRef.library().putSearchCache(searchRoot, criteria, paths);

            clips = std::move(results);
            juce::String title = "Search";
            if (criteria.query.isNotEmpty())
                title = criteria.query;
            else if (const auto bpmTitle = searchBpmTitle(criteria); bpmTitle.isNotEmpty())
                title = bpmTitle;
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

    SavedSearchEntry entry;
    entry.search = activeSearch;

    juce::StringArray parts;
    if (activeSearch.query.isNotEmpty())
        parts.add(activeSearch.query);
    if (const auto bpmLabel = searchBpmTitle(activeSearch); bpmLabel.isNotEmpty())
        parts.add(bpmLabel);
    if (activeSearch.keyRoot >= 0 && activeSearch.keyRoot < 12)
        parts.add(kNoteNames[(size_t) activeSearch.keyRoot]);
    if (const auto barsLabel = searchBarsLabel(activeSearch); barsLabel.isNotEmpty())
        parts.add(barsLabel);
    entry.name = parts.isEmpty() ? "Search" : parts.joinIntoString(" - ");

    processorRef.addSavedSearch(entry);
    activeSavedSearchIdx = (int) processorRef.savedSearches.size() - 1;
    refreshSidebar();
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
        // Starred library mode already lists only favourites — don't re-filter.
        if (starredFilter && browseMode != 1 && !starred)
            continue;
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
    if (selectedIdx >= 0 && selectedIdx != index
        && !(processorRef.editLock || processorRef.effectsLock)
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

    if (processorRef.editLock || processorRef.effectsLock)
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
    const bool edited = (processorRef.editLock || processorRef.effectsLock)
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

    auto sustain = buildSustainPedalAutomation(notes, out.lengthBeats / juce::jmax(0.25, stretch), groove);
    if (!sustain.empty())
    {
        // Automation is stored in unstretched beat time matching note events after stretch.
        for (auto& p : sustain)
            p.beat *= stretch;
        out.automation[64] = std::move(sustain);
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

void MidiBrowserEditor::copyFileToFolder(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    auto chooser = std::make_shared<juce::FileChooser>("Copy to Folder",
                                                       rootDir.isDirectory() ? rootDir
                                                           : juce::File::getSpecialLocation(
                                                                 juce::File::userDocumentsDirectory),
                                                       "");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [chooser, file](const juce::FileChooser& fc)
        {
            const auto destDir = fc.getResult();
            if (!destDir.isDirectory())
                return;
            const auto dest = destDir.getChildFile(file.getFileName());
            if (dest.existsAsFile())
                dest.deleteFile();
            file.copyFileTo(dest);
        });
}

void MidiBrowserEditor::copyStarredToFolder()
{
    auto chooser = std::make_shared<juce::FileChooser>("Copy All Starred to Folder",
                                                       rootDir.isDirectory() ? rootDir
                                                           : juce::File::getSpecialLocation(
                                                                 juce::File::userDocumentsDirectory),
                                                       "");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto destDir = fc.getResult();
            if (!destDir.isDirectory())
                return;
            for (const auto& path : processorRef.starredFiles)
            {
                const juce::File src(path);
                if (!src.existsAsFile())
                    continue;
                const auto dest = destDir.getChildFile(src.getFileName());
                if (dest.existsAsFile())
                    dest.deleteFile();
                src.copyFileTo(dest);
            }
        });
}

void MidiBrowserEditor::copyRenderedClipToFolder()
{
    const auto* clip = selectedClip();
    if (clip == nullptr)
        return;
    const auto rendered = buildRenderedClip();
    if (rendered.notes.empty())
        return;

    auto chooser = std::make_shared<juce::FileChooser>("Copy Edited Clip to Folder",
                                                       rootDir.isDirectory() ? rootDir
                                                           : juce::File::getSpecialLocation(
                                                                 juce::File::userDocumentsDirectory),
                                                       "");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser, rendered, name = clip->name](const juce::FileChooser& fc)
        {
            const auto destDir = fc.getResult();
            if (!destDir.isDirectory())
                return;
            const auto dest = destDir.getChildFile(
                juce::File::createLegalFileName(name + " (edited).mid"));
            if (dest.existsAsFile())
                dest.deleteFile();
            writeMidiFile(rendered, dest, rendered.bpm);
        });
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
    // Frame from source notes so pitch/octave edits stay visible in the strip
    // (fitting to resolved notes would re-center and hide transposition).
    const int rootPc = edit.root >= 0 ? edit.root : (clip->root >= 0 ? clip->root : 0);
    miniRoll.setNotes(applyGroove(resolved.notes, groove), resolved.bars, groove,
                      rootPc, clip->notes, edit.mode, 4);
    const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
    miniRoll.setTimeStretch(stretch);
    previewHeader.repaint();
}

// ── Preview header ───────────────────────────────────────────────────────────

void MidiBrowserEditor::PreviewHeader::paint(juce::Graphics& g)
{
    g.fillAll(inspectorTokens().panelBg);
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
    transport.setEditorOpen(edOpen);
    transport.setEffectsOpen(fxOpen);
    if (getHeight() > 0)
        lastWindowH = getHeight();

    // Layout sizes already include contentScale() — no AffineTransform (breaks hit-testing).
    const int side = sidebar.idealWidth();
    // Window width always budgets the full expanded sidebar so shrinking it
    // in-place can hand width to the file list without growing the window.
    const int sideBudget = sidebar.isCollapsed() ? side : metrics::sidebarExpandedWidth();
    const int baseTable = metrics::fileTableWidth();
    // Grow the browser (and window) up to the width needed for visible columns,
    // then stop — editor/effects stay fixed.
    const int columnsW = fileList.idealContentWidth();
    const int tableMax = juce::jmax(baseTable, columnsW);
    const int panes = (edOpen ? metrics::editorPaneWidth() : 0)
                    + (fxOpen ? metrics::effectsPaneWidth() : 0);
    const int baseW = sideBudget + baseTable + panes;
    const int maxW = sideBudget + tableMax + panes;
    const int minW = metrics::sidebarRailWidth() + metrics::browserMinWidthScaled()
                     + (edOpen ? metrics::openRollMinWidth() : 0)
                     + (fxOpen ? metrics::effectsPaneWidth() : 0);
    setResizeLimits(minW, metrics::scaled(420), maxW, 2000);

    const int previewExtra = processorRef.previewOpen
                                 ? metrics::miniRollH() + metrics::scaled(34) : 0;
    const int targetH = juce::jmax(metrics::scaled(460) + previewExtra, lastWindowH);

    // Prefer fitting columns when they exceed the base table width; otherwise
    // keep the current width (user can drag between base and max).
    const int neededW = sideBudget + juce::jmax(baseTable, columnsW) + panes;
    const int currentW = getWidth() > 0 ? getWidth() : baseW;
    const int targetW = juce::jlimit(baseW, maxW, juce::jmax(currentW, neededW));

    layoutAnimFromW = currentW;
    layoutTargetW = targetW;
    layoutAnimStartMs = juce::Time::getMillisecondCounterHiRes();
    if (std::abs(layoutAnimFromW - layoutTargetW) < 2)
    {
        setSize(targetW, targetH);
        resized();
    }
}

void MidiBrowserEditor::resized()
{
    content.setTransform({});
    content.setBounds(getLocalBounds());
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
    const int sideW = sidebar.idealWidth();
    const int sideBudget = sidebar.isCollapsed() ? sideW : metrics::sidebarExpandedWidth();
    sidebar.setBounds(row.removeFromLeft(sideW));

    const int panes = (edOpen ? metrics::editorPaneWidth() : 0)
                    + (fxOpen ? metrics::effectsPaneWidth() : 0);
    const int baseTable = metrics::fileTableWidth() + juce::jmax(0, sideBudget - sideW);
    const int columnsW = fileList.idealContentWidth();
    const int tableMax = juce::jmax(baseTable, columnsW);
    // Any extra window width goes only to the browser column; other panes stay fixed.
    const int available = juce::jmax(baseTable, row.getWidth() - panes);
    const int browserW = juce::jlimit(baseTable, tableMax, available);
    auto browserCol = row.removeFromLeft(browserW);

    int paneX = sideW + browserW;
    const int paneY = row.getY();
    const int paneH = row.getHeight();
    if (edOpen)
    {
        // Full-bleed piano roll — no card margins or rounded chrome.
        rollEditor.setBounds(paneX, paneY, metrics::editorPaneWidth(), paneH);
        paneX += metrics::editorPaneWidth();
    }
    if (fxOpen)
        effectsInspector.setBounds(paneX, paneY, metrics::effectsPaneWidth(), paneH);
    const int previewHeaderH = metrics::scaled(26);

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

namespace {

/** In-app settings popover — inspector-style rows with title + popup. */
class SettingsPanel : public juce::Component
{
public:
    std::function<void()> onChanged;

    SettingsPanel()
    {
        appearancePopup.setItems({ "System", "Light", "Dark" }, tweaks().appearance.load());
        spacingPopup.setItems({ "Compact", "Comfortable" }, tweaks().density.load());
        sizePopup.setItems({ "Small", "Medium", "Large" }, tweaks().size.load());

        auto wire = [this](fx::FlatPopup& p, std::function<void(int)> apply)
        {
            p.onChange = [this, apply](int idx)
            {
                apply(idx);
                if (onChanged) onChanged();
            };
            addAndMakeVisible(p);
        };
        wire(appearancePopup, [](int i) { tweaks().appearance.store(i); });
        wire(spacingPopup, [](int i) { tweaks().density.store(i); });
        wire(sizePopup, [](int i) { tweaks().size.store(i); });

        setSize(260, 148);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        g.setColour(t.panelBg);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        g.setColour(t.controlHairline);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 0.5f);

        g.setFont(uiFont(13.0f, true));
        g.setColour(colours::text());
        g.drawText("Settings", getLocalBounds().removeFromTop(34).reduced(14, 0),
                   juce::Justification::centredLeft);

        g.setFont(uiFont(11.5f, false));
        g.setColour(t.rowLabel);
        auto body = getLocalBounds().withTrimmedTop(34).reduced(14, 4);
        const int rowH = 28;
        const char* labels[] = { "Appearance", "Spacing", "Size" };
        for (int i = 0; i < 3; ++i)
        {
            auto row = body.removeFromTop(rowH);
            g.drawText(labels[i], row, juce::Justification::centredLeft);
            body.removeFromTop(2);
        }
    }

    void resized() override
    {
        auto body = getLocalBounds().withTrimmedTop(34).reduced(14, 4);
        const int rowH = 28;
        const int ctrlH = 22;
        auto place = [&](fx::FlatPopup& p)
        {
            auto row = body.removeFromTop(rowH);
            const int w = juce::jmin(p.idealWidth() + 8, 120);
            p.setBounds(row.removeFromRight(w).withSizeKeepingCentre(w, ctrlH));
            body.removeFromTop(2);
        };
        place(appearancePopup);
        place(spacingPopup);
        place(sizePopup);
    }

private:
    fx::FlatPopup appearancePopup, spacingPopup, sizePopup;
};

} // namespace

void MidiBrowserEditor::showTweaksMenu()
{
    if (auto* existing = content.findChildWithID("settingsOverlay"))
    {
        content.removeChildComponent(existing);
        delete existing;
        return;
    }

    struct Overlay : juce::Component
    {
        SettingsPanel panel;
        std::function<void()> onDismiss;

        Overlay()
        {
            addAndMakeVisible(panel);
            panel.setSize(280, 156);
        }

        void resized() override
        {
            panel.setBounds(getLocalBounds().withSizeKeepingCentre(panel.getWidth(), panel.getHeight()));
        }

        void paint(juce::Graphics& g) override
        {
            // Soft scrim, no drop shadow on the panel itself.
            g.setColour(juce::Colours::black.withAlpha(0.18f));
            g.fillAll();
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (!panel.getBounds().contains(e.getPosition()) && onDismiss)
                onDismiss();
        }
    };

    auto* overlay = new Overlay();
    overlay->setComponentID("settingsOverlay");
    overlay->panel.onChanged = [this]
    {
        lnf.refreshColours();
        sendLookAndFeelChange();
        applyLayoutState();
        resized();
        repaint();
    };
    overlay->onDismiss = [this, overlay]
    {
        content.removeChildComponent(overlay);
        delete overlay;
    };
    overlay->setBounds(content.getLocalBounds());
    content.addAndMakeVisible(overlay);
    overlay->toFront(true);
}

void MidiBrowserEditor::refreshSidebar()
{
    sidebar.setSavedDirs(processorRef.savedBrowserDirs,
                         rootDir.isDirectory() ? rootDir.getFullPathName() : juce::String());
    sidebar.setSavedSearches(processorRef.savedSearches, activeSavedSearchIdx);
    sidebar.setBrowseMode(browseMode);
    sidebar.setStarredFilter(starredFilter);
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

    const bool arrowNav = key == juce::KeyPress::upKey || key == juce::KeyPress::downKey
        || key == juce::KeyPress::pageUpKey || key == juce::KeyPress::pageDownKey
        || key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey
        || key == juce::KeyPress::returnKey;

    if (arrowNav)
    {
        // Sticky pane: arrows stay with the last clicked window.
        if (keyNavTarget == KeyNavTarget::Editor)
        {
            if (rollEditor.isVisible() && rollEditor.hasSelection() && key.getModifiers().isShiftDown())
            {
                if (rollEditor.keyPressed(key))
                    return true;
            }
            // Consume arrows in the editor so they don't cycle focus elsewhere.
            return true;
        }
        if (keyNavTarget == KeyNavTarget::Effects)
            return true;   // effects are mouse-driven; don't steal browser rows

        fileList.grabBrowseFocus();
        return fileList.keyPressed(key);
    }

    if (keyNavTarget == KeyNavTarget::Editor && rollEditor.isVisible() && rollEditor.keyPressed(key))
        return true;

    if (keyNavTarget == KeyNavTarget::Browser)
    {
        if (!fileList.hasKeyboardFocus(true))
            fileList.grabBrowseFocus();
        return fileList.keyPressed(key);
    }

    return false;
}

void MidiBrowserEditor::claimKeyNav(KeyNavTarget target)
{
    keyNavTarget = target;
    // Hosts often only deliver keys to the focused plugin component. Hold focus
    // on the editor shell and route by sticky target instead of child focus.
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (focused != this && !isParentOf(focused))
            focused->giveAwayKeyboardFocus();
    grabKeyboardFocus();
    if (target == KeyNavTarget::Browser)
        fileList.grabKeyboardFocus();
    else if (target == KeyNavTarget::Editor && rollEditor.isVisible())
        rollEditor.grabKeyboardFocus();
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
        const double stretch = 1.0 / juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        const double lenBeats = juce::jmax(0.25, (double) (resolved.bars * kStepsPerBar) / 4.0 * stretch);
        const double mult = juce::jlimit(0.25, 4.0, processorRef.bpmMultiplier.load());
        double loopStart = processorRef.previewLoopStartBeat.load();
        double loopEnd = processorRef.previewLoopEndBeat.load();
        if (loopEnd <= loopStart + 1.0e-9)
        {
            loopStart = 0.0;
            loopEnd = lenBeats;
        }
        const double loopLen = juce::jmax(0.25, loopEnd - loopStart);

        double beat = 0.0;
        if (processorRef.syncToHost.load())
        {
            const double scaled = processorRef.hostBeatPos.load() * mult;
            beat = loopStart + std::fmod(std::fmod(scaled, loopLen) + loopLen, loopLen);
        }
        else
        {
            beat = processorRef.freerunBeat.load();
            if (beat < loopStart || beat >= loopEnd)
                beat = loopStart + std::fmod(std::max(0.0, beat - loopStart), loopLen);
        }
        // Playhead is drawn in unstretched step space.
        step = beat / stretch * 4.0;
    }
    rollEditor.setPlayheadStep(step, sounding);
    miniRoll.setPlayheadStep(step, sounding);
}

} // namespace pflow
