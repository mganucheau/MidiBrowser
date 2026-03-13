#include "FileBrowserPanel.h"

namespace pflow {

FileBrowserPanel::FileBrowserPanel()
{
    lblHeader.setText("BROWSER", juce::dontSendNotification);
    lblHeader.setFont(juce::Font(13.0f, juce::Font::bold));
    lblHeader.setColour(juce::Label::textColourId, colours::textDim());
    addAndMakeVisible(lblHeader);

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
    fileTree->setDragAndDropDescription("MidiFileDrag");
    fileTree->setItemHeight(20);
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
    g.setColour(colours::panelBorder());
    g.drawLine((float)getWidth(), 0.0f, (float)getWidth(), (float)getHeight(), 1.0f);
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
