#include "ControlPanel.h"
#include "PluginProcessor.h"
#include "UndoActions.h"
#include <cmath>

namespace pflow {

namespace {

constexpr float kToggleFontH  = 13.0f;
constexpr float kTextBtnFontH = 12.0f;

int textWidthPx(float fontHeight, const juce::String& t)
{
    return juce::roundToInt(std::ceil(
        juce::Font(juce::FontOptions(fontHeight)).getStringWidthFloat(t)));
}

int minToggleWidth(int comboPad, const juce::String& label)
{
    const int tick = juce::roundToInt(std::ceil(kToggleFontH * 1.1f));
    const int textLeft = 4 + tick + 10;
    return textLeft + textWidthPx(kToggleFontH, label) + comboPad * 2 + 10;
}

int minTextButtonWidth(int comboPad, const juce::String& label)
{
    return textWidthPx(kTextBtnFontH, label) + comboPad * 2 + 18;
}

void commitRandomizeTakeCompWithUndo(PatternFlowProcessor& proc)
{
    auto before = proc.getActiveTakeCompSegmentsSnapshot();
    proc.randomizeActiveComp();
    auto after = proc.getActiveTakeCompSegmentsSnapshot();
    if (takeCompSegmentsEquivalent(before, after)) return;
    {
        juce::ScopedLock sl(proc.laneLock);
        proc.ensureDefaultTakeComp();
        proc.takeComps[0].segments = before;
    }
    proc.rebuildMasterClip();
    proc.undoManager.beginNewTransaction();
    proc.undoManager.perform(new SetTakeCompSegmentsAction(proc, before, after));
}

void commitCycleCompLanesWithUndo(PatternFlowProcessor& proc)
{
    auto before = proc.getActiveTakeCompSegmentsSnapshot();
    proc.cycleCompSegmentsToNextLane();
    auto after = proc.getActiveTakeCompSegmentsSnapshot();
    if (takeCompSegmentsEquivalent(before, after)) return;
    {
        juce::ScopedLock sl(proc.laneLock);
        proc.ensureDefaultTakeComp();
        proc.takeComps[0].segments = before;
    }
    proc.rebuildMasterClip();
    proc.undoManager.beginNewTransaction();
    proc.undoManager.perform(new SetTakeCompSegmentsAction(proc, before, after));
}

int maxScaleTypeComboInnerWidth(float fontHeight)
{
    int m = 0;
    for (int i = 0; i < (int)ScaleType::Count; ++i)
        m = juce::jmax(m, textWidthPx(fontHeight, scaleTypeName((ScaleType)i)));
    return m;
}

} // namespace

ControlPanel::ControlPanel(PatternFlowProcessor& proc) : processor(proc)
{
    btnScaleToggle.setColour(juce::ToggleButton::textColourId, colours::text());
    btnScaleToggle.setColour(juce::ToggleButton::tickColourId, colours::accent());
    btnScaleToggle.setTooltip("Enable or disable scale quantisation (choose root and type anytime)");
    btnScaleToggle.setToggleState(processor.scaleEnabled.load(), juce::dontSendNotification);
    btnScaleToggle.onClick = [this]
    {
        processor.scaleEnabled.store(btnScaleToggle.getToggleState());
    };
    addAndMakeVisible(btnScaleToggle);

    const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    for (int i = 0; i < 12; ++i)
        cmbScaleRoot.addItem(noteNames[i], i + 1);
    cmbScaleRoot.setSelectedId(processor.scaleRoot.load() + 1);
    cmbScaleRoot.setTooltip("Scale root");
    cmbScaleRoot.addListener(this);
    addAndMakeVisible(cmbScaleRoot);

    for (int i = 0; i < (int)ScaleType::Count; ++i)
        cmbScaleType.addItem(scaleTypeName((ScaleType)i), i + 1);
    cmbScaleType.setSelectedId(processor.scaleType.load() + 1);
    cmbScaleType.setTooltip("Scale type");
    cmbScaleType.addListener(this);
    addAndMakeVisible(cmbScaleType);

    btnC0.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnC0.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnC0.setComponentID("ActionButton");
    btnC0.setTooltip("Transpose all MIDI to C0 octave (0-11)");
    btnC0.onClick = [this] { if (onC0Clicked) onC0Clicked(); };
    addAndMakeVisible(btnC0);

    btnTranspose.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnTranspose.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnTranspose.setComponentID("ActionButton");
    btnTranspose.setTooltip("Transpose all MIDI to the selected scale root and scale type");
    btnTranspose.onClick = [this]
    {
        if (onTransposeClicked) onTransposeClicked();
    };
    addAndMakeVisible(btnTranspose);

    btnComp.setColour(juce::ToggleButton::textColourId, colours::text());
    btnComp.setColour(juce::ToggleButton::tickColourId, colours::accent());
    btnComp.setTooltip("Comp mode: paint and edit comp regions. Turn off to move clips; existing comps stay for playback.");
    btnComp.onClick = [this]
    {
        bool on = btnComp.getToggleState();
        processor.compsEnabled.store(on);
        if (on)
            processor.ensureDefaultTakeComp();
        refreshTakeCompUI();
        if (onTakeCompSwitched) onTakeCompSwitched();
    };
    addAndMakeVisible(btnComp);

    sldRandomRegions.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    sldRandomRegions.setTextBoxStyle(juce::Slider::TextBoxRight, false, 26, 22);
    sldRandomRegions.setRange(2.0, 16.0, 1.0);
    sldRandomRegions.setValue((double)processor.compRandomRegionCount.load(), juce::dontSendNotification);
    lastRandomKnobInt = juce::jlimit(2, 16, juce::roundToInt((float)sldRandomRegions.getValue()));
    sldRandomRegions.setTooltip("How many regions Random creates (2–16). Dragging runs Random at each step.");
    sldRandomRegions.onValueChange = [this]
    {
        int newN = juce::jlimit(2, 16, juce::roundToInt((float)sldRandomRegions.getValue()));
        processor.compRandomRegionCount.store(newN);
        if (!processor.compsEnabled.load())
        {
            lastRandomKnobInt = newN;
            return;
        }
        if (newN == lastRandomKnobInt)
            return;
        const int from = lastRandomKnobInt;
        const int to = newN;
        const int step = (to > from) ? 1 : -1;
        for (int n = from + step;; n += step)
        {
            processor.compRandomRegionCount.store(n);
            commitRandomizeTakeCompWithUndo(processor);
            if (onTakeCompSwitched) onTakeCompSwitched();
            if (n == to) break;
        }
        lastRandomKnobInt = to;
    };
    addAndMakeVisible(sldRandomRegions);

    btnRandomComp.setComponentID("ActionButton");
    btnRandomComp.setTooltip("Fill the comp with random regions across the session");
    btnRandomComp.onClick = [this]
    {
        if (!processor.compsEnabled.load()) return;
        commitRandomizeTakeCompWithUndo(processor);
        if (onTakeCompSwitched) onTakeCompSwitched();
    };
    addAndMakeVisible(btnRandomComp);

    btnSwapComp.setComponentID("ActionButton");
    btnSwapComp.setTooltip("Move all comp regions to the next track down; last track wraps to the first");
    btnSwapComp.onClick = [this]
    {
        if (!processor.compsEnabled.load()) return;
        commitCycleCompLanesWithUndo(processor);
        if (onTakeCompSwitched) onTakeCompSwitched();
    };
    addAndMakeVisible(btnSwapComp);

    refreshTakeCompUI();
}

void ControlPanel::resized()
{
    auto row1 = getLocalBounds();
    const int comboPad = metrics::comboTextPadding;
    const int itemGap = 6;
    const int btnH1 = juce::jlimit(28, 34, row1.getHeight() - 8);
    const int rowY1 = row1.getY() + (row1.getHeight() - btnH1) / 2;
    const int cx = row1.getCentreX();
    const int halfDiv = 1;
    juce::Rectangle<int> leftArea(row1.getX(), row1.getY(), cx - row1.getX() - halfDiv, row1.getHeight());
    juce::Rectangle<int> rightArea(cx + halfDiv, row1.getY(), row1.getRight() - (cx + halfDiv), row1.getHeight());

    const int compToggleW = minToggleWidth(comboPad, "Comp");
    const int knobSide = juce::jmin(40, btnH1 + 8);
    const int knobW = knobSide + 30;
    const int randW = minTextButtonWidth(comboPad, "Random");
    const int swapW = minTextButtonWidth(comboPad, "Swap");
    const int compClusterW = compToggleW + itemGap + knobW + itemGap + randW + itemGap + swapW;
    int x = leftArea.getX() + (leftArea.getWidth() - compClusterW) / 2;
    btnComp.setBounds(x, rowY1, compToggleW, btnH1);
    x += compToggleW + itemGap;
    sldRandomRegions.setBounds(x, rowY1 + (btnH1 - knobSide) / 2, knobW, knobSide);
    x += knobW + itemGap;
    btnRandomComp.setBounds(x, rowY1, randW, btnH1);
    x += randW + itemGap;
    btnSwapComp.setBounds(x, rowY1, swapW, btnH1);

    const int scaleToggleW = minToggleWidth(comboPad, "Scale");
    const int rootW = textWidthPx(kTextBtnFontH, "C#") + comboPad * 2 + 28;
    const int typeW = maxScaleTypeComboInnerWidth(kTextBtnFontH) + comboPad * 2 + 30;
    const int transW = minTextButtonWidth(comboPad, "Transpose");
    const int c0W = minTextButtonWidth(comboPad, "C0");
    const int scaleClusterW = scaleToggleW + itemGap + rootW + itemGap + typeW + itemGap + transW + itemGap + c0W;
    x = rightArea.getX() + (rightArea.getWidth() - scaleClusterW) / 2;
    btnScaleToggle.setBounds(x, rowY1, scaleToggleW, btnH1);
    x += scaleToggleW + itemGap;
    cmbScaleRoot.setBounds(x, rowY1, rootW, btnH1);
    x += rootW + itemGap;
    cmbScaleType.setBounds(x, rowY1, typeW, btnH1);
    x += typeW + itemGap;
    btnTranspose.setBounds(x, rowY1, transW, btnH1);
    x += transW + itemGap;
    btnC0.setBounds(x, rowY1, c0W, btnH1);
}

void ControlPanel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds();
    if (r.getHeight() < 8) return;
    g.setColour(colours::panelBorder().withAlpha(0.5f));
    const int cx = r.getCentreX();
    g.drawVerticalLine(cx, (float)r.getY() + 3.0f, (float)r.getBottom() - 3.0f);
}

