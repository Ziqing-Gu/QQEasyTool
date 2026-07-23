#include "BreathEq.h"

#include <complex>
#include <cmath>

namespace
{
constexpr auto minFrequency = 20.0;
constexpr auto maxFrequency = 20000.0;
constexpr auto defaultSampleRate = 48000.0;

double clampFrequency(double frequency)
{
    return juce::jlimit(minFrequency, maxFrequency, frequency);
}

double dbToGain(double db)
{
    return std::pow(10.0, db / 20.0);
}

int sanitizeSlope(int slopeDbPerOct)
{
    if (slopeDbPerOct <= 12)
        return 12;
    if (slopeDbPerOct <= 24)
        return 24;
    if (slopeDbPerOct <= 36)
        return 36;
    return 48;
}

int biquadCountForSlope(int slopeDbPerOct)
{
    return sanitizeSlope(slopeDbPerOct) / 12;
}

juce::var bandToVar(const QQDeBreathEqBand& band)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("enabled", band.enabled);
    object->setProperty("frequency_hz", band.frequencyHz);
    object->setProperty("gain_db", band.gainDb);
    object->setProperty("q", band.q);
    return juce::var(object);
}

QQDeBreathEqBand bandFromVar(const juce::var& value)
{
    QQDeBreathEqBand band;
    if (! value.isObject())
        return band;

    band.enabled = static_cast<bool>(value.getProperty("enabled", false));
    band.frequencyHz = static_cast<double>(value.getProperty("frequency_hz", 1000.0));
    band.gainDb = static_cast<double>(value.getProperty("gain_db", 0.0));
    band.q = static_cast<double>(value.getProperty("q", 1.0));
    return band;
}

std::complex<double> responseAt(const QQDeBreathEqProcessor::Coefficients& c, double frequencyHz, double sampleRate)
{
    const auto omega = 2.0 * juce::MathConstants<double>::pi * frequencyHz / sampleRate;
    const auto z1 = std::complex<double>(std::cos(-omega), std::sin(-omega));
    const auto z2 = std::complex<double>(std::cos(-2.0 * omega), std::sin(-2.0 * omega));
    const auto numerator = c.b0 + c.b1 * z1 + c.b2 * z2;
    const auto denominator = 1.0 + c.a1 * z1 + c.a2 * z2;
    return numerator / denominator;
}
} // namespace

bool QQDeBreathEqState::hasActiveProcessing() const
{
    if (! enabled)
        return false;

    if (highPassEnabled || lowPassEnabled)
        return true;

    for (const auto& band : bands)
        if (band.enabled && std::abs(band.gainDb) > 0.001)
            return true;

    return false;
}

int QQDeBreathEqState::enabledBandCount() const
{
    auto count = 0;
    for (const auto& band : bands)
        if (band.enabled)
            ++count;
    return count;
}

QQDeBreathEqState sanitizeBreathEqState(QQDeBreathEqState state)
{
    state.bypassed = false;
    state.highPassHz = juce::jlimit(20.0, 2000.0, state.highPassHz);
    state.highPassQ = juce::jlimit(0.10, 12.0, state.highPassQ);
    state.highPassSlopeDbPerOct = sanitizeSlope(state.highPassSlopeDbPerOct);
    state.lowPassHz = juce::jlimit(2000.0, 20000.0, state.lowPassHz);
    state.lowPassQ = juce::jlimit(0.10, 12.0, state.lowPassQ);
    state.lowPassSlopeDbPerOct = sanitizeSlope(state.lowPassSlopeDbPerOct);

    if (state.lowPassHz <= state.highPassHz + 200.0)
        state.lowPassHz = juce::jmin(20000.0, state.highPassHz + 200.0);

    for (auto& band : state.bands)
    {
        band.frequencyHz = clampFrequency(band.frequencyHz);
        band.gainDb = juce::jlimit(-24.0, 24.0, band.gainDb);
        band.q = juce::jlimit(0.10, 12.0, band.q);
    }

    return state;
}

juce::String serializeBreathEqState(const QQDeBreathEqState& stateIn)
{
    const auto state = sanitizeBreathEqState(stateIn);

    auto* root = new juce::DynamicObject();
    root->setProperty("version", 1);
    root->setProperty("enabled", state.enabled);
    root->setProperty("bypassed", state.bypassed);
    root->setProperty("high_pass_enabled", state.highPassEnabled);
    root->setProperty("low_pass_enabled", state.lowPassEnabled);
    root->setProperty("high_pass_hz", state.highPassHz);
    root->setProperty("high_pass_q", state.highPassQ);
    root->setProperty("high_pass_slope_db_per_oct", state.highPassSlopeDbPerOct);
    root->setProperty("low_pass_hz", state.lowPassHz);
    root->setProperty("low_pass_q", state.lowPassQ);
    root->setProperty("low_pass_slope_db_per_oct", state.lowPassSlopeDbPerOct);

    juce::Array<juce::var> bands;
    for (const auto& band : state.bands)
        bands.add(bandToVar(band));
    root->setProperty("bands", bands);

    return juce::JSON::toString(juce::var(root), false);
}

