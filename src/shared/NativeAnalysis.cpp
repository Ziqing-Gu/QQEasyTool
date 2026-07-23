#include "NativeAnalysis.h"

#include "NativeBreathModelData.h"
#include "NativeFilterData.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <numeric>
#include <vector>

namespace
{
constexpr auto frameMs = 25.0;
constexpr auto hopMs = 5.0;
constexpr auto eps = 1.0e-12;
constexpr auto autoBreathMinSeconds = 0.12;
constexpr auto autoBreathShortReviewSeconds = 0.16;

struct Region
{
    double start = 0.0;
    double end = 0.0;
    juce::String type = "Breath";
    double confidence = 1.0;
};

struct Features
{
    std::vector<std::array<double, QQDeBreathNativeModel::kFeatureCount>> x;
    std::vector<double> fullDb;
    std::vector<double> sibilanceLowDensityDb;
    std::vector<double> sibilanceMidLowDensityDb;
    std::vector<double> sibilanceHigh1DensityDb;
    std::vector<double> sibilanceHigh2DensityDb;
    int hop = 0;
};

struct FrameRefs
{
    double floor = -90.0;
    double low = -72.0;
    double vocal = -34.0;
    double peak = -12.0;
    double dynamic = 56.0;
    double strong = -34.0;
    double airyMin = -78.0;
    double airyMax = -52.0;
    double nearMin = -50.0;
    double nearMax = -26.0;
};

double safeDb(double value)
{
    return 20.0 * std::log10(std::max(value, eps));
}

double safePowerDb(double value)
{
    return 10.0 * std::log10(std::max(value, eps));
}

double sigmoid(double value)
{
    if (value >= 0.0)
    {
        const auto z = std::exp(-value);
        return 1.0 / (1.0 + z);
    }

    const auto z = std::exp(value);
    return z / (1.0 + z);
}

double percentileCopy(std::vector<double> values, double percentile)
{
    values.erase(std::remove_if(values.begin(), values.end(), [] (double v) { return ! std::isfinite(v); }), values.end());

    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());

    if (values.size() == 1)
        return values.front();

    const auto position = juce::jlimit(0.0, 100.0, percentile) / 100.0 * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<size_t>(std::floor(position));
    const auto upper = std::min(values.size() - 1, lower + 1);
    const auto frac = position - static_cast<double>(lower);
    return values[lower] * (1.0 - frac) + values[upper] * frac;
}

double percentileRange(const std::vector<double>& values, int start, int end, double percentile)
{
    start = juce::jlimit(0, static_cast<int>(values.size()), start);
    end = juce::jlimit(start, static_cast<int>(values.size()), end);

    if (end <= start)
        return 0.0;

    return percentileCopy(std::vector<double>(values.begin() + start, values.begin() + end), percentile);
}

int paddedFrameCount(int sampleCount, int frame, int hop)
{
    if (sampleCount <= 0)
        return 0;

    if (sampleCount < frame)
        return 1;

    const auto extra = static_cast<int>(std::ceil(static_cast<double>(sampleCount - frame) / static_cast<double>(hop)) * hop
                                      + frame - sampleCount);
    return (sampleCount + std::max(0, extra) - frame) / hop + 1;
}

std::vector<double> paddedCopy(const std::vector<double>& input, int paddedLength)
{
    std::vector<double> out(static_cast<size_t>(std::max(0, paddedLength)), 0.0);
    const auto copyCount = std::min(static_cast<int>(input.size()), paddedLength);
    if (copyCount > 0)
        std::copy(input.begin(), input.begin() + copyCount, out.begin());
    return out;
}

std::vector<double> frameRms(const std::vector<double>& input, int frame, int hop)
{
    const auto frames = paddedFrameCount(static_cast<int>(input.size()), frame, hop);
    const auto paddedLength = frames > 0 ? (frames - 1) * hop + frame : 0;
    auto padded = paddedCopy(input, paddedLength);

    std::vector<double> prefix(padded.size() + 1, 0.0);
    for (size_t i = 0; i < padded.size(); ++i)
        prefix[i + 1] = prefix[i] + padded[i] * padded[i];

    std::vector<double> out(static_cast<size_t>(frames), 0.0);
    for (auto i = 0; i < frames; ++i)
    {
        const auto start = i * hop;
        const auto sum = prefix[static_cast<size_t>(start + frame)] - prefix[static_cast<size_t>(start)];
        out[static_cast<size_t>(i)] = std::sqrt(sum / static_cast<double>(frame) + 1.0e-20);
    }

    return out;
}

std::vector<double> hanning(int width)
{
    std::vector<double> kernel(static_cast<size_t>(std::max(0, width)), 0.0);

    if (width <= 0)
        return kernel;

    if (width == 1)
    {
        kernel[0] = 1.0;
        return kernel;
    }

    for (auto i = 0; i < width; ++i)
        kernel[static_cast<size_t>(i)] = 0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * i / static_cast<double>(width - 1));

    return kernel;
}

std::vector<double> convolveSame(const std::vector<double>& input, const std::vector<double>& kernel)
{
    if (kernel.empty() || input.empty())
        return input;

    const auto n = static_cast<int>(input.size());
    const auto m = static_cast<int>(kernel.size());
    const auto startOffset = (m - 1) / 2;
    std::vector<double> out(input.size(), 0.0);

    for (auto i = 0; i < n; ++i)
    {
        auto sum = 0.0;
        for (auto k = 0; k < m; ++k)
        {
            const auto j = i + startOffset - k;
            if (j >= 0 && j < n)
                sum += input[static_cast<size_t>(j)] * kernel[static_cast<size_t>(k)];
        }
        out[static_cast<size_t>(i)] = sum;
    }

    return out;
}

std::vector<double> smoothArray(const std::vector<double>& input, int width)
{
    width = std::max(1, width);
    if (width <= 1)
        return input;

    std::vector<double> kernel(static_cast<size_t>(width), 1.0 / static_cast<double>(width));
    return convolveSame(input, kernel);
}

std::vector<double> smoothProbability(const std::vector<double>& input, int width)
{
    if (width <= 1)
        return input;

    auto kernel = hanning(width);
    const auto sum = std::accumulate(kernel.begin(), kernel.end(), 0.0);
    if (sum > 0.0)
        for (auto& v : kernel)
            v /= sum;

    return convolveSame(input, kernel);
}

std::vector<double> movingMeanEdge(const std::vector<double>& input, int width)
{
    width = std::max(1, width);
    if (width <= 1 || input.empty())
        return input;

    const auto n = static_cast<int>(input.size());
    const auto left = width / 2;
    const auto right = width - 1 - left;
    std::vector<double> padded(static_cast<size_t>(n + left + right), 0.0);

    for (auto i = 0; i < static_cast<int>(padded.size()); ++i)
    {
        const auto source = juce::jlimit(0, n - 1, i - left);
        padded[static_cast<size_t>(i)] = input[static_cast<size_t>(source)];
    }

    std::vector<double> prefix(padded.size() + 1, 0.0);
    for (size_t i = 0; i < padded.size(); ++i)
        prefix[i + 1] = prefix[i] + padded[i];

    std::vector<double> out(input.size(), 0.0);
    for (auto i = 0; i < n; ++i)
        out[static_cast<size_t>(i)] = (prefix[static_cast<size_t>(i + width)] - prefix[static_cast<size_t>(i)]) / static_cast<double>(width);

    return out;
}

std::vector<double> maximumFilterNearest(const std::vector<double>& input, int width)
{
    width = std::max(1, width);
    if (input.empty() || width <= 1)
        return input;

    const auto n = static_cast<int>(input.size());
    const auto left = width / 2;
    const auto right = width - 1 - left;
    const auto total = n + left + right;

    auto valueAt = [&] (int paddedIndex)
    {
        const auto source = juce::jlimit(0, n - 1, paddedIndex - left);
        return input[static_cast<size_t>(source)];
    };

    std::deque<int> deque;
    std::vector<double> out(input.size(), 0.0);

    for (auto i = 0; i < total; ++i)
    {
        while (! deque.empty() && deque.front() <= i - width)
            deque.pop_front();

        const auto current = valueAt(i);
        while (! deque.empty() && valueAt(deque.back()) <= current)
            deque.pop_back();

        deque.push_back(i);

        if (i >= width - 1)
        {
            const auto outIndex = i - (width - 1);
            if (outIndex >= 0 && outIndex < n)
                out[static_cast<size_t>(outIndex)] = valueAt(deque.front());
        }
    }

    return out;
}

