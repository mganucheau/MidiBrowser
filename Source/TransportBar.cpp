#include "TransportBar.h"

namespace pflow {

// ── StatusPill ───────────────────────────────────────────────────────────────

void TransportBar::StatusPill::setText(const juce::String& t)
{
    if (text == t && !editing) return;
    text = t;
    repaint();
}

void TransportBar::StatusPill::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // Prototype: white raised pill on gray toolbar.
    g.setColour(colours::panel());
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(colours::line());
    g.drawRoundedRectangle(r.reduced(0.5f), r.getHeight() * 0.5f, 0.5f);

    g.setColour(colours::text());
    g.setFont(monoFont(12.0f, true));
    g.drawFittedText(editing ? editBuffer + "|" : text,
                     getLocalBounds().reduced(12, 0),
                     juce::Justification::centred, 1);
}

void TransportBar::StatusPill::mouseDown(const juce::MouseEvent&)
{
    if (editing) return;
    if (synced)
    {
        if (onToggleSync) onToggleSync();
        return;
    }
    // Free: wait to distinguish single-click (re-sync) vs double-click (edit).
    pendingSyncToggle = true;
    startTimer(220);
}

void TransportBar::StatusPill::mouseDoubleClick(const juce::MouseEvent&)
{
    if (synced) return;
    pendingSyncToggle = false;
    stopTimer();
    editing = true;
    editBuffer = text.upToFirstOccurrenceOf(" ", false, false);
    setWantsKeyboardFocus(true);
    grabKeyboardFocus();
    repaint();
}

void TransportBar::StatusPill::timerCallback()
{
    stopTimer();
    if (pendingSyncToggle && onToggleSync)
        onToggleSync();
    pendingSyncToggle = false;
}

void TransportBar::StatusPill::focusLost(FocusChangeType)
{
    if (!editing) return;
    editing = false;
    const double v = juce::jlimit(20.0, 300.0, editBuffer.getDoubleValue());
    if (onCommitBpm) onCommitBpm(std::round(v * 10.0) / 10.0);
    repaint();
}

bool TransportBar::StatusPill::keyPressed(const juce::KeyPress& key)
{
    if (!editing) return false;
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::escapeKey)
    {
        if (key == juce::KeyPress::returnKey)
        {
            const double v = juce::jlimit(20.0, 300.0, editBuffer.getDoubleValue());
            if (onCommitBpm) onCommitBpm(std::round(v * 10.0) / 10.0);
        }
        editing = false;
        repaint();
        return true;
    }
    if (key == juce::KeyPress::backspaceKey)
    {
        if (editBuffer.isNotEmpty())
            editBuffer = editBuffer.dropLastCharacters(1);
        repaint();
        return true;
    }
    const auto ch = key.getTextCharacter();
    if ((ch >= '0' && ch <= '9') || ch == '.')
    {
        editBuffer += juce::String::charToString(ch);
        repaint();
        return true;
    }
    return false;
}

// ── TransportBar ─────────────────────────────────────────────────────────────

TransportBar::TransportBar()
{
    btnPlay.ghost = true;
    btnPlay.onClick = [this] { if (onPlayPause) onPlayPause(); };
    addAndMakeVisible(btnPlay);

    btnStop.ghost = true;
    btnStop.onClick = [this] { if (onStop) onStop(); };
    addAndMakeVisible(btnStop);

    statusPill.onToggleSync = [this]
    {
        synced = !synced;
        refreshBpm();
        resized();
        if (onSyncChanged) onSyncChanged(synced);
    };
    statusPill.onCommitBpm = [this](double bpm)
    {
        freeBpm = bpm;
        refreshBpm();
        if (onFreeBpmChanged) onFreeBpmChanged(freeBpm);
    };
    addAndMakeVisible(statusPill);

    btnDragToDaw.accentText = false;
    btnDragToDaw.setTooltip("Drag edited clip onto a DAW track");
    btnDragToDaw.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    btnDragToDaw.onDragStart = [this] { if (onDragToDaw) onDragToDaw(); };
    addAndMakeVisible(btnDragToDaw);

    btnAppearance.ghost = true;
    btnAppearance.onClick = [this] { if (onToggleAppearance) onToggleAppearance(); };
    addAndMakeVisible(btnAppearance);

    btnEditor.ghost = true;
    btnEditor.setTooltip("Toggle editor (E)");
    btnEditor.onClick = [this] { if (onToggleEditor) onToggleEditor(); };
    addAndMakeVisible(btnEditor);

    btnEffects.ghost = true;
    btnEffects.setTooltip("Toggle effects (F)");
    btnEffects.onClick = [this] { if (onToggleEffects) onToggleEffects(); };
    addAndMakeVisible(btnEffects);

    refreshAppearanceIcon();
    refreshBpm();
}

