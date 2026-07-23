#include "shared/NativeAnalysis.h"

#include <juce_core/juce_core.h>

namespace
{
juce::String argValue(const juce::StringArray& args, const juce::String& name, const juce::String& fallback = {})
{
    const auto index = args.indexOf(name);
    if (index >= 0 && index + 1 < args.size())
        return args[index + 1];
    return fallback;
}

int printUsage()
{
    juce::Logger::outputDebugString("Usage: qq_debreath_native_probe --input input.wav --output-json result.json [--threshold 0.86]");
    return 2;
}
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    juce::ignoreUnused(argc);

    juce::StringArray args;
    for (auto i = 1; argv[i] != nullptr; ++i)
        args.add(juce::String(argv[i]));

    const auto input = argValue(args, "--input");
    const auto outputJson = argValue(args, "--output-json");
    if (input.isEmpty() || outputJson.isEmpty())
        return printUsage();

    QQDeBreathBridgeAnalysisConfig config;
    config.inputWav = juce::File(input);
    config.outputJson = juce::File(outputJson);
    config.outDir = config.outputJson.getParentDirectory();
    config.sourceKey = config.inputWav.getFileNameWithoutExtension();
    config.threshold = argValue(args, "--threshold", "0.86").getDoubleValue();

    const auto result = QQDeBreathNativeAnalysis::run(config, [] { return false; });
    if (! result.succeeded)
    {
        juce::Logger::outputDebugString("native analysis failed: " + result.errorMessage);
        return 1;
    }

    juce::Logger::outputDebugString("sample_rate: " + juce::String(result.sampleRate));
    juce::Logger::outputDebugString("duration: " + juce::String(result.durationSeconds, 6));
    juce::Logger::outputDebugString("breath_count: " + juce::String(result.breathCount));
    juce::Logger::outputDebugString("sibilance_count: " + juce::String(result.sibilanceCount));
    juce::Logger::outputDebugString("noize_count: " + juce::String(result.noizeCount));
    return 0;
}
