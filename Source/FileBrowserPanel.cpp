#include "FileBrowserPanel.h"
#include <cmath>

namespace pflow {

namespace {

double phaseBeatForSession(double sessionBeat, bool loopEnabled,
                           double loopStart, double loopEnd, double sessionLenBeats)
{
    if (loopEnabled)
    {
        const double loopLen = loopEnd - loopStart;
        if (loopLen > 1.0e-6 && sessionBeat + 1.0e-9 >= loopStart)
            return loopStart + std::fmod(sessionBeat - loopStart, loopLen);
    }
    if (sessionLenBeats > 1.0e-9)
    {
        double phase = std::fmod(sessionBeat, sessionLenBeats);
        if (phase < 0.0) phase += sessionLenBeats;
        return phase;
    }
    return sessionBeat;
}

juce::File getOrCreateEmptyBrowserStubDir()
{
    auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("MidiBrowser")
                 .getChildFile("browser_empty");
    d.createDirectory();
    return d;
}

bool isBlackKey(int noteNum)
{
    const int n = noteNum % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

void drawGroupedInset(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(colours::bgLight());
    g.fillRoundedRectangle(bounds.toFloat(), metrics::groupedRadius);
}

} // namespace

int FileBrowserPanel::folderBarHeightPx()
{
    return metrics::toolbarH;
}

FileBrowserPanel::FileBrowserPanel()
{
    btnSetRoot.setComponentID("HIGPrimaryButton");
    btnSetRoot.setTooltip("Choose a folder of MIDI files");
    btnSetRoot.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select MIDI folder", getRootDirectory(), "");
        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser& fc)
            {
                const auto result = fc.getResult();
                if (result.isDirectory())
                    setRootDirectory(result);
            });
    };
    addAndMakeVisible(btnSetRoot);

    btnBookmarks.setComponentID("HIGIconButton");
    btnBookmarks.getProperties().set("hig_icon", higIcon::star);
    btnBookmarks.setTooltip("Saved folders");
    btnBookmarks.onClick = [this] { showBookmarksMenu(); };
    addAndMakeVisible(btnBookmarks);

    btnFileUp.setComponentID("HIGIconButton");
    btnFileUp.getProperties().set("hig_icon", higIcon::chevronUp);
    btnFileUp.setTooltip("Previous MIDI file");
    btnFileUp.onClick = [this] { selectAdjacentMidiFile(-1); };
    addAndMakeVisible(btnFileUp);

    btnFileDown.setComponentID("HIGIconButton");
    btnFileDown.getProperties().set("hig_icon", higIcon::chevronDown);
    btnFileDown.setTooltip("Next MIDI file");
    btnFileDown.onClick = [this] { selectAdjacentMidiFile(1); };
    addAndMakeVisible(btnFileDown);

    btnTrim.setComponentID("HIGChipToggle");
    btnTrim.setClickingTogglesState(true);
    btnTrim.setTooltip("Trim empty measures from preview (disk unchanged)");
    btnTrim.onClick = [this] { setTrimEmptyMeasuresMode(btnTrim.getToggleState()); };
    addAndMakeVisible(btnTrim);

    btnMute.setComponentID("HIGChipToggle");
    btnMute.setClickingTogglesState(true);
    btnMute.setTooltip("Mute live preview to your instrument track");
    btnMute.onClick = [this]
    {
        previewMuted = btnMute.getToggleState();
        repaint();
    };
    addAndMakeVisible(btnMute);

    dirThread = std::make_unique<juce::TimeSliceThread>("BrowserDir");
    dirThread->startThread(juce::Thread::Priority::background);

    fileFilter = std::make_unique<juce::WildcardFileFilter>("*.mid;*.midi;*.MID;*.MIDI", "*", "MIDI files");

    dirContents = std::make_unique<juce::DirectoryContentsList>(fileFilter.get(), *dirThread);
    dirContents->setDirectory(getOrCreateEmptyBrowserStubDir(), true, true);

    fileTree = std::make_unique<PflowFileTreeComponent>(*dirContents);
    fileTree->getViewport()->setScrollBarsShown(true, true, false, false);
    fileTree->setDragAndDropDescription("MidiFileDrag");
    fileTree->setItemHeight(juce::roundToInt(22.0f * (metrics::browserFontSize / 13.0f)));
    fileTree->setWantsKeyboardFocus(true);
    fileTree->addListener(this);
    fileTree->addMouseListener(static_cast<juce::MouseListener*>(this), true);
    addAndMakeVisible(*fileTree);
    fileTree->setVisible(false);

    browserEmptyHint_.setText("Open a folder to browse MIDI files\nDrag files into your DAW", juce::dontSendNotification);
    browserEmptyHint_.setJustificationType(juce::Justification::centred);
    browserEmptyHint_.setColour(juce::Label::textColourId, colours::textDim());
    browserEmptyHint_.setFont(fontFor(TextStyle::Callout));
    browserEmptyHint_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(browserEmptyHint_);

    refreshComponentColours();
    btnMute.setToggleState(previewMuted, juce::dontSendNotification);
    setWantsKeyboardFocus(true);
    startTimerHz(30);
}

