#include "FileBrowserPanel.h"
#include <cmath>

namespace pflow {

namespace {

/** Same wrap as MidiBrowserProcessor::processBlock preview phase (loop / session length). */
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

} // namespace

int FileBrowserPanel::folderBarHeightPx()
{
    return juce::jlimit(22, 28, juce::jmax(28, metrics::titleBarH - 8));
}

namespace {

juce::File getOrCreateEmptyBrowserStubDir()
{
    auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("MidiBrowser")
                 .getChildFile("browser_empty");
    d.createDirectory();
    return d;
}

} // namespace

FileBrowserPanel::FileBrowserPanel()
{
    btnSetRoot.setButtonText("Select a folder");
    btnSetRoot.setComponentID("BrowserFolderButton");
    btnSetRoot.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select MIDI folder", juce::File(), "");
        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result.isDirectory())
                    setRootDirectory(result);
            });
    };
    addAndMakeVisible(btnSetRoot);

    dirThread = std::make_unique<juce::TimeSliceThread>("BrowserDir");
    dirThread->startThread(juce::Thread::Priority::background);

    fileFilter = std::make_unique<juce::WildcardFileFilter>("*.mid;*.midi;*.MID;*.MIDI", "*", "MIDI files");

    dirContents = std::make_unique<juce::DirectoryContentsList>(fileFilter.get(), *dirThread);
    dirContents->setDirectory(getOrCreateEmptyBrowserStubDir(), true, true);

    fileTree = std::make_unique<PflowFileTreeComponent>(*dirContents);
    fileTree->getViewport()->setScrollBarsShown(true, true, false, false);
    fileTree->setColour(juce::TreeView::backgroundColourId, colours::bgLight());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);
    fileTree->setDragAndDropDescription("MidiFileDrag");
    fileTree->setItemHeight(juce::roundToInt(18.0f * (metrics::browserFontSize / 12.0f)));
    fileTree->setWantsKeyboardFocus(true);
    fileTree->addListener(this);
    fileTree->addMouseListener(static_cast<juce::MouseListener*>(this), true);
    addAndMakeVisible(*fileTree);
    fileTree->setVisible(false);

    browserEmptyHint_.setText("Select a folder to browse MIDI files", juce::dontSendNotification);
    browserEmptyHint_.setJustificationType(juce::Justification::centred);
    browserEmptyHint_.setColour(juce::Label::textColourId, colours::textDim());
    browserEmptyHint_.setFont(juce::Font(juce::FontOptions(14.0f)));
    browserEmptyHint_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(browserEmptyHint_);

    setWantsKeyboardFocus(true);

    startTimerHz(30);
}

FileBrowserPanel::~FileBrowserPanel()
{
    if (fileTree) { fileTree->removeListener(this); fileTree->removeMouseListener(static_cast<juce::MouseListener*>(this)); }
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

void FileBrowserPanel::resized()
{
    auto b = getLocalBounds();
    const int previewH = 150;
    auto abovePreview = b;
    abovePreview.removeFromBottom(previewH);
    const int btnH = folderBarHeightPx();
    btnSetRoot.setBounds(0, abovePreview.getY(), getWidth(), btnH);
    abovePreview.removeFromTop(btnH);
    fileTree->setBounds(abovePreview);
    browserEmptyHint_.setBounds(abovePreview);
}

bool FileBrowserPanel::keyPressed(const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) && fileTree)
    {
        fileTree->grabKeyboardFocus();
        return fileTree->keyPressed(key);
    }
    return false;
}

void FileBrowserPanel::mouseDown(const juce::MouseEvent& e)
{
    if (fileTree)
    {
        fileTree->grabKeyboardFocus();
        if (e.eventComponent == fileTree.get() || fileTree->isParentOf(e.eventComponent))
        {
            dragStartPos = e.position;
            externalDragStarted = false;
        }
    }
    const int previewH = 150;
    const float bottomPad = 10.0f;
    const float msBtnW = 30.0f;
    const float msBtnH = 22.0f;
    const int frameMargin = 8;
    float stripY = (float)(getHeight() - previewH);
    float msY = stripY + (float)previewH - bottomPad - msBtnH;
    float msX = (float)(getWidth() - frameMargin) - msBtnW;
    auto muteRect = juce::Rectangle<float>(msX, msY, msBtnW, msBtnH);
    if (e.position.y >= (float)(getHeight() - previewH) && e.position.y < (float)getHeight())
    {
        if (muteRect.contains(e.position))
        {
            previewMuted = !previewMuted;
            repaint();
            return;
        }
    }
}

void FileBrowserPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (!fileTree || externalDragStarted) return;
    if (fileTree->getNumSelectedFiles() <= 0) return;
    juce::File f = fileTree->getSelectedFile(0);
    if (!f.hasFileExtension("mid;midi") || !f.existsAsFile()) return;
    float dist = e.position.getDistanceFrom(dragStartPos);
    if (dist > 4.0f)
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

void FileBrowserPanel::toggleTruncateEmptyMeasuresMode()
{
    truncateEmptyMeasuresMode_ = !truncateEmptyMeasuresMode_;
    if (currentPreviewMidiFile_.existsAsFile())
        rebuildPreviewClipFromDisk();
    repaint();
}

void FileBrowserPanel::timerCallback()
{
    repaint();
}

