#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EditModel.h"
#include "GrooveEngine.h"
#include "UiAtoms.h"

namespace pflow {

/** Cupertino Effects inspector: foldable Timing / Dynamics / Length / Pitch & Scale. */
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
    class ParamSlider : public juce::Component
    {
    public:
        ParamSlider(const juce::String& label, int minV, int maxV, int defV, bool bipolar);
        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;
        void setValue(int v, juce::NotificationType notify = juce::sendNotification);
        int getValue() const { return value; }
        std::function<void(int)> onChange;
        juce::String valueText;
    private:
        void setFromX(float x);
        juce::String label;
        int minV, maxV, defV, value;
        bool bipolar = false;
        juce::Rectangle<float> track;
    };

    /** Discrete steps with custom labels (swing grid, tempo). */
    class DiscreteSlider : public juce::Component
    {
    public:
        DiscreteSlider(const juce::String& label, int numSteps, int defStep);
        void setLabels(const juce::StringArray& labels);
        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;
        void setStep(int s, juce::NotificationType notify = juce::sendNotification);
        int getStep() const { return step; }
        std::function<void(int)> onChange;
    private:
        void setFromX(float x);
        juce::String label;
        juce::StringArray labels;
        int numSteps, defStep, step;
        juce::Rectangle<float> track;
    };

    class InlineSettingRow : public juce::Component
    {
    public:
        InlineSettingRow(const juce::String& label, juce::Component& control, int ctrlW = 110);
        void resized() override;
        void paint(juce::Graphics&) override;
    private:
        juce::String label;
        juce::Component& control;
        int controlW;
    };

    class PitchShiftRow : public juce::Component
    {
    public:
        PitchShiftRow();
        void resized() override;
        void paint(juce::Graphics&) override;
        void setFromEdit(const ClipEdit& e, int clipRootPc);
        std::function<void(int)> onChange;
    private:
        void setValue(int v, juce::NotificationType notify = juce::sendNotification);
        void bump(int delta);
        int value = 0;
        int clipRoot = -1;
        ClipEdit editCtx;
        IconBtn btnDown { icons::minus, "Pitch down" };
        IconBtn btnUp { icons::plus, "Pitch up" };
        juce::Label valueBox;
        juce::Label notePreview;
    };

    class Section : public juce::Component
    {
    public:
        explicit Section(const juce::String& title);
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void resized() override;
        void setOpen(bool);
        bool isOpen() const { return open; }
        int idealHeight() const;
        juce::String title;
        bool open = false;
        std::vector<juce::Component*> rows;
        std::function<void()> onToggle;
    static constexpr int kHeaderH = 32;
        static constexpr int kRowH = 32;
        static constexpr int kPadH = 8;
    };

    void notifyGroove();
    void notifyEdit();
    void layoutSections();

    GrooveParams groove;
    ClipEdit edit;
    double bpmMultiplier = 1.0;
    bool effectsLocked = false;
    bool pitchLocked = false;
    bool hasClip = false;
    int clipRootPc = -1;

    IconBtn btnReset { icons::undo, "Reset effects to defaults" };
    IconBtn btnEffectsLock { icons::lockOpen, "Lock effects while browsing" };

    Section playback { "Playback" };
    Section timing { "Timing" };
    Section dynamics { "Dynamics" };
    Section lengthSec { "Length" };
    Section pitch { "Pitch & Scale" };

    ParamSlider swing { "Swing", 0, 100, 0, false };
    juce::ComboBox swingGridPicker;
    juce::ComboBox tempoPicker;
    ParamSlider pocket { "Pocket", -100, 100, 0, true };
    ParamSlider humanize { "Humanize", 0, 100, 0, false };
    ParamSlider dynamicsSl { "Dynamics", -100, 100, 0, true };
    ParamSlider intensity { "Intensity", 0, 200, 100, false };
    ParamSlider lengthSl { "Length", 25, 200, 100, false };

    juce::ComboBox octavePicker, rootPicker, modePicker;
    MiniSwitch fitSwitch { "" };
    MiniSwitch mapSwitch { "" };
    InlineSettingRow swingGridRow { "Swing speed", swingGridPicker, 110 };
    InlineSettingRow tempoRow { "Tempo", tempoPicker, 88 };
    ChipBtn btnTrim { "Trim", icons::scissors };
    InlineSettingRow trimRow { "Trim", btnTrim, 88 };
    InlineSettingRow octaveRow { "Octave", octavePicker, 72 };
    PitchShiftRow pitchShiftRow;
    InlineSettingRow keyRow { "Key", rootPicker, 96 };
    InlineSettingRow modeRow { "Mode", modePicker, 118 };
    InlineSettingRow fitRow { "Fit to scale", fitSwitch, 52 };
    InlineSettingRow mapRow { "Map to root", mapSwitch, 52 };

    juce::Viewport viewport;
    juce::Component body;

    static constexpr int headerH = 36;
    static constexpr int kBodyPadH = 8;
    static constexpr int kSliderMaxW = 196;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsInspector)
};

} // namespace pflow
