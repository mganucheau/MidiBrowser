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
    void setClipBpm(double);
    void setFreeBpm(double);
    void setFolderPath(const juce::String&);
    void setEditorOpen(bool);

    std::function<void()> onPlayPause;
    std::function<void()> onStop;
    std::function<void()> onToggleEditor;
    std::function<void(bool)> onSyncChanged;
    std::function<void(double)> onFreeBpmChanged;

private:
    void refreshBpm();

    IconBtn btnPlay { icons::play, "Play / pause preview" };
    IconBtn btnStop { icons::stop, "Stop" };
    PillToggle syncToggle { "Synced to DAW", "Free-run" };
    juce::Label bpmLabel;
    juce::Label pathLabel;
    ChipBtn btnEditor { "Editor", icons::arrowsIn };

    bool playing = false;
    bool synced = true;
    bool editorOpen = true;
    double clipBpm = 124.0;
    double freeBpm = 124.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace pflow