std::vector<double> normalize01(const std::vector<double>& input)
{
    if (input.empty())
        return {};

    const auto lo = percentileCopy(input, 5.0);
    const auto hi = percentileCopy(input, 95.0);
    std::vector<double> out(input.size(), 0.0);

    if (hi <= lo + 1.0e-12)
        return out;

    for (size_t i = 0; i < input.size(); ++i)
        out[i] = juce::jlimit(0.0, 1.0, (input[i] - lo) / (hi - lo));

    return out;
}

std::vector<double> gradient(const std::vector<double>& input)
{
    if (input.empty())
        return {};

    if (input.size() == 1)
        return { 0.0 };

    std::vector<double> out(input.size(), 0.0);
    out.front() = input[1] - input[0];
    out.back() = input[input.size() - 1] - input[input.size() - 2];

    for (size_t i = 1; i + 1 < input.size(); ++i)
        out[i] = (input[i + 1] - input[i - 1]) * 0.5;

    return out;
}

FrameRefs frameLevelRefs(const std::vector<double>& fullDb)
{
    std::vector<double> finite;
    finite.reserve(fullDb.size());
    for (auto v : fullDb)
        if (std::isfinite(v))
            finite.push_back(v);

    if (finite.empty())
        return {};

    FrameRefs refs;
    refs.peak = percentileCopy(finite, 99.5);

    std::vector<double> active;
    active.reserve(finite.size());
    for (auto v : finite)
        if (v > refs.peak - 90.0)
            active.push_back(v);

    if (active.size() < std::max<size_t>(32, static_cast<size_t>(std::floor(static_cast<double>(finite.size()) * 0.01))))
        active = finite;

    refs.floor = percentileCopy(active, 10.0);
    refs.low = percentileCopy(active, 25.0);
    refs.vocal = percentileCopy(active, 84.0);
    refs.dynamic = std::max(18.0, refs.vocal - refs.floor);
    refs.strong = refs.vocal - std::max(6.0, std::min(13.0, refs.dynamic * 0.18));

    refs.airyMin = refs.floor + std::max(4.0, refs.dynamic * 0.06);
    refs.airyMax = std::min(refs.vocal - std::max(9.0, refs.dynamic * 0.22), refs.low + refs.dynamic * 0.45);
    if (refs.airyMax <= refs.airyMin + 6.0)
        refs.airyMax = refs.airyMin + 6.0;

    refs.nearMin = std::max(refs.airyMin, refs.vocal - std::max(26.0, refs.dynamic * 0.42));
    refs.nearMax = refs.vocal + std::max(2.0, std::min(7.0, refs.dynamic * 0.10));
    if (refs.nearMax <= refs.nearMin + 6.0)
        refs.nearMax = refs.nearMin + 6.0;

    return refs;
}

void distanceToStrong(const std::vector<double>& fullDb,
                      int hop,
                      int sr,
                      const FrameRefs& refs,
                      std::vector<double>& prevStrong,
                      std::vector<double>& nextStrong)
{
    const auto n = static_cast<int>(fullDb.size());
    std::vector<int> strongIdx;
    for (auto i = 0; i < n; ++i)
        if (fullDb[static_cast<size_t>(i)] > refs.strong)
            strongIdx.push_back(i);

    prevStrong.assign(fullDb.size(), 99.0);
    nextStrong.assign(fullDb.size(), 99.0);

    if (strongIdx.empty())
        return;

    auto previous = -1;
    auto nextPointer = 0;
    for (auto i = 0; i < n; ++i)
    {
        while (nextPointer < static_cast<int>(strongIdx.size()) && strongIdx[static_cast<size_t>(nextPointer)] < i)
        {
            previous = strongIdx[static_cast<size_t>(nextPointer)];
            ++nextPointer;
        }

        if (nextPointer < static_cast<int>(strongIdx.size()) && strongIdx[static_cast<size_t>(nextPointer)] == i)
            previous = i;

        const auto next = nextPointer < static_cast<int>(strongIdx.size()) ? strongIdx[static_cast<size_t>(nextPointer)] : -1;
        prevStrong[static_cast<size_t>(i)] = previous >= 0 ? static_cast<double>(i - previous) * hop / sr : 999999.0 * hop / sr;
        nextStrong[static_cast<size_t>(i)] = next >= 0 ? static_cast<double>(next - i) * hop / sr : 999999.0 * hop / sr;
    }
}

const QQDeBreathNativeFilters::BandFilter* findFilter(double sr, const char* name)
{
    const QQDeBreathNativeFilters::BandFilter* best = nullptr;
    auto bestDistance = std::numeric_limits<double>::max();

    for (const auto& filter : QQDeBreathNativeFilters::kBandFilters)
    {
        if (juce::String(filter.name) != name)
            continue;

        const auto distance = std::abs(filter.sampleRate - sr);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = &filter;
        }
    }

    return best;
}

void sosFilterInPlace(std::vector<double>& data, const std::array<QQDeBreathNativeFilters::SosSection, 2>& sections)
{
    for (const auto& section : sections)
    {
        const auto b0 = section.b0 / section.a0;
        const auto b1 = section.b1 / section.a0;
        const auto b2 = section.b2 / section.a0;
        const auto a1 = section.a1 / section.a0;
        const auto a2 = section.a2 / section.a0;
        auto z1 = 0.0;
        auto z2 = 0.0;

        for (auto& sample : data)
        {
            const auto y = b0 * sample + z1;
            z1 = b1 * sample - a1 * y + z2;
            z2 = b2 * sample - a2 * y;
            sample = y;
        }
    }
}

std::vector<double> sosFiltFilt(const std::vector<double>& input,
                                const std::array<QQDeBreathNativeFilters::SosSection, 2>& sections)
{
    if (input.empty())
        return {};

    const auto pad = std::min<int>(15, static_cast<int>(input.size()) - 1);
    std::vector<double> data;
    data.reserve(input.size() + static_cast<size_t>(pad * 2));

    for (auto i = pad; i >= 1; --i)
        data.push_back(2.0 * input.front() - input[static_cast<size_t>(i)]);

    data.insert(data.end(), input.begin(), input.end());

    for (auto i = 1; i <= pad; ++i)
        data.push_back(2.0 * input.back() - input[input.size() - 1 - static_cast<size_t>(i)]);

    sosFilterInPlace(data, sections);
    std::reverse(data.begin(), data.end());
    sosFilterInPlace(data, sections);
    std::reverse(data.begin(), data.end());

    return std::vector<double>(data.begin() + pad, data.begin() + pad + static_cast<std::ptrdiff_t>(input.size()));
}

std::vector<double> bandRms(const std::vector<double>& mono, double sr, const char* bandName, int frame, int hop)
{
    if (const auto* filter = findFilter(sr, bandName))
        return frameRms(sosFiltFilt(mono, filter->sections), frame, hop);

    return frameRms(mono, frame, hop);
}

std::vector<double> zcrFrames(const std::vector<double>& mono, int frame, int hop)
{
    const auto frames = paddedFrameCount(static_cast<int>(mono.size()), frame, hop);
    const auto paddedLength = frames > 0 ? (frames - 1) * hop + frame : 0;
    auto padded = paddedCopy(mono, paddedLength);
    std::vector<double> out(static_cast<size_t>(frames), 0.0);

    for (auto i = 0; i < frames; ++i)
    {
        const auto start = i * hop;
        auto count = 0;
        for (auto j = 1; j < frame; ++j)
        {
            const auto a = padded[static_cast<size_t>(start + j - 1)] < 0.0;
            const auto b = padded[static_cast<size_t>(start + j)] < 0.0;
            if (a != b)
                ++count;
        }
        out[static_cast<size_t>(i)] = static_cast<double>(count) / static_cast<double>(std::max(1, frame - 1));
    }

    return out;
}

