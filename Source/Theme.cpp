#include "Theme.h"

namespace pflow {

PatternFlowLookAndFeel::PatternFlowLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId,  colours::bg);
    setColour(juce::TextEditor::backgroundColourId,       colours::bgLight);
    setColour(juce::TextEditor::textColourId,             colours::text);
    setColour(juce::TextEditor::outlineColourId,          colours::panelBorder);
    setColour(juce::ComboBox::backgroundColourId,         colours::bgLight);
    setColour(juce::ComboBox::textColourId,               colours::text);
    setColour(juce::ComboBox::outlineColourId,            colours::panelBorder);
    setColour(juce::PopupMenu::backgroundColourId,        colours::bgLight);
    setColour(juce::PopupMenu::textColourId,              colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour(juce::ScrollBar::thumbColourId,             colours::accentDim);
    setColour(juce::ListBox::backgroundColourId,          colours::panel);
    setColour(juce::ListBox::textColourId,                colours::text);
    setColour(juce::Label::textColourId,                  colours::text);
}

void PatternFlowLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                               int w, int h, float sliderPos,
                                               float startAngle, float endAngle,
                                               juce::Slider&)
{
    auto radius  = (float)juce::jmin(w, h) * 0.4f;
    auto centreX = (float)x + (float)w * 0.5f;
    auto centreY = (float)y + (float)h * 0.5f;
    auto angle   = startAngle + sliderPos * (endAngle - startAngle);

    // Track
    juce::Path track;
    track.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(colours::knobTrack);
    g.strokePath(track, juce::PathStrokeType(3.0f));

    // Value arc
    juce::Path valueArc;
    valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                           startAngle, angle, true);
    g.setColour(colours::accent);
    g.strokePath(valueArc, juce::PathStrokeType(3.0f));

    // Thumb dot
    juce::Point<float> thumbPos(centreX + radius * std::cos(angle - juce::MathConstants<float>::halfPi),
                                centreY + radius * std::sin(angle - juce::MathConstants<float>::halfPi));
    g.setColour(colours::accentBright);
    g.fillEllipse(thumbPos.x - 4.0f, thumbPos.y - 4.0f, 8.0f, 8.0f);
}

void PatternFlowLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                    juce::Button& btn,
                                                    const juce::Colour&,
                                                    bool highlighted, bool down)
{
    auto bounds = btn.getLocalBounds().toFloat().reduced(1.0f);
    auto baseColour = down ? colours::accent : (highlighted ? colours::bgLighter : colours::bgLight);
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, metrics::cornerRadius);
    g.setColour(colours::panelBorder);
    g.drawRoundedRectangle(bounds, metrics::cornerRadius, 1.0f);
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
    return juce::Font(12.0f);
}

} // namespace pflow