FileBrowserPanel::~FileBrowserPanel()
{
    if (fileTree)
    {
        fileTree->removeListener(this);
        fileTree->removeMouseListener(static_cast<juce::MouseListener*>(this));
    }
}

void FileBrowserPanel::setRootDirectory(const juce::File& dir)
{
    fileTree->setVisible(true);
    browserEmptyHint_.setVisible(false);
    dirContents->setDirectory(dir, true, true);
    fileTree->refresh();
    if (onDirectoryChanged)
        onDirectoryChanged(dir.getFullPathName());
}

juce::File FileBrowserPanel::getRootDirectory() const
{
    return dirContents != nullptr ? dirContents->getDirectory() : juce::File();
}

void FileBrowserPanel::resized()
{
    auto b = getLocalBounds();
    const int previewH = metrics::previewH;
    const int gap = metrics::toolbarGap;
    const int iconW = metrics::iconButtonSize;

    auto abovePreview = b;
    abovePreview.removeFromBottom(previewH + gap);

    auto topRow = abovePreview.removeFromTop(metrics::toolbarH);
    btnFileDown.setBounds(topRow.removeFromRight(iconW));
    topRow.removeFromRight(gap);
    btnFileUp.setBounds(topRow.removeFromRight(iconW));
    topRow.removeFromRight(gap);
    btnBookmarks.setBounds(topRow.removeFromRight(iconW));
    topRow.removeFromRight(gap);
    btnSetRoot.setBounds(topRow);

    abovePreview.removeFromTop(gap);
    fileTree->setBounds(abovePreview.reduced(0, 0));
    browserEmptyHint_.setBounds(abovePreview);

    auto previewBlock = b.removeFromBottom(previewH);
    auto controls = previewBlock.removeFromBottom(metrics::previewControlsH);
    previewBlock.removeFromBottom(4);

    const int chipW = 76;
    btnMute.setBounds(controls.removeFromRight(chipW));
    controls.removeFromRight(gap);
    btnTrim.setBounds(controls.removeFromRight(chipW + 8));
}

bool FileBrowserPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::upKey)
    {
        selectAdjacentMidiFile(-1);
        return true;
    }
    if (key == juce::KeyPress::downKey)
    {
        selectAdjacentMidiFile(1);
        return true;
    }
    return false;
}

void FileBrowserPanel::selectAdjacentMidiFile(int direction)
{
    if (fileTree)
    {
        fileTree->grabKeyboardFocus();
        fileTree->selectAdjacentMidiFile(direction);
    }
}

void FileBrowserPanel::mouseDown(const juce::MouseEvent& e)
{
    if (!fileTree) return;

    fileTree->grabKeyboardFocus();
    if (e.eventComponent == fileTree.get() || fileTree->isParentOf(e.eventComponent))
    {
        dragStartPos = e.position;
        externalDragStarted = false;
    }
}

void FileBrowserPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (!fileTree || externalDragStarted) return;
    if (fileTree->getNumSelectedFiles() <= 0) return;

    const juce::File f = fileTree->getSelectedFile(0);
    if (!f.hasFileExtension("mid;midi") || !f.existsAsFile()) return;

    if (e.position.getDistanceFrom(dragStartPos) > 4.0f)
    {
        externalDragStarted = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles(
            juce::StringArray(f.getFullPathName()), false, fileTree.get());
    }
}

void FileBrowserPanel::selectionChanged()
{
    updatePreviewForSelection();
}

void FileBrowserPanel::updatePreviewForSelection()
{
    if (fileTree->getNumSelectedFiles() <= 0)
    {
        hasPreviewClip = false;
        currentPreviewMidiFile_ = juce::File();
    }
    else
    {
        const juce::File f = fileTree->getSelectedFile(0);
        if (f.hasFileExtension("mid;midi") && f.existsAsFile())
        {
            currentPreviewMidiFile_ = f;
            rebuildPreviewClipFromDisk();
        }
        else
        {
            hasPreviewClip = false;
            currentPreviewMidiFile_ = juce::File();
        }
    }
    repaint();
}

