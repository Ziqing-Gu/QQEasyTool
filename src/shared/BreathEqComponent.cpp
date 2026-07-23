#include "BreathEqComponent.h"

#include <cmath>

namespace
{
constexpr auto minFrequency = 20.0;
constexpr auto maxFrequency = 20000.0;
constexpr auto minGainDb = -24.0;
constexpr auto maxGainDb = 24.0;
constexpr auto nodeRadius = 7.0f;

float normalizedLogFrequency(double frequencyHz)
{
    const auto f = juce::jlimit(minFrequency, maxFrequency, frequencyHz);
    return static_cast<float>((std::log(f) - std::log(minFrequency)) / (std::log(maxFrequency) - std::log(minFrequency)));
}

double frequencyFromNormalized(float normalized)
{
    const auto clamped = juce::jlimit(0.0f, 1.0f, normalized);
    return std::exp(std::log(minFrequency) + static_cast<double>(clamped) * (std::log(maxFrequency) - std::log(minFrequency)));
}

juce::String frequencyLabel(double frequencyHz)
{
    if (frequencyHz >= 1000.0)
        return juce::String(frequencyHz / 1000.0, frequencyHz >= 10000.0 ? 0 : 1) + "k";

    return juce::String(static_cast<int>(std::round(frequencyHz)));
}

juce::String slopeLabel(int slopeDbPerOct)
{
    return juce::String(slopeDbPerOct) + " dB/oct";
}

int nextSlope(int slopeDbPerOct, int direction)
{
    const int slopes[] = { 12, 24, 36, 48 };
    auto index = 0;
    for (auto i = 0; i < 4; ++i)
        if (slopeDbPerOct >= slopes[i])
            index = i;

    index = juce::jlimit(0, 3, index + (direction >= 0 ? 1 : -1));
    return slopes[index];
}
} // namespace

QQDeBreathBreathEqComponent::QQDeBreathBreathEqComponent()
{
    setWantsKeyboardFocus(true);
}

void QQDeBreathBreathEqComponent::setState(const QQDeBreathEqState& newState, bool notify)
{
    state = sanitizeBreathEqState(newState);
    if (selectedBand >= 0 && ! state.bands[static_cast<size_t>(selectedBand)].enabled)
        selectedBand = -1;

    if (notify)
        notifyStateChanged();

    repaint();
}

void QQDeBreathBreathEqComponent::setTheme(const juce::String& title, juce::Colour primary, juce::Colour secondary)
{
    themeTitle = title;
    primaryColour = primary;
    secondaryColour = secondary;
    repaint();
}

void QQDeBreathBreathEqComponent::setSpectrum(const std::vector<float>& magnitudes)
{
    spectrum = magnitudes;
    postSpectrum.clear();
    repaint();
}

void QQDeBreathBreathEqComponent::setSpectra(const std::vector<float>& preMagnitudes, const std::vector<float>& postMagnitudes)
{
    spectrum = preMagnitudes;
    postSpectrum = postMagnitudes;
    repaint();
}

void QQDeBreathBreathEqComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff020617));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    const auto area = graphArea();
    paintGrid(g, area);
    paintSpectrum(g, area);
    paintResponse(g, area);
    paintNodes(g, area);

    g.setFont(juce::Font(13.0f, juce::Font::plain));
    g.setColour(state.enabled ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
    const auto title = themeTitle
                     + juce::String("  HP ") + (state.highPassEnabled ? frequencyLabel(state.highPassHz) + "/" + slopeLabel(state.highPassSlopeDbPerOct) : "off")
                     + "  LP " + (state.lowPassEnabled ? frequencyLabel(state.lowPassHz) + "/" + slopeLabel(state.lowPassSlopeDbPerOct) : "off")
                     + "  Bands " + juce::String(state.enabledBandCount()) + "/6";
    g.drawText(title, getLocalBounds().reduced(14, 8).removeFromTop(24), juce::Justification::centredLeft);

    if (! state.enabled)
    {
        g.setColour(juce::Colour(0x990f172a));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText(themeTitle + " Off", area.toNearestInt(), juce::Justification::centred);
    }
}

