#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UiAtoms.h"

namespace pflow {

// Cupertino toolbar: traffic lights · title · play/stop · BPM pill · spacer ·
// editor / effects toggles. BPM is editable when unsynced.

class TransportBar : public juce::Component
{
public:
    TransportBar();

    void resized() override;
    void paint(juce::Graphics&) override;

    void setPlaying(bool);
    void setSynced(bool, juce::NotificationType notify = juce::dontSendNotification);
    void setHostBpm(double);
    void setBpmMultiplier(double);
    void setFreeBpm(double);
    void setClipBpm(double);
    void setEditorOpen(bool);
    void setEffectsOpen(bool);

    std::function<void()> onPlayPause;
    std::function<void()> onStop;
    std::function<void()> onToggleEditor;
    std::function<void()> onToggleEffects;
    std::function<void(bool)> onSyncChanged;
    std::function<void(double)> onFreeBpmChanged;
    std::function<void(double)> onMultiplierChanged;
    std::function<void()> onDragToDaw;

private:
    void refreshBpm();

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
    PillToggle syncToggle { "Synced", "Free" };
    juce::Label bpmLabel;
    ChipBtn btnHalf { "/2" };
    ChipBtn btnDouble { "x2" };
    DragChip btnDragToDaw { "Drag to DAW" };
    IconBtn btnEditor { icons::noteKeys, "Toggle editor" };
    IconBtn btnEffects { icons::sliders, "Toggle effects" };

    bool playing = false;
    bool synced = true;
    bool editorOpen = false;
    bool effectsOpen = false;
    double hostBpm = 124.0;
    double freeBpm = 124.0;
    double multiplier = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace pflow