void FileBrowserPanel::rebuildPreviewClipFromDisk()
{
    if (!currentPreviewMidiFile_.existsAsFile())
    {
        hasPreviewClip = false;
        return;
    }

    previewClip = parseMidiFile(currentPreviewMidiFile_);
    hasPreviewClip = !previewClip.notes.empty();
    if (truncateEmptyMeasuresMode_ && hasPreviewClip)
        trimEmptyMeasuresInClip(previewClip, 4.0);
}

void FileBrowserPanel::setTrimEmptyMeasuresMode(bool enabled)
{
    if (truncateEmptyMeasuresMode_ == enabled)
        return;
    truncateEmptyMeasuresMode_ = enabled;
    btnTrim.setToggleState(enabled, juce::dontSendNotification);
    if (currentPreviewMidiFile_.existsAsFile())
        rebuildPreviewClipFromDisk();
    if (onTrimModeChanged)
        onTrimModeChanged(enabled);
    repaint();
}

void FileBrowserPanel::toggleTrimEmptyMeasuresMode()
{
    setTrimEmptyMeasuresMode(!truncateEmptyMeasuresMode_);
}

void FileBrowserPanel::showBookmarksMenu()
{
    juce::PopupMenu menu;
    const auto root = getRootDirectory();
    const bool canSave = root.isDirectory() && root.getFullPathName().isNotEmpty();

    menu.addItem(1, "Save current folder", canSave);
    menu.addSeparator();

    juce::StringArray saved;
    if (onGetSavedFolders)
        saved = onGetSavedFolders();

    if (saved.isEmpty())
    {
        menu.addItem(-1, "(No saved folders)", false);
    }
    else
    {
        for (int i = 0; i < saved.size(); ++i)
        {
            const juce::File dir(saved[i]);
            const auto label = dir.getFileName().isNotEmpty() ? dir.getFileName() : saved[i];
            menu.addItem(100 + i, label);
        }

        juce::PopupMenu removeMenu;
        for (int i = 0; i < saved.size(); ++i)
        {
            const juce::File dir(saved[i]);
            const auto label = dir.getFileName().isNotEmpty() ? dir.getFileName() : saved[i];
            removeMenu.addItem(1000 + i, label);
        }
        menu.addSeparator();
        menu.addSubMenu("Remove saved folder", removeMenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(btnBookmarks),
        [this, saved](int result)
        {
            if (result == 1)
            {
                const auto path = getRootDirectory().getFullPathName();
                if (path.isNotEmpty() && onSaveFolder)
                    onSaveFolder(path);
                return;
            }
            if (result >= 1000)
            {
                const int idx = result - 1000;
                if (onRemoveSavedFolder && juce::isPositiveAndBelow(idx, saved.size()))
                    onRemoveSavedFolder(saved[idx]);
                return;
            }
            if (result >= 100)
            {
                const int idx = result - 100;
                if (juce::isPositiveAndBelow(idx, saved.size()))
                {
                    const juce::File dir(saved[idx]);
                    if (dir.isDirectory())
                        setRootDirectory(dir);
                }
            }
        });
}

void FileBrowserPanel::timerCallback()
{
    repaint();
}

void FileBrowserPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    const int previewH = metrics::previewH;
    const int gap = metrics::toolbarGap;
    const int pad = metrics::grid;

    auto treeArea = bounds;
    treeArea.removeFromBottom(previewH + gap);
    treeArea.removeFromTop(metrics::toolbarH + gap);
    drawGroupedInset(g, treeArea.reduced(0, 0));

    auto previewArea = bounds.removeFromBottom(previewH);
    drawGroupedInset(g, previewArea);

    const int pianoKeyWidth = 22;
    const int frameMargin = pad;
    const int controlsH = metrics::previewControlsH;
    auto rollArea = previewArea.reduced(frameMargin);
    rollArea.removeFromBottom(controlsH + 4);

    const int contentTop = rollArea.getY();
    const int contentH = rollArea.getHeight();
    const int gridLeft = rollArea.getX() + pianoKeyWidth;
    const int gridW = rollArea.getWidth() - pianoKeyWidth;

    bool drawPlayhead = false;
    double displayPlayheadBeat = 0.0;
    if (hasPreviewClip && previewClip.lengthBeats > 1.0e-9
        && onGetHostPlaying && onGetHostPlaying()
        && onGetPlayheadState && onGetSessionLengthBeats)
    {
        const auto [sessionBeat, loopStart, loopEnd, loopEnabled] = onGetPlayheadState();
        const double sessionLen = onGetSessionLengthBeats();
        const double phase = phaseBeatForSession(sessionBeat, loopEnabled, loopStart, loopEnd, sessionLen);
        displayPlayheadBeat = std::fmod(phase, previewClip.lengthBeats);
        if (displayPlayheadBeat < 0.0) displayPlayheadBeat += previewClip.lengthBeats;
        drawPlayhead = true;
    }

    if (hasPreviewClip && previewClip.lengthBeats > 0)
    {
        int minN = 127, maxN = 0;
        for (auto& n : previewClip.notes)
        {
            minN = std::min(minN, n.noteNumber);
            maxN = std::max(maxN, n.noteNumber);
        }
        int contentMid = (minN + maxN) / 2;
        constexpr int minRange = 24;
        int botNote = std::min(minN - 1, contentMid - minRange / 2);
        int topNote = std::max(maxN + 1, contentMid + minRange / 2);
        botNote = std::max(0, botNote);
        topNote = std::min(127, topNote);
        int range = std::max(minRange, topNote - botNote + 1);
        float noteH = (float) contentH / (float) range;
        float beatsToPx = (float) gridW / (float) std::max(0.25, previewClip.lengthBeats);

        for (int n = botNote; n <= topNote + 1; ++n)
        {
            float y = (float) contentTop + contentH - (float) (n - botNote + 1) * noteH;
            bool black = isBlackKey(n);
            g.setColour(black ? colours::pianoBlackKey() : colours::pianoWhiteKey());
            g.fillRect((float) rollArea.getX(), y, (float) pianoKeyWidth, noteH);
            g.setColour(colours::separator().withAlpha(0.25f));
            g.drawHorizontalLine((int) y, (float) rollArea.getX(), (float) (rollArea.getX() + pianoKeyWidth));
        }
        g.setColour(colours::separator());
        g.drawVerticalLine(gridLeft, (float) contentTop, (float) (contentTop + contentH));

        for (int n = botNote; n <= topNote + 1; ++n)
        {
            float y = (float) contentTop + contentH - (float) (n - botNote + 1) * noteH;
            bool black = isBlackKey(n);
            g.setColour(black ? colours::pianoBlackKey().withAlpha(0.45f) : juce::Colours::transparentBlack);
            g.fillRect((float) gridLeft, y, (float) gridW, noteH);
            g.setColour(colours::pianoGrid());
            g.drawHorizontalLine((int) y, (float) gridLeft, (float) (gridLeft + gridW));
        }

        for (auto& n : previewClip.notes)
        {
            float x = (float) n.startBeat * beatsToPx;
            float w = (float) n.lengthBeats * beatsToPx;
            if (x + w < 0 || x > (float) gridW) continue;
            if (x < 0) { w += x; x = 0; }
            if (x + w > (float) gridW) w = (float) gridW - x;
            w = std::max(2.0f, w);
            float y = (float) contentTop + contentH - (float) (n.noteNumber - botNote + 1) * noteH;
            g.setColour(colours::accent().withAlpha(0.45f + 0.45f * n.velocity / 127.0f));
            g.fillRoundedRectangle((float) gridLeft + x, y + 1.0f, w, noteH - 2.0f, 3.0f);
        }

        if (drawPlayhead)
        {
            float playheadX = (float) gridLeft + (float) displayPlayheadBeat * beatsToPx;
            g.setColour(colours::playhead());
            g.fillRect(playheadX - 1.0f, (float) contentTop, 2.0f, (float) contentH);
        }
    }
    else
    {
        g.setColour(colours::textDim());
        g.setFont(fontFor(TextStyle::Callout));
        g.drawText("Select a MIDI file to preview", rollArea, juce::Justification::centred, true);
    }
}

void FileBrowserPanel::refreshComponentColours()
{
    browserEmptyHint_.setColour(juce::Label::textColourId, colours::textDim());
    browserEmptyHint_.setFont(fontFor(TextStyle::Callout));
    if (fileTree)
    {
        fileTree->setColour(juce::TreeView::backgroundColourId, juce::Colours::transparentBlack);
        fileTree->setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
        fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::selection());
        fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, colours::text());
    }
}

void FileBrowserPanel::fileClicked(const juce::File& file, const juce::MouseEvent&)
{
    if (file.hasFileExtension("mid;midi") && onFileDragStarted)
        onFileDragStarted(file);
}

void FileBrowserPanel::fileDoubleClicked(const juce::File& file)
{
    juce::ignoreUnused(file);
}

} // namespace pflow