void spectralDetailFrames(const std::vector<double>& mono,
                          int sr,
                          int frame,
                          int hop,
                          std::vector<double>& flatness,
                          std::vector<double>& centroid,
                          std::vector<double>& crestDb,
                          std::vector<double>& lowFlatness,
                          std::vector<double>& airFlatness,
                          std::vector<double>& sibilanceLowDensityDb,
                          std::vector<double>& sibilanceMidLowDensityDb,
                          std::vector<double>& sibilanceHigh1DensityDb,
                          std::vector<double>& sibilanceHigh2DensityDb,
                          const QQDeBreathNativeAnalysis::ShouldCancel& shouldCancel)
{
    const auto frames = paddedFrameCount(static_cast<int>(mono.size()), frame, hop);
    const auto paddedLength = frames > 0 ? (frames - 1) * hop + frame : 0;
    auto padded = paddedCopy(mono, paddedLength);
    auto window = hanning(frame);

    auto fftOrder = 0;
    auto fftSize = 1;
    while (fftSize < frame)
    {
        fftSize <<= 1;
        ++fftOrder;
    }

    juce::dsp::FFT fft(fftOrder);
    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);

    const auto bins = fftSize / 2;
    flatness.assign(static_cast<size_t>(frames), 0.0);
    centroid.assign(static_cast<size_t>(frames), 0.0);
    crestDb.assign(static_cast<size_t>(frames), 0.0);
    lowFlatness.assign(static_cast<size_t>(frames), 0.0);
    airFlatness.assign(static_cast<size_t>(frames), 0.0);
    sibilanceLowDensityDb.assign(static_cast<size_t>(frames), safePowerDb(eps));
    sibilanceMidLowDensityDb.assign(static_cast<size_t>(frames), safePowerDb(eps));
    sibilanceHigh1DensityDb.assign(static_cast<size_t>(frames), safePowerDb(eps));
    sibilanceHigh2DensityDb.assign(static_cast<size_t>(frames), safePowerDb(eps));

    for (auto i = 0; i < frames; ++i)
    {
        if (shouldCancel && shouldCancel())
            return;

        std::fill(fftData.begin(), fftData.end(), 0.0f);
        const auto start = i * hop;
        for (auto j = 0; j < frame; ++j)
            fftData[static_cast<size_t>(j)] = static_cast<float>(padded[static_cast<size_t>(start + j)] * window[static_cast<size_t>(j)]);

        fft.performFrequencyOnlyForwardTransform(fftData.data());

        auto sum = 0.0;
        auto logSum = 0.0;
        auto maxMag = 0.0;
        auto weighted = 0.0;
        auto lowSum = 0.0;
        auto lowLogSum = 0.0;
        auto lowCount = 0;
        auto airSum = 0.0;
        auto airLogSum = 0.0;
        auto airCount = 0;
        std::array<double, 4> sibilancePowerSums {};
        std::array<int, 4> sibilancePowerCounts {};

        for (auto bin = 0; bin <= bins; ++bin)
        {
            const auto mag = std::max(static_cast<double>(fftData[static_cast<size_t>(bin)]), eps);
            const auto power = mag * mag;
            const auto freq = static_cast<double>(bin) * sr / static_cast<double>(fftSize);
            sum += mag;
            logSum += std::log(mag);
            maxMag = std::max(maxMag, mag);
            weighted += mag * freq;

            if (freq >= 80.0 && freq <= std::min(2500.0, sr / 2.0))
            {
                lowSum += mag;
                lowLogSum += std::log(mag);
                ++lowCount;
            }

            if (freq >= 2500.0 && freq <= std::min(11000.0, sr / 2.0))
            {
                airSum += mag;
                airLogSum += std::log(mag);
                ++airCount;
            }

            auto densityBand = -1;
            if (freq >= 150.0 && freq < 1200.0)
                densityBand = 0;
            else if (freq >= 1200.0 && freq < 3200.0)
                densityBand = 1;
            else if (freq >= 3200.0 && freq < 5500.0)
                densityBand = 2;
            else if (freq >= 5500.0 && freq <= std::min(10000.0, sr / 2.0))
                densityBand = 3;

            if (densityBand >= 0)
            {
                sibilancePowerSums[static_cast<size_t>(densityBand)] += power;
                ++sibilancePowerCounts[static_cast<size_t>(densityBand)];
            }
        }

        const auto count = static_cast<double>(bins + 1);
        const auto mean = sum / count + eps;
        flatness[static_cast<size_t>(i)] = std::exp(logSum / count) / mean;
        centroid[static_cast<size_t>(i)] = juce::jlimit(0.0, 1.0, (weighted / (sum + eps)) / std::max(1.0, sr / 2.0));
        crestDb[static_cast<size_t>(i)] = safeDb((maxMag + eps) / mean);
        lowFlatness[static_cast<size_t>(i)] = lowCount > 0 ? std::exp(lowLogSum / lowCount) / (lowSum / lowCount + eps)
                                                           : flatness[static_cast<size_t>(i)];
        airFlatness[static_cast<size_t>(i)] = airCount > 0 ? std::exp(airLogSum / airCount) / (airSum / airCount + eps)
                                                           : flatness[static_cast<size_t>(i)];

        auto densityDb = [&] (size_t band)
        {
            const auto countForBand = sibilancePowerCounts[band];
            return countForBand > 0 ? safePowerDb(sibilancePowerSums[band] / static_cast<double>(countForBand))
                                    : safePowerDb(eps);
        };
        sibilanceLowDensityDb[static_cast<size_t>(i)] = densityDb(0);
        sibilanceMidLowDensityDb[static_cast<size_t>(i)] = densityDb(1);
        sibilanceHigh1DensityDb[static_cast<size_t>(i)] = densityDb(2);
        sibilanceHigh2DensityDb[static_cast<size_t>(i)] = densityDb(3);
    }
}
std::vector<double> prepareForAnalysis(const juce::AudioBuffer<float>& audio)
{
    const auto channels = std::max(1, audio.getNumChannels());
    const auto samples = audio.getNumSamples();
    std::vector<double> mono(static_cast<size_t>(samples), 0.0);

    for (auto channel = 0; channel < channels; ++channel)
    {
        std::vector<double> values(static_cast<size_t>(samples), 0.0);
        for (auto sample = 0; sample < samples; ++sample)
        {
            auto value = channel < audio.getNumChannels() ? static_cast<double>(audio.getSample(channel, sample)) : 0.0;
            if (! std::isfinite(value) || std::abs(value) > 32.0)
                value = 0.0;
            values[static_cast<size_t>(sample)] = value;
        }

        const auto median = percentileCopy(values, 50.0);
        for (auto sample = 0; sample < samples; ++sample)
            mono[static_cast<size_t>(sample)] += (values[static_cast<size_t>(sample)] - median) / channels;
    }

    std::vector<double> active;
    active.reserve(mono.size());
    for (auto v : mono)
        if (std::abs(v) > 1.0e-9)
            active.push_back(std::abs(v));

    if (active.empty())
        return mono;

    const auto bodyRef = percentileCopy(active, 90.0);
    const auto normalPeakRef = percentileCopy(active, 98.0);

    if (bodyRef > 1.0e-9)
    {
        const auto limit = std::max({ bodyRef * 7.0, normalPeakRef * 1.8, 0.02 });
        for (auto& v : mono)
            v = std::tanh(v / limit) * limit;

        active.clear();
        for (auto v : mono)
            if (std::abs(v) > 1.0e-9)
                active.push_back(std::abs(v));

        if (active.empty())
            return mono;
    }

    const auto robustPeak = percentileCopy(active, 98.0);
    const auto robustBody = percentileCopy(active, 90.0);
    if (robustPeak <= 1.0e-9)
        return mono;

    const auto gainPeak = 0.55 / robustPeak;
    const auto gainBody = 0.18 / std::max(robustBody, 1.0e-9);
    const auto gain = juce::jlimit(1.0 / 64.0, 64.0, std::max(gainPeak, gainBody));
    for (auto& v : mono)
        v *= gain;

    return mono;
}

std::vector<double> sourceMono(const juce::AudioBuffer<float>& audio)
{
    const auto channels = std::max(1, audio.getNumChannels());
    std::vector<double> mono(static_cast<size_t>(audio.getNumSamples()), 0.0);
    for (auto channel = 0; channel < channels; ++channel)
        for (auto sample = 0; sample < audio.getNumSamples(); ++sample)
            mono[static_cast<size_t>(sample)] += static_cast<double>(audio.getSample(channel, sample)) / channels;
    return mono;
}

