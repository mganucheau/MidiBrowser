#include "FileBrowserPanel.h"

namespace pflow {

FileBrowserPanel::FileBrowserPanel()
{
    lblHeader.setText(juce::String::charToString(0x266B) + "  BROWSER", juce::dontSendNotification);
    lblHeader.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("SemiBold")));
    lblHeader.setColour(juce::Label::textColourId, colours::text());
    addAndMakeVisible(lblHeader);

    btnSetRoot.setButtonText("Select a folder");
    btnSetRoot.setComponentID("ActionButton");
    btnSetRoot.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnSetRoot.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
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

    auto defaultDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    dirContents = std::make_unique<juce::DirectoryContentsList>(fileFilter.get(), *dirThread);
    dirContents->setDirectory(defaultDir, true, true);

    fileTree = std::make_unique<juce::FileTreeComponent>(*dirContents);
    fileTree->setColour(juce::FileTreeComponent::backgroundColourId, colours::bgLight());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);
    fileTree->setDragAndDropDescription("MidiFileDrag");
    fileTree->setItemHeight(18);  // Match lane title text (12pt) row height
    fileTree->setWantsKeyboardFocus(true);
    fileTree->addListener(this);
    fileTree->addMouseListener(static_cast<juce::MouseListener*>(this), true);
    addAndMakeVisible(*fileTree);
    setWantsKeyboardFocus(true);

    previewFileLabel.setVisible(false);
    startTimerHz(30);
}

FileBrowserPanel::~FileBrowserPanel()
{
    if (fileTree) { fileTree->removeListener(this); fileTree->removeMouseListener(static_cast<juce::MouseListener*>(this)); }
}

void FileBrowserPanel::setRootDirectory(const juce::File& dir)
{
    dirContents->setDirectory(dir, true, true);
    fileTree->refresh();
    if (onDirectoryChanged)
        onDirectoryChanged(dir.getFullPathName());
}

void FileBrowserPanel::resized()
{
    auto b = getLocalBounds();
    lblHeader.setBounds(b.removeFromTop(26).reduced(metrics::padding, 1));
    btnSetRoot.setBounds(b.removeFromTop(metrics::buttonH).reduced(metrics::padding, 1));
    const int previewH = 150;
    b.removeFromBottom(previewH);
    fileTree->setBounds(b.reduced(metrics::padding / 2, 1));
}

bool FileBrowserPanel::keyPressed(const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::returnKey || key == juce::KeyPress::leftKey) && tryAddSelectedToNewLane())
        return true;
    // Up/down: pass to file tree for selection navigation (tree handles natively when focused)
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
    const float btnSize = 18.0f;
    const float btnGap = 6.0f;
    const int msRowH = 28;
    const int frameMargin = 8;
    float stripY = (float)(getHeight() - previewH);
    float msY = stripY + (float)(previewH - msRowH);
    float msX = (float)(getWidth() - frameMargin - btnSize * 2 - btnGap);
    auto muteRect = juce::Rectangle<float>(msX, msY + (msRowH - btnSize) * 0.5f, btnSize, btnSize);
    auto soloRect = juce::Rectangle<float>(msX + btnSize + btnGap, msY + (msRowH - btnSize) * 0.5f, btnSize, btnSize);
    if (e.position.y >= getHeight() - previewH && e.position.y < getHeight())
    {
        if (muteRect.contains(e.position))
        {
            previewMuted = !previewMuted;
            repaint();
            return;
        }
        if (soloRect.contains(e.position))
        {
            previewSoloed = !previewSoloed;
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
        // Use internal JUCE drag so clips can be added while the host is playing
        startDragging(f.getFullPathName(), fileTree.get());
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
    }
    else
    {
        juce::File f = fileTree->getSelectedFile(0);
        if (f.hasFileExtension("mid;midi") && f.existsAsFile())
        {
            previewClip = parseMidiFile(f);
            hasPreviewClip = !previewClip.notes.empty();
            previewPlayheadBeat = 0.0;  // Reset when selection changes
        }
        else
            hasPreviewClip = false;
    }
    repaint();
}

void FileBrowserPanel::timerCallback()
{
    if (!hasPreviewClip || previewClip.lengthBeats <= 0) return;
    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    if (previewLastTime <= 0) previewLastTime = now;
    double delta = now - previewLastTime;
    previewLastTime = now;
    previewPlayheadBeat += delta * 2.0;  // ~120bpm
    if (previewPlayheadBeat >= previewClip.lengthBeats)
        previewPlayheadBeat = std::fmod(previewPlayheadBeat, previewClip.lengthBeats);
    repaint();
}