void TransportBar::refreshAppearanceIcon()
{
    btnAppearance.icon = usesDarkAppearance() ? icons::sun : icons::moon;
    btnAppearance.setTooltip(usesDarkAppearance() ? "Switch to light appearance"
                                                  : "Switch to dark appearance");
    btnAppearance.repaint();
}

void TransportBar::setPlaying(bool p)
{
    if (playing == p) return;
    playing = p;
    btnPlay.active = playing;
    btnPlay.repaint();
}

void TransportBar::setSynced(bool s, juce::NotificationType notify)
{
    synced = s;
    statusPill.synced = s;
    refreshBpm();
    resized();
    if (notify != juce::dontSendNotification && onSyncChanged)
        onSyncChanged(synced);
}

void TransportBar::setHostBpm(double b)
{
    if (std::abs(b - hostBpm) < 0.01) return;
    hostBpm = b;
    if (synced) refreshBpm();
}

void TransportBar::setBpmMultiplier(double m)
{
    multiplier = juce::jlimit(0.25, 4.0, m);
    if (synced) refreshBpm();
}

void TransportBar::setFreeBpm(double b)
{
    freeBpm = juce::jlimit(20.0, 300.0, b);
    if (!synced) refreshBpm();
}

void TransportBar::setClipBpm(double b)
{
    if (b > 1.0)
        freeBpm = juce::jlimit(20.0, 300.0, b);
    if (!synced) refreshBpm();
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
    const juce::String label = juce::String(shown, 1) + (synced ? " bpm · Synced" : " bpm · Free");
    statusPill.synced = synced;
    statusPill.setText(label);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced(0, 8);
    const int btn = 28;
    const int gap = 6;
    const int titleW = 108;

    auto mid = [&](juce::Rectangle<int> a, int h)
    {
        return a.withSizeKeepingCentre(a.getWidth(), h);
    };

    r.removeFromLeft(titleW);

    // Right cluster: appearance · editor · effects
    btnEffects.setBounds(mid(r.removeFromRight(btn), btn));
    r.removeFromRight(gap);
    btnEditor.setBounds(mid(r.removeFromRight(btn), btn));
    r.removeFromRight(gap);
    btnAppearance.setBounds(mid(r.removeFromRight(btn), btn));

    btnDragToDaw.setVisible(editorOpen && hasClip);
    if (btnDragToDaw.isVisible())
    {
        r.removeFromRight(10);
        btnDragToDaw.setBounds(mid(r.removeFromRight(btnDragToDaw.idealWidth()), 26));
    }

    // Center transport: play · stop · status pill
    const int pillW = 148;
    const int transportW = btn + gap + btn + gap + pillW;
    auto transport = r.withSizeKeepingCentre(transportW, getHeight());
    btnPlay.setBounds(mid(transport.removeFromLeft(btn), btn));
    transport.removeFromLeft(gap);
    btnStop.setBounds(mid(transport.removeFromLeft(btn), btn));
    transport.removeFromLeft(gap);
    statusPill.setBounds(mid(transport.removeFromLeft(pillW), 26));
}

void TransportBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(colours::bg());
    g.fillRect(b);
    g.setColour(colours::line());
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    g.setColour(colours::text());
    g.setFont(uiFont(13.0f, true));
    g.drawText("MidiBrowser", 14, 0, 100, getHeight(), juce::Justification::centredLeft);
}

} // namespace pflow