Features featuresForAudio(const std::vector<double>& mono,
                          int sr,
                          const QQDeBreathNativeAnalysis::ShouldCancel& shouldCancel)
{
    Features features;
    const auto frame = static_cast<int>(frameMs / 1000.0 * sr);
    features.hop = static_cast<int>(hopMs / 1000.0 * sr);
    const auto hop = features.hop;

    const auto full = frameRms(mono, frame, hop);
    const auto sub = bandRms(mono, sr, "sub", frame, hop);
    const auto low = bandRms(mono, sr, "low", frame, hop);
    const auto body = bandRms(mono, sr, "body", frame, hop);
    const auto presence = bandRms(mono, sr, "presence", frame, hop);
    const auto air = bandRms(mono, sr, "air", frame, hop);
    const auto sib = bandRms(mono, sr, "sib", frame, hop);
    const auto ultra = sr > 22000 ? bandRms(mono, sr, "ultra", frame, hop) : air;

    features.fullDb.resize(full.size(), 0.0);
    for (size_t i = 0; i < full.size(); ++i)
        features.fullDb[i] = safeDb(full[i] + eps);

    const auto refs = frameLevelRefs(features.fullDb);
    std::vector<double> prevStrong;
    std::vector<double> nextStrong;
    distanceToStrong(features.fullDb, hop, sr, refs, prevStrong, nextStrong);

    std::vector<double> flat;
    std::vector<double> centroid;
    std::vector<double> crestDb;
    std::vector<double> lowFlat;
    std::vector<double> airFlat;
    spectralDetailFrames(mono, sr, frame, hop, flat, centroid, crestDb, lowFlat, airFlat,
                         features.sibilanceLowDensityDb,
                         features.sibilanceMidLowDensityDb,
                         features.sibilanceHigh1DensityDb,
                         features.sibilanceHigh2DensityDb,
                         shouldCancel);

    const auto zcr = zcrFrames(mono, frame, hop);
    std::vector<double> diff(mono.size(), 0.0);
    for (size_t i = 1; i < mono.size(); ++i)
        diff[i] = mono[i] - mono[i - 1];
    const auto jitter = frameRms(diff, frame, hop);
    const auto localMeanDb = smoothArray(features.fullDb, static_cast<int>(0.20 / (static_cast<double>(hop) / sr)));

    std::vector<double> fullDelta(features.fullDb.size(), 0.0);
    std::vector<double> airDb(air.size(), 0.0);
    std::vector<double> airDelta(air.size(), 0.0);
    for (size_t i = 0; i < air.size(); ++i)
    {
        airDb[i] = safeDb(air[i] + eps);
        if (i > 0)
        {
            fullDelta[i] = features.fullDb[i] - features.fullDb[i - 1];
            airDelta[i] = airDb[i] - airDb[i - 1];
        }
    }

    const auto n = full.size();
    features.x.resize(n);
    for (size_t i = 0; i < n; ++i)
    {
        features.x[i] = {
            features.fullDb[i],
            safeDb(sub[i] + eps),
            safeDb(low[i] + eps),
            safeDb(body[i] + eps),
            safeDb(presence[i] + eps),
            safeDb(air[i] + eps),
            safeDb(sib[i] + eps),
            safeDb(ultra[i] + eps),
            safeDb((air[i] + eps) / (low[i] + eps)),
            safeDb((body[i] + eps) / (low[i] + eps)),
            safeDb((sib[i] + eps) / (air[i] + eps)),
            safeDb((presence[i] + eps) / (body[i] + eps)),
            safeDb((sub[i] + eps) / (low[i] + eps)),
            i < flat.size() ? flat[i] : 0.0,
            i < zcr.size() ? zcr[i] : 0.0,
            safeDb((i < jitter.size() ? jitter[i] : 0.0) + eps),
            juce::jlimit(-24.0, 24.0, fullDelta[i]),
            juce::jlimit(-24.0, 24.0, airDelta[i]),
            i < localMeanDb.size() ? localMeanDb[i] : features.fullDb[i],
            juce::jlimit(-36.0, 36.0, features.fullDb[i] - (i < localMeanDb.size() ? localMeanDb[i] : features.fullDb[i])),
            std::min(i < prevStrong.size() ? prevStrong[i] : 99.0, 3.0),
            std::min(i < nextStrong.size() ? nextStrong[i] : 99.0, 3.0),
            std::min(std::min(i < prevStrong.size() ? prevStrong[i] : 99.0, i < nextStrong.size() ? nextStrong[i] : 99.0), 3.0),
            i < centroid.size() ? centroid[i] : 0.0,
            juce::jlimit(0.0, 48.0, i < crestDb.size() ? crestDb[i] : 0.0),
            i < lowFlat.size() ? lowFlat[i] : 0.0,
            i < airFlat.size() ? airFlat[i] : 0.0,
            juce::jlimit(-1.0, 1.0, (i < airFlat.size() ? airFlat[i] : 0.0) - (i < lowFlat.size() ? lowFlat[i] : 0.0))
        };
    }

    return features;
}

double predictBreathProbability(const std::array<double, QQDeBreathNativeModel::kFeatureCount>& raw)
{
    std::array<double, QQDeBreathNativeModel::kFeatureCount> x {};
    for (auto i = 0; i < QQDeBreathNativeModel::kFeatureCount; ++i)
    {
        const auto scale = std::abs(QQDeBreathNativeModel::kScale[static_cast<size_t>(i)]) > 1.0e-12
                         ? QQDeBreathNativeModel::kScale[static_cast<size_t>(i)]
                         : 1.0;
        x[static_cast<size_t>(i)] = (raw[static_cast<size_t>(i)] - QQDeBreathNativeModel::kMean[static_cast<size_t>(i)]) / scale;
    }

    auto score = QQDeBreathNativeModel::kBaseline;

    for (const auto& tree : QQDeBreathNativeModel::kTrees)
    {
        auto nodeIndex = 0;
        for (;;)
        {
            const auto& node = tree[static_cast<size_t>(nodeIndex)];
            if (node.isLeaf)
            {
                score += node.value;
                break;
            }

            const auto value = x[static_cast<size_t>(node.feature)];
            const auto goLeft = std::isnan(value) ? node.missingGoesLeft : value <= node.threshold;
            nodeIndex = goLeft ? node.left : node.right;
        }
    }

    return sigmoid(score);
}

std::vector<std::pair<int, int>> contiguousRegions(const std::vector<bool>& mask)
{
    std::vector<std::pair<int, int>> out;
    auto start = -1;
    for (auto i = 0; i < static_cast<int>(mask.size()); ++i)
    {
        if (mask[static_cast<size_t>(i)] && start < 0)
            start = i;
        else if (! mask[static_cast<size_t>(i)] && start >= 0)
        {
            out.emplace_back(start, i);
            start = -1;
        }
    }

    if (start >= 0)
        out.emplace_back(start, static_cast<int>(mask.size()));

    return out;
}

std::vector<std::pair<int, int>> mergeFrameRegions(const std::vector<std::pair<int, int>>& regions, int maxGap)
{
    if (regions.empty())
        return {};

    std::vector<std::pair<int, int>> out;
    out.push_back(regions.front());

    for (size_t i = 1; i < regions.size(); ++i)
    {
        if (regions[i].first - out.back().second <= maxGap)
            out.back().second = regions[i].second;
        else
            out.push_back(regions[i]);
    }

    return out;
}

