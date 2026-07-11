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

    std::function<void(const GrooveParams&)> onGrooveChanged;
    std::function<void(const ClipEdit&)> onEditChanged;
    std::function<void(double)> onBpmMultiplierChanged;
    std::function<void(bool)> onEffectsLockToggled;
    std::function<void(bool)> onPitchLockToggled;
    std::function<void()> onResetGroove;

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

    class LabeledRow : public juce::Component
    {
    public:
        LabeledRow(const juce::String& caption, juce::Component& control);
        void resized() override;
        void paint(juce::Graphics&) override;
    private:
        juce::String caption;
        juce::Component& control;
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
        static constexpr int kRowH = 44;
        static constexpr int kPadH = 12;
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

    IconBtn btnLock { icons::lockOpen, "Lock effects while browsing" };
    IconBtn btnReset { icons::undo, "Reset effects to defaults" };
    IconBtn btnPitchLock { icons::lockOpen, "Lock pitch edits while browsing" };

    Section timing { "Timing" };
    Section dynamics { "Dynamics" };
    Section lengthSec { "Length" };
    Section pitch { "Pitch & Scale" };

    ParamSlider swing { "Swing", 0, 100, 0, false };
    DiscreteSlider swingGrid { "Swing speed", 6, 1 };
    DiscreteSlider tempo { "Tempo", 3, 1 };
    ParamSlider pocket { "Pocket", -100, 100, 0, true };
    ParamSlider humanize { "Humanize", 0, 100, 0, false };
    ParamSlider dynamicsSl { "Dynamics", -100, 100, 0, true };
    ParamSlider intensity { "Intensity", 0, 200, 100, false };
    ParamSlider lengthSl { "Length", 25, 200, 100, false };

    juce::ComboBox octavePicker, rootPicker, modePicker;
    MiniSwitch fitSwitch { "Fit to scale" };
    MiniSwitch mapSwitch { "Map to root" };
    LabeledRow octaveRow { "Octave", octavePicker };
    LabeledRow keyRow { "Key", rootPicker };
    LabeledRow modeRow { "Mode", modePicker };
    LabeledRow fitRow { "Fit to scale", fitSwitch };
    LabeledRow mapRow { "Map to root", mapSwitch };

    juce::Viewport viewport;
    juce::Component body;

    static constexpr int headerH = 36;
    static constexpr int kBodyPadH = 12;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsInspector)
};

} // namespace pflow