bool deserializeBreathEqState(const juce::String& text, QQDeBreathEqState& state)
{
    state = {};

    if (text.trim().isEmpty())
        return true;

    const auto root = juce::JSON::parse(text);
    if (! root.isObject() || static_cast<int>(root.getProperty("version", 0)) != 1)
        return false;

    state.enabled = static_cast<bool>(root.getProperty("enabled", false));
    state.bypassed = static_cast<bool>(root.getProperty("bypassed", false));
    state.highPassEnabled = static_cast<bool>(root.getProperty("high_pass_enabled", false));
    state.lowPassEnabled = static_cast<bool>(root.getProperty("low_pass_enabled", false));
    state.highPassHz = static_cast<double>(root.getProperty("high_pass_hz", 80.0));
    state.highPassQ = static_cast<double>(root.getProperty("high_pass_q", 0.70710678118));
    state.highPassSlopeDbPerOct = static_cast<int>(root.getProperty("high_pass_slope_db_per_oct", 12));
    state.lowPassHz = static_cast<double>(root.getProperty("low_pass_hz", 12000.0));
    state.lowPassQ = static_cast<double>(root.getProperty("low_pass_q", 0.70710678118));
    state.lowPassSlopeDbPerOct = static_cast<int>(root.getProperty("low_pass_slope_db_per_oct", 12));

    if (auto* bands = root.getProperty("bands", {}).getArray())
    {
        for (auto i = 0; i < juce::jmin(static_cast<int>(bands->size()), QQDeBreathEqState::maxBands); ++i)
            state.bands[static_cast<size_t>(i)] = bandFromVar(bands->getReference(i));
    }

    state = sanitizeBreathEqState(state);
    return true;
}

double breathEqMagnitudeDbAt(const QQDeBreathEqState& stateIn, double frequencyHz, double sampleRate)
{
    const auto state = sanitizeBreathEqState(stateIn);
    if (! state.hasActiveProcessing())
        return 0.0;

    QQDeBreathEqProcessor::Coefficients c;
    auto response = std::complex<double>(1.0, 0.0);
    const auto sr = sampleRate > 0.0 ? sampleRate : defaultSampleRate;

    if (state.highPassEnabled)
    {
        for (auto i = 0; i < biquadCountForSlope(state.highPassSlopeDbPerOct); ++i)
        {
            c = QQDeBreathEqProcessor::makeHighPass(sr, state.highPassHz, 0.70710678118);
            response *= responseAt(c, frequencyHz, sr);
        }
    }

    if (state.lowPassEnabled)
    {
        for (auto i = 0; i < biquadCountForSlope(state.lowPassSlopeDbPerOct); ++i)
        {
            c = QQDeBreathEqProcessor::makeLowPass(sr, state.lowPassHz, 0.70710678118);
            response *= responseAt(c, frequencyHz, sr);
        }
    }

    for (const auto& band : state.bands)
    {
        if (band.enabled && std::abs(band.gainDb) > 0.001)
        {
            c = QQDeBreathEqProcessor::makePeak(sr, band.frequencyHz, band.gainDb, band.q);
            response *= responseAt(c, frequencyHz, sr);
        }
    }

    return 20.0 * std::log10(std::max(1.0e-12, std::abs(response)));
}

void QQDeBreathEqProcessor::prepare(double sampleRate, int channels, const QQDeBreathEqState& stateIn)
{
    const auto state = sanitizeBreathEqState(stateIn);
    if (hasCurrentState
        && statesEqual(state, currentState)
        && juce::approximatelyEqual(sampleRate, currentSampleRate)
        && channels == currentChannels)
        return;

    currentState = state;
    hasCurrentState = true;
    currentSampleRate = sampleRate;
    currentChannels = channels;
    filters.clear();

    if (state.hasActiveProcessing() && sampleRate > 0.0)
    {
        if (state.highPassEnabled)
            for (auto i = 0; i < biquadCountForSlope(state.highPassSlopeDbPerOct); ++i)
                filters.push_back(makeHighPass(sampleRate, state.highPassHz, 0.70710678118));

        for (const auto& band : state.bands)
            if (band.enabled && std::abs(band.gainDb) > 0.001)
                filters.push_back(makePeak(sampleRate, band.frequencyHz, band.gainDb, band.q));

        if (state.lowPassEnabled)
            for (auto i = 0; i < biquadCountForSlope(state.lowPassSlopeDbPerOct); ++i)
                filters.push_back(makeLowPass(sampleRate, state.lowPassHz, 0.70710678118));
    }

    channelStates.assign(static_cast<size_t>(juce::jmax(0, channels)),
                         std::vector<BiquadState>(filters.size()));
}

void QQDeBreathEqProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (filters.empty() || buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    if (static_cast<int>(channelStates.size()) < buffer.getNumChannels())
        channelStates.resize(static_cast<size_t>(buffer.getNumChannels()), std::vector<BiquadState>(filters.size()));

    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        auto& states = channelStates[static_cast<size_t>(channel)];
        if (states.size() != filters.size())
            states.assign(filters.size(), {});

        for (size_t filterIndex = 0; filterIndex < filters.size(); ++filterIndex)
        {
            const auto& c = filters[filterIndex];
            auto& s = states[filterIndex];

            for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto input = static_cast<double>(data[sample]);
                const auto output = c.b0 * input + s.z1;
                s.z1 = c.b1 * input - c.a1 * output + s.z2;
                s.z2 = c.b2 * input - c.a2 * output;
                data[sample] = static_cast<float>(output);
            }
        }
    }
}

void QQDeBreathEqProcessor::reset()
{
    for (auto& channel : channelStates)
        for (auto& state : channel)
            state = {};
}

QQDeBreathEqProcessor::Coefficients QQDeBreathEqProcessor::makeHighPass(double sampleRate, double frequencyHz, double q)
{
    const auto frequency = juce::jlimit(20.0, sampleRate * 0.45, frequencyHz);
    const auto safeQ = juce::jlimit(0.10, 12.0, q);
    const auto omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const auto sinValue = std::sin(omega);
    const auto cosValue = std::cos(omega);
    const auto alpha = sinValue / (2.0 * safeQ);
    const auto a0 = 1.0 + alpha;

    Coefficients c;
    c.b0 = ((1.0 + cosValue) * 0.5) / a0;
    c.b1 = (-(1.0 + cosValue)) / a0;
    c.b2 = ((1.0 + cosValue) * 0.5) / a0;
    c.a1 = (-2.0 * cosValue) / a0;
    c.a2 = (1.0 - alpha) / a0;
    return c;
}

QQDeBreathEqProcessor::Coefficients QQDeBreathEqProcessor::makeLowPass(double sampleRate, double frequencyHz, double q)
{
    const auto frequency = juce::jlimit(20.0, sampleRate * 0.45, frequencyHz);
    const auto safeQ = juce::jlimit(0.10, 12.0, q);
    const auto omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const auto sinValue = std::sin(omega);
    const auto cosValue = std::cos(omega);
    const auto alpha = sinValue / (2.0 * safeQ);
    const auto a0 = 1.0 + alpha;

    Coefficients c;
    c.b0 = ((1.0 - cosValue) * 0.5) / a0;
    c.b1 = (1.0 - cosValue) / a0;
    c.b2 = ((1.0 - cosValue) * 0.5) / a0;
    c.a1 = (-2.0 * cosValue) / a0;
    c.a2 = (1.0 - alpha) / a0;
    return c;
}

QQDeBreathEqProcessor::Coefficients QQDeBreathEqProcessor::makePeak(double sampleRate, double frequencyHz, double gainDb, double q)
{
    const auto frequency = juce::jlimit(20.0, sampleRate * 0.45, frequencyHz);
    const auto safeQ = juce::jlimit(0.10, 12.0, q);
    const auto amplitude = std::pow(10.0, gainDb / 40.0);
    const auto omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const auto sinValue = std::sin(omega);
    const auto cosValue = std::cos(omega);
    const auto alpha = sinValue / (2.0 * safeQ);
    const auto a0 = 1.0 + alpha / amplitude;

    Coefficients c;
    c.b0 = (1.0 + alpha * amplitude) / a0;
    c.b1 = (-2.0 * cosValue) / a0;
    c.b2 = (1.0 - alpha * amplitude) / a0;
    c.a1 = (-2.0 * cosValue) / a0;
    c.a2 = (1.0 - alpha / amplitude) / a0;
    return c;
}

double QQDeBreathEqProcessor::magnitudeDbAt(const Coefficients& coefficients, double frequencyHz, double sampleRate)
{
    return 20.0 * std::log10(std::max(1.0e-12, std::abs(responseAt(coefficients, frequencyHz, sampleRate))));
}

bool QQDeBreathEqProcessor::statesEqual(const QQDeBreathEqState& a, const QQDeBreathEqState& b)
{
    constexpr auto epsilon = 1.0e-9;
    if (a.enabled != b.enabled
        || a.highPassEnabled != b.highPassEnabled
        || a.lowPassEnabled != b.lowPassEnabled
        || a.highPassSlopeDbPerOct != b.highPassSlopeDbPerOct
        || a.lowPassSlopeDbPerOct != b.lowPassSlopeDbPerOct
        || std::abs(a.highPassHz - b.highPassHz) > epsilon
        || std::abs(a.lowPassHz - b.lowPassHz) > epsilon
        || std::abs(a.highPassQ - b.highPassQ) > epsilon
        || std::abs(a.lowPassQ - b.lowPassQ) > epsilon)
        return false;

    for (size_t i = 0; i < a.bands.size(); ++i)
    {
        const auto& left = a.bands[i];
        const auto& right = b.bands[i];
        if (left.enabled != right.enabled
            || std::abs(left.frequencyHz - right.frequencyHz) > epsilon
            || std::abs(left.gainDb - right.gainDb) > epsilon
            || std::abs(left.q - right.q) > epsilon)
            return false;
    }

    return true;
}