std::vector<Region> probabilityToRegions(const std::vector<double>& probability, int sr, int hop, double threshold)
{
    const auto smoothed = smoothProbability(probability, 11);
    std::vector<bool> mask(smoothed.size(), false);
    for (size_t i = 0; i < smoothed.size(); ++i)
        mask[i] = smoothed[i] >= threshold;

    const auto maxGap = static_cast<int>(0.14 / (static_cast<double>(hop) / sr));
    const auto frameRegions = mergeFrameRegions(contiguousRegions(mask), maxGap);

    std::vector<Region> out;
    for (const auto& [aFrame, bFrame] : frameRegions)
    {
        const auto start = std::max<juce::int64>(0, static_cast<juce::int64>(aFrame) * hop);
        const auto end = static_cast<juce::int64>(bFrame) * hop + static_cast<int>(frameMs / 1000.0 * sr);
        const auto duration = static_cast<double>(end - start) / sr;

        if (duration >= 0.12 && duration <= 1.35)
        {
            auto confidence = 0.0;
            if (bFrame > aFrame)
            {
                for (auto i = aFrame; i < bFrame; ++i)
                    confidence += smoothed[static_cast<size_t>(i)];
                confidence /= static_cast<double>(bFrame - aFrame);
            }
            out.push_back({ static_cast<double>(start) / sr, static_cast<double>(end) / sr, "Breath", confidence });
        }
    }

    std::vector<Region> filtered;
    for (const auto& item : out)
    {
        if (! filtered.empty())
        {
            const auto gap = item.start - filtered.back().end;
            const auto dur = item.end - item.start;
            const auto prevDur = filtered.back().end - filtered.back().start;
            if (gap < 1.0 && dur < 0.10 && item.confidence < filtered.back().confidence * 0.60 && prevDur >= 0.24)
                continue;
        }
        filtered.push_back(item);
    }

    return filtered;
}

std::vector<Region> spectralBreathRegions(const Features& features,
                                          const std::vector<double>& probability,
                                          int sr,
                                          int sampleCount)
{
    const auto refs = frameLevelRefs(features.fullDb);
    std::vector<bool> mask(probability.size(), false);

    for (size_t i = 0; i < probability.size(); ++i)
    {
        const auto& x = features.x[i];
        const auto airLow = x[8];
        const auto flat = x[13];
        const auto zcr = x[14];
        const auto edgeDistance = x[22];
        const auto voicedLow = x[9];
        const auto fullDb = features.fullDb[i];
        const auto prob = probability[i];

        const auto airy = airLow > -2.5
                       && airLow < 6.0
                       && flat > 0.16
                       && zcr > 0.075
                       && voicedLow < 8.0
                       && fullDb > refs.airyMin
                       && fullDb < refs.airyMax
                       && prob > 0.72;

        const auto nearVoiceBreath = airLow > 1.5
                                  && airLow < 10.5
                                  && flat > 0.13
                                  && zcr > 0.055
                                  && prob > 0.86
                                  && fullDb > refs.nearMin
                                  && fullDb < refs.nearMax
                                  && edgeDistance < 0.75;

        mask[i] = airy || nearVoiceBreath;
    }

    const auto maxGap = static_cast<int>(0.10 / (static_cast<double>(features.hop) / sr));
    const auto frameRegions = mergeFrameRegions(contiguousRegions(mask), maxGap);
    std::vector<Region> out;

    for (const auto& [aFrame, bFrame] : frameRegions)
    {
        const auto start = std::max(0.0, static_cast<double>(aFrame * features.hop) / sr);
        const auto end = std::min(static_cast<double>(sampleCount) / sr,
                                  static_cast<double>(bFrame * features.hop + static_cast<int>(frameMs / 1000.0 * sr)) / sr);
        const auto duration = end - start;

        if (duration >= 0.12 && duration <= 1.45)
        {
            auto confidence = 0.0;
            if (bFrame > aFrame)
            {
                for (auto i = aFrame; i < bFrame; ++i)
                    confidence += probability[static_cast<size_t>(i)];
                confidence /= static_cast<double>(bFrame - aFrame);
            }
            out.push_back({ start, end, "Breath", confidence });
        }
    }

    return out;
}

struct RobustReference
{
    double median = 0.0;
    double scale = 1.0;
    bool valid = false;

    double z(double value) const
    {
        if (! valid || ! std::isfinite(value))
            return 0.0;
        return juce::jlimit(-8.0, 8.0, (value - median) / scale);
    }
};

RobustReference robustReference(const std::vector<double>& values)
{
    RobustReference reference;
    std::vector<double> finite;
    finite.reserve(values.size());
    for (const auto value : values)
        if (std::isfinite(value))
            finite.push_back(value);

    if (finite.size() < 24)
        return reference;

    reference.median = percentileCopy(finite, 50.0);
    std::vector<double> deviations;
    deviations.reserve(finite.size());
    for (const auto value : finite)
        deviations.push_back(std::abs(value - reference.median));

    const auto madScale = 1.4826 * percentileCopy(deviations, 50.0);
    const auto centralScale = (percentileCopy(finite, 90.0) - percentileCopy(finite, 10.0)) / 2.563;
    reference.scale = madScale > 1.0e-6 ? madScale : centralScale;
    reference.valid = std::isfinite(reference.scale) && reference.scale > 1.0e-6;
    return reference;
}

std::vector<double> movingMeanMasked(const std::vector<double>& values,
                                     const std::vector<bool>& mask,
                                     int width,
                                     double fallback)
{
    if (values.empty())
        return {};

    const auto count = static_cast<int>(values.size());
    width = std::max(1, width);
    const auto left = width / 2;
    const auto right = width - 1 - left;
    std::vector<double> valuePrefix(values.size() + 1, 0.0);
    std::vector<int> countPrefix(values.size() + 1, 0);
    for (auto i = 0; i < count; ++i)
    {
        const auto valid = i < static_cast<int>(mask.size()) && mask[static_cast<size_t>(i)]
                        && std::isfinite(values[static_cast<size_t>(i)]);
        valuePrefix[static_cast<size_t>(i + 1)] = valuePrefix[static_cast<size_t>(i)]
                                                + (valid ? values[static_cast<size_t>(i)] : 0.0);
        countPrefix[static_cast<size_t>(i + 1)] = countPrefix[static_cast<size_t>(i)] + (valid ? 1 : 0);
    }

    std::vector<double> out(values.size(), fallback);
    for (auto i = 0; i < count; ++i)
    {
        const auto start = std::max(0, i - left);
        const auto end = std::min(count, i + right + 1);
        const auto validCount = countPrefix[static_cast<size_t>(end)] - countPrefix[static_cast<size_t>(start)];
        if (validCount > 0)
            out[static_cast<size_t>(i)] = (valuePrefix[static_cast<size_t>(end)] - valuePrefix[static_cast<size_t>(start)])
                                        / static_cast<double>(validCount);
    }
    return out;
}

double powerAverageDb(double aDb, double bDb)
{
    const auto a = std::pow(10.0, juce::jlimit(-300.0, 300.0, aDb) / 10.0);
    const auto b = std::pow(10.0, juce::jlimit(-300.0, 300.0, bDb) / 10.0);
    return safePowerDb((a + b) * 0.5);
}

bool frameTouchesRegion(int frameIndex,
                        int hop,
                        int sr,
                        const std::vector<Region>& regions,
                        double guardSeconds)
{
    const auto start = static_cast<double>(frameIndex * hop) / sr;
    const auto end = start + frameMs / 1000.0;
    for (const auto& region : regions)
    {
        if (! region.type.equalsIgnoreCase("Breath"))
            continue;
        if (end > region.start - guardSeconds && start < region.end + guardSeconds)
            return true;
    }
    return false;
}

