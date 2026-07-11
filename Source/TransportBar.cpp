#include "TransportBar.h"

namespace pflow {

TransportBar::TransportBar()
{
    btnPlay.onClick = [this] { if (onPlayPause) onPlayPause(); };
    btnStop.onClick = [this] { if (onStop) onStop(); };
    addAndMakeVisible(btnPlay);
    addAndMakeVisible(btnStop);

    syncToggle.setClickingTogglesState(true);
    syncToggle.setToggleState(true, juce::dontSendNotification);
    syncToggle.onClick = [this]
    {
        synced = syncToggle.getToggleState();
        syncIcon.active = synced;
        refreshBpm();
        resized();
        if (onSyncChanged)
            onSyncChanged(synced);
    };
    addAndMakeVisible(syncToggle);

    syncIcon.active = true;
    syncIcon.setClickingTogglesState(true);
    syncIcon.setToggleState(true, juce::dontSendNotification);
    syncIcon.onClick = [this]
    {
        synced = syncIcon.getToggleState();
        syncToggle.setToggleState(synced, juce::dontSendNotification);
        syncIcon.active = synced;
        refreshBpm();
        if (onSyncChanged)
            onSyncChanged(synced);
    };
    addAndMakeVisible(syncIcon);

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

    btnHalf.mono = true;
    btnHalf.setTooltip("Play at half the DAW tempo (stretches notes for audition + export)");
    btnHalf.onClick = [this]
    {
        multiplier = juce::jmax(0.25, multiplier * 0.5);
        refreshBpm();
        if (onMultiplierChanged) onMultiplierChanged(multiplier);
    };
    addAndMakeVisible(btnHalf);

    btnDouble.mono = true;
    btnDouble.setTooltip("Play at double the DAW tempo (compresses notes for audition + export)");
    btnDouble.onClick = [this]
    {
        multiplier = juce::jmin(4.0, multiplier * 2.0);
        refreshBpm();
        if (onMultiplierChanged) onMultiplierChanged(multiplier);
    };
    addAndMakeVisible(btnDouble);

    btnDragToDaw.accentText = true;
    btnDragToDaw.setTooltip("Drag onto a DAW track to drop a MIDI file with the edits applied");
    btnDragToDaw.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    btnDragToDaw.onDragStart = [this] { if (onDragToDaw) onDragToDaw(); };
    addAndMakeVisible(btnDragToDaw);

    btnEditor.active = false;
    btnEditor.icon = icons::arrowsOut;
    btnEditor.setTooltip("Open editor");
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
    syncIcon.setToggleState(s, juce::dontSendNotification);
    syncIcon.active = s;
    refreshBpm();
    resized();
    if (notify != juce::dontSendNotification && onSyncChanged)
        onSyncChanged(synced);
}

void TransportBar::setHostBpm(double b)
{
    if (std::abs(b - hostBpm) < 0.01)
        return;
    hostBpm = b;
    if (synced)
        refreshBpm();
}

void TransportBar::setBpmMultiplier(double m)
{
    multiplier = juce::jlimit(0.25, 4.0, m);
    refreshBpm();
}

void TransportBar::setFreeBpm(double b)
{
    freeBpm = juce::jlimit(20.0, 300.0, b);
    if (!synced)
        refreshBpm();
}

void TransportBar::setClipBpm(double b)
{
    if (b > 1.0)
        freeBpm = juce::jlimit(20.0, 300.0, b);
    if (!synced)
        refreshBpm();
}

void TransportBar::setEditorOpen(bool open)
{
    editorOpen = open;
    btnEditor.icon = open ? icons::arrowsIn : icons::arrowsOut;
    btnEditor.active = open;
    btnEditor.setTooltip(open ? "Close editor" : "Open editor");
    btnEditor.repaint();
    resized();
}

void TransportBar::refreshBpm()
{
    syncToggle.onLabel = synced ? "Synced" : "Free";
    syncToggle.offLabel = syncToggle.onLabel;

    const double shown = synced ? hostBpm * multiplier : freeBpm;
    bpmLabel.setText(juce::String(shown, 1), juce::dontSendNotification);
    bpmLabel.setEditable(false, !synced, false);
    bpmLabel.setFont(monoFont(14.0f, true));
    bpmLabel.setColour(juce::Label::textColourId,
                       synced ? (multiplier != 1.0 ? colours::accent() : colours::text())
                              : colours::accent());
    bpmLabel.setColour(juce::Label::backgroundColourId,
                       synced ? juce::Colours::transparentBlack : colours::elev());
    bpmLabel.setColour(juce::TextEditor::textColourId, colours::accent());
    bpmLabel.setColour(juce::TextEditor::highlightedTextColourId, colours::text());
    bpmLabel.setTooltip(synced ? "DAW tempo x multiplier" : "Playback tempo, 20-300");

    btnHalf.active = multiplier < 1.0;
    btnDouble.active = multiplier > 1.0;
    btnHalf.setEnabled(synced);
    btnDouble.setEnabled(synced);
    btnHalf.repaint();
    btnDouble.repaint();
    bpmLabel.repaint();
    syncToggle.repaint();
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced(metrics::padS(), 0);
    const bool narrow = getWidth() < 460;
    const int btnSize = juce::jmin(28, getHeight() - 12);
    auto mid = [&](juce::Rectangle<int> a, int h) { return a.withSizeKeepingCentre(a.getWidth(), h); };

    r.removeFromLeft(narrow ? 30 : 118);

    auto play = r.removeFromLeft(btnSize);
    btnPlay.setBounds(mid(play, btnSize));
    r.removeFromLeft(2);
    btnStop.setBounds(mid(r.removeFromLeft(btnSize), btnSize));
    r.removeFromLeft(8);

    // Folded: infinity sync icon. Open: Synced/Free pill.
    syncToggle.setVisible(editorOpen);
    syncIcon.setVisible(!editorOpen);
    if (editorOpen)
        syncToggle.setBounds(mid(r.removeFromLeft(juce::jmin(syncToggle.idealWidth(),
                                                              narrow ? 72 : 96)), 24));
    else
        syncIcon.setBounds(mid(r.removeFromLeft(btnSize), btnSize));
    r.removeFromLeft(6);
    bpmLabel.setBounds(mid(r.removeFromLeft(narrow ? 48 : 56), 22));
    r.removeFromLeft(2);

    if (editorOpen)
    {
        btnHalf.setVisible(true);
        btnDouble.setVisible(true);
        btnHalf.setBounds(mid(r.removeFromLeft(narrow ? 30 : 34), 22));
        r.removeFromLeft(2);
        btnDouble.setBounds(mid(r.removeFromLeft(narrow ? 30 : 34), 22));

        btnDragToDaw.setVisible(!narrow);
        if (btnDragToDaw.isVisible())
        {
            btnDragToDaw.setBounds(mid(r.removeFromRight(btnDragToDaw.idealWidth()), 24));
            r.removeFromRight(8);
        }
        // Editor toggle flush to the right edge of the bar content.
        btnEditor.setBounds(mid(r.removeFromRight(btnSize), btnSize));
    }
    else
    {
        btnHalf.setVisible(false);
        btnDouble.setVisible(false);
        btnDragToDaw.setVisible(false);
        // Flush editor icon to the right — no trailing gap.
        btnEditor.setBounds(mid(r.removeFromRight(btnSize), btnSize));
    }
}

void TransportBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds();
    g.setColour(colours::panel());
    g.fillRect(b);
    g.setColour(colours::line());
    g.fillRect(b.removeFromBottom(1));

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
        g.setFont(uiFont(14.0f, true));
        g.drawText("Midi Browser", 32, 0, 84, getHeight(), juce::Justification::centredLeft);
    }
}

} // namespace pflow