void QQDeBreathBreathEqComponent::paintGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(juce::Colour(0xff07111f));
    g.fillRoundedRectangle(area, 6.0f);

    const double freqs[] = { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    for (auto frequency : freqs)
    {
        const auto x = frequencyToX(frequency);
        g.setColour(juce::Colour(frequency == 1000.0 ? 0xff334155 : 0xff1e293b));
        g.drawVerticalLine(static_cast<int>(std::round(x)), area.getY(), area.getBottom());
        g.setColour(juce::Colour(0xff64748b));
        g.drawText(frequencyLabel(frequency), static_cast<int>(x - 22.0f), static_cast<int>(area.getBottom() - 18.0f), 44, 16, juce::Justification::centred);
    }

    const double gains[] = { -24.0, -12.0, 0.0, 12.0, 24.0 };
    for (auto gain : gains)
    {
        const auto y = gainToY(gain);
        g.setColour(juce::Colour(gain == 0.0 ? 0xff475569 : 0xff1e293b));
        g.drawHorizontalLine(static_cast<int>(std::round(y)), area.getX(), area.getRight());
        g.setColour(juce::Colour(0xff64748b));
        g.drawText((gain > 0.0 ? "+" : "") + juce::String(gain, 0), static_cast<int>(area.getX() + 6.0f), static_cast<int>(y - 8.0f), 36, 16, juce::Justification::centredLeft);
    }
}

void QQDeBreathBreathEqComponent::paintSpectrum(juce::Graphics& g, juce::Rectangle<float> area)
{
    if (spectrum.size() < 2)
        return;

    juce::Path path;
    path.startNewSubPath(area.getX(), area.getBottom());

    for (size_t i = 0; i < spectrum.size(); ++i)
    {
        const auto norm = static_cast<float>(i) / static_cast<float>(spectrum.size() - 1);
        const auto frequency = frequencyFromNormalized(norm);
        const auto x = frequencyToX(frequency);
        const auto mag = juce::jlimit(0.0f, 1.0f, spectrum[i]);
        const auto y = area.getBottom() - mag * area.getHeight();
        path.lineTo(x, y);
    }

    path.lineTo(area.getRight(), area.getBottom());
    path.closeSubPath();

    g.setColour(primaryColour.withAlpha(0.22f));
    g.fillPath(path);
    g.setColour(primaryColour.withAlpha(0.48f));
    g.strokePath(path, juce::PathStrokeType(1.0f));

    if (postSpectrum.size() >= 2)
    {
        juce::Path postPath;
        for (size_t i = 0; i < postSpectrum.size(); ++i)
        {
            const auto norm = static_cast<float>(i) / static_cast<float>(postSpectrum.size() - 1);
            const auto frequency = frequencyFromNormalized(norm);
            const auto x = frequencyToX(frequency);
            const auto mag = juce::jlimit(0.0f, 1.0f, postSpectrum[i]);
            const auto y = area.getBottom() - mag * area.getHeight();

            if (i == 0)
                postPath.startNewSubPath(x, y);
            else
                postPath.lineTo(x, y);
        }

        g.setColour(secondaryColour);
        g.strokePath(postPath, juce::PathStrokeType(1.5f));
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(11.0f, juce::Font::plain));
        g.drawText("Pre", area.getRight() - 88.0f, area.getY() + 8.0f, 36.0f, 16.0f, juce::Justification::centredLeft);
        g.setColour(secondaryColour);
        g.drawText("Post", area.getRight() - 48.0f, area.getY() + 8.0f, 42.0f, 16.0f, juce::Justification::centredLeft);
    }
}

void QQDeBreathBreathEqComponent::paintResponse(juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::Path response;
    constexpr auto points = 260;

    for (auto i = 0; i < points; ++i)
    {
        const auto norm = static_cast<float>(i) / static_cast<float>(points - 1);
        const auto frequency = frequencyFromNormalized(norm);
        const auto db = breathEqMagnitudeDbAt(state, frequency, 48000.0);
        const auto x = frequencyToX(frequency);
        const auto y = gainToY(juce::jlimit(minGainDb, maxGainDb, db));

        if (i == 0)
            response.startNewSubPath(x, y);
        else
            response.lineTo(x, y);
    }

    g.setColour(state.hasActiveProcessing() ? primaryColour : juce::Colour(0xff475569));
    g.strokePath(response, juce::PathStrokeType(2.0f));
}

