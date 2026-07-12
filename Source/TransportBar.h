#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UiAtoms.h"

namespace pflow {

/** Prototype toolbar: MidiBrowser · play/stop · bpm pill · moon / editor / effects. */
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
    void setHasClip(bool has);
    void refreshAppearanceIcon();

    std::function<void()> onPlayPause;
    std::function<void()> onStop;
    std::function<void()> onToggleEditor;
    std::function<void()> onToggleEffects;
    std::function<void(bool)> onSyncChanged;
    std::function<void(double)> onFreeBpmChanged;
    std::function<void()> onDragToDaw;
    std::function<void()> onToggleAppearance;

private:
    void refreshBpm();

    class StatusPill : public juce::Component,
                       private juce::Timer
    {
    public:
        std::function<void()> onToggleSync;
        std::function<void(double)> onCommitBpm;
        bool synced = true;
        bool editable = false;

        void setText(const juce::String& t);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;
        bool keyPressed(const juce::KeyPress&) override;
        void focusLost(FocusChangeType) override;
        void timerCallback() override;

        juce::String text;
        bool editing = false;
        juce::String editBuffer;
        bool pendingSyncToggle = false;
    };

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

    IconBtn btnPlay { icons::play, "Play preview" };
    IconBtn btnStop { icons::stop, "Stop preview" };
    StatusPill statusPill;
    IconBtn btnSync { icons::infinity, "Sync to host tempo" };
    DragChip btnDragToDaw { "Drag to DAW" };
    IconBtn btnAppearance { icons::moon, "Toggle light / dark" };
    IconBtn btnEditor { icons::noteKeys, "Toggle editor" };
    IconBtn btnEffects { icons::sliders, "Toggle effects" };

    bool playing = false;
    bool synced = true;
    bool editorOpen = false;
    bool effectsOpen = false;
    bool hasClip = false;
    double hostBpm = 124.0;
    double freeBpm = 124.0;
    double multiplier = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace pflow
