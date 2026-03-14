#include "FileBrowserPanel.h"

namespace pflow {

FileBrowserPanel::FileBrowserPanel()
{
    lblHeader.setText(juce::String::charToString(0x266B) + "  BROWSER", juce::dontSendNotification);
    lblHeader.setFont(juce::Font(13.0f, juce::Font::bold));
    lblHeader.setColour(juce::Label::textColourId, colours::textDim());
    addAndMakeVisible(lblHeader);

    btnSetRoot.setButtonText(juce::String::charToString(0x1F4C2) + " Set Folder...");
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
    fileTree->setColour(juce::FileTreeComponent::backgroundColourId, colours::panel());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);
    fileTree->setDragAndDropDescription("MidiFileDrag");
    fileTree->setItemHeight(22);
    fileTree->addListener(this);
    addAndMakeVisible(*fileTree);
}

FileBrowserPanel::~FileBrowserPanel()
{
    fileTree->removeListener(this);
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
    lblHeader.setBounds(b.removeFromTop(22).reduced(metrics::padding, 2));
    btnSetRoot.setBounds(b.removeFromTop(metrics::buttonH).reduced(metrics::padding, 2));
    fileTree->setBounds(b.reduced(2, 2));
}

void FileBrowserPanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel());

    // Right border separator
    g.setColour(colours::panelBorder());
    g.drawLine((float)getWidth() - 0.5f, 0.0f, (float)getWidth() - 0.5f, (float)getHeight(), 1.0f);

    // Subtle top accent line
    g.setColour(colours::accent().withAlpha(0.15f));
    g.fillRect(0.0f, 0.0f, (float)getWidth(), 2.0f);

    // Refresh file tree colours on each paint (handles theme toggle)
    fileTree->setColour(juce::FileTreeComponent::backgroundColourId, colours::panel());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    lblHeader.setColour(juce::Label::textColourId, colours::textDim());
}

void FileBrowserPanel::refreshComponentColours()
{
    fileTree->setColour(juce::FileTreeComponent::backgroundColourId, colours::panel());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    fileTree->setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);
    lblHeader.setColour(juce::Label::textColourId, colours::textDim());
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
