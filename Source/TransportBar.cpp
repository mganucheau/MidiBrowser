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

    btnHalf.mono = true;
    btnHalf.setTooltip("Play at half the DAW tempo");
    btnHalf.onClick = [this]
    {
        multiplier = juce::jmax(0.25, multiplier * 0.5);
        refreshBpm();
        if (onMultiplierChanged) onMultiplierChanged(multiplier);
    };
    addAndMakeVisible(btnHalf);

    btnDouble.mono = true;
    btnDouble.setTooltip("Play at double the DAW tempo");
    btnDouble.onClick = [this]
    {
        multiplier = juce::jmin(4.0, multiplier * 2.0);
        refreshBpm();
        if (onMultiplierChanged) onMultiplierChanged(multiplier);
    };
    addAndMakeVisible(btnDouble);

    btnDragToDaw.accentText = true;
    btnDragToDaw.setTooltip("Drag onto a DAW track");
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

    btnHalf.active = multiplier < 1.0;
    btnDouble.active = multiplier > 1.0;
    btnHalf.setEnabled(synced);
    btnDouble.setEnabled(synced);
    btnHalf.setVisible(synced);
    btnDouble.setVisible(synced);
    btnHalf.repaint();
    btnDouble.repaint();
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

    // Traffic lights + title reserved on the left (painted).
    r.removeFromLeft(118);

    btnPlay.setBounds(mid(r.removeFromLeft(btnSize), btnSize));
    r.removeFromLeft(4);
    btnStop.setBounds(mid(r.removeFromLeft(btnSize), btnSize));
    r.removeFromLeft(10);

    syncToggle.setBounds(mid(r.removeFromLeft(juce::jmin(syncToggle.idealWidth(), 88)), 24));
    r.removeFromLeft(6);
    bpmLabel.setBounds(mid(r.removeFromLeft(88), 22));
    r.removeFromLeft(4);

    if (synced)
    {
        btnHalf.setBounds(mid(r.removeFromLeft(32), 22));
        r.removeFromLeft(2);
        btnDouble.setBounds(mid(r.removeFromLeft(32), 22));
    }

    // Right: effects, editor, optional drag
    btnEffects.setBounds(mid(r.removeFromRight(btnSize), btnSize));
    r.removeFromRight(4);
    btnEditor.setBounds(mid(r.removeFromRight(btnSize), btnSize));

    btnDragToDaw.setVisible(editorOpen && r.getWidth() > 120);
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

    // Traffic lights
    const float cy = (float) getHeight() * 0.5f;
    const float dots[] = { 14.0f, 32.0f, 50.0f };
    const juce::Colour cols[] = {
        juce::Colour(0xffff5f57), juce::Colour(0xfffebc2e), juce::Colour(0xff28c840)
    };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour(cols[i]);
        g.fillEllipse(dots[i], cy - 5.0f, 10.0f, 10.0f);
    }

    g.setColour(colours::text());
    g.setFont(uiFont(13.0f, true));
    g.drawText("MidiBrowser", 68, 0, 90, getHeight(), juce::Justification::centredLeft);
}

} // namespace pflow