void QQDeBreathBreathEqComponent::paintNodes(juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::ignoreUnused(area);

    if (state.highPassEnabled)
    {
        const auto x = frequencyToX(state.highPassHz);
        const auto y = gainToY(-18.0);
        const auto selected = isSelectedHighPass();
        g.setColour(selected ? juce::Colour(0xfff8fafc) : primaryColour);
        g.fillEllipse(x - nodeRadius, y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);
        g.setColour(juce::Colours::white);
        g.drawText("HP", static_cast<int>(x - 20.0f), static_cast<int>(y - 24.0f), 40, 18, juce::Justification::centred);
        if (selected)
        {
            g.setColour(juce::Colour(0xffcbd5e1));
            g.setFont(juce::Font(12.0f, juce::Font::plain));
            const auto label = frequencyLabel(state.highPassHz) + "  " + slopeLabel(state.highPassSlopeDbPerOct);
            g.drawText(label, static_cast<int>(x - 72.0f), static_cast<int>(y - 42.0f), 144, 18, juce::Justification::centred);
        }
    }

    if (state.lowPassEnabled)
    {
        const auto x = frequencyToX(state.lowPassHz);
        const auto y = gainToY(-18.0);
        const auto selected = isSelectedLowPass();
        g.setColour(selected ? juce::Colour(0xfff8fafc) : secondaryColour);
        g.fillEllipse(x - nodeRadius, y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);
        g.setColour(juce::Colours::white);
        g.drawText("LP", static_cast<int>(x - 20.0f), static_cast<int>(y - 24.0f), 40, 18, juce::Justification::centred);
        if (selected)
        {
            g.setColour(juce::Colour(0xffcbd5e1));
            g.setFont(juce::Font(12.0f, juce::Font::plain));
            const auto label = frequencyLabel(state.lowPassHz) + "  " + slopeLabel(state.lowPassSlopeDbPerOct);
            g.drawText(label, static_cast<int>(x - 72.0f), static_cast<int>(y - 42.0f), 144, 18, juce::Justification::centred);
        }
    }

    for (auto i = 0; i < QQDeBreathEqState::maxBands; ++i)
    {
        const auto& band = state.bands[static_cast<size_t>(i)];
        if (! band.enabled)
            continue;

        const auto x = frequencyToX(band.frequencyHz);
        const auto y = gainToY(band.gainDb);
        const auto selected = i == selectedBand;

        g.setColour(selected ? juce::Colour(0xfff8fafc) : secondaryColour.withAlpha(0.92f));
        g.fillEllipse(x - nodeRadius, y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);
        g.setColour(juce::Colour(0xff0f172a));
        g.drawEllipse(x - nodeRadius, y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f, 1.2f);
        g.setColour(juce::Colour(0xffe2e8f0));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(juce::String(i + 1), static_cast<int>(x - nodeRadius), static_cast<int>(y - nodeRadius), static_cast<int>(nodeRadius * 2.0f), static_cast<int>(nodeRadius * 2.0f), juce::Justification::centred);

        if (selected)
        {
            g.setFont(juce::Font(12.0f, juce::Font::plain));
            g.setColour(juce::Colour(0xffcbd5e1));
            const auto label = frequencyLabel(band.frequencyHz) + "  " + juce::String(band.gainDb, 1) + " dB  Q " + juce::String(band.q, 2);
            g.drawText(label, static_cast<int>(x - 85.0f), static_cast<int>(y - 36.0f), 170, 18, juce::Justification::centred);
        }
    }
}