std::vector<Region> adaptiveSibilanceRegions(const Features& features,
                                             const std::vector<Region>& breathRegions,
                                             int sr,
                                             int sampleCount)
{
    const auto frameCount = static_cast<int>(features.x.size());
    if (frameCount < 24 || features.hop <= 0
        || features.sibilanceLowDensityDb.size() != features.x.size()
        || features.sibilanceMidLowDensityDb.size() != features.x.size()
        || features.sibilanceHigh1DensityDb.size() != features.x.size()
        || features.sibilanceHigh2DensityDb.size() != features.x.size())
        return {};

    const auto refs = frameLevelRefs(features.fullDb);
    const auto activeFloor = refs.floor + refs.dynamic * 0.10;
    std::vector<bool> eligible(static_cast<size_t>(frameCount), false);
    std::vector<double> high1ToMid(static_cast<size_t>(frameCount), 0.0);
    std::vector<double> high2ToMid(static_cast<size_t>(frameCount), 0.0);
    std::vector<double> combinedRatio(static_cast<size_t>(frameCount), 0.0);
    std::vector<double> combinedHighDb(static_cast<size_t>(frameCount), 0.0);
    std::vector<double> centroid(static_cast<size_t>(frameCount), 0.0);
    std::vector<double> zcr(static_cast<size_t>(frameCount), 0.0);
    std::vector<double> airFlatness(static_cast<size_t>(frameCount), 0.0);

    std::vector<double> calibrationHigh1ToMid;
    std::vector<double> calibrationHigh2ToMid;
    std::vector<double> calibrationCombinedRatio;
    std::vector<double> calibrationHighDb;
    std::vector<double> calibrationCentroid;
    std::vector<double> calibrationZcr;
    std::vector<double> calibrationFlatness;

    for (auto i = 0; i < frameCount; ++i)
    {
        const auto index = static_cast<size_t>(i);
        const auto& x = features.x[index];
        const auto lowDb = features.sibilanceLowDensityDb[index];
        const auto midLowDb = features.sibilanceMidLowDensityDb[index];
        const auto high1Db = features.sibilanceHigh1DensityDb[index];
        const auto high2Db = features.sibilanceHigh2DensityDb[index];
        const auto lowReferenceDb = powerAverageDb(lowDb, midLowDb);
        combinedHighDb[index] = powerAverageDb(high1Db, high2Db);
        high1ToMid[index] = high1Db - midLowDb;
        high2ToMid[index] = high2Db - midLowDb;
        combinedRatio[index] = combinedHighDb[index] - lowReferenceDb;
        centroid[index] = x[23];
        zcr[index] = x[14];
        airFlatness[index] = x[26];

        const auto outsideBreath = ! frameTouchesRegion(i, features.hop, sr, breathRegions, 0.012);
        const auto relativelyActive = features.fullDb[index] >= activeFloor;
        eligible[index] = outsideBreath && relativelyActive;
        if (! eligible[index])
            continue;

        calibrationHigh1ToMid.push_back(high1ToMid[index]);
        calibrationHigh2ToMid.push_back(high2ToMid[index]);
        calibrationCombinedRatio.push_back(combinedRatio[index]);
        calibrationHighDb.push_back(combinedHighDb[index]);
        calibrationCentroid.push_back(centroid[index]);
        calibrationZcr.push_back(zcr[index]);
        calibrationFlatness.push_back(airFlatness[index]);
    }

    if (calibrationCombinedRatio.size() < 48)
        return {};

    const auto high1Reference = robustReference(calibrationHigh1ToMid);
    const auto high2Reference = robustReference(calibrationHigh2ToMid);
    const auto combinedReference = robustReference(calibrationCombinedRatio);
    const auto highEnergyReference = robustReference(calibrationHighDb);
    const auto centroidReference = robustReference(calibrationCentroid);
    const auto zcrReference = robustReference(calibrationZcr);
    const auto flatnessReference = robustReference(calibrationFlatness);
    if (! high1Reference.valid || ! high2Reference.valid || ! combinedReference.valid
        || ! highEnergyReference.valid)
        return {};

    std::vector<double> ratioEvidence(static_cast<size_t>(frameCount), 0.0);
    for (auto i = 0; i < frameCount; ++i)
    {
        const auto index = static_cast<size_t>(i);
        ratioEvidence[index] = std::max({ high1Reference.z(high1ToMid[index]),
                                          high2Reference.z(high2ToMid[index]),
                                          combinedReference.z(combinedRatio[index]) });
    }

    const auto localWindow = std::max(3, static_cast<int>(std::llround(0.40 * sr / features.hop)));
    const auto ratioFallback = combinedReference.z(combinedReference.median);
    const auto localBase = movingMeanMasked(ratioEvidence, eligible, localWindow, ratioFallback);
    std::vector<double> localContrast(static_cast<size_t>(frameCount), 0.0);
    std::vector<double> calibrationLocalContrast;
    calibrationLocalContrast.reserve(calibrationCombinedRatio.size());
    for (auto i = 0; i < frameCount; ++i)
    {
        const auto index = static_cast<size_t>(i);
        localContrast[index] = ratioEvidence[index] - localBase[index];
        if (eligible[index])
            calibrationLocalContrast.push_back(localContrast[index]);
    }
    const auto localReference = robustReference(calibrationLocalContrast);
    if (! localReference.valid)
        return {};

    std::vector<double> score(static_cast<size_t>(frameCount), 0.0);
    std::vector<bool> strong(static_cast<size_t>(frameCount), false);
    std::vector<bool> weak(static_cast<size_t>(frameCount), false);
    for (auto i = 0; i < frameCount; ++i)
    {
        const auto index = static_cast<size_t>(i);
        if (! eligible[index])
            continue;

        const auto high1Z = high1Reference.z(high1ToMid[index]);
        const auto high2Z = high2Reference.z(high2ToMid[index]);
        const auto combinedZ = combinedReference.z(combinedRatio[index]);
        const auto ratioZ = std::max({ high1Z, high2Z, combinedZ });
        const auto highEnergyZ = highEnergyReference.z(combinedHighDb[index]);
        const auto localZ = localReference.z(localContrast[index]);
        const auto spectralSupport = juce::jlimit(-2.0, 4.0,
                                                   0.45 * centroidReference.z(centroid[index])
                                                 + 0.35 * zcrReference.z(zcr[index])
                                                 + 0.20 * flatnessReference.z(airFlatness[index]));
        const auto relativeScore = 0.70 * ratioZ
                                 + 0.15 * localZ
                                 + 0.10 * highEnergyZ
                                 + 0.05 * spectralSupport;

        score[index] = relativeScore;
        const auto hasBandRatioPeak = std::max(high1Z, high2Z) >= 2.00;
        strong[index] = ratioZ >= 2.50
                     && relativeScore >= 2.05
                     && localZ >= 0.15
                     && highEnergyZ >= -0.50
                     && hasBandRatioPeak;
        weak[index] = ratioZ >= 0.75
                   && relativeScore >= 0.65
                   && localZ >= -0.65
                   && highEnergyZ >= -1.00;
    }

    auto weakRuns = contiguousRegions(weak);
    std::vector<std::pair<int, int>> acceptedRuns;
    for (const auto& run : weakRuns)
    {
        auto hasStrongFrame = false;
        for (auto i = run.first; i < run.second; ++i)
            hasStrongFrame = hasStrongFrame || strong[static_cast<size_t>(i)];
        if (hasStrongFrame)
            acceptedRuns.push_back(run);
    }

    acceptedRuns = mergeFrameRegions(acceptedRuns,
                                      std::max(1, static_cast<int>(std::llround(0.012 * sr / features.hop))));

    std::vector<Region> out;
    const auto duration = static_cast<double>(sampleCount) / sr;
    for (const auto& run : acceptedRuns)
    {
        const auto start = std::max(0.0, static_cast<double>(run.first * features.hop) / sr - 0.008);
        const auto end = std::min(duration,
                                  static_cast<double>(run.second * features.hop) / sr + frameMs / 1000.0 + 0.008);
        if (end - start < 0.025 || end - start > 0.80)
            continue;

        auto confidence = 0.0;
        for (auto i = run.first; i < run.second; ++i)
            confidence += sigmoid(score[static_cast<size_t>(i)] - 0.75);
        confidence /= std::max(1, run.second - run.first);
        out.push_back({ start, end, "Sibilance", confidence });
    }
    return out;
}
std::vector<Region> subtractBreathRegions(const std::vector<Region>& candidates,
                                          const std::vector<Region>& breathRegions)
{
    std::vector<Region> out;
    constexpr auto guard = 0.008;
    constexpr auto minimumDuration = 0.025;

    for (const auto& candidate : candidates)
    {
        std::vector<Region> pieces { candidate };
        for (const auto& breath : breathRegions)
        {
            if (! breath.type.equalsIgnoreCase("Breath"))
                continue;

            std::vector<Region> next;
            const auto blockStart = breath.start - guard;
            const auto blockEnd = breath.end + guard;
            for (const auto& piece : pieces)
            {
                if (piece.end <= blockStart || piece.start >= blockEnd)
                {
                    next.push_back(piece);
                    continue;
                }

                if (blockStart - piece.start >= minimumDuration)
                {
                    auto left = piece;
                    left.end = blockStart;
                    next.push_back(left);
                }
                if (piece.end - blockEnd >= minimumDuration)
                {
                    auto right = piece;
                    right.start = blockEnd;
                    next.push_back(right);
                }
            }
            pieces = std::move(next);
            if (pieces.empty())
                break;
        }
        out.insert(out.end(), pieces.begin(), pieces.end());
    }
    return out;
}

