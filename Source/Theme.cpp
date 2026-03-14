#include "Theme.h"

namespace pflow {

PatternFlowLookAndFeel::PatternFlowLookAndFeel()
{
    refreshColours();
}

void PatternFlowLookAndFeel::refreshColours()
{
    setColour(juce::ResizableWindow::backgroundColourId,  colours::bg());
    setColour(juce::TextEditor::backgroundColourId,       colours::bgLight());
    setColour(juce::TextEditor::textColourId,             colours::text());
    setColour(juce::TextEditor::outlineColourId,          colours::panelBorder());
    setColour(juce::ComboBox::backgroundColourId,         colours::bgLight());
    setColour(juce::ComboBox::textColourId,               colours::text());
    setColour(juce::ComboBox::outlineColourId,            colours::panelBorder());
    setColour(juce::ComboBox::arrowColourId,              colours::text());
    setColour(juce::PopupMenu::backgroundColourId,        colours::bgLight());
    setColour(juce::PopupMenu::textColourId,              colours::text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent());
    setColour(juce::PopupMenu::highlightedTextColourId,   juce::Colours::white);
    setColour(juce::ScrollBar::thumbColourId,             colours::accentDim());
    setColour(juce::ListBox::backgroundColourId,          colours::panel());
    setColour(juce::ListBox::textColourId,                colours::text());
    setColour(juce::Label::textColourId,                  colours::text());
    setColour(juce::ToggleButton::textColourId,           colours::text());
    setColour(juce::ToggleButton::tickColourId,           colours::accent());
    setColour(juce::ToggleButton::tickDisabledColourId,   colours::textDim());
    setColour(juce::TextButton::buttonColourId,           colours::bgLighter());
    setColour(juce::TextButton::textColourOffId,          colours::text());
    setColour(juce::TextButton::textColourOnId,           colours::textBright());

    // File tree / TreeView colours
    setColour(juce::TreeView::backgroundColourId,         colours::panel());
    setColour(juce::TreeView::linesColourId,              colours::panelBorder());
    setColour(juce::TreeView::selectedItemBackgroundColourId, colours::accent().withAlpha(0.2f));
    setColour(juce::TreeView::dragAndDropIndicatorColourId, colours::accent());

    // DirectoryContentsDisplayComponent (file browser text)
    setColour(juce::DirectoryContentsDisplayComponent::textColourId,    colours::text());
    setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::accent());
    setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);

    // Tooltip
    setColour(juce::TooltipWindow::backgroundColourId,    colours::bgLight());
    setColour(juce::TooltipWindow::textColourId,          colours::text());
    setColour(juce::TooltipWindow::outlineColourId,       colours::panelBorder());
}

void PatternFlowLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                               int w, int h, float sliderPos,
                                               float startAngle, float endAngle,
                                               juce::Slider&)
{
    auto radius  = (float)juce::jmin(w, h) * 0.38f;
    auto centreX = (float)x + (float)w * 0.5f;
    auto centreY = (float)y + (float)h * 0.5f;
    auto angle   = startAngle + sliderPos * (endAngle - startAngle);

    // Knob body (filled circle with subtle gradient feel)
    auto bodyRadius = radius - 2.0f;
    g.setColour(colours::bgLighter());
    g.fillEllipse(centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour(colours::panelBorder());
    g.drawEllipse(centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.2f);

    // Track arc (background)
    juce::Path track;
    track.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(colours::knobTrack());
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    if (sliderPos > 0.0f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                               startAngle, angle, true);
        g.setColour(colours::accent());
        g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer indicator (dot on knob edge instead of line from center)
    float dotDist = bodyRadius * 0.65f;
    float px = centreX + dotDist * std::cos(angle - juce::MathConstants<float>::halfPi);
    float py = centreY + dotDist * std::sin(angle - juce::MathConstants<float>::halfPi);
    g.setColour(colours::textBright());
    g.fillEllipse(px - 3.0f, py - 3.0f, 6.0f, 6.0f);
}

void PatternFlowLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                    juce::Button& btn,
                                                    const juce::Colour&,
                                                    bool highlighted, bool down)
{
    auto bounds = btn.getLocalBounds().toFloat().reduced(1.0f);
    auto baseColour = down ? colours::accent()
                           : (highlighted ? colours::bgLighter().brighter(0.08f) : colours::bgLighter());
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, metrics::cornerRadius);
    if (!down)
    {
        g.setColour(colours::panelBorder().withAlpha(0.6f));
        g.drawRoundedRectangle(bounds, metrics::cornerRadius, 1.0f);
    }
}

void PatternFlowLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(getLabelFont(label));
    auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
    g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                     juce::jmax(1, (int)((float)textArea.getHeight() / g.getCurrentFont().getHeight())),
                     label.getMinimumHorizontalScale());
}

juce::Font PatternFlowLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(14.0f);
}

} // namespace pflow
