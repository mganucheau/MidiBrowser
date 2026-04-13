#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <memory>
#include "PluginProcessor.h"
#include "FileBrowserPanel.h"
#include "ArrangementView.h"
#include "PianoRollEditor.h"
#include "ControlPanel.h"
#include "Theme.h"

namespace pflow {

class PatternFlowEditor : public juce::AudioProcessorEditor,
                          public juce::DragAndDropContainer,
                          public juce::Timer,
                          public juce::KeyListener,
                          public juce::ComboBox::Listener
{
public:
    explicit PatternFlowEditor(PatternFlowProcessor&);
    ~PatternFlowEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void focusGained(juce::Component::FocusChangeType) override;
    void timerCallback() override;
    using juce::Component::keyPressed;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void comboBoxChanged(juce::ComboBox*) override;

private:
    PatternFlowProcessor& processorRef;

    PatternFlowLookAndFeel lnf;

    // Theme transition (Material motion-inspired crossfade)
    juce::Image themeTransitionSnapshot_;
    float themeTransitionAlpha_ = 0.0f;
    juce::uint32 themeTransitionStartMs_ = 0;

    // Panels
    ControlPanel      controlPanel;
    FileBrowserPanel  fileBrowser;
    ArrangementView   arrangementView;
    PianoRollEditor   pianoRoll;

    // Top row: logo | transport (rec, grid, loop, bars, step, extend, trim) | settings
    juce::Label lblTitle;
    juce::ShapeButton btnRecord { "Rec", colours::bgLighter(), colours::bgLighter(), colours::bgLighter() };
    juce::ComboBox cmbGridSnap;
    juce::Label lblSessionBars { {}, "Bars" };
    juce::ComboBox cmbSessionBars;
    juce::TextButton btnStep { "Step" };
    juce::TextButton btnExtend { "Extend" };
    juce::TextButton btnTrim { "Trim" };
    juce::TextButton btnSettings { "" };

    void updateRecordButton();
    void refreshTransportColours();

    // Resizable panels (VS Code-style): stored sizes, min/max
    int browserWidth_    = metrics::browserWidth;
    int pianoRollHeight_ = metrics::pianoRollH;
    bool fileBrowserVisible_ = true;
    static constexpr int resizerStripSize = 5;

    class ResizerStrip : public juce::Component
    {
    public:
        enum class Direction { Vertical, Horizontal };
        ResizerStrip(Direction d, int& valueRef, int minVal, int maxVal,
                     std::function<int()> getTotalSize, std::function<void()> onResize);
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
    private:
        Direction direction_;
        int* valueRef_;
        int minVal_, maxVal_;
        std::function<int()> getTotalSize_;
        std::function<void()> onResize_;
        int dragStartValue_ = 0;
        int dragStartPos_   = 0;
    };
    std::unique_ptr<ResizerStrip> leftResizer_;
    std::unique_ptr<ResizerStrip> bottomResizer_;

    void showSettingsDialog();
    void exportMidi();
    void zoomToFit();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternFlowEditor)
};

} // namespace pflow