std::vector<Region> mergeTimeRegions(std::vector<Region> regions, double maxGap)
{
    regions.erase(std::remove_if(regions.begin(), regions.end(), [] (const Region& r) { return r.end <= r.start; }), regions.end());
    std::sort(regions.begin(), regions.end(), [] (const Region& a, const Region& b)
    {
        if (a.start != b.start)
            return a.start < b.start;
        return a.end < b.end;
    });

    if (regions.empty())
        return {};

    std::vector<Region> out;
    out.push_back(regions.front());

    for (size_t i = 1; i < regions.size(); ++i)
    {
        if (regions[i].type == out.back().type && regions[i].start - out.back().end <= maxGap)
        {
            out.back().end = std::max(out.back().end, regions[i].end);
            out.back().confidence = std::max(out.back().confidence, regions[i].confidence);
        }
        else
        {
            out.push_back(regions[i]);
        }
    }

    return out;
}

std::vector<double> boundaryScoreCurve(const std::vector<double>& mono, int sr)
{
    std::vector<double> absMono(mono.size(), 0.0);
    for (size_t i = 0; i < mono.size(); ++i)
        absMono[i] = std::abs(mono[i]);

    const auto rmsWindow = std::max(8, static_cast<int>(std::llround(10.0 / 1000.0 * sr)));
    std::vector<double> square(mono.size(), 0.0);
    for (size_t i = 0; i < mono.size(); ++i)
        square[i] = mono[i] * mono[i];
    auto rmsMean = movingMeanEdge(square, rmsWindow);
    for (auto& v : rmsMean)
        v = std::sqrt(v + 1.0e-20);

    const auto meanAbs = movingMeanEdge(absMono, static_cast<int>(std::llround(10.0 / 1000.0 * sr)));
    const auto peak = maximumFilterNearest(absMono, std::max(3, static_cast<int>(std::llround(14.0 / 1000.0 * sr))));

    std::vector<double> motion(mono.size(), 0.0);
    for (size_t i = 1; i < mono.size(); ++i)
        motion[i] = std::abs(mono[i] - mono[i - 1]);
    motion = movingMeanEdge(motion, static_cast<int>(std::llround(10.0 / 1000.0 * sr)));

    auto slope = gradient(rmsMean);
    for (auto& v : slope)
        v = std::abs(v);
    slope = movingMeanEdge(slope, static_cast<int>(std::llround(12.0 / 1000.0 * sr)));

    auto energyDb = rmsMean;
    auto peakDb = peak;
    auto meanDb = meanAbs;
    auto motionDb = motion;
    for (auto& v : energyDb) v = safeDb(v + eps);
    for (auto& v : peakDb) v = safeDb(v + eps);
    for (auto& v : meanDb) v = safeDb(v + eps);
    for (auto& v : motionDb) v = safeDb(v + eps);

    const auto energyN = normalize01(energyDb);
    const auto peakN = normalize01(peakDb);
    const auto meanN = normalize01(meanDb);
    const auto motionN = normalize01(motionDb);
    const auto slopeN = normalize01(slope);

    std::vector<double> score(mono.size(), 0.0);
    for (size_t i = 0; i < score.size(); ++i)
        score[i] = 0.34 * energyN[i] + 0.25 * peakN[i] + 0.18 * meanN[i] + 0.16 * motionN[i] + 0.07 * slopeN[i];

    return movingMeanEdge(score, static_cast<int>(std::llround(8.0 / 1000.0 * sr)));
}

double snapTimeToStablePoint(const std::vector<double>& score, int sr, double timeSeconds, double searchMs)
{
    if (score.empty())
        return timeSeconds;

    const auto center = static_cast<int>(std::llround(timeSeconds * sr));
    const auto radius = std::max(1, static_cast<int>(std::llround(searchMs / 1000.0 * sr)));
    const auto a = juce::jlimit(0, static_cast<int>(score.size()), center - radius);
    const auto b = juce::jlimit(0, static_cast<int>(score.size()), center + radius + 1);

    if (b <= a)
        return timeSeconds;

    auto local = std::vector<double>(score.begin() + a, score.begin() + b);
    local = normalize01(local);

    auto bestValue = std::numeric_limits<double>::max();
    auto bestIndex = 0;
    for (auto i = 0; i < static_cast<int>(local.size()); ++i)
    {
        const auto position = a + i;
        const auto distance = std::abs(position - center) / static_cast<double>(std::max(1, radius));
        const auto combined = 0.92 * local[static_cast<size_t>(i)] + 0.08 * distance;
        if (combined < bestValue)
        {
            bestValue = combined;
            bestIndex = i;
        }
    }

    return static_cast<double>(a + bestIndex) / sr;
}

std::vector<Region> snapRegionsToLowPoints(const std::vector<Region>& regions,
                                           const std::vector<double>& source,
                                           int sr)
{
    if (regions.empty())
        return regions;

    const auto score = boundaryScoreCurve(source, sr);
    std::vector<Region> out;
    out.reserve(regions.size());

    for (auto region : regions)
    {
        const auto duration = std::max(0.0, region.end - region.start);
        auto search = region.type.equalsIgnoreCase("Breath") ? 180.0 : 140.0;
        search = std::min(search, std::max(90.0, duration * 450.0));
        const auto start = snapTimeToStablePoint(score, sr, region.start, search);
        const auto end = snapTimeToStablePoint(score, sr, region.end, search);

        if (end - start >= 0.04)
        {
            region.start = start;
            region.end = end;
        }

        out.push_back(region);
    }

    return out;
}

bool keepAutoBreathRegion(const Region& region, const std::vector<double>& absMono, int sr)
{
    const auto duration = region.end - region.start;
    if (duration < autoBreathMinSeconds)
        return false;

    if (duration < autoBreathShortReviewSeconds && region.confidence >= 0.93)
        return true;

    if (duration >= autoBreathShortReviewSeconds)
        return true;

    const auto a = static_cast<int>(std::llround(region.start * sr));
    const auto b = static_cast<int>(std::llround(region.end * sr));
    const auto segmentPeak = safeDb(percentileRange(absMono, a, b, 95.0) + eps);

    const auto radius = std::max(1, static_cast<int>(std::llround(18.0 / 1000.0 * sr / 2.0)));
    const auto startEdge = safeDb(percentileRange(absMono, a - radius, a + radius, 95.0) + eps);
    const auto endEdge = safeDb(percentileRange(absMono, b - radius, b + radius, 95.0) + eps);
    return std::max(startEdge, endEdge) <= segmentPeak - 1.5;
}

std::vector<Region> filterAutoBreathRegions(const std::vector<Region>& regions,
                                            const std::vector<double>& analysisMono,
                                            int sr)
{
    if (regions.empty())
        return regions;

    std::vector<double> absMono(analysisMono.size(), 0.0);
    for (size_t i = 0; i < analysisMono.size(); ++i)
        absMono[i] = std::abs(analysisMono[i]);

    std::vector<Region> out;
    for (const auto& region : regions)
    {
        if (! region.type.equalsIgnoreCase("Breath") || keepAutoBreathRegion(region, absMono, sr))
            out.push_back(region);
    }
    return out;
}

int regionPriority(const juce::String& type)
{
    if (type.equalsIgnoreCase("Breath"))
        return 3;
    if (type.equalsIgnoreCase("Sibilance"))
        return 2;
    if (type.equalsIgnoreCase("Noize"))
        return 1;
    return 0;
}

std::vector<Region> normalizeRegions(std::vector<Region> regions, double duration)
{
    std::vector<Region> clean;
    for (auto region : regions)
    {
        region.start = juce::jlimit(0.0, duration, region.start);
        region.end = juce::jlimit(0.0, duration, region.end);
        region.type = region.type.equalsIgnoreCase("Noize") ? "Noize"
                    : region.type.equalsIgnoreCase("Sibilance") ? "Sibilance"
                    : "Breath";
        if (region.end - region.start >= 0.005)
            clean.push_back(region);
    }

    std::sort(clean.begin(), clean.end(), [] (const Region& a, const Region& b)
    {
        if (a.start != b.start)
            return a.start < b.start;
        return regionPriority(a.type) > regionPriority(b.type);
    });

    std::vector<Region> out;
    for (auto region : clean)
    {
        if (out.empty() || region.start >= out.back().end)
        {
            out.push_back(region);
        }
        else if (regionPriority(region.type) >= regionPriority(out.back().type))
        {
            out.back().end = std::min(out.back().end, region.start);
            if (out.back().end - out.back().start < 0.005)
                out.pop_back();
            out.push_back(region);
        }
        else if (region.end > out.back().end)
        {
            region.start = out.back().end;
            out.push_back(region);
        }
    }

    std::vector<Region> finalRegions;
    for (const auto& region : out)
        if (region.end - region.start >= 0.005 && ! region.type.equalsIgnoreCase("Vocal Only"))
            finalRegions.push_back(region);

    return finalRegions;
}

