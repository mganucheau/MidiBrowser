#include "TransportBar.h"

namespace pflow {

TransportBar::TransportBar()
{
    btnPlay.onClick = [this] { if (onPlayPause) onPlayPause(); };
    btnStop.onClick = [this] { if (onStop) onStop(); };
    addAndMakeVisible(btnPlay);
    addAndMakeVisible(btnStop);

    syncToggle.setToggleState(true, juce::dontSendNotification);
    syncToggle.onClick = [this]
    {
        synced = syncToggle.getToggleState();
        refreshBpm();
        resized();
        if (onSyncChanged)
            onSyncChanged(synced);
    };
    addAndMakeVisible(syncToggle);

    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.onTextChange = [this]
    {
        const double v = juce::jlimit(20.0, 300.0, bpmLabel.getText().getDoubleValue());
        freeBpm = std::round(v * 10.0) / 10.0;
        refreshBpm();
        if (onFreeBpmChanged)
            onFreeBpmChanged(freeBpm);
    };
    addAndMakeVisible(bpmLabel);

    pathLabel.setJustificationType(juce::Justification::centredRight);
    pathLabel.setInterceptsMouseClicks(false, false);
    pathLabel.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(pathLabel);

    btnDragToDaw.accentText = true;
    btnDragToDaw.setTooltip("Drag onto a DAW track to drop a MIDI file with the edits applied");
    btnDragToDaw.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    btnDragToDaw.onDragStart = [this] { if (onDragToDaw) onDragToDaw(); };
    addAndMakeVisible(btnDragToDaw);

    btnEditor.active = true;
    btnEditor.onClick = [this] { if (onToggleEditor) onToggleEditor(); };
    addAndMakeVisible(btnEditor);

    refreshBpm();
}

void TransportBar::setPlaying(bool p)
{
    if (playing == p) return;
    playing = p;
    btnPlay.icon = playing ? icons::pause : icons::play;
    btnPlay.repaint();
}

void TransportBar::setSynced(bool s, juce::NotificationType notify)
{
    synced = s;
    syncToggle.setToggleState(s, juce::dontSendNotification);
    refreshBpm();
    resized();
    if (notify != juce::dontSendNotification && onSyncChanged)
        onSyncChanged(synced);
}

void TransportBar::setClipBpm(double b)
{
    clipBpm = b;
    if (synced)
        refreshBpm();
}

void TransportBar::setFreeBpm(double b)
{
    freeBpm = juce::jlimit(20.0, 300.0, b);
    if (!synced)
        refreshBpm();
}

void TransportBar::setFolderPath(const juce::String& p)
{
    pathLabel.setText(p, juce::dontSendNotification);
}

void TransportBar::setEditorOpen(bool open)
{
    editorOpen = open;
    btnEditor.icon = open ? icons::arrowsIn : icons::arrowsOut;
    btnEditor.active = open;
    btnEditor.repaint();
    resized();
}

void TransportBar::refreshBpm()
{
    const double shown = synced ? clipBpm : freeBpm;
    bpmLabel.setText(juce::String(shown, 1), juce::dontSendNotification);
    bpmLabel.setEditable(false, !synced, false);   // double-click to edit when free-run
    bpmLabel.setFont(monoFont(12.0f, true));
    bpmLabel.setColour(juce::Label::textColourId, synced ? colours::text2() : colours::accent());
    bpmLabel.setColour(juce::Label::backgroundColourId,
                       synced ? juce::Colours::transparentBlack : colours::elev());
    bpmLabel.setColour(juce::TextEditor::textColourId, colours::accent());
    bpmLabel.setColour(juce::TextEditor::highlightedTextColourId, colours::text());
    bpmLabel.setTooltip(synced ? "Clip tempo (synced to DAW)" : "Playback tempo, 20-300");
    bpmLabel.repaint();
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced(metrics::padS(), 0);
    const bool narrow = getWidth() < 460;
    const int btnSize = juce::jmin(28, getHeight() - 12);
    auto mid = [&](juce::Rectangle<int> a, int h) { return a.withSizeKeepingCentre(a.getWidth(), h); };

    // Left: glyph (painted) + wordmark space
    r.removeFromLeft(narrow ? 30 : 118);

    auto play = r.removeFromLeft(btnSize);
    btnPlay.setBounds(mid(play, btnSize));
    r.removeFromLeft(2);
    btnStop.setBounds(mid(r.removeFromLeft(btnSize), btnSize));
    r.removeFromLeft(8);

    syncToggle.setBounds(mid(r.removeFromLeft(juce::jmin(syncToggle.idealWidth(),
                                                          narrow ? 96 : 140)), 24));
    r.removeFromLeft(6);
    bpmLabel.setBounds(mid(r.removeFromLeft(52), 22));

    // Right: Editor button, always fully inside with normal padding; the
    // Drag-to-DAW chip sits to its left when the window is expanded.
    auto editorArea = r.removeFromRight(btnEditor.idealWidth());
    btnEditor.setBounds(mid(editorArea, 24));
    r.removeFromRight(6);

    btnDragToDaw.setVisible(!narrow && editorOpen);
    if (btnDragToDaw.isVisible())
    {
        btnDragToDaw.setBounds(mid(r.removeFromRight(btnDragToDaw.idealWidth()), 24));
        r.removeFromRight(8);
    }

    pathLabel.setFont(monoFont(10.5f, false));
    pathLabel.setColour(juce::Label::textColourId, colours::text3());
    pathLabel.setVisible(!narrow && r.getWidth() > 60);
    pathLabel.setBounds(mid(r, 22));
}

void TransportBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour(colours::panel());
    g.fillRect(b);
    g.setColour(colours::line());
    g.fillRect(b.removeFromBottom(1));

    // App glyph: rounded square with two "note" bars.
    const bool narrow = getWidth() < 460;
    auto glyph = juce::Rectangle<float>(8.0f, (float) getHeight() * 0.5f - 9.0f, 18.0f, 18.0f);
    g.setColour(colours::accent());
    g.fillRoundedRectangle(glyph, 5.0f);
    g.setColour(colours::accentInk());
    g.fillRoundedRectangle(glyph.getX() + 4.0f, glyph.getY() + 5.0f, 7.0f, 2.6f, 1.3f);
    g.fillRoundedRectangle(glyph.getX() + 7.0f, glyph.getY() + 10.4f, 7.0f, 2.6f, 1.3f);

    if (!narrow)
    {
        g.setColour(colours::text());
        g.setFont(uiFont(13.5f, true));
        g.drawText("Midi Browser", 32, 0, 84, getHeight(), juce::Justification::centredLeft);
    }
}

} // namespace pflow
