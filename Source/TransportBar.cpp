#include "TransportBar.h"

namespace pflow {

TransportBar::TransportBar()
{
    btnPlay.onClick = [this]
    {
        if (playing)
        {
            if (onStop) onStop();
        }
        else if (onPlayPause)
            onPlayPause();
    };
    addAndMakeVisible(btnPlay);

    btnSync.setClickingTogglesState(true);
    btnSync.setToggleState(true, juce::dontSendNotification);
    btnSync.onClick = [this]
    {
        synced = btnSync.getToggleState();
        refreshBpm();
        resized();
        if (onSyncChanged)
            onSyncChanged(synced);
    };
    addAndMakeVisible(btnSync);

    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    bpmLabel.onTextChange = [this]
    {
        const double v = juce::jlimit(20.0, 300.0, bpmLabel.getText().getDoubleValue());
        freeBpm = std::round(v * 10.0) / 10.0;
        refreshBpm();
        if (onFreeBpmChanged)
            onFreeBpmChanged(freeBpm);
    };
    addAndMakeVisible(bpmLabel);

    btnDragToDaw.accentText = true;
    btnDragToDaw.setTooltip("Drag edited clip onto a DAW track");
    btnDragToDaw.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    btnDragToDaw.onDragStart = [this] { if (onDragToDaw) onDragToDaw(); };
    addAndMakeVisible(btnDragToDaw);

    btnEditor.ghost = true;
    btnEditor.setTooltip("Toggle editor (E)");
    btnEditor.onClick = [this] { if (onToggleEditor) onToggleEditor(); };
    addAndMakeVisible(btnEditor);

    btnEffects.ghost = true;
    btnEffects.setTooltip("Toggle effects (F)");
    btnEffects.onClick = [this] { if (onToggleEffects) onToggleEffects(); };
    addAndMakeVisible(btnEffects);

    refreshBpm();
}

void TransportBar::setPlaying(bool p)
{
    if (playing == p) return;
    playing = p;
    btnPlay.icon = playing ? icons::stop : icons::play;
    btnPlay.setTooltip(playing ? "Stop preview" : "Play preview");
    btnPlay.repaint();
}

void TransportBar::setSynced(bool s, juce::NotificationType notify)
{
    synced = s;
    btnSync.setToggleState(s, juce::dontSendNotification);
    btnSync.active = s;
    btnSync.repaint();
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
    if (synced)
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
    btnEditor.active = open;
    btnEditor.repaint();
    resized();
}

void TransportBar::setEffectsOpen(bool open)
{
    effectsOpen = open;
    btnEffects.active = open;
    btnEffects.repaint();
    resized();
}

void TransportBar::setHasClip(bool has)
{
    hasClip = has;
    resized();
}

void TransportBar::refreshBpm()
{
    const double shown = synced ? hostBpm * multiplier : freeBpm;
    bpmLabel.setText(juce::String(shown, 1) + " bpm", juce::dontSendNotification);
    bpmLabel.setEditable(false, !synced, false);
    bpmLabel.setFont(monoFont(12.0f, true));
    bpmLabel.setColour(juce::Label::textColourId, colours::text());
    bpmLabel.setColour(juce::Label::backgroundColourId,
                       synced ? juce::Colours::transparentBlack : colours::elev());
    bpmLabel.setColour(juce::TextEditor::textColourId, colours::accent());
    bpmLabel.setTooltip(synced ? "DAW tempo × multiplier" : "Playback tempo, 20–300");
    bpmLabel.repaint();
}

void TransportBar::resized()
{
    auto r = getLocalBounds();
    const int btnSize = 26;
    const int titleW = 118;
    const int transportW = btnSize + 8 + btnSize + 8 + 88;

    r.removeFromLeft(titleW);

    auto mid = [&](juce::Rectangle<int> a, int h)
    {
        return a.withSizeKeepingCentre(a.getWidth(), h);
    };

    btnEffects.setBounds(mid(r.removeFromRight(btnSize), btnSize));
    r.removeFromRight(4);
    btnEditor.setBounds(mid(r.removeFromRight(btnSize), btnSize));

    btnDragToDaw.setVisible(editorOpen && hasClip);
    if (btnDragToDaw.isVisible())
    {
        r.removeFromRight(8);
        btnDragToDaw.setBounds(mid(r.removeFromRight(btnDragToDaw.idealWidth()), 24));
    }

    auto transport = r.withSizeKeepingCentre(transportW, getHeight());
    btnPlay.setBounds(mid(transport.removeFromLeft(btnSize), btnSize));
    transport.removeFromLeft(8);
    btnSync.setBounds(mid(transport.removeFromLeft(btnSize), btnSize));
    transport.removeFromLeft(8);
    bpmLabel.setBounds(mid(transport.removeFromLeft(88), 22));
}

void TransportBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(colours::toolbarTop(), 0, 0,
                                           colours::toolbarBot(), 0, b.getHeight(), false));
    g.fillRect(b);
    g.setColour(colours::lineStrong());
    g.fillRect(b.removeFromBottom(1.0f));

    g.setColour(colours::text());
    g.setFont(uiFont(13.0f, true));
    g.drawText("MidiBrowser", 12, 0, 110, getHeight(), juce::Justification::centredLeft);
}

} // namespace pflow
