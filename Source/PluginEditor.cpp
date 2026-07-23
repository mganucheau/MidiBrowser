#include "PluginEditor.h"
#include "BuildInfo.h"
#include "NativeWindowChrome.h"
#include <set>

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

bool clipMatchesFilter(const StepClip& clip, const BrowserSearch& s)
{
    if (s.bpmMin > 0.0 && clip.bpm < s.bpmMin - 0.5)
        return false;
    if (s.bpmMax > 0.0 && clip.bpm > s.bpmMax + 0.5)
        return false;

    if (s.keyMask != 0)
    {
        if (clip.root < 0 || (s.keyMask & (uint16_t) (1u << clip.root)) == 0)
            return false;
    }
    else if (s.keyRoot >= 0 && clip.root != s.keyRoot)
    {
        return false;
    }

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

    if (s.complexityMin > 0 && clip.complexity < s.complexityMin)
        return false;
    if (s.complexityMax > 0 && clip.complexity > s.complexityMax)
        return false;

    return true;
}

bool clipMatchesQuery(const juce::File& file, const BrowserSearch& s)
{
    if (s.query.isEmpty())
        return true;
    return file.getFileNameWithoutExtension().containsIgnoreCase(s.query);
}

bool clipMatches(const StepClip& clip, const juce::File& file, const BrowserSearch& s)
{
    return clipMatchesQuery(file, s) && clipMatchesFilter(clip, s);
}

/** Query + recurse identity for disk cache (filters are applied after load). */
BrowserSearch searchScanKey(const BrowserSearch& s)
{
    BrowserSearch k;
    k.query = s.query;
    k.subdirs = s.subdirs;
    return k;
}

