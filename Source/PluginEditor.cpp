#include "PluginEditor.h"

namespace pflow {

namespace {

FileListEntry entryForClip(const StepClip& clip, const juce::File& f, bool edited)
{
    FileListEntry e;
    e.file = f;
    e.name = f.getFileNameWithoutExtension();
    e.kind = clip.kind;
    e.rootName = clip.root >= 0 ? juce::String(kNoteNames[(size_t) clip.root]) : juce::String();
    e.bpm = clip.bpm;
    e.edited = edited;
    return e;
}

} // namespace

MidiBrowserEditor::MidiBrowserEditor(MidiBrowserProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setLookAndFeel(&lnf);
    lnf.refreshColours();

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
    transport.setSynced(processorRef.syncToHost.load());
    transport.setFreeBpm(processorRef.freeBpm.load());
    addAndMakeVisible(transport);

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
    addAndMakeVisible(sidebar);

    // ── File list ──
    fileList.onOpenFolder = [this] { chooseFolder(); };
    fileList.onSelect = [this](int idx) { selectIndex(idx); };
    fileList.onPlayRow = [this](int idx)
    {
        selectIndex(idx);
        processorRef.previewArmed.store(true);
    };
    addAndMakeVisible(fileList);

    // ── Piano-roll editor ──
    rollEditor.onEditChanged = [this](const ClipEdit& e)
    {
        if (const auto* clip = selectedClip())
        {
            processorRef.editFor(clip->filePath) = e;
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
        }
    };
    addAndMakeVisible(rollEditor);

    // ── Folded mini preview ──
    addChildComponent(miniHeader);
    addChildComponent(miniRoll);

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
    std::vector<FileListEntry> entries;
    for (const auto& f : files)
    {
        auto clip = makeStepClip(parseMidiFile(f));
        const bool edited = !editIsClean(processorRef.editFor(f.getFullPathName()));
        entries.push_back(entryForClip(clip, f, edited));
        clips.push_back(std::move(clip));
    }

    fileList.setFolderName(rootDir.isDirectory() ? rootDir.getFileName() : "Select a folder");
    fileList.setEntries(std::move(entries));
    transport.setFolderPath(rootDir.isDirectory() ? rootDir.getFullPathName() : juce::String());

    int nextSel = clips.empty() ? -1 : 0;
    if (previousPath.isNotEmpty())
        for (int i = 0; i < (int) clips.size(); ++i)
            if (clips[(size_t) i].filePath == previousPath)
                nextSel = i;

    selectedIdx = -1;
    if (nextSel >= 0)
    {
        fileList.setSelectedIndex(nextSel, juce::dontSendNotification);
        selectIndex(nextSel);
    }
    else
    {
        rollEditor.clearClip();
        processorRef.setPreviewState({}, false, false, false);
    }
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
    selectedIdx = index;

    // Reset playhead to 0 on selection change (spec).
    processorRef.freerunBeat.store(0.0);

    const auto& clip = clips[(size_t) index];
    const auto edit = selectedEdit();
    const auto groove = selectedGroove();

    transport.setClipBpm(clip.bpm);
    rollEditor.setClip(clip, edit, groove);

    const auto resolved = resolveClip(clip, edit);
    miniRoll.setNotes(applyGroove(resolved.notes, groove), resolved.bars, groove);

    pushPreviewToProcessor();
}

void MidiBrowserEditor::refreshEntryMeta(int index)
{
    if (!juce::isPositiveAndBelow(index, (int) clips.size()))
        return;
    const auto& clip = clips[(size_t) index];
    const bool edited = !editIsClean(processorRef.editFor(clip.filePath));
    fileList.updateEntry(index, entryForClip(clip, juce::File(clip.filePath), edited));
}

void MidiBrowserEditor::pushPreviewToProcessor()
{
    const auto* clip = selectedClip();
    if (clip == nullptr)
    {
        processorRef.setPreviewState({}, false, false, false);
        return;
    }

    // Non-destructive pipeline: resolveClip → applyGroove → velocities. The
    // processor plays this snapshot; the source clip and file stay untouched.
    const auto edit = selectedEdit();
    const auto groove = selectedGroove();
    const auto resolved = resolveClip(*clip, edit);
    const auto notes = applyGroove(resolved.notes, groove);

    MidiClip preview;
    preview.name = clip->name;
    preview.lengthBeats = (double) (resolved.bars * kStepsPerBar) / 4.0;
    for (const auto& n : notes)
    {
        NoteEvent ev;
        ev.noteNumber = juce::jlimit(0, 127, n.pitch);
        ev.velocity = juce::jlimit(1, 127, (int) std::lround(effectiveVelocity(n, groove) * 127.0));
        ev.startBeat = n.start / 4.0;
        ev.lengthBeats = juce::jmax(0.05, n.len / 4.0);
        ev.channel = juce::jlimit(1, 16, n.channel);
        preview.notes.push_back(ev);
    }
    processorRef.setPreviewState(preview, !preview.notes.empty(), false, false);
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
    transport.setEditorOpen(open);
    if (getHeight() > 0)
        lastWindowH = getHeight();

    const int w = open ? metrics::openWindowW : metrics::foldedWindowW;
    setResizeLimits(open ? 640 : 280, 420, 1600, 2000);
    setSize(w, juce::jmax(460, lastWindowH));
    resized();
}

void MidiBrowserEditor::resized()
{
    auto r = getLocalBounds();
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
    g.setFont(uiFont(11.0f, true));
    g.drawText(clip != nullptr ? clip->name : "Preview",
               r.withTrimmedRight(52), juce::Justification::centredLeft, true);

    if (clip != nullptr)
    {
        const auto resolved = resolveClip(*clip, owner.selectedEdit());
        g.setColour(colours::text3());
        g.setFont(monoFont(10.0f, false));
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

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sidebar),
        [this](int result)
        {
            if (result == 0) return;
            auto& t = tweaks();
            if (result >= 400)      t.grid.store(result - 400);
            else if (result >= 300) t.density.store(result - 300);
            else if (result >= 200) t.accent.store(result - 200);
            else if (result >= 100) t.theme.store(result - 100);
            lnf.refreshColours();
            sendLookAndFeelChange();
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
    return fileList.keyPressed(key);
}

void MidiBrowserEditor::timerCallback()
{
    const bool sounding = processorRef.isPreviewSounding();
    transport.setPlaying(processorRef.previewArmed.load());
    fileList.setPlaying(sounding);

    double step = 0.0;
    if (const auto* clip = selectedClip())
    {
        const auto resolved = resolveClip(*clip, selectedEdit());
        const double lenBeats = juce::jmax(0.25, (double) (resolved.bars * kStepsPerBar) / 4.0);
        double beat = 0.0;
        if (processorRef.syncToHost.load())
        {
            beat = std::fmod(processorRef.hostBeatPos.load(), lenBeats);
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
