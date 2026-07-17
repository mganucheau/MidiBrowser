#include "TransportBar.h"

namespace pflow {

namespace {
constexpr float kHeaderIconScale = 1.0f;
constexpr int kBtnW = 34;
constexpr int kBtnH = 24;
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
    constexpr float kRadius = 5.0f;
    auto r = getLocalBounds().toFloat();
    g.setColour(ds::ctl().withMultipliedAlpha(foregroundAlpha > 0.9f ? 1.0f : 0.85f));
    g.fillRoundedRectangle(r, kRadius);
    g.setColour(ds::ctlb().withMultipliedAlpha(foregroundAlpha));
    g.drawRoundedRectangle(r.reduced(0.5f), kRadius, 1.0f);

    auto inner = r.reduced(12.0f, 0.0f);
    const auto value = editing ? editBuffer + "|"
                               : text.upToFirstOccurrenceOf(" ", false, false);
    const auto valueFont = ds::font(ds::Type::NumericDisplay);
    g.setColour(ds::tx().withMultipliedAlpha(foregroundAlpha));
    g.setFont(valueFont);
    const float valueW = juce::GlyphArrangement::getStringWidth(valueFont, value) + 4.0f;
    g.drawText(value, inner.removeFromLeft(valueW), juce::Justification::centredLeft, false);
    g.setColour(ds::tx3().withMultipliedAlpha(foregroundAlpha));
    g.setFont(ds::font(ds::Type::Caption).withHeight(9.0f).withExtraKerningFactor(0.08f));
    g.drawText("BPM", inner, juce::Justification::centredLeft, false);
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

// ── DragChip ─────────────────────────────────────────────────────────────────

int TransportBar::DragChip::idealWidth() const
{
    int w = 8 + 10 + 6 + 72 + 8; // pad + grip + gap + label + pad
    if (clipName.isNotEmpty())
        w += 6 + juce::jmin(140, clipName.length() * 7);
    return juce::jlimit(120, 220, w);
}

void TransportBar::DragChip::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto face = getLocalBounds().toFloat();
    juce::Colour fill = ds::ctl();
    if (down) fill = usesDarkAppearance() ? fill.brighter(0.06f) : fill.darker(0.06f);
    else if (over) fill = usesDarkAppearance() ? fill.brighter(0.03f) : fill.darker(0.03f);
    g.setColour(fill.withMultipliedAlpha(foregroundAlpha > 0.9f ? 1.0f : 0.85f));
    g.fillRoundedRectangle(face, 6.0f);
    g.setColour(ds::ctlb().withMultipliedAlpha(foregroundAlpha));
    g.drawRoundedRectangle(face.reduced(0.5f), 6.0f, 1.0f);