bool FileBrowserPanel::tryAddSelectedToNewLane()
{
    if (fileTree->getNumSelectedFiles() <= 0) return false;
    juce::File f = fileTree->getSelectedFile(0);
    if (!f.hasFileExtension("mid;midi") || !f.existsAsFile()) return false;
    MidiClip clip = parseMidiFile(f);
    if (onClipAddFromBrowser)
        onClipAddFromBrowser(clip);
    else if (onClipAddToNewLane)
        onClipAddToNewLane(clip);
    return true;
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
    const int msRowH = 28;
    const float btnSize = 18.0f;
    const float btnGap = 6.0f;
    const int frameMargin = 8;

    float stripY = (float)(getHeight() - previewH);
    g.setColour(colours::bgLighter());
    g.fillRect(0.0f, stripY, (float)getWidth(), (float)previewH);
    g.setColour(colours::panelBorder());
    g.drawHorizontalLine(getHeight() - previewH - 1, 0.0f, (float)getWidth());

    // Piano roll content area (above M/S row)
    int contentTop = (int)stripY + frameMargin;
    int contentH = previewH - frameMargin - msRowH - frameMargin;
    int gridLeft = frameMargin + pianoKeyWidth;
    int gridW = getWidth() - frameMargin * 2 - pianoKeyWidth;

    auto contentRect = juce::Rectangle<int>(gridLeft, contentTop, gridW, contentH);

    double displayPlayheadBeat = 0.0;
    if (hasPreviewClip && previewClip.lengthBeats > 0)
    {
        if (onGetPlayheadState && onGetSessionLengthBeats)
        {
            auto [sessionBeat, loopStart, loopEnd, loopEnabled] = onGetPlayheadState();
            double sessionLen = onGetSessionLengthBeats();
            double sessionBeatLooped = std::fmod(sessionBeat, sessionLen);
            if (sessionBeatLooped < 0) sessionBeatLooped += sessionLen;
            displayPlayheadBeat = std::fmod(sessionBeatLooped, previewClip.lengthBeats);
            if (displayPlayheadBeat < 0) displayPlayheadBeat += previewClip.lengthBeats;
        }
        else
        {
            displayPlayheadBeat = previewPlayheadBeat;
        }
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

        // Playhead (hidden when muted)
        if (!previewMuted)
        {
            float playheadX = (float)gridLeft + (float)displayPlayheadBeat * beatsToPx;
            g.setColour(colours::playhead());
            g.fillRect(playheadX - 1.0f, (float)contentTop, 2.0f, (float)contentH);
        }
    }
    else
    {
        g.setColour(colours::text());
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("no midi no cry", contentRect.expanded(frameMargin), juce::Justification::centred);
    }

    // M/S buttons below preview (centered in bottom row)
    float msY = stripY + (float)(previewH - msRowH) + (msRowH - btnSize) * 0.5f;
    float msX = (float)(getWidth() - frameMargin - btnSize * 2 - btnGap);
    float cxM = msX + btnSize * 0.5f;
    float cxS = msX + btnSize + btnGap + btnSize * 0.5f;
    float cy = msY + btnSize * 0.5f;
    g.setColour(previewMuted ? colours::muteRed() : colours::panelBorder());
    if (previewMuted)
        g.fillEllipse(cxM - btnSize * 0.5f, cy - btnSize * 0.5f, btnSize, btnSize);
    else
        g.drawEllipse(cxM - btnSize * 0.5f, cy - btnSize * 0.5f, btnSize, btnSize, 1.2f);
    g.setColour(previewMuted ? juce::Colours::white : colours::textDim());
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("M", cxM - 6.0f, cy - 6.0f, 12.0f, 12.0f, juce::Justification::centred);

    g.setColour(previewSoloed ? colours::soloGreen() : colours::panelBorder());
    if (previewSoloed)
        g.fillEllipse(cxS - btnSize * 0.5f, cy - btnSize * 0.5f, btnSize, btnSize);
    else
        g.drawEllipse(cxS - btnSize * 0.5f, cy - btnSize * 0.5f, btnSize, btnSize, 1.2f);
    g.setColour(previewSoloed ? juce::Colours::white : colours::textDim());
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("S", cxS - 6.0f, cy - 6.0f, 12.0f, 12.0f, juce::Justification::centred);

    fileTree->setColour(juce::FileTreeComponent::backgroundColourId, colours::bgLight());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    lblHeader.setColour(juce::Label::textColourId, colours::text());
}

void FileBrowserPanel::refreshComponentColours()
{
    fileTree->setColour(juce::FileTreeComponent::backgroundColourId, colours::bgLight());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);
    lblHeader.setColour(juce::Label::textColourId, colours::text());
    btnSetRoot.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnSetRoot.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
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
    if (file.hasFileExtension("mid;midi"))
    {
        auto clip = parseMidiFile(file);
        if (onClipDoubleClicked)
            onClipDoubleClicked(clip);
    }
}

} // namespace pflow