void QQDeBreathBreathEqComponent::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    const auto pos = event.position;

    if (event.mods.isAltDown() && event.mods.isLeftButtonDown())
    {
        state = {};
        selectedBand = -1;
        selectedFilter = DragTarget::none;
        notifyStateChanged();
        repaint();
        return;
    }

    const auto band = hitTestBand(pos);
    if (event.mods.isPopupMenu())
    {
        if (hitTestHighPass(pos))
        {
            state.highPassEnabled = false;
            selectedFilter = DragTarget::none;
            dragTarget = DragTarget::none;
            notifyStateChanged();
            repaint();
            return;
        }

        if (hitTestLowPass(pos))
        {
            state.lowPassEnabled = false;
            selectedFilter = DragTarget::none;
            dragTarget = DragTarget::none;
            notifyStateChanged();
            repaint();
            return;
        }

        if (band >= 0)
        {
            state.bands[static_cast<size_t>(band)] = {};
            if (selectedBand == band)
                selectedBand = -1;
            notifyStateChanged();
            repaint();
        }
        return;
    }

    if (event.mods.isLeftButtonDown() && graphArea().contains(pos) && band < 0)
    {
        const auto area = graphArea();
        const auto normalizedX = (pos.x - area.getX()) / area.getWidth();
        if (normalizedX <= 0.08f)
        {
            state.enabled = true;
            state.highPassEnabled = true;
            state.highPassHz = xToFrequency(pos.x);
            dragTarget = DragTarget::highPass;
            selectedFilter = DragTarget::highPass;
            selectedBand = -1;
            notifyStateChanged();
            repaint();
            return;
        }

        if (normalizedX >= 0.92f)
        {
            state.enabled = true;
            state.lowPassEnabled = true;
            state.lowPassHz = xToFrequency(pos.x);
            dragTarget = DragTarget::lowPass;
            selectedFilter = DragTarget::lowPass;
            selectedBand = -1;
            notifyStateChanged();
            repaint();
            return;
        }
    }

    if (event.getNumberOfClicks() >= 2 && event.mods.isLeftButtonDown())
    {
        const auto slot = firstFreeBand();
        if (slot >= 0 && graphArea().contains(pos))
        {
            state.enabled = true;
            auto& newBand = state.bands[static_cast<size_t>(slot)];
            newBand.enabled = true;
            newBand.frequencyHz = xToFrequency(pos.x);
            newBand.gainDb = yToGain(pos.y);
            newBand.q = 1.0;
            selectedBand = slot;
            selectedFilter = DragTarget::none;
            dragTarget = DragTarget::band;
            dragBand = slot;
            notifyStateChanged();
            repaint();
        }
        return;
    }

    if (band >= 0)
    {
        selectedBand = band;
        selectedFilter = DragTarget::none;
        dragTarget = DragTarget::band;
        dragBand = band;
        repaint();
        return;
    }

    if (hitTestHighPass(pos))
    {
        selectedBand = -1;
        selectedFilter = DragTarget::highPass;
        dragTarget = DragTarget::highPass;
        repaint();
        return;
    }

    if (hitTestLowPass(pos))
    {
        selectedBand = -1;
        selectedFilter = DragTarget::lowPass;
        dragTarget = DragTarget::lowPass;
        repaint();
        return;
    }

    selectedBand = -1;
    selectedFilter = DragTarget::none;
    repaint();
}

void QQDeBreathBreathEqComponent::mouseDrag(const juce::MouseEvent& event)
{
    auto changed = false;

    if (dragTarget == DragTarget::band && dragBand >= 0)
    {
        auto& band = state.bands[static_cast<size_t>(dragBand)];
        band.frequencyHz = xToFrequency(event.position.x);
        band.gainDb = event.mods.isShiftDown()
                    ? std::round(yToGain(event.position.y) * 10.0) / 10.0
                    : std::round(yToGain(event.position.y) * 2.0) / 2.0;
        changed = true;
    }
    else if (dragTarget == DragTarget::highPass)
    {
        state.highPassEnabled = true;
        state.highPassHz = juce::jlimit(20.0, 2000.0, xToFrequency(event.position.x));
        changed = true;
    }
    else if (dragTarget == DragTarget::lowPass)
    {
        state.lowPassEnabled = true;
        state.lowPassHz = juce::jlimit(2000.0, 20000.0, xToFrequency(event.position.x));
        changed = true;
    }

    if (changed)
    {
        state.enabled = true;
        state = sanitizeBreathEqState(state);
        notifyStateChanged();
        repaint();
    }
}

void QQDeBreathBreathEqComponent::mouseMove(const juce::MouseEvent& event)
{
    updateCursor(event.position);
}

void QQDeBreathBreathEqComponent::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    dragTarget = DragTarget::none;
    dragBand = -1;
}

void QQDeBreathBreathEqComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (hitTestHighPass(event.position) || selectedFilter == DragTarget::highPass)
    {
        state.highPassSlopeDbPerOct = nextSlope(state.highPassSlopeDbPerOct, wheel.deltaY >= 0.0f ? 1 : -1);
        selectedBand = -1;
        selectedFilter = DragTarget::highPass;
        notifyStateChanged();
        repaint();
        return;
    }

    if (hitTestLowPass(event.position) || selectedFilter == DragTarget::lowPass)
    {
        state.lowPassSlopeDbPerOct = nextSlope(state.lowPassSlopeDbPerOct, wheel.deltaY >= 0.0f ? 1 : -1);
        selectedBand = -1;
        selectedFilter = DragTarget::lowPass;
        notifyStateChanged();
        repaint();
        return;
    }

    auto band = selectedBand;
    if (const auto hit = hitTestBand(event.position); hit >= 0)
        band = hit;

    if (band < 0)
        return;

    auto& target = state.bands[static_cast<size_t>(band)];
    const auto step = event.mods.isShiftDown() ? 0.05 : 0.15;
    target.q = juce::jlimit(0.10, 12.0, target.q + (wheel.deltaY >= 0.0f ? step : -step));
    selectedBand = band;
    notifyStateChanged();
    repaint();
}

juce::Rectangle<float> QQDeBreathBreathEqComponent::graphArea() const
{
    return getLocalBounds().toFloat().reduced(16.0f, 36.0f).withTrimmedBottom(12.0f);
}

double QQDeBreathBreathEqComponent::xToFrequency(float x) const
{
    const auto area = graphArea();
    return frequencyFromNormalized((x - area.getX()) / area.getWidth());
}

float QQDeBreathBreathEqComponent::frequencyToX(double frequencyHz) const
{
    const auto area = graphArea();
    return area.getX() + normalizedLogFrequency(frequencyHz) * area.getWidth();
}

double QQDeBreathBreathEqComponent::yToGain(float y) const
{
    const auto area = graphArea();
    const auto normalized = juce::jlimit(0.0f, 1.0f, (y - area.getY()) / area.getHeight());
    return maxGainDb + static_cast<double>(normalized) * (minGainDb - maxGainDb);
}

float QQDeBreathBreathEqComponent::gainToY(double gainDb) const
{
    const auto area = graphArea();
    const auto normalized = static_cast<float>((juce::jlimit(minGainDb, maxGainDb, gainDb) - maxGainDb) / (minGainDb - maxGainDb));
    return area.getY() + normalized * area.getHeight();
}

int QQDeBreathBreathEqComponent::hitTestBand(juce::Point<float> position) const
{
    for (auto i = 0; i < QQDeBreathEqState::maxBands; ++i)
    {
        const auto& band = state.bands[static_cast<size_t>(i)];
        if (! band.enabled)
            continue;

        const auto point = juce::Point<float>(frequencyToX(band.frequencyHz), gainToY(band.gainDb));
        if (point.getDistanceFrom(position) <= nodeRadius + 4.0f)
            return i;
    }

    return -1;
}

bool QQDeBreathBreathEqComponent::hitTestHighPass(juce::Point<float> position) const
{
    if (! state.highPassEnabled)
        return false;

    const auto point = juce::Point<float>(frequencyToX(state.highPassHz), gainToY(-18.0));
    return point.getDistanceFrom(position) <= nodeRadius + 5.0f;
}

bool QQDeBreathBreathEqComponent::hitTestLowPass(juce::Point<float> position) const
{
    if (! state.lowPassEnabled)
        return false;

    const auto point = juce::Point<float>(frequencyToX(state.lowPassHz), gainToY(-18.0));
    return point.getDistanceFrom(position) <= nodeRadius + 5.0f;
}

bool QQDeBreathBreathEqComponent::isSelectedHighPass() const
{
    return selectedFilter == DragTarget::highPass && state.highPassEnabled;
}

bool QQDeBreathBreathEqComponent::isSelectedLowPass() const
{
    return selectedFilter == DragTarget::lowPass && state.lowPassEnabled;
}

int QQDeBreathBreathEqComponent::firstFreeBand() const
{
    for (auto i = 0; i < QQDeBreathEqState::maxBands; ++i)
        if (! state.bands[static_cast<size_t>(i)].enabled)
            return i;

    return -1;
}

void QQDeBreathBreathEqComponent::notifyStateChanged()
{
    state = sanitizeBreathEqState(state);
    if (onStateChanged)
        onStateChanged(state);
}

void QQDeBreathBreathEqComponent::updateCursor(juce::Point<float> position)
{
    if (hitTestBand(position) >= 0 || hitTestHighPass(position) || hitTestLowPass(position))
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}