    auto inner = face.reduced(8.0f, 0.0f);
    {
        auto grip = inner.removeFromLeft(10.0f).withSizeKeepingCentre(8.0f, 12.0f);
        g.setColour(ds::tx3().withMultipliedAlpha(foregroundAlpha));
        for (int col = 0; col < 2; ++col)
            for (int row = 0; row < 3; ++row)
                g.fillEllipse(grip.getX() + (float) col * 4.0f,
                              grip.getY() + (float) row * 4.0f,
                              2.0f, 2.0f);
    }
    inner.removeFromLeft(6.0f);
    g.setColour(ds::tx2().withMultipliedAlpha(foregroundAlpha));
    g.setFont(ds::font(ds::Type::Metadata).withHeight(11.5f));
    g.drawText("Drag to DAW", inner.removeFromLeft(72.0f), juce::Justification::centredLeft, false);
    if (clipName.isNotEmpty())
    {
        inner.removeFromLeft(6.0f);
        g.setColour(ds::tx3().withMultipliedAlpha(foregroundAlpha));
        g.setFont(ds::font(ds::Type::Caption).withHeight(11.0f));
        g.drawText(clipName, inner, juce::Justification::centredLeft, true);
    }
}

// ── TransportBar ─────────────────────────────────────────────────────────────

TransportBar::TransportBar()
{
    auto prep = [](IconBtn& b, bool ghost)
    {
        b.ghost = ghost;
        b.iconScale = kHeaderIconScale;
        b.setWantsKeyboardFocus(true);
    };

    prep(btnPlay, true);
    btnPlay.setTooltip("Play / pause preview (Space)");
    btnPlay.onClick = [this] { if (onPlayPause) onPlayPause(); };
    addAndMakeVisible(btnPlay);

    prep(btnStop, true);
    btnStop.setTooltip("Stop preview");
    btnStop.onClick = [this] { if (onStop) onStop(); };
    addAndMakeVisible(btnStop);

    statusPill.onCommitBpm = [this](double bpm)
    {
        freeBpm = bpm;
        refreshBpm();
        if (onFreeBpmChanged) onFreeBpmChanged(freeBpm);
    };
    addAndMakeVisible(statusPill);

    prep(btnSync, false);
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
    btnDragToDaw.cornerRadius = 6.0f;
    btnDragToDaw.setTooltip("Drag edited clip onto a DAW track (or right-click to copy)");
    btnDragToDaw.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    btnDragToDaw.onDragStart = [this] { if (onDragToDaw) onDragToDaw(); };
    btnDragToDaw.onCopyToFolder = [this] { if (onCopyToFolder) onCopyToFolder(); };
    addAndMakeVisible(btnDragToDaw);

    prep(btnEditor, false);
    btnEditor.setTooltip("Toggle editor\nShortcut: E");
    btnEditor.onClick = [this] { if (onToggleEditor) onToggleEditor(); };
    addAndMakeVisible(btnEditor);

    prep(btnEffects, false);
    btnEffects.setTooltip("Toggle toolkit\nShortcut: F");
    btnEffects.onClick = [this] { if (onToggleEffects) onToggleEffects(); };
    addAndMakeVisible(btnEffects);

    prep(btnTheme, false);
    btnTheme.setTooltip("Toggle light / dark");
    btnTheme.onClick = [this]
    {
        if (onToggleTheme) onToggleTheme();
        refreshThemeIcon();
    };
    addAndMakeVisible(btnTheme);

    setOpaque(true);
    refreshBpm();
    refreshThemeIcon();
    startTimerHz(8);
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

void TransportBar::setClipName(const juce::String& name)
{
    if (btnDragToDaw.clipName == name) return;
    btnDragToDaw.clipName = name;
    resized();
    btnDragToDaw.repaint();
}

void TransportBar::refreshBpm()
{
    const double shown = synced ? hostBpm * multiplier : freeBpm;
    statusPill.synced = synced;
    statusPill.editable = !synced;
    statusPill.setText(juce::String(shown, 1) + " bpm");
    btnSync.active = synced;
    btnSync.setTooltip(synced ? "Synced to host - click for free tempo"
                              : "Free tempo - click to sync to host");
    btnSync.repaint();
}

void TransportBar::refreshThemeIcon()
{
    btnTheme.icon = usesDarkAppearance() ? icons::sun : icons::moon;
    btnTheme.repaint();
}

void TransportBar::timerCallback()
{
    const bool active = nativeChrome::isWindowKey(*this);
    if (active == windowActive) return;
    windowActive = active;
    const float a = windowActive ? 1.0f : 0.50f;
    statusPill.foregroundAlpha = a;
    btnDragToDaw.foregroundAlpha = a;
    btnPlay.setAlpha(a);
    btnStop.setAlpha(a);
    btnSync.setAlpha(a);
    btnEditor.setAlpha(a);
    btnEffects.setAlpha(a);
    btnTheme.setAlpha(a);
    repaint();
}

bool TransportBar::hitInteractive(juce::Point<int> p) const
{
    auto covers = [p](const juce::Component& c)
    {
        return c.isVisible() && c.getBounds().contains(p);
    };
    return covers(btnPlay) || covers(btnStop) || covers(statusPill) || covers(btnSync)
        || covers(btnDragToDaw) || covers(btnEditor) || covers(btnEffects) || covers(btnTheme);
}

void TransportBar::mouseDown(const juce::MouseEvent& e)
{
    if (hitInteractive(e.getPosition()))
        return;
#if JUCE_MAC
    if (nativeChrome::performWindowDrag(*this))
        return;
#endif
    if (auto* top = getTopLevelComponent())
        windowDragger.startDraggingComponent(top, e.getEventRelativeTo(top));
}

void TransportBar::mouseDrag(const juce::MouseEvent& e)
{
    if (hitInteractive(e.getMouseDownPosition()))
        return;
#if JUCE_MAC
    juce::ignoreUnused(e);
#else
    if (auto* top = getTopLevelComponent())
        windowDragger.dragComponent(top, e.getEventRelativeTo(top), nullptr);
#endif
}

void TransportBar::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (hitInteractive(e.getPosition()))
        return;
#if JUCE_MAC
    nativeChrome::zoomWindow(*this);
#else
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->maximiseButtonPressed();
#endif
}

void TransportBar::resized()
{
    auto r = getLocalBounds();
    const int h = r.getHeight();
    const int y = (h - kBtnH) / 2;
    const int gap = 8;

#if JUCE_MAC
    lightsZoneW = 76;
#else
    lightsZoneW = 16;
#endif

    // Left: traffic-light zone + app name
    auto left = r.removeFromLeft(lightsZoneW + 14 + 110);
    titleBounds = juce::Rectangle<int>(lightsZoneW + 14, 0, 110, h);
    juce::ignoreUnused(left);

    // Right: theme · toolkit · piano roll · divider · drag chip
    btnTheme.setBounds(r.removeFromRight(kBtnW).withY(y).withHeight(kBtnH));
    r.removeFromRight(4);
    btnEffects.setBounds(r.removeFromRight(kBtnW).withY(y).withHeight(kBtnH));
    r.removeFromRight(4);
    btnEditor.setBounds(r.removeFromRight(kBtnW).withY(y).withHeight(kBtnH));
    r.removeFromRight(8);
    dividerBounds = r.removeFromRight(1).withY((h - 16) / 2).withHeight(16);
    r.removeFromRight(8);

    const int chipH = 26;
    btnDragToDaw.setVisible((editorOpen || effectsOpen) && hasClip);
    if (btnDragToDaw.isVisible())
    {
        const int dragW = btnDragToDaw.idealWidth();
        btnDragToDaw.setBounds(r.removeFromRight(dragW).withY((h - chipH) / 2).withHeight(chipH));
        r.removeFromRight(8);
    }
    else
    {
        btnDragToDaw.setBounds({});
    }

    // Centre: play|stop group + BPM + sync
    const int groupW = kBtnW * 2;
    const int pillW = 88;
    const int centerW = groupW + gap + pillW + gap + kBtnW;
    const int rightEdge = btnDragToDaw.isVisible() ? btnDragToDaw.getX()
                                                   : dividerBounds.getX();
    int centerX = titleBounds.getRight() + (rightEdge - titleBounds.getRight() - centerW) / 2;
    centerX = juce::jmax(titleBounds.getRight() + 8, centerX);
    if (centerX + centerW > rightEdge - 8)
        centerX = juce::jmax(titleBounds.getRight() + 8, rightEdge - 8 - centerW);

    transportGroupBounds = { centerX, y, groupW, kBtnH };
    btnPlay.setBounds(transportGroupBounds.getX(), y, kBtnW, kBtnH);
    btnStop.setBounds(transportGroupBounds.getX() + kBtnW, y, kBtnW, kBtnH);
    statusPill.setBounds(transportGroupBounds.getRight() + gap, y, pillW, kBtnH);
    btnSync.setBounds(statusPill.getRight() + gap, y, kBtnW, kBtnH);
}

void TransportBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(ds::chrome());
    g.fillRect(b);
    g.setColour(ds::hl());
    g.fillRect(0.0f, (float) getHeight() - 1.0f, (float) getWidth(), 1.0f);

    const float fgAlpha = windowActive ? 1.0f : 0.50f;

    g.setColour(ds::tx2().withMultipliedAlpha(fgAlpha));
    g.setFont(ds::font(ds::Type::AppName));
    g.drawText("Midi Toolkit", titleBounds, juce::Justification::centredLeft, false);

    // Grouped transport face (outer corners only — play/stop stay ghost)
    if (!transportGroupBounds.isEmpty())
    {
        auto face = transportGroupBounds.toFloat();
        g.setColour(ds::ctl().withMultipliedAlpha(windowActive ? 1.0f : 0.85f));
        g.fillRoundedRectangle(face, 5.0f);
        g.setColour(ds::ctlb().withMultipliedAlpha(fgAlpha));
        g.drawRoundedRectangle(face.reduced(0.5f), 5.0f, 1.0f);
        const float midX = face.getCentreX();
        g.fillRect(midX - 0.5f, face.getY() + 4.0f, 1.0f, face.getHeight() - 8.0f);
    }

    if (dividerBounds.getWidth() > 0)
    {
        g.setColour(ds::hl().withMultipliedAlpha(fgAlpha));
        g.fillRect(dividerBounds.toFloat());
    }
}

} // namespace pflow