void ControlPanel::refreshTakeCompUI()
{
    btnComp.setToggleState(processor.compsEnabled.load(), juce::dontSendNotification);
    sldRandomRegions.setValue((double)juce::jlimit(2, 16, processor.compRandomRegionCount.load()),
                              juce::dontSendNotification);
    lastRandomKnobInt = juce::jlimit(2, 16, processor.compRandomRegionCount.load());

    const bool en = processor.compsEnabled.load();
    sldRandomRegions.setEnabled(en);
    btnRandomComp.setEnabled(en);
    btnSwapComp.setEnabled(en);
}

void ControlPanel::refreshComponentColours()
{
    btnScaleToggle.setColour(juce::ToggleButton::textColourId, colours::text());
    btnScaleToggle.setColour(juce::ToggleButton::tickColourId, colours::accent());
    btnC0.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnC0.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnTranspose.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnTranspose.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnComp.setColour(juce::ToggleButton::textColourId, colours::text());
    btnComp.setColour(juce::ToggleButton::tickColourId, colours::accent());
    sldRandomRegions.setColour(juce::Slider::rotarySliderFillColourId, colours::accent());
    sldRandomRegions.setColour(juce::Slider::rotarySliderOutlineColourId, colours::textDim());
    sldRandomRegions.setColour(juce::Slider::thumbColourId, colours::textBright());
    sldRandomRegions.setColour(juce::Slider::textBoxTextColourId, colours::text());
    sldRandomRegions.setColour(juce::Slider::textBoxBackgroundColourId, colours::bgLighter());
    sldRandomRegions.setColour(juce::Slider::textBoxOutlineColourId, colours::panelBorder());
    btnRandomComp.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnRandomComp.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnSwapComp.setColour(juce::TextButton::buttonColourId, colours::bgLighter());
    btnSwapComp.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

void ControlPanel::comboBoxChanged(juce::ComboBox* combo)
{
    if (combo == &cmbScaleRoot)
        processor.scaleRoot.store(combo->getSelectedId() - 1);
    else if (combo == &cmbScaleType)
        processor.scaleType.store(juce::jlimit(0, (int)ScaleType::Count - 1, combo->getSelectedId() - 1));
}

} // namespace pflow
