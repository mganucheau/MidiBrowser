#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "UiAtoms.h"
#include "NativeWindowChrome.h"

namespace pflow {

/** Cupertino 52px header: lights zone · app name · transport · drag chip · toggles. */
class TransportBar : public juce::Component,
                     private juce::Timer
{
public:
    TransportBar();

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void setPlaying(bool);
    void setSynced(bool, juce::NotificationType notify = juce::dontSendNotification);
    void setHostBpm(double);
    void setBpmMultiplier(double);
    void setFreeBpm(double);
    void setClipBpm(double);
    void setEditorOpen(bool);
    void setEffectsOpen(bool);
    void setHasClip(bool has);
    void setClipName(const juce::String& name);
    /** Standalone macOS reserves a traffic-light gutter; plugins must not. */
    void setReserveTrafficLights(bool reserve);

    std::function<void()> onPlayPause;
    std::function<void()> onStop;
    std::function<void()> onToggleEditor;
    std::function<void()> onToggleEffects;
    std::function<void(bool)> onSyncChanged;
    std::function<void(double)> onFreeBpmChanged;
    std::function<void()> onDragToDaw;
    std::function<void()> onCopyToFolder;

private:
    void refreshBpm();
    void timerCallback() override;
    bool hitInteractive(juce::Point<int> p) const;

    class StatusPill : public juce::Component,
                       private juce::Timer
    {
    public:
        std::function<void()> onToggleSync;
        std::function<void(double)> onCommitBpm;
        bool synced = true;
        bool editable = false;
        float foregroundAlpha = 1.0f;

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
        std::function<void()> onCopyToFolder;
        juce::String clipName;
        float foregroundAlpha = 1.0f;

        void paintButton(juce::Graphics& g, bool over, bool down) override;
        int idealWidth() const;

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem(1, "Copy to Folder...", onCopyToFolder != nullptr);
                m.showMenuAsync(juce::PopupMenu::Options()
                                    .withTargetComponent(this)
                                    .withDeletionCheck(*this),
                    [this](int result)
                    {
                        if (result == 1 && onCopyToFolder)
                            onCopyToFolder();
                    });
                return;
            }
            ChipBtn::mouseDown(e);
        }
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
    IconBtn btnSync { icons::sync, "Sync to host tempo" };
    DragChip btnDragToDaw { "Drag to DAW" };
    IconBtn btnEditor { icons::noteKeys, "Toggle editor" };
    IconBtn btnEffects { icons::sidebarRight, "Toggle toolkit" };

    juce::Rectangle<int> titleBounds, transportGroupBounds, dividerBounds;

    bool playing = false;
    bool synced = true;
    bool editorOpen = false;
    bool effectsOpen = false;
    bool hasClip = false;
    bool windowActive = true;
    bool reserveTrafficLights = false;
    double hostBpm = 124.0;
    double freeBpm = 124.0;
    double multiplier = 1.0;
    int lightsZoneW = 16;
    juce::ComponentDragger windowDragger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace pflow
