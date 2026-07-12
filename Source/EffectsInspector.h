#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EditModel.h"
#include "GrooveEngine.h"
#include "UiAtoms.h"
#include "EffectsInspectorWidgets.h"

namespace pflow {

/** Flat macOS-style effects inspector (Playback / Timing / Performance / Pitch & Scale). */
class EffectsInspector : public juce::Component
{
public:
    EffectsInspector();

    void resized() override;
    void paint(juce::Graphics&) override;

    void setGroove(const GrooveParams&, juce::NotificationType notify = juce::dontSendNotification);
    GrooveParams getGroove() const { return groove; }

    void setEdit(const ClipEdit&, juce::NotificationType notify = juce::dontSendNotification);
    ClipEdit getEdit() const { return edit; }

    void setBpmMultiplier(double mult, juce::NotificationType notify = juce::dontSendNotification);
    double getBpmMultiplier() const { return bpmMultiplier; }

    void setEffectsLocked(bool locked);
    void setPitchLocked(bool locked);
    void setHasClip(bool has);
    void setClipRoot(int rootPc);
    void setTrimState(bool active, bool enabled, const juce::String& label);

    std::function<void(const GrooveParams&)> onGrooveChanged;
    std::function<void(const ClipEdit&)> onEditChanged;
    std::function<void(double)> onBpmMultiplierChanged;
    std::function<void(bool)> onEffectsLockToggled;
    std::function<void(bool)> onPitchLockToggled;
    std::function<void()> onResetGroove;
    std::function<void()> onTrimClicked;

private:
    void notifyGroove();
    void notifyEdit();
    void layoutSections();
    void refreshPitchAnnotation();
    int resolvedPitchMidi() const;

    GrooveParams groove;
    ClipEdit edit;
    double bpmMultiplier = 1.0;
    bool effectsLocked = false;
    bool pitchLocked = false;
    bool hasClip = false;
    int clipRootPc = -1;

    IconBtn btnReset { icons::undo, "Reset effects to defaults" };
    IconBtn btnEffectsLock { icons::lockOpen, "Lock effects while browsing" };

    fx::Section playback { "Playback" };
    fx::Section timing { "Timing" };
    fx::Section performance { "Performance" };
    fx::Section pitchSec { "Pitch & Scale" };

    fx::TempoToggle tempoToggle;
    fx::InlineRow tempoRow;
    fx::FlatTextButton btnTrim { "Trim" };

    fx::FlatSliderRow swing;
    fx::FlatPopup swingStylePopup;
    fx::InlineRow swingStyleRow;
    fx::FlatSliderRow pocket;
    fx::FlatSliderRow humanize;

    fx::FlatSliderRow dynamicsSl;
    fx::FlatSliderRow intensitySl;
    fx::FlatSliderRow lengthSl;

    fx::FlatStepper octaveStepper;
    fx::PitchRow pitchRow;
    fx::FlatPopup keyPopup;
    fx::FlatPopup modePopup;
    fx::KeyModeRow keyModeRow;
    fx::FlatSwitch fitSwitch;
    fx::FlatSwitch mapSwitch;

    fx::InlineRow trimRow;
    fx::InlineRow fitRow;
    fx::InlineRow mapRow;

    juce::Viewport viewport;
    juce::Component body;

    static constexpr int kHeaderBarH = 32;
    static constexpr int kPanelW = 244;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsInspector)
};

} // namespace pflow
