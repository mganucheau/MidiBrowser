#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UiAtoms.h"

namespace pflow {

// ── Transport bar ────────────────────────────────────────────────────────────
// app glyph + wordmark · play/stop · DAW-sync pill · BPM · (spacer) ·
// folder path · Editor fold button. BPM is a static readout when synced and
// an editable number input (20–300) driving playback tempo when free-run.

class TransportBar : public juce::Component
{
public:
    TransportBar();

    void resized() override;
    void paint(juce::Graphics&) override;

    void setPlaying(bool);
    void setSynced(bool, juce::NotificationType notify = juce::dontSendNotification);
    void setHostBpm(double);      // live DAW tempo (shown ×multiplier when synced)
    void setBpmMultiplier(double);
    void setFreeBpm(double);
    void setClipBpm(double);      // embedded tempo from the selected MIDI file
    void setFolderPath(const juce::String&);
    void setEditorOpen(bool);

    std::function<void()> onPlayPause;
    std::function<void()> onStop;
    std::function<void()> onToggleEditor;
    std::function<void(bool)> onSyncChanged;
    std::function<void(double)> onFreeBpmChanged;
    std::function<void(double)> onMultiplierChanged;   // ÷2 / ×2 while synced
    std::function<void()> onDragToDaw;   // begin external drag of the edited clip

private:
    void refreshBpm();

    /** Chip that starts an external drag instead of clicking. */
    class DragChip : public ChipBtn
    {
    public:
        using ChipBtn::ChipBtn;
        std::function<void()> onDragStart;
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!dragging && e.getDistanceFromDragStart() > 5 && onDragStart)
            {
                dragging = true;
                onDragStart();
            }
        }
        void mouseUp(const juce::MouseEvent& e) override
        {
            dragging = false;
            ChipBtn::mouseUp(e);
        }

    private:
        bool dragging = false;
    };

    IconBtn btnPlay { icons::play, "Play / pause preview" };
    IconBtn btnStop { icons::stop, "Stop" };
    PillToggle syncToggle { "Synced to DAW", "Free-run" };
    juce::Label bpmLabel;
    ChipBtn btnHalf { "/2" };
    ChipBtn btnDouble { "x2" };
    juce::Label pathLabel;
    DragChip btnDragToDaw { "Drag to DAW" };
    ChipBtn btnEditor { "Editor", icons::arrowsIn };

    bool playing = false;
    bool synced = true;
    bool editorOpen = true;
    double hostBpm = 124.0;
    double freeBpm = 124.0;
    double multiplier = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace pflow