/** Non-mutating dedupe: returns primary path -> all locations; hiddenPaths are non-primaries. */
std::map<juce::String, juce::StringArray>
computeDedupeLocations(const std::vector<StepClip>& clips,
                       std::set<juce::String>& hiddenPaths)
{
    hiddenPaths.clear();
    std::map<juce::String, juce::StringArray> locationsByPrimary;
    if (clips.empty())
        return locationsByPrimary;

    auto contentKey = [](const juce::File& f) -> juce::String
    {
        juce::uint32 h = 2166136261u;
        if (juce::FileInputStream in (f); in.openedOk())
        {
            char buf[4096];
            while (! in.isExhausted())
            {
                const auto n = in.read(buf, (int) sizeof(buf));
                for (int i = 0; i < n; ++i)
                {
                    h ^= (juce::uint8) buf[i];
                    h *= 16777619u;
                }
            }
        }
        return f.getFileName().toLowerCase()
             + "|" + juce::String(f.getSize())
             + "|" + juce::String::toHexString((int) h);
    };

    std::map<juce::String, juce::String> keyToPrimary;
    for (const auto& clip : clips)
    {
        const juce::File f(clip.filePath);
        const juce::String key = contentKey(f);
        if (key.isEmpty())
            continue;

        if (auto it = keyToPrimary.find(key); it != keyToPrimary.end())
        {
            const auto& primary = it->second;
            auto& locs = locationsByPrimary[primary];
            if (locs.isEmpty())
                locs.add(primary);
            if (!locs.contains(clip.filePath))
                locs.add(clip.filePath);
            if (clip.filePath != primary)
                hiddenPaths.insert(clip.filePath);
            continue;
        }

        keyToPrimary[key] = clip.filePath;
        locationsByPrimary[clip.filePath] = juce::StringArray { clip.filePath };
    }
    return locationsByPrimary;
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
    transport.setReserveTrafficLights(
        processorRef.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
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
        const auto path = dir.getFullPathName();
        if (processorRef.savedBrowserDirs.contains(path))
            processorRef.removeSavedBrowserDir(path);
        else
            processorRef.addSavedBrowserDir(path);
        refreshSidebar();
        if (sidebar.isCollapsed())
            sidebar.setCollapsed(false);
    };
    sidebar.onRefreshFolder = [this]
    {
        if (!rootDir.isDirectory() && processorRef.lastBrowserDir.isNotEmpty())
            rootDir = juce::File(processorRef.lastBrowserDir);
        if (!rootDir.isDirectory() || sidebarScanning)
            return;

        sidebarScanning = true;
        sidebar.setScanning(true);
        const int gen = ++folderScanGeneration;
        const juce::File dir = rootDir;
        const bool recursive = sidebar.getIncludeSubdirs();
        const juce::String previousPath =
            (selectedIdx >= 0 && selectedIdx < (int) clips.size())
                ? clips[(size_t) selectedIdx].filePath : juce::String();

        juce::Component::SafePointer<MidiBrowserEditor> safe(this);
        juce::Thread::launch([safe, dir, recursive, gen, previousPath]
        {
            std::vector<StepClip> found;
            try
            {
                if (recursive)
                {
                    scanMidiFiles(dir, true, found,
                                  [](const StepClip&, const juce::File&) { return true; });
                }
                else
                {
                    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.mid;*.midi");
                    files.sort();
                    for (const auto& f : files)
                        found.push_back(makeStepClip(parseMidiFile(f)));
                }
            }
            catch (...)
            {
                found.clear();
            }

            juce::MessageManager::callAsync([safe, results = std::move(found), gen, previousPath,
                                             dir]() mutable
            {
                if (safe == nullptr || gen != safe->folderScanGeneration.load())
                    return;
                safe->sidebarScanning = false;
                safe->sidebar.setScanning(false);
                if (!safe->rootDir.isDirectory()
                    || safe->rootDir.getFullPathName() != dir.getFullPathName())
                    return;

                safe->clips = std::move(results);
                auto name = dir.getFileName();
                if (name.isEmpty()) name = dir.getFullPathName();
                if (safe->sidebar.getIncludeSubdirs()) name += " (all)";
                safe->fileList.setFolderName(name);
                safe->rebuildEntries();

                int nextSel = safe->clips.empty() ? -1 : 0;
                if (previousPath.isNotEmpty())
                    for (int i = 0; i < (int) safe->clips.size(); ++i)
                        if (safe->clips[(size_t) i].filePath == previousPath)
                            nextSel = i;
                if (nextSel >= 0 && safe->displayForClip(nextSel) < 0)
                {
                    nextSel = -1;
                    for (const auto& row : safe->displayRows)
                        if (!row.isDirectory) { nextSel = row.clipIndex; break; }
                }
                safe->selectedIdx = -1;
                if (nextSel >= 0)
                    safe->selectIndex(nextSel);
                else
                {
                    safe->rollEditor.clearClip();
                    safe->syncEffectsInspector();
                    safe->processorRef.setPreviewState({}, false, false, false);
                }
                safe->refreshSidebar();
            });
        });
    };
    sidebar.onIncludeSubdirsChanged = [this](bool on)
    {
        // `on` is the new toggle state (sidebar already applied it).
        if (!rootDir.isDirectory() && processorRef.lastBrowserDir.isNotEmpty())
            rootDir = juce::File(processorRef.lastBrowserDir);
        if (!rootDir.isDirectory())
            return;

        processorRef.lastBrowserDir = rootDir.getFullPathName();
        setBrowseMode(0);
        starredFilter = false;
        sidebar.setStarredFilter(false);
        activeSavedSearchIdx = -1;
        ++searchGeneration;
        // Force recursiveBrowse from the toggle (do not rely on stale flag).
        recursiveBrowse = on;
        rescanFolder(true);
        refreshSidebar();
        fileList.grabBrowseFocus();
        persistBrowserSession();
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
        // Toggle / focus the SEARCH section.
        sidebar.setSearchFormOpen(!sidebar.isSearchFormOpen());
    };
    sidebar.onRunSearch = [this](const BrowserSearch& s) { runSearchAsync(s); };
    sidebar.onFilterChanged = [this] { applyBrowserFilter(); };
    sidebar.onExitSearch = [this]
    {
        // × clears criteria, collapses the form, and leaves search results.
        ++searchGeneration; // cancel any in-flight async search
        fileList.setSearching(false);
        if (browseMode == 2)
        {
            setBrowseMode(0);
            searchDuplicateLocations.clear();
            activeSavedSearchIdx = -1;
            if (rootDir.isDirectory())
                rescanFolder(true);
            else if (processorRef.lastBrowserDir.isNotEmpty()
                     && juce::File(processorRef.lastBrowserDir).isDirectory())
            {
                setRootDirectory(juce::File(processorRef.lastBrowserDir), true);
            }
            else
            {
                clips.clear();
                rebuildEntries();
                rollEditor.clearClip();
                syncEffectsInspector();
                processorRef.setPreviewState({}, false, false, false);
            }
        }
        refreshSidebar();
        fileList.grabBrowseFocus();
    };
    sidebar.onPickSavedSearch = [this](int idx)
    {
        if (!juce::isPositiveAndBelow(idx, (int) processorRef.savedSearches.size()))
            return;
        const auto& entry = processorRef.savedSearches[(size_t) idx];
        activeSavedSearchIdx = idx;
        activeSearch = entry.search;
        sidebar.setCriteria(activeSearch);
        starredFilter = false;
        sidebar.setStarredFilter(false);

        // Instant restore from in-session snapshot (no rescan / reparse).
        if (const auto it = savedSearchSnapshots.find(idx); it != savedSearchSnapshots.end())
        {
            const auto& snap = it->second;
            if (LibraryStore::searchesEqual(snap.criteria, entry.search)
                && (entry.rootPath.isEmpty() || snap.rootPath == entry.rootPath))
            {
                applySearchSnapshot(snap.clips, snap.locations, snap.criteria, idx,
                                    snap.rootPath, false);
                refreshSidebar();
                return;
            }
        }

        // Persisted result paths on the saved search itself (cross-session).
        if (entry.resultPaths.size() > 0)
        {
            std::vector<StepClip> found;
            found.reserve((size_t) entry.resultPaths.size());
            for (const auto& path : entry.resultPaths)
            {
                const juce::File f(path);
                if (f.existsAsFile())
                    found.push_back(makeStepClip(parseMidiFile(f)));
            }
            if (!found.empty())
            {
                applySearchSnapshot(std::move(found), {}, entry.search, idx,
                                    entry.rootPath, false);
                refreshSidebar();
                return;
            }
            // All cached files are gone — fall through to a fresh scan.
        }

        juce::File searchRoot(entry.rootPath);
        if (!searchRoot.isDirectory())
            searchRoot = rootDir.isDirectory() ? rootDir
                : juce::File(processorRef.lastBrowserDir);
        runSearchAsync(entry.search, idx, searchRoot);
        refreshSidebar();
    };
    sidebar.onRemoveSavedSearch = [this](int idx)
    {
        processorRef.removeSavedSearch(idx);
        forgetSavedSearchSnapshot(idx);
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
    sidebar.onContentHeightChanged = [this] { updateWindowLimits(); };
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
            if ((processorRef.sectionLocks & (toolkitLock::Pitch | toolkitLock::Playback)) != 0)
            {
                applyPitchLock(e, processorRef.lockedEdit);
                if ((processorRef.sectionLocks & toolkitLock::Playback) != 0)
                    processorRef.lockedEdit.extendMult = e.extendMult;
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
            if ((processorRef.sectionLocks & toolkitLock::AnyGroove) != 0)
                captureGrooveSectionLock(k, processorRef.lockedGroove, processorRef.sectionLocks);
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
            if ((processorRef.sectionLocks & (toolkitLock::Pitch | toolkitLock::Playback)) != 0)
            {
                applyPitchLock(e, processorRef.lockedEdit);
                if ((processorRef.sectionLocks & toolkitLock::Playback) != 0)
                    processorRef.lockedEdit.extendMult = e.extendMult;
                processorRef.lockAutoTrim = e.hasTrim();
            }
            rollEditor.setClip(*clip, e, selectedGroove());
            refreshEntryMeta(selectedIdx);
            pushPreviewToProcessor();
            updateMiniPreview();
        }
    };
    effectsInspector.onSectionLocksChanged = [this](uint32_t locks)
    {
        processorRef.sectionLocks = locks;
        processorRef.syncLockFlagsFromSections();
        if (locks != 0)
        {
            captureGrooveSectionLock(selectedGroove(), processorRef.lockedGroove, locks);
            if ((locks & (toolkitLock::Pitch | toolkitLock::Playback)) != 0)
            {
                applyPitchLock(selectedEdit(), processorRef.lockedEdit);
                processorRef.lockedEdit.extendMult = selectedEdit().extendMult;
                processorRef.lockAutoTrim = selectedEdit().hasTrim();
            }
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
    effectsInspector.onTrimClicked = [this] { rollEditor.toggleTrim(); };
    effectsInspector.onResetUnlocked = [this]
    {
        if (const auto* clip = selectedClip())
        {
            const auto locks = processorRef.sectionLocks;
            auto& g = processorRef.grooveFor(clip->filePath);
            auto& e = processorRef.editFor(clip->filePath);
            // Mirror inspector resets into processor state for unlocked sections.
            if ((locks & toolkitLock::AnyGroove) == 0)
                processorRef.clipGrooves.erase(clip->filePath);
            else
            {
                auto next = GrooveParams();
                applyGrooveSectionLock(processorRef.lockedGroove, next, locks);
                g = next;
            }
            if ((locks & toolkitLock::Pitch) == 0)
            {
                e.octave = -1;
                e.pitchShift = 0;
                e.octaveRange = 0;
                e.pitchMin = 0;
                e.pitchMax = 127;
                e.fitScale = false;
                e.mapToRoot = false;
                e.root = -1;
                e.mode = Mode::Ionian;
                e.noteFilterMask = 0x0FFF;
                e.noteFilterType = NoteFilterType::Mute;
            }
            if ((locks & toolkitLock::Playback) == 0)
                e.extendMult = 1;
            if ((locks & toolkitLock::Pitch) == 0 && (locks & toolkitLock::Playback) == 0)
                processorRef.lockedEdit = ClipEdit();
            else
                applyPitchLock(e, processorRef.lockedEdit);
            rollEditor.setClip(*clip, e, selectedGroove());
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
    processorRef.syncLockFlagsFromSections();
    effectsInspector.setSectionLocks(processorRef.sectionLocks);
    effectsInspector.setBpmMultiplier(processorRef.bpmMultiplier.load());
    effectsInspector.onActivated = [this] { claimKeyNav(KeyNavTarget::Effects); };
    content.addAndMakeVisible(effectsInspector);

    restoreBrowserSession();
    refreshSidebar();

    setResizable(true, true);
    applyLayoutState();
    startTimerHz(30);
    setWantsKeyboardFocus(true);
    updateMiniPreview();
    applyNativeWindowChrome();
}

MidiBrowserEditor::~MidiBrowserEditor()
{
    persistBrowserSession();
    juce::Desktop::getInstance().removeDarkModeSettingListener(this);
    stopTimer();
    setLookAndFeel(nullptr);
}

void MidiBrowserEditor::parentHierarchyChanged()
{
    applyNativeWindowChrome();
}

void MidiBrowserEditor::visibilityChanged()
{
    if (isShowing())
        applyNativeWindowChrome();
}

void MidiBrowserEditor::applyNativeWindowChrome()
{
    if (processorRef.wrapperType != juce::AudioProcessor::wrapperType_Standalone)
        return;
    if (getPeer() == nullptr)
        return;
    nativeChrome::applyCupertinoTitlebar(*this);
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
    recursiveBrowse = false;
    searchDuplicateLocations.clear();
    sidebar.setStarredFilter(false);
    rootDir = dir;
    processorRef.lastBrowserDir = dir.getFullPathName();
    rescanFolder(keepSelection);
    refreshSidebar();
    fileList.grabBrowseFocus();
    persistBrowserSession();
}

void MidiBrowserEditor::persistBrowserSession()
{
    processorRef.browseMode = browseMode;
    processorRef.starredFilter = starredFilter;
    processorRef.includeSubdirs = sidebar.getIncludeSubdirs();
    // Live sidebar criteria (query + filters) — not a stale search snapshot.
    processorRef.browserSessionSearch = sidebar.getCriteria();
    if (browseMode == 2)
        activeSearch = processorRef.browserSessionSearch;
    processorRef.activeSavedSearchIdx = activeSavedSearchIdx;
    if (rootDir.isDirectory())
        processorRef.lastBrowserDir = rootDir.getFullPathName();

    if (juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
        processorRef.selectedClipPath = clips[(size_t) selectedIdx].filePath;
    else
        processorRef.selectedClipPath.clear();

    processorRef.browserResultPaths.clear();
    if (browseMode == 2)
    {
        processorRef.browserResultPaths.ensureStorageAllocated((int) clips.size());
        for (const auto& c : clips)
            if (c.filePath.isNotEmpty())
                processorRef.browserResultPaths.add(c.filePath);
    }
}

void MidiBrowserEditor::selectPathOrFirst(const juce::String& path)
{
    int nextSel = -1;
    if (path.isNotEmpty())
    {
        for (int i = 0; i < (int) clips.size(); ++i)
        {
            if (clips[(size_t) i].filePath == path && displayForClip(i) >= 0)
            {
                nextSel = i;
                break;
            }
        }
    }
    if (nextSel < 0)
    {
        for (const auto& row : displayRows)
            if (!row.isDirectory)
            {
                nextSel = row.clipIndex;
                break;
            }
    }

    selectedIdx = -1;
    if (nextSel >= 0)
        selectIndex(nextSel);
    else
    {
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
        transport.setHasClip(false);
        transport.setClipName({});
    }
}

void MidiBrowserEditor::restoreBrowserSession()
{
    // Restore sidebar chrome before any rescan so filters / include-subdirs apply.
    sidebar.setIncludeSubdirs(processorRef.includeSubdirs);
    sidebar.setCriteria(processorRef.browserSessionSearch);

    if (processorRef.lastBrowserDir.isNotEmpty())
    {
        const juce::File dir(processorRef.lastBrowserDir);
        if (dir.isDirectory())
            rootDir = dir;
    }

    browseMode = juce::jlimit(0, 2, processorRef.browseMode);
    starredFilter = processorRef.starredFilter;
    activeSavedSearchIdx = processorRef.activeSavedSearchIdx;
    activeSearch = processorRef.browserSessionSearch;
    recursiveBrowse = processorRef.includeSubdirs;
    sidebar.setBrowseMode(browseMode);
    sidebar.setStarredFilter(starredFilter);

    const juce::String wantPath = processorRef.selectedClipPath;

    if (browseMode == 1)
    {
        loadStarredClips(false);
        selectPathOrFirst(wantPath);
        fileList.grabBrowseFocus();
    }
    else if (browseMode == 2)
    {
        std::vector<StepClip> found;
        found.reserve((size_t) processorRef.browserResultPaths.size());
        for (const auto& path : processorRef.browserResultPaths)
        {
            const juce::File f(path);
            if (f.existsAsFile())
                found.push_back(makeStepClip(parseMidiFile(f)));
        }

        if (!found.empty())
        {
            // Apply list without the snapshot helper's default first-row select.
            activeSearch = processorRef.browserSessionSearch;
            recursiveBrowse = false;
            setBrowseMode(2);
            clips = std::move(found);
            juce::String title = "Search";
            if (activeSearch.query.isNotEmpty())
                title = activeSearch.query;
            fileList.setFolderName(title);
            rebuildEntries();
            selectPathOrFirst(wantPath);
            fileList.grabBrowseFocus();
        }
        else
        {
            // Fall back to a live search (may hit LibraryStore path cache).
            runSearchAsync(activeSearch, activeSavedSearchIdx);
        }
    }
    else if (rootDir.isDirectory())
    {
        // Folder mode — rescan without wiping the restored filter criteria.
        searchDuplicateLocations.clear();
        rescanFolder(false, false);
        selectPathOrFirst(wantPath);
        fileList.grabBrowseFocus();
    }

    persistBrowserSession();
}

void MidiBrowserEditor::rescanFolder(bool keepSelection, bool autoSelect)
{
    const juce::String previousPath =
        (keepSelection && selectedIdx >= 0 && selectedIdx < (int) clips.size())
            ? clips[(size_t) selectedIdx].filePath : juce::String();

    // Prefer the sidebar toggle; keep recursiveBrowse aligned for rebuildEntries.
    recursiveBrowse = sidebar.getIncludeSubdirs();
    clips.clear();
    if (rootDir.isDirectory())
    {
        if (recursiveBrowse)
        {
            scanMidiFiles(rootDir, true, clips,
                          [](const StepClip&, const juce::File&) { return true; });
        }
        else
        {
            auto files = rootDir.findChildFiles(juce::File::findFiles, false, "*.mid;*.midi");
            files.sort();
            for (const auto& f : files)
                clips.push_back(makeStepClip(parseMidiFile(f)));
        }
    }

    if (rootDir.isDirectory())
    {
        auto name = rootDir.getFileName();
        if (name.isEmpty()) name = rootDir.getFullPathName();
        if (recursiveBrowse) name += " (all)";
        fileList.setFolderName(name);
    }
    else
    {
        fileList.setFolderName("Select a folder");
    }

    rebuildEntries();

    if (!autoSelect)
        return;

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

void MidiBrowserEditor::scanAllFolders()
{
    sidebar.setIncludeSubdirs(true);
    if (rootDir.isDirectory()
        || (processorRef.lastBrowserDir.isNotEmpty()
            && juce::File(processorRef.lastBrowserDir).isDirectory()))
    {
        if (!rootDir.isDirectory())
            rootDir = juce::File(processorRef.lastBrowserDir);
        rescanFolder(true);
        refreshSidebar();
        fileList.grabBrowseFocus();
    }
}

void MidiBrowserEditor::applyBrowserFilter()
{
    const juce::String previousPath =
        (selectedIdx >= 0 && selectedIdx < (int) clips.size())
            ? clips[(size_t) selectedIdx].filePath : juce::String();

    rebuildEntries();

    int nextSel = selectedIdx;
    if (nextSel >= 0 && displayForClip(nextSel) < 0)
        nextSel = -1;
    if (nextSel < 0 && previousPath.isNotEmpty())
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath && displayForClip(i) >= 0)
            {
                nextSel = i;
                break;
            }
    if (nextSel < 0)
    {
        for (const auto& row : displayRows)
            if (!row.isDirectory) { nextSel = row.clipIndex; break; }
    }

    if (nextSel >= 0)
    {
        if (nextSel != selectedIdx)
            selectIndex(nextSel);
        else if (const int d = displayForClip(selectedIdx); d >= 0)
            fileList.setSelectedIndex(d, juce::dontSendNotification);
    }
    else if (selectedIdx >= 0)
    {
        selectedIdx = -1;
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
    }
    persistBrowserSession();
}

void MidiBrowserEditor::setBrowseMode(int mode)
{
    browseMode = juce::jlimit(0, 2, mode);
    sidebar.setBrowseMode(browseMode);
    persistBrowserSession();
}

void MidiBrowserEditor::loadStarredClips(bool autoSelect)
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

    if (!autoSelect)
        return;

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
    recursiveBrowse = false;
    clips.clear();
    searchDuplicateLocations.clear();

    const juce::File searchRoot = rootDir.isDirectory() ? rootDir
        : juce::File(processorRef.lastBrowserDir);
    if (searchRoot.isDirectory())
    {
        // Disk scan matches query only; FILTER is applied in rebuildEntries.
        const auto matcher = [criteria](const StepClip&, const juce::File& f)
        {
            return clipMatchesQuery(f, criteria);
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

    int nextSel = -1;
    if (previousPath.isNotEmpty())
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath && displayForClip(i) >= 0)
                nextSel = i;
    if (nextSel < 0)
        for (const auto& row : displayRows)
            if (!row.isDirectory) { nextSel = row.clipIndex; break; }

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

void MidiBrowserEditor::rememberSearchSnapshot(int savedIdx, SearchSnapshot snap)
{
    lastSearchSnapshot = snap;
    if (savedIdx >= 0)
        savedSearchSnapshots[savedIdx] = std::move(snap);
}

void MidiBrowserEditor::forgetSavedSearchSnapshot(int index)
{
    savedSearchSnapshots.erase(index);
    std::map<int, SearchSnapshot> next;
    for (auto& [k, v] : savedSearchSnapshots)
    {
        if (k < index)
            next[k] = std::move(v);
        else if (k > index)
            next[k - 1] = std::move(v);
    }
    savedSearchSnapshots = std::move(next);
}

void MidiBrowserEditor::applySearchSnapshot(std::vector<StepClip> found,
                                            std::map<juce::String, juce::StringArray> locMap,
                                            const BrowserSearch& criteria, int savedIdx,
                                            const juce::String& rootPath, bool showSearching)
{
    if (showSearching)
        fileList.setSearching(true);

    activeSearch = criteria;
    activeSavedSearchIdx = savedIdx;
    recursiveBrowse = false;
    setBrowseMode(2);

    SearchSnapshot snap;
    snap.criteria = criteria;
    snap.rootPath = rootPath;
    snap.clips = found;          // full query hits; filter applied on display
    snap.locations = std::move(locMap);
    rememberSearchSnapshot(savedIdx, std::move(snap));

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
    int nextSel = -1;
    for (const auto& row : displayRows)
        if (!row.isDirectory) { nextSel = row.clipIndex; break; }
    if (nextSel >= 0)
        selectIndex(nextSel);
    else
    {
        rollEditor.clearClip();
        syncEffectsInspector();
        processorRef.setPreviewState({}, false, false, false);
        persistBrowserSession();
    }
    fileList.grabBrowseFocus();
}

void MidiBrowserEditor::runSearchAsync(const BrowserSearch& criteria, int savedIdx,
                                       juce::File searchRoot)
{
    // Keep SEARCH/FILTER sections open; just blur the query field.
    sidebar.deactivateSearch(false);

    if (!searchRoot.isDirectory())
        searchRoot = rootDir.isDirectory() ? rootDir
            : juce::File(processorRef.lastBrowserDir);
    if (!searchRoot.isDirectory())
    {
        fileList.setSearching(false);
        fileList.setFolderName("Open a folder to search");
        clips.clear();
        rebuildEntries();
        return;
    }

    const int gen = ++searchGeneration;
    activeSearch = criteria;
    if (savedIdx < 0)
        activeSavedSearchIdx = -1;
    else
        activeSavedSearchIdx = savedIdx;
    starredFilter = false;
    recursiveBrowse = false;
    sidebar.setStarredFilter(false);
    setBrowseMode(2);

    // Cache key is query-only; filters apply afterward in rebuildEntries.
    const auto cacheKey = searchScanKey(criteria);

    if (const auto* cached = processorRef.library().findSearchCache(searchRoot, cacheKey))
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
            if (savedIdx >= 0)
                processorRef.updateSavedSearchResults(savedIdx, cached->resultPaths,
                                                      searchRoot.getFullPathName());
            applySearchSnapshot(std::move(found), {}, criteria, savedIdx,
                                searchRoot.getFullPathName(), false);
            return;
        }
    }

    fileList.setSearching(true);

    juce::Component::SafePointer<MidiBrowserEditor> safe(this);
    juce::Thread::launch([safe, criteria, cacheKey, searchRoot, gen, savedIdx]
    {
        std::vector<StepClip> found;
        juce::StringArray resultPaths;
        try
        {
            const auto matcher = [criteria](const StepClip&, const juce::File& f)
            {
                return clipMatchesQuery(f, criteria);
            };
            scanMidiFiles(searchRoot, criteria.subdirs, found, matcher);

            for (const auto& c : found)
                resultPaths.add(c.filePath);
        }
        catch (...)
        {
            found.clear();
            resultPaths.clear();
        }

        juce::MessageManager::callAsync([safe, results = std::move(found), criteria, cacheKey,
                                         searchRoot, paths = std::move(resultPaths),
                                         gen, savedIdx]() mutable
        {
            if (safe == nullptr || gen != safe->searchGeneration.load())
                return;

            safe->processorRef.library().putSearchCache(searchRoot, cacheKey, paths);
            if (savedIdx >= 0)
                safe->processorRef.updateSavedSearchResults(savedIdx, paths,
                                                            searchRoot.getFullPathName());
            safe->applySearchSnapshot(std::move(results), {}, criteria, savedIdx,
                                      searchRoot.getFullPathName(), false);
        });
    });
}

void MidiBrowserEditor::saveCurrentSearch()
{
    // Prefer the live sidebar form; fall back to the last-run search.
    BrowserSearch criteria = sidebar.getCriteria();
    const bool formEmpty = criteria.query.isEmpty()
        && criteria.keyRoot < 0 && criteria.keyMask == 0
        && criteria.bpmMin <= 0.0 && criteria.bpmMax <= 0.0
        && criteria.barsMin <= 0 && criteria.barsMax <= 0
        && criteria.complexityMin <= 0 && criteria.complexityMax <= 0;
    if (formEmpty && browseMode == 2)
        criteria = activeSearch;
    if (formEmpty && browseMode != 2)
        return;

    SavedSearchEntry entry;
    entry.search = criteria;
    if (LibraryStore::searchesEqual(lastSearchSnapshot.criteria, criteria)
        && lastSearchSnapshot.rootPath.isNotEmpty())
        entry.rootPath = lastSearchSnapshot.rootPath;
    else if (rootDir.isDirectory())
        entry.rootPath = rootDir.getFullPathName();
    else
        entry.rootPath = processorRef.lastBrowserDir;

    // Capture result paths so the saved search reopens instantly next session.
    if (LibraryStore::searchesEqual(lastSearchSnapshot.criteria, criteria)
        && !lastSearchSnapshot.clips.empty())
    {
        entry.resultPaths.ensureStorageAllocated((int) lastSearchSnapshot.clips.size());
        for (const auto& c : lastSearchSnapshot.clips)
            if (c.filePath.isNotEmpty())
                entry.resultPaths.add(c.filePath);
    }
    else if (browseMode == 2 && !clips.empty())
    {
        entry.resultPaths.ensureStorageAllocated((int) clips.size());
        for (const auto& c : clips)
            if (c.filePath.isNotEmpty())
                entry.resultPaths.add(c.filePath);
    }

    juce::StringArray parts;
    if (criteria.query.isNotEmpty())
        parts.add(criteria.query);
    if (const auto bpmLabel = searchBpmTitle(criteria); bpmLabel.isNotEmpty())
        parts.add(bpmLabel);
    if (criteria.keyMask != 0)
    {
        juce::StringArray keys;
        for (int i = 0; i < 12; ++i)
            if ((criteria.keyMask & (uint16_t) (1u << i)) != 0)
                keys.add(kNoteNames[(size_t) i]);
        if (keys.size() > 0)
            parts.add(keys.joinIntoString(" "));
    }
    else if (criteria.keyRoot >= 0 && criteria.keyRoot < 12)
    {
        parts.add(kNoteNames[(size_t) criteria.keyRoot]);
    }
    if (const auto barsLabel = searchBarsLabel(criteria); barsLabel.isNotEmpty())
        parts.add(barsLabel);
    entry.name = parts.isEmpty() ? "Search" : parts.joinIntoString(" - ");

    processorRef.addSavedSearch(entry);
    activeSavedSearchIdx = (int) processorRef.savedSearches.size() - 1;

    // Attach the last completed results so re-opening is instant this session.
    if (LibraryStore::searchesEqual(lastSearchSnapshot.criteria, criteria))
        savedSearchSnapshots[activeSavedSearchIdx] = lastSearchSnapshot;

    // Keep the generic search cache warm for the same criteria.
    if (entry.resultPaths.size() > 0 && entry.rootPath.isNotEmpty())
        processorRef.library().putSearchCache(juce::File(entry.rootPath),
                                              searchScanKey(criteria),
                                              entry.resultPaths);

    refreshSidebar();
    persistBrowserSession();
}

void MidiBrowserEditor::rebuildEntries()
{
    displayRows.clear();
    std::vector<FileListEntry> entries;

    const auto filter = sidebar.getFilter();

    // Build the clip pool that passes the FILTER (search already narrowed by query).
    std::vector<int> passing;
    passing.reserve(clips.size());
    for (int i = 0; i < (int) clips.size(); ++i)
    {
        const auto& clip = clips[(size_t) i];
        const bool starred = processorRef.isStarred(clip.filePath);
        // Starred library mode already lists only favourites — don't re-filter stars.
        if (starredFilter && browseMode != 1 && !starred)
            continue;
        if (!clipMatchesFilter(clip, filter))
            continue;
        passing.push_back(i);
    }

    std::set<juce::String> hiddenDupes;
    searchDuplicateLocations.clear();
    if (filter.removeDuplicates && !passing.empty())
    {
        std::vector<StepClip> pool;
        pool.reserve(passing.size());
        for (int i : passing)
            pool.push_back(clips[(size_t) i]);
        searchDuplicateLocations = computeDedupeLocations(pool, hiddenDupes);
    }

    if (browseMode == 0 && rootDir.isDirectory() && !recursiveBrowse)
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

    for (int i : passing)
    {
        const auto& clip = clips[(size_t) i];
        if (hiddenDupes.count(clip.filePath) != 0)
            continue;

        const bool starred = processorRef.isStarred(clip.filePath);
        const bool edited = (processorRef.sectionLocks & toolkitLock::Pitch) != 0
                                && !editIsClean(processorRef.editFor(clip.filePath));
        auto entry = entryForClip(clip, juce::File(clip.filePath), edited, starred);
        if (auto it = searchDuplicateLocations.find(clip.filePath);
            it != searchDuplicateLocations.end() && it->second.size() > 1)
            entry.locations = it->second;
        entries.push_back(std::move(entry));
        displayRows.push_back({ false, i, juce::File(clip.filePath) });
    }

    fileList.setEntries(std::move(entries));
    sidebar.setFilterHistograms(clips);

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
    effectsInspector.setClipSourceOctave(clip != nullptr ? clipReferenceOctave(*clip) : 4);
    effectsInspector.setClipComplexity(clip != nullptr ? clip->complexity : 50);
    if (clip != nullptr)
        effectsInspector.setClipScaleAnalysis(analyseClipScale(*clip));
    else
        effectsInspector.setClipScaleAnalysis({});
    effectsInspector.setEdit(selectedEdit(), juce::dontSendNotification);
    effectsInspector.setGroove(selectedGroove(), juce::dontSendNotification);
    effectsInspector.setBpmMultiplier(processorRef.bpmMultiplier.load(), juce::dontSendNotification);
    effectsInspector.setSectionLocks(processorRef.sectionLocks);
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

    const auto locks = processorRef.sectionLocks;

    // Carry Fit/Map/Key/Mode across unlocked browse so audition stays in the
    // chosen scale (still ephemeral — cleared when Pitch stays unlocked).
    ClipEdit carriedScale;
    bool haveCarriedScale = false;
    if (selectedIdx >= 0 && selectedIdx != index
        && (locks & toolkitLock::Pitch) == 0
        && juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
    {
        const auto& prev = clips[(size_t) selectedIdx];
        const auto& prevEdit = processorRef.editFor(prev.filePath);
        if (prevEdit.fitScale || prevEdit.mapToRoot)
        {
            carriedScale.fitScale = prevEdit.fitScale;
            carriedScale.mapToRoot = prevEdit.mapToRoot;
            carriedScale.root = prevEdit.root;
            carriedScale.mode = prevEdit.mode;
            haveCarriedScale = prevEdit.root >= 0;
        }
        processorRef.clipEdits.erase(prev.filePath);
        refreshEntryMeta(selectedIdx);
    }
    // No groove-section locks: discard per-file groove so browsing auditions raw clips.
    if (selectedIdx >= 0 && selectedIdx != index && (locks & toolkitLock::AnyGroove) == 0
        && juce::isPositiveAndBelow(selectedIdx, (int) clips.size()))
    {
        const auto& prev = clips[(size_t) selectedIdx];
        processorRef.clipGrooves.erase(prev.filePath);
    }

    selectedIdx = index;

    // Always release held notes before the new clip starts sounding.
    processorRef.requestNoteFlush();

    const auto& clip = clips[(size_t) index];

    if ((locks & (toolkitLock::Pitch | toolkitLock::Playback)) != 0)
    {
        auto& e = processorRef.editFor(clip.filePath);
        if ((locks & toolkitLock::Pitch) != 0)
            applyPitchLock(processorRef.lockedEdit, e);
        if ((locks & toolkitLock::Playback) != 0)
            e.extendMult = processorRef.lockedEdit.extendMult;
        if (processorRef.lockAutoTrim
            && (locks & (toolkitLock::Pitch | toolkitLock::Playback)) != 0)
        {
            ClipEdit probe = e;
            probe.clearTrim();
            const auto preTrim = resolveClip(clip, probe);
            e.clearTrim();
            e.removedBars = emptyBars(preTrim.notes, clip.bars);
        }
        refreshEntryMeta(index);
    }
    else
    {
        // Unlocked: seed octave / range / key / mode from clip analysis.
        auto& e = processorRef.editFor(clip.filePath);
        if (editIsClean(e) || e.root < 0)
        {
            const auto analysis = analyseClipScale(clip);
            e.octave = -1;
            e.octaveRange = analysis.octaveRange;
            e.root = analysis.primaryRoot;
            e.mode = analysis.primaryMode;
        }
        if (haveCarriedScale)
        {
            e.fitScale = carriedScale.fitScale;
            e.mapToRoot = carriedScale.mapToRoot;
            e.root = carriedScale.root;
            e.mode = carriedScale.mode;
        }
    }

    if ((locks & toolkitLock::AnyGroove) != 0)
    {
        auto& g = processorRef.grooveFor(clip.filePath);
        g = GrooveParams();
        applyGrooveSectionLock(processorRef.lockedGroove, g, locks);
    }

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
    transport.setClipName(clip.name);
    rollEditor.setClip(clip, edit, groove);
    applyTimeStretchFromMultiplier();
    syncEffectsInspector();

    if (const int d = displayForClip(index); d >= 0)
        fileList.setSelectedIndex(d, juce::dontSendNotification);

    pushPreviewToProcessor();
    updateMiniPreview();
    persistBrowserSession();
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
    const bool edited = (processorRef.sectionLocks & toolkitLock::Pitch) != 0
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
    auto notes = applyGroove(resolved.notes, groove, clip->complexity);
    // Complexity morph can invent ±1/±2 ornaments — keep Fit to Scale honest.
    if (edit.fitScale && edit.root >= 0)
        refitNotesToScale(notes, edit.root, edit.mode);
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
    // Soft update while tweaking Toolkit params so held notes aren't choked.
    processorRef.setPreviewState(preview, !preview.notes.empty(), false, false, true);
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
        miniRoll.setNotes({}, 4, GrooveParams{}, 0, {}, Mode::Ionian, 4, 0x0FFF);
        previewHeader.repaint();
        transport.setHasClip(false);
        transport.setClipName({});
        return;
    }

    transport.setHasClip(true);
    transport.setClipName(clip->name);
    const auto edit = selectedEdit();
    const auto groove = selectedGroove();
    const auto resolved = resolveClip(*clip, edit);
    // Frame from source notes so pitch/octave edits stay visible in the strip
    // (fitting to resolved notes would re-center and hide transposition).
    const int rootPc = edit.root >= 0 ? edit.root : (clip->root >= 0 ? clip->root : 0);
    auto grooved = applyGroove(resolved.notes, groove, clip->complexity);
    if (edit.fitScale && edit.root >= 0)
        refitNotesToScale(grooved, edit.root, edit.mode);
    miniRoll.setNotes(grooved, resolved.bars, groove,
                      rootPc, clip->notes, edit.mode, 4, edit.noteFilterMask);
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
    auto caret = r.removeFromLeft(14).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
    // Open = chevron down (expanded); closed = chevron right (folded to title row).
    drawIcon(g, owner.processorRef.previewOpen ? icons::caretDown : icons::caretRight,
             caret, colours::text2(), 1.5f);
    r.removeFromLeft(6);

    g.setColour(colours::text());
    g.setFont(uiFont(12.0f, true));
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

void MidiBrowserEditor::updateWindowLimits()
{
    const bool edOpen = processorRef.editorOpen;
    const bool fxOpen = processorRef.effectsOpen;
    const int side = sidebar.idealWidth();
    const int sideBudget = sidebar.isCollapsed() ? side : metrics::sidebarExpandedWidth();
    const int baseTable = metrics::fileTableWidth();
    const int columnsW = fileList.idealContentWidth();
    const int tableMax = juce::jmax(baseTable, columnsW);
    const int panes = (edOpen ? metrics::editorPaneWidth() : 0)
                    + (fxOpen ? metrics::effectsPaneWidth() : 0);
    const int maxW = sideBudget + tableMax + panes;
    const int minW = metrics::sidebarRailWidth() + metrics::browserMinWidthScaled()
                     + (edOpen ? metrics::openRollMinWidth() : 0)
                     + (fxOpen ? metrics::effectsPaneWidth() : 0);
    // Min height = transport + full library column (content + Settings).
    const int minH = metrics::transportH() + sidebar.idealMinHeight();
    setResizeLimits(minW, minH, maxW, 2000);
    if (getHeight() > 0 && getHeight() < minH)
        setSize(juce::jmax(getWidth(), minW), minH);
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
    updateWindowLimits();

    // Always reserve the Preview title row; add mini-roll height when expanded.
    const int previewExtra = metrics::scaled(26)
        + (processorRef.previewOpen ? metrics::miniRollH() : 0);
    const int targetH = juce::jmax(metrics::transportH() + sidebar.idealMinHeight(),
                                   juce::jmax(metrics::scaled(460) + previewExtra, lastWindowH));

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
    // Preview title row always lives under the browser list (fold to minimize).
    previewHeader.setVisible(true);
    miniRoll.setVisible(previewOpen);

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

    // Preview stack pinned under the file list: title row always, roll when open.
    const int previewHeaderH = metrics::scaled(26);
    if (previewOpen)
    {
        auto preview = browserCol.removeFromBottom(metrics::miniRollH());
        previewHeader.setBounds(browserCol.removeFromBottom(previewHeaderH));
        miniRoll.setBounds(preview);
    }
    else
    {
        previewHeader.setBounds(browserCol.removeFromBottom(previewHeaderH));
        miniRoll.setBounds({});
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

/** In-app settings — Caps B2 rows with roomier vertical rhythm. */
class SettingsPanel : public juce::Component
{
public:
    std::function<void()> onChanged;
    std::function<void()> onClose;

    // Percents relative to the new 100% baseline (former 115% size).
    static constexpr int kTextPctChoices[] = { 70, 85, 100, 115, 130 };
    static constexpr int kPanelW = 380;
    static constexpr int kPanelH = 340;
    static constexpr int kHeaderH = 44;

    SettingsPanel()
    {
        appearancePopup.setItems({ "System", "Light", "Dark" }, tweaks().appearance.load());
        sizePopup.setItems({ "Small", "Medium", "Large" }, tweaks().size.load());
        spacingPopup.setItems({ "Compact", "Comfortable" }, tweaks().density.load());

        juce::StringArray pctLabels;
        int pctIdx = 2; // 100%
        const int currentPct = tweaks().textScalePct.load();
        int bestDist = 1000;
        for (int i = 0; i < (int) (sizeof(kTextPctChoices) / sizeof(kTextPctChoices[0])); ++i)
        {
            pctLabels.add(juce::String(kTextPctChoices[i]) + "%");
            const int d = std::abs(kTextPctChoices[i] - currentPct);
            if (d < bestDist)
            {
                bestDist = d;
                pctIdx = i;
            }
        }
        textPopup.setItems(pctLabels, pctIdx);

        tooltipsSwitch.setToggleState(tweaks().showTooltips.load() != 0, juce::dontSendNotification);
        tooltipsSwitch.setTooltip("Show hover tips on controls");

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
        wire(sizePopup, [](int i) { tweaks().size.store(i); });
        wire(textPopup, [](int i)
        {
            const int n = (int) (sizeof(kTextPctChoices) / sizeof(kTextPctChoices[0]));
            tweaks().textScalePct.store(kTextPctChoices[juce::jlimit(0, n - 1, i)]);
        });
        wire(spacingPopup, [](int i) { tweaks().density.store(i); });

        tooltipsSwitch.onClick = [this]
        {
            tweaks().showTooltips.store(tooltipsSwitch.getToggleState() ? 1 : 0);
            if (onChanged) onChanged();
        };
        addAndMakeVisible(tooltipsSwitch);

        closeBtn.ghost = true;
        closeBtn.iconScale = 0.95f;
        closeBtn.setTooltip("Close");
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible(closeBtn);

        setSize(kPanelW, kPanelH + 28);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto bounds = getLocalBounds().toFloat();
        g.setColour(t.panelBg);
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(t.controlHairline);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

        auto header = getLocalBounds().removeFromTop(kHeaderH).reduced(18, 0);
        header.removeFromRight(28); // room for close
        auto cap = uiFont(11.0f, true);
        g.setFont(cap);
        g.setColour(t.valueText);
        g.drawText("SETTINGS", header, juce::Justification::centredLeft, false);

        g.setColour(t.divider);
        g.fillRect(18, kHeaderH, getWidth() - 36, 1);

        auto body = getLocalBounds().withTrimmedTop(kHeaderH + 8).reduced(18, 10);
        const int rowH = 44;
        const int rowGap = 10;
        const char* labels[] = {
            "Appearance",
            "UI size",
            "Text size",
            "Row spacing",
            "Show tooltips"
        };
        const char* hints[] = {
            "Light, dark, or follow the system",
            "Scales spacing, panels, and controls",
            "Scales type and icons only",
            "Tighter or roomier list rows",
            ""
        };
        for (int i = 0; i < 5; ++i)
        {
            auto row = body.removeFromTop(rowH);
            // Leave room for the control on the right.
            auto textCol = row.withTrimmedRight(150);
            g.setColour(colours::text());
            g.setFont(uiFont(12.0f, false));
            g.drawText(labels[i], textCol.removeFromTop(hints[i][0] != 0 ? 18 : rowH),
                       juce::Justification::centredLeft, false);
            if (hints[i][0] != 0)
            {
                g.setColour(colours::text2());
                g.setFont(uiFont(10.5f, false));
                g.drawText(hints[i], textCol, juce::Justification::centredLeft, false);
            }
            body.removeFromTop(rowGap);
        }

        // Build stamp for user testing — version | build time | commit.
        auto footer = getLocalBounds().removeFromBottom(28).reduced(18, 0);
        g.setColour(t.divider);
        g.fillRect(18, footer.getY(), getWidth() - 36, 1);
        g.setFont(uiFont(10.0f, false));
        g.setColour(colours::text3());
        g.drawText(build_info::stamp(), footer.withTrimmedTop(6),
                   juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        const int iconBtn = metrics::chromeIconButton();
        closeBtn.setBounds(juce::Rectangle<int>(getWidth() - 18 - iconBtn, 0, iconBtn, kHeaderH)
                               .withSizeKeepingCentre(iconBtn, iconBtn));

        auto body = getLocalBounds().withTrimmedTop(kHeaderH + 8).reduced(18, 10);
        const int rowH = 44;
        const int rowGap = 10;
        const int ctrlH = metrics::scaled(26);
        auto placePopup = [&](fx::FlatPopup& p)
        {
            auto row = body.removeFromTop(rowH);
            const int w = juce::jmin(juce::jmax(p.idealWidth(), 110), row.getWidth() / 2);
            p.setBounds(row.removeFromRight(w).withSizeKeepingCentre(w, ctrlH));
            body.removeFromTop(rowGap);
        };
        placePopup(appearancePopup);
        placePopup(sizePopup);
        placePopup(textPopup);
        placePopup(spacingPopup);

        auto tipRow = body.removeFromTop(rowH);
        const int sw = tooltipsSwitch.idealWidth();
        tooltipsSwitch.setBounds(tipRow.removeFromRight(sw).withSizeKeepingCentre(sw, 18));
    }

private:
    fx::FlatPopup appearancePopup, sizePopup, textPopup, spacingPopup;
    fx::FlatSwitch tooltipsSwitch;
    IconBtn closeBtn { icons::x, "Close" };
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
            panel.setSize(panel.getWidth(), panel.getHeight());
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
    overlay->panel.onClose = overlay->onDismiss;
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
    sidebar.setCurrentClipCount(browseMode == 0 ? (int) clips.size() : 0);
    sidebar.setStarredCount(processorRef.starredFiles.size());
}

// ── ticking ──────────────────────────────────────────────────────────────────

bool MidiBrowserEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && sidebar.keyPressed(key))
        return true;

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
