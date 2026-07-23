#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <array>
#include <vector>

struct QQDeBreathEqBand
{
    bool enabled = false;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
};

struct QQDeBreathEqState
{
    static constexpr int maxBands = 6;

    bool enabled = false;
    bool bypassed = false;
    bool highPassEnabled = false;
    bool lowPassEnabled = false;
    double highPassHz = 80.0;
    double highPassQ = 0.70710678118;
    int highPassSlopeDbPerOct = 12;
    double lowPassHz = 12000.0;
    double lowPassQ = 0.70710678118;
    int lowPassSlopeDbPerOct = 12;
    std::array<QQDeBreathEqBand, maxBands> bands {};

    bool hasActiveProcessing() const;
    int enabledBandCount() const;
};

QQDeBreathEqState sanitizeBreathEqState(QQDeBreathEqState state);
juce::String serializeBreathEqState(const QQDeBreathEqState& state);
bool deserializeBreathEqState(const juce::String& text, QQDeBreathEqState& state);
double breathEqMagnitudeDbAt(const QQDeBreathEqState& state, double frequencyHz, double sampleRate);

class QQDeBreathEqProcessor
{
public:
    struct Coefficients
    {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
    };

    void prepare(double sampleRate, int channels, const QQDeBreathEqState& state);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();
    static Coefficients makeHighPass(double sampleRate, double frequencyHz, double q);
    static Coefficients makeLowPass(double sampleRate, double frequencyHz, double q);
    static Coefficients makePeak(double sampleRate, double frequencyHz, double gainDb, double q);
    static double magnitudeDbAt(const Coefficients& coefficients, double frequencyHz, double sampleRate);

private:
    struct BiquadState
    {
        double z1 = 0.0;
        double z2 = 0.0;
    };

    static bool statesEqual(const QQDeBreathEqState& a, const QQDeBreathEqState& b);

    double currentSampleRate = 0.0;
    int currentChannels = 0;
    QQDeBreathEqState currentState;
    bool hasCurrentState = false;
    std::vector<Coefficients> filters;
    std::vector<std::vector<BiquadState>> channelStates;
};
