#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "shared/BreathEq.h"

class QQDeBreathBreathEqComponent final : public juce::Component
{
public:
    QQDeBreathBreathEqComponent();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    void setState(const QQDeBreathEqState& newState, bool notify = false);
    QQDeBreathEqState getState() const { return state; }
    void setTheme(const juce::String& title, juce::Colour primary, juce::Colour secondary);
    void setSpectrum(const std::vector<float>& magnitudes);
    void setSpectra(const std::vector<float>& preMagnitudes, const std::vector<float>& postMagnitudes);

    std::function<void(const QQDeBreathEqState&)> onStateChanged;

private:
    enum class DragTarget
    {
        none,
        highPass,
        lowPass,
        band
    };

    juce::Rectangle<float> graphArea() const;
    double xToFrequency(float x) const;
    float frequencyToX(double frequencyHz) const;
    double yToGain(float y) const;
    float gainToY(double gainDb) const;
    int hitTestBand(juce::Point<float> position) const;
    bool hitTestHighPass(juce::Point<float> position) const;
    bool hitTestLowPass(juce::Point<float> position) const;
    bool isSelectedHighPass() const;
    bool isSelectedLowPass() const;
    int firstFreeBand() const;
    void notifyStateChanged();
    void updateCursor(juce::Point<float> position);
    void paintGrid(juce::Graphics& g, juce::Rectangle<float> area);
    void paintSpectrum(juce::Graphics& g, juce::Rectangle<float> area);
    void paintResponse(juce::Graphics& g, juce::Rectangle<float> area);
    void paintNodes(juce::Graphics& g, juce::Rectangle<float> area);

    QQDeBreathEqState state;
    juce::String themeTitle = "Breath EQ";
    juce::Colour primaryColour { 0xff38bdf8 };
    juce::Colour secondaryColour { 0xffffd166 };
    std::vector<float> spectrum;
    std::vector<float> postSpectrum;
    DragTarget dragTarget = DragTarget::none;
    DragTarget selectedFilter = DragTarget::none;
    int dragBand = -1;
    int selectedBand = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QQDeBreathBreathEqComponent)
};