juce::AudioBuffer<float> readAudioFile(const juce::File& file, double& sampleRate, juce::String& error)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(file));
    if (reader == nullptr)
    {
        error = "Could not read input wav: " + file.getFullPathName();
        return {};
    }

    if (reader->lengthInSamples > std::numeric_limits<int>::max())
    {
        error = "Input is too long for the native analyzer buffer.";
        return {};
    }

    juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels),
                                    static_cast<int>(reader->lengthInSamples));
    if (! reader->read(&buffer, 0, buffer.getNumSamples(), 0, true, true))
    {
        error = "Failed reading input samples.";
        return {};
    }

    sampleRate = reader->sampleRate;
    return buffer;
}

juce::String resultToJson(const QQDeBreathBridgeAnalysisResult& result, const QQDeBreathBridgeAnalysisConfig& config)
{
    auto root = new juce::DynamicObject();
    root->setProperty("app", "QQEasyTool");
    root->setProperty("schema_version", 1);
    root->setProperty("algorithm_version", "cpp_native_breath_only_0_95");
    root->setProperty("status", result.succeeded ? "ok" : "error");

    auto input = new juce::DynamicObject();
    input->setProperty("path", config.inputWav.getFullPathName());
    input->setProperty("sample_rate", result.sampleRate);
    input->setProperty("channels", result.channels);
    input->setProperty("num_samples", static_cast<double>(result.numSamples));
    input->setProperty("duration", result.durationSeconds);
    root->setProperty("input", juce::var(input));

    auto model = new juce::DynamicObject();
    model->setProperty("type", "embedded_cpp_hist_gradient_boosting");
    model->setProperty("sha256", QQDeBreathNativeModel::kModelSha256);
    root->setProperty("model", juce::var(model));

    auto params = new juce::DynamicObject();
    params->setProperty("threshold", config.threshold);
    params->setProperty("fade_ms", config.fadeMs);
    params->setProperty("breath_target_db", config.breathTargetDb);
    params->setProperty("breath_gain_db", config.breathGainDb);
    params->setProperty("output_breath_regions", config.outputBreathRegions);
    params->setProperty("output_sibilance_regions", config.outputSibilanceRegions);
    params->setProperty("detect_noize", config.detectNoize);
    root->setProperty("params", juce::var(params));

    juce::Array<juce::var> regions;
    auto id = 1;
    for (const auto& region : result.regions)
    {
        auto object = new juce::DynamicObject();
        object->setProperty("id", id++);
        object->setProperty("type", region.type);
        object->setProperty("start_time", region.startTime);
        object->setProperty("end_time", region.endTime);
        object->setProperty("start_sample", static_cast<double>(region.startSample));
        object->setProperty("end_sample", static_cast<double>(region.endSample));
        regions.add(juce::var(object));
    }
    root->setProperty("regions", regions);

    return juce::JSON::toString(juce::var(root), true);
}

QQDeBreathBridgeAnalysisResult cancelledResult(const juce::String& sourceKey)
{
    QQDeBreathBridgeAnalysisResult result;
    result.hasResult = true;
    result.cancelled = true;
    result.succeeded = false;
    result.sourceKey = sourceKey;
    result.status = "Analysis cancelled.";
    return result;
}
} // namespace

QQDeBreathBridgeAnalysisResult QQDeBreathNativeAnalysis::run(const QQDeBreathBridgeAnalysisConfig& config,
                                                             const ShouldCancel& shouldCancel)
{
    QQDeBreathBridgeAnalysisResult result;
    result.hasResult = true;
    result.sourceKey = config.sourceKey;
    result.status = "Native C++ analysis started.";
    result.schemaVersion = 1;
    result.resultJson = config.outputJson;

    if (shouldCancel && shouldCancel())
        return cancelledResult(config.sourceKey);

    if (! config.inputWav.existsAsFile())
    {
        result.status = "Native C++ analysis failed.";
        result.errorMessage = "Input wav does not exist: " + config.inputWav.getFullPathName();
        return result;
    }

    juce::String error;
    auto sampleRate = 0.0;
    auto audio = readAudioFile(config.inputWav, sampleRate, error);
    if (audio.getNumSamples() <= 0 || sampleRate <= 0.0)
    {
        result.status = "Native C++ analysis failed.";
        result.errorMessage = error.isNotEmpty() ? error : "Input audio is empty.";
        return result;
    }

    result.sampleRate = static_cast<int>(std::llround(sampleRate));
    result.channels = audio.getNumChannels();
    result.numSamples = audio.getNumSamples();
    result.durationSeconds = static_cast<double>(audio.getNumSamples()) / sampleRate;

    if (shouldCancel && shouldCancel())
        return cancelledResult(config.sourceKey);

    const auto analysisMono = prepareForAnalysis(audio);
    const auto originalMono = sourceMono(audio);

    if (shouldCancel && shouldCancel())
        return cancelledResult(config.sourceKey);

    const auto features = featuresForAudio(analysisMono, result.sampleRate, shouldCancel);

    if (shouldCancel && shouldCancel())
        return cancelledResult(config.sourceKey);

    std::vector<double> probability(features.x.size(), 0.0);
    for (size_t i = 0; i < features.x.size(); ++i)
    {
        if ((i & 1023u) == 0u && shouldCancel && shouldCancel())
            return cancelledResult(config.sourceKey);
        probability[i] = predictBreathProbability(features.x[i]);
    }

    auto breathRegions = probabilityToRegions(probability, result.sampleRate, features.hop, config.threshold);
    auto spectral = spectralBreathRegions(features, probability, result.sampleRate, audio.getNumSamples());
    breathRegions.insert(breathRegions.end(), spectral.begin(), spectral.end());
    breathRegions = mergeTimeRegions(breathRegions, 0.08);
    breathRegions = snapRegionsToLowPoints(breathRegions, originalMono, result.sampleRate);
    breathRegions = filterAutoBreathRegions(breathRegions, analysisMono, result.sampleRate);
    breathRegions = normalizeRegions(breathRegions, result.durationSeconds);

    // QQEasyTool 0.95 is intentionally Breath-only. The 0.94 Sibilance detector
    // remains frozen in the dedicated 0.94 source backup for future training work.
    std::vector<Region> regions;
    if (config.outputBreathRegions)
        regions.insert(regions.end(), breathRegions.begin(), breathRegions.end());
    regions = normalizeRegions(regions, result.durationSeconds);

    for (const auto& region : regions)
    {
        QQDeBreathBridgeRegion bridgeRegion;
        bridgeRegion.type = region.type;
        bridgeRegion.startTime = region.start;
        bridgeRegion.endTime = region.end;
        bridgeRegion.startSample = static_cast<juce::int64>(std::llround(region.start * sampleRate));
        bridgeRegion.endSample = static_cast<juce::int64>(std::llround(region.end * sampleRate));
        bridgeRegion.startSample = juce::jlimit<juce::int64>(0, result.numSamples, bridgeRegion.startSample);
        bridgeRegion.endSample = juce::jlimit<juce::int64>(bridgeRegion.startSample, result.numSamples, bridgeRegion.endSample);
        result.regions.add(bridgeRegion);

        if (bridgeRegion.type.equalsIgnoreCase("Breath"))
            ++result.breathCount;
        else if (bridgeRegion.type.equalsIgnoreCase("Noize"))
            ++result.noizeCount;
        else if (bridgeRegion.type.equalsIgnoreCase("Sibilance"))
            ++result.sibilanceCount;
    }

    result.succeeded = true;
    result.status = "Native C++ analysis complete.";

    if (config.outputJson != juce::File{})
    {
        config.outputJson.getParentDirectory().createDirectory();
        config.outputJson.replaceWithText(resultToJson(result, config), false, false, "\n");
    }

    return result;
}
