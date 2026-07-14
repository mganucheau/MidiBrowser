#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EditModel.h"
#include "GrooveEngine.h"
#include "UiAtoms.h"
#include "EffectsInspectorWidgets.h"

namespace pflow {

/** Flat macOS-style effects inspector (Playback / Timing / Performance / Pitch & Scale / Effects). */
class EffectsInspector : public juce::Component
{
public:
    EffectsInspector();

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

    void setGroove(const GrooveParams&, juce::NotificationType notify = juce::dontSendNotification);
    GrooveParams getGroove() const { return groove; }

    void setEdit(const ClipEdit&, juce::NotificationType notify = juce::dontSendNotification);
    ClipEdit getEdit() const { return edit; }

    void setBpmMultiplier(double mult, juce::NotificationType notify = juce::dontSendNotification);
    double getBpmMultiplier() const { return bpmMultiplier; }

    void setSectionLocks(uint32_t locks);
    uint32_t getSectionLocks() const { return sectionLocks; }
    void setHasClip(bool has);
    void setClipRoot(int rootPc);
    /** Clip baseline complexity (1..100) — drives the Complexity slider position. */
    void setClipComplexity(int complexity);
    void setTrimState(bool active, bool enabled, const juce::String& label = {});

    std::function<void(const GrooveParams&)> onGrooveChanged;
    std::function<void(const ClipEdit&)> onEditChanged;
    std::function<void(double)> onBpmMultiplierChanged;
    /** Fired when header or section lock bits change. */
    std::function<void(uint32_t)> onSectionLocksChanged;
    /** Reset unlocked sections only (header refresh). */
    std::function<void()> onResetUnlocked;
    std::function<void()> onTrimClicked;
    std::function<void()> onActivated;

private:
    void notifyGroove();
    void notifyEdit();
    void layoutSections();
    void refreshDirtySections();
    void refreshPitchAnnotation();
    void refreshComplexitySlider();
    void syncArticulationKnobsToUi();
    int resolvedPitchMidi() const;
    void resetPitchEditFields();
    void resetPlaybackSection();
    void resetTimingSection();
    void resetPerformanceSection();
    void resetEffectsSection();
    void toggleSectionLock(uint32_t bit);
    void setAllSectionsLocked(bool locked);
    void refreshHeaderLockButton();
    void resetUnlockedSections();

    GrooveParams groove;
    ClipEdit edit;
    double bpmMultiplier = 1.0;
    uint32_t sectionLocks = 0;
    bool hasClip = false;
    int clipRootPc = -1;
    int clipComplexity = 50;

    IconBtn btnReset { icons::undo, "Reset effects to defaults" };
    IconBtn btnEffectsLock { icons::lockOpen, "Lock effects while browsing" };

    fx::Section playback { "Playback" };
    fx::Section timing { "Timing" };
    fx::Section performance { "Performance" };
    fx::Section pitchSec { "Pitch & Scale" };
    fx::Section effectsSec { "Effects" };

    fx::TempoToggle tempoToggle;
    fx::InlineRow tempoRow;
    fx::FlatSwitch trimSwitch;
    fx::FlatPopup extendPopup;
    fx::InlineRow extendRow;

    fx::FlatPopup swingTimePopup;
    fx::InlineRow swingTimeRow;
    fx::FlatPopup quantizeTimePopup;
    fx::InlineRow quantizeTimeRow;
    fx::FlatSliderRow quantizeStrengthSl;
    fx::FlatSliderRow swing;
    fx::FlatSliderRow pocket;
    fx::FlatSliderRow humanize;
    fx::FlatSliderRow lengthSl;

    fx::FlatSliderRow dynamicsSl;
    fx::FlatSliderRow intensitySl;
    fx::FlatPopup articulationPopup;
    fx::InlineRow articulationRow;
    fx::FlatSliderRow articulationStrengthSl;
    fx::FlatRangeSliderRow velocityRangeSl;
    fx::FlatPopup sustainPopup;
    fx::InlineRow sustainRow;

    fx::FlatSliderRow complexitySl;
    fx::FlatSliderRow variationsSl;
    fx::FlatPopup delayTimePopup;
    fx::InlineRow delayTimeRow;
    fx::FlatSliderRow delayAmountSl;
    fx::FlatSliderRow delayFeedbackSl;

    fx::FlatStepper octaveStepper;
    fx::InlineRow octaveRow;
    fx::FlatPopup octaveRangePopup;
    fx::InlineRow octaveRangeRow;
    fx::PitchRow pitchRow;
    fx::FlatPopup pitchMinPopup;
    fx::FlatPopup pitchMaxPopup;
    fx::RangeRow pitchRangeRow;
    fx::FlatPopup keyPopup;
    fx::FlatPopup modePopup;
    fx::InlineRow keyRow;
    fx::InlineRow modeRow;
    fx::FlatSwitch fitSwitch;
    fx::FlatSwitch mapSwitch;

    fx::InlineRow trimRow;
    fx::InlineRow fitRow;
    fx::InlineRow mapRow;

    juce::Viewport viewport;
    juce::Component body;

    static constexpr int kPanelW = 248;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsInspector)
};

} // namespace pflow
