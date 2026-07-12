#include "TransportBar.h"

namespace pflow {

namespace {
constexpr int kTitlePad = 14;
constexpr float kHeaderIconScale = 1.0f;
}

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
    if (editing || synced) return;
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
    auto prep = [](IconBtn& b)
    {
        b.ghost = true;
        b.iconScale = kHeaderIconScale;
    };

    prep(btnPlay);
    btnPlay.onClick = [this] { if (onPlayPause) onPlayPause(); };
    addAndMakeVisible(btnPlay);

    prep(btnStop);
    btnStop.onClick = [this] { if (onStop) onStop(); };
    addAndMakeVisible(btnStop);

    statusPill.onCommitBpm = [this](double bpm)
    {
        freeBpm = bpm;
        refreshBpm();
        if (onFreeBpmChanged) onFreeBpmChanged(freeBpm);
    };
    addAndMakeVisible(statusPill);

    prep(btnSync);
    btnSync.setTooltip("Sync to host tempo");
    btnSync.onClick = [this]
    {
        synced = !synced;
        refreshBpm();
        resized();
        if (onSyncChanged) onSyncChanged(synced);
    };
    addAndMakeVisible(btnSync);

    btnDragToDaw.accentText = false;
    btnDragToDaw.setTooltip("Drag edited clip onto a DAW track");
    btnDragToDaw.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    btnDragToDaw.onDragStart = [this] { if (onDragToDaw) onDragToDaw(); };
    addAndMakeVisible(btnDragToDaw);

    prep(btnEditor);
    btnEditor.setTooltip("Toggle editor (E)");
    btnEditor.onClick = [this] { if (onToggleEditor) onToggleEditor(); };
    addAndMakeVisible(btnEditor);

    prep(btnEffects);
    btnEffects.setTooltip("Toggle effects (F)");
    btnEffects.onClick = [this] { if (onToggleEffects) onToggleEffects(); };
    addAndMakeVisible(btnEffects);

    refreshBpm();
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
    statusPill.synced = synced;
    statusPill.editable = !synced;
    statusPill.setText(juce::String(shown, 1) + " bpm");
    btnSync.active = synced;
    btnSync.setTooltip(synced ? "Synced to host — click for free tempo"
                              : "Free tempo — click to sync to host");
    btnSync.repaint();
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
    r.removeFromRight(kTitlePad);

    // Right cluster: editor · effects
    btnEffects.setBounds(mid(r.removeFromRight(btn), btn));
    r.removeFromRight(gap);
    btnEditor.setBounds(mid(r.removeFromRight(btn), btn));

    btnDragToDaw.setVisible(editorOpen && hasClip);
    if (btnDragToDaw.isVisible())
    {
        r.removeFromRight(10);
        btnDragToDaw.setBounds(mid(r.removeFromRight(btnDragToDaw.idealWidth()), 26));
    }

    // Center transport: play · stop · bpm pill · sync
    const int pillW = 88;
    const int transportW = btn + gap + btn + gap + pillW + gap + btn;
    auto transport = r.withSizeKeepingCentre(transportW, getHeight());
    btnPlay.setBounds(mid(transport.removeFromLeft(btn), btn));
    transport.removeFromLeft(gap);
    btnStop.setBounds(mid(transport.removeFromLeft(btn), btn));
    transport.removeFromLeft(gap);
    statusPill.setBounds(mid(transport.removeFromLeft(pillW), 26));
    transport.removeFromLeft(gap);
    btnSync.setBounds(mid(transport.removeFromLeft(btn), btn));
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
    g.drawText("MidiBrowser", kTitlePad, 0, 100, getHeight(), juce::Justification::centredLeft);
}

} // namespace pflow
