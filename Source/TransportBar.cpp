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
        refreshBpm();
        resized();
        if (onSyncChanged)
            onSyncChanged(synced);
    };
    addAndMakeVisible(syncToggle);

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
    syncToggle.onLabel = synced ? "Synced" : "Free";
    syncToggle.offLabel = syncToggle.onLabel;

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
    syncToggle.repaint();
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced(12, 0);
    const int btnSize = 26;
    auto mid = [&](juce::Rectangle<int> a, int h)
    {
        return a.withSizeKeepingCentre(a.getWidth(), h);
    };

    r.removeFromLeft(4); // title painted in paint()

    btnPlay.setBounds(mid(r.removeFromLeft(btnSize), btnSize));
    r.removeFromLeft(4);
    btnStop.setBounds(mid(r.removeFromLeft(btnSize), btnSize));
    r.removeFromLeft(10);

    syncToggle.setBounds(mid(r.removeFromLeft(juce::jmin(syncToggle.idealWidth(), 88)), 24));
    r.removeFromLeft(6);
    bpmLabel.setBounds(mid(r.removeFromLeft(88), 22));

    btnEffects.setBounds(mid(r.removeFromRight(btnSize), btnSize));
    r.removeFromRight(4);
    btnEditor.setBounds(mid(r.removeFromRight(btnSize), btnSize));

    btnDragToDaw.setVisible(editorOpen && hasClip);
    if (btnDragToDaw.isVisible())
    {
        r.removeFromRight(8);
        btnDragToDaw.setBounds(mid(r.removeFromRight(btnDragToDaw.idealWidth()), 24));
    }
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
    g.drawText("MidiBrowser", 12, 0, 100, getHeight(), juce::Justification::centredLeft);
}

} // namespace pflow