static bool isBlackKey(int noteNum)
{
    int n = noteNum % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

void FileBrowserPanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::bgLight());
    g.setColour(colours::panelBorder());
    g.drawLine((float)getWidth() - 0.5f, 0.0f, (float)getWidth() - 0.5f, (float)getHeight(), 1.0f);
    const int previewH = 150;
    const int pianoKeyWidth = 22;
    const float bottomPad = 10.0f;
    const float msBtnW = 30.0f;
    const float msBtnH = 22.0f;
    const int msRowH = (int)std::ceil((double)bottomPad + (double)msBtnH + 10.0);
    const int frameMargin = (int)std::round(bottomPad);

    float stripY = (float)(getHeight() - previewH);
    g.setColour(colours::accentDim().withAlpha(0.18f));
    g.fillRect(0.0f, stripY, (float)getWidth(), (float)previewH);
    g.setColour(colours::panelBorder());
    g.drawHorizontalLine(getHeight() - previewH - 1, 0.0f, (float)getWidth());

    if (truncateEmptyMeasuresMode_)
    {
        const auto badge = juce::Rectangle<float>(8.0f, stripY + 6.0f, 52.0f, 18.0f);
        g.setColour(colours::accent().withAlpha(0.35f));
        g.fillRoundedRectangle(badge, 4.0f);
        g.setColour(colours::accent());
        g.drawRoundedRectangle(badge, 4.0f, 1.0f);
        g.setColour(colours::text());
        g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("SemiBold")));
        g.drawText("Trim", badge, juce::Justification::centred);
    }

    // Piano roll content area (above M/S row)
    int contentTop = (int)stripY + frameMargin;
    int contentH = previewH - frameMargin - msRowH - frameMargin;
    int gridLeft = frameMargin + pianoKeyWidth;
    int gridW = getWidth() - frameMargin * 2 - pianoKeyWidth;

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
        // Ensure at least 2 octaves (24 semitones) of range, centered on content
        int contentMid = (minN + maxN) / 2;
        int minRange = 24;
        int halfRange = minRange / 2;
        int botNote = std::min(minN - 1, contentMid - halfRange);
        int topNote = std::max(maxN + 1, contentMid + halfRange);
        botNote = std::max(0, botNote);
        topNote = std::min(127, topNote);
        int range = std::max(minRange, topNote - botNote + 1);
        float noteH = (float)contentH / (float)range;
        float beatsToPx = (float)gridW / (float)std::max(0.25, previewClip.lengthBeats);

        // Piano keys (left strip)
        for (int n = botNote; n <= topNote + 1; ++n)
        {
            float y = (float)contentTop + contentH - (float)(n - botNote + 1) * noteH;
            bool black = isBlackKey(n);
            g.setColour(black ? colours::pianoBlackKey() : colours::pianoWhiteKey());
            g.fillRect((float)frameMargin, y, (float)pianoKeyWidth, noteH);
            g.setColour(colours::panelBorder().withAlpha(0.3f));
            g.drawHorizontalLine((int)y, (float)frameMargin, (float)(frameMargin + pianoKeyWidth));
        }
        g.setColour(colours::panelBorder());
        g.drawVerticalLine(gridLeft, (float)contentTop, (float)(contentTop + contentH));

        // Grid rows (pitch) and notes
        for (int n = botNote; n <= topNote + 1; ++n)
        {
            float y = (float)contentTop + contentH - (float)(n - botNote + 1) * noteH;
            bool black = isBlackKey(n);
            g.setColour(black ? colours::pianoBlackKey().withAlpha(0.5f) : juce::Colours::transparentBlack);
            g.fillRect((float)gridLeft, y, (float)gridW, noteH);
            g.setColour(colours::pianoGrid().withAlpha(0.55f));
            g.drawHorizontalLine((int)y, (float)gridLeft, (float)(gridLeft + gridW));
        }

        // Notes as horizontal bars
        for (auto& n : previewClip.notes)
        {
            float x = (float)n.startBeat * beatsToPx;
            float w = (float)n.lengthBeats * beatsToPx;
            if (x + w < 0 || x > (float)gridW) continue;
            if (x < 0) { w += x; x = 0; }
            if (x + w > (float)gridW) w = (float)gridW - x;
            w = std::max(2.0f, w);
            float y = (float)contentTop + contentH - (float)(n.noteNumber - botNote + 1) * noteH;
            g.setColour(colours::accent().withAlpha(0.5f + 0.5f * n.velocity / 127.0f));
            g.fillRoundedRectangle((float)gridLeft + x, y + 1.0f, w, noteH - 2.0f, 2.0f);
        }

        if (drawPlayhead)
        {
            float playheadX = (float)gridLeft + (float)displayPlayheadBeat * beatsToPx;
            g.setColour(colours::playhead());
            g.fillRect(playheadX - 1.0f, (float)contentTop, 2.0f, (float)contentH);
        }
    }

    // Mute: preview to instrument track is silent when engaged
    const float cr = metrics::cornerRadius;
    float msY = stripY + (float)previewH - bottomPad - msBtnH;
    float msX = (float)(getWidth() - frameMargin) - msBtnW;
    auto muteR = juce::Rectangle<float>(msX, msY, msBtnW, msBtnH);

    g.setColour(previewMuted ? colours::muteInactive() : colours::panelBorder());
    if (previewMuted)
        g.fillRoundedRectangle(muteR, cr);
    else
        g.drawRoundedRectangle(muteR, cr, 1.2f);
    g.setColour(previewMuted ? juce::Colours::white : colours::textDim());
    g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    g.drawText("M", muteR, juce::Justification::centred);
}

void FileBrowserPanel::refreshComponentColours()
{
    browserEmptyHint_.setColour(juce::Label::textColourId, colours::textDim());
    fileTree->setColour(juce::TreeView::backgroundColourId, colours::bgLight());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);
}

void FileBrowserPanel::fileClicked(const juce::File& file, const juce::MouseEvent&)
{
    if (file.hasFileExtension("mid;midi"))
    {
        if (onFileDragStarted)
            onFileDragStarted(file);
    }
}

void FileBrowserPanel::fileDoubleClicked(const juce::File& file)
{
    juce::ignoreUnused(file);
}

} // namespace pflow
