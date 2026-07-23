#include "BridgeAnalysis.h"

namespace
{
#ifndef QQDEBREATH_BRIDGE_EXE_PATH
#define QQDEBREATH_BRIDGE_EXE_PATH ""
#endif

#ifndef QQDEBREATH_TOOL_EXE_PATH
#define QQDEBREATH_TOOL_EXE_PATH ""
#endif

juce::String propString(const juce::var& object, const juce::Identifier& name)
{
    return object.getProperty(name, {}).toString();
}

juce::int64 propInt64(const juce::var& object, const juce::Identifier& name)
{
    return static_cast<juce::int64>(object.getProperty(name, 0));
}

double propDouble(const juce::var& object, const juce::Identifier& name)
{
    return static_cast<double>(object.getProperty(name, 0.0));
}

juce::File fileFromJsonPath(const juce::String& path, const juce::File& baseDir)
{
    if (path.isEmpty())
        return {};

    juce::File file(path);
    return juce::File::isAbsolutePath(path) ? file : baseDir.getChildFile(path);
}
} // namespace

juce::File QQDeBreathBridgeAnalysis::getDefaultBridgeExe()
{
    return juce::File(juce::String::fromUTF8(QQDEBREATH_BRIDGE_EXE_PATH));
}

juce::File QQDeBreathBridgeAnalysis::getDefaultToolExe()
{
    return juce::File(juce::String::fromUTF8(QQDEBREATH_TOOL_EXE_PATH));
}

QQDeBreathBridgeAnalysisResult QQDeBreathBridgeAnalysis::run(const QQDeBreathBridgeAnalysisConfig& config,
                                                             const ShouldCancel& shouldCancel)
{
    QQDeBreathBridgeAnalysisResult result;
    result.sourceKey = config.sourceKey;
    result.resultJson = config.outputJson;

    if (! config.bridgeExe.existsAsFile())
    {
        result.status = "Bridge executable not found.";
        result.errorMessage = config.bridgeExe.getFullPathName();
        return result;
    }

    if (! config.toolExe.existsAsFile())
    {
        result.status = "QQDeBreathTool.exe not found.";
        result.errorMessage = config.toolExe.getFullPathName();
        return result;
    }

    if (! config.inputWav.existsAsFile())
    {
        result.status = "Input wav not found.";
        result.errorMessage = config.inputWav.getFullPathName();
        return result;
    }

    if (! config.outDir.createDirectory())
    {
        result.status = "Could not create analysis output directory.";
        result.errorMessage = config.outDir.getFullPathName();
        return result;
    }

    config.outputJson.deleteFile();
    if (config.cancelFile != juce::File())
        config.cancelFile.deleteFile();

    juce::StringArray args;
    args.add(config.bridgeExe.getFullPathName());
    args.add("--tool");
    args.add(config.toolExe.getFullPathName());
    args.add("--input");
    args.add(config.inputWav.getFullPathName());
    args.add("--output-json");
    args.add(config.outputJson.getFullPathName());
    args.add("--out-dir");
    args.add(config.outDir.getFullPathName());
    args.add("--threshold");
    args.add(juce::String(config.threshold, 6));
    args.add("--fade-ms");
    args.add(juce::String(config.fadeMs, 3));
    args.add("--breath-target-db");
    args.add(juce::String(config.breathTargetDb, 3));
    args.add("--breath-gain-db");
    args.add(juce::String(config.breathGainDb, 3));
    args.add("--detect-noize");
    args.add(config.detectNoize ? "1" : "0");
    args.add("--timeout-ms");
    args.add(juce::String(config.timeoutMs));
    if (config.cancelFile != juce::File())
    {
        args.add("--cancel-file");
        args.add(config.cancelFile.getFullPathName());
    }

    juce::ChildProcess process;
    if (! process.start(args))
    {
        result.status = "Failed to start bridge process.";
        return result;
    }

    const auto startTime = juce::Time::getMillisecondCounter();
    auto cancelStartTime = static_cast<uint32_t>(0);
    auto cancelRequested = false;
    juce::String processOutput;

    while (process.isRunning())
    {
        if (shouldCancel != nullptr && shouldCancel())
        {
            if (! cancelRequested)
            {
                cancelRequested = true;
                cancelStartTime = juce::Time::getMillisecondCounter();

                if (config.cancelFile != juce::File())
                    config.cancelFile.replaceWithText("cancel", false, false, "\n");
            }

            if (juce::Time::getMillisecondCounter() - cancelStartTime > 5000u)
            {
                process.kill();
                result.cancelled = true;
                result.status = "Analysis cancelled.";
                return result;
            }
        }

        if (juce::Time::getMillisecondCounter() - startTime > static_cast<uint32_t>(config.timeoutMs))
        {
            process.kill();
            result.status = "Bridge process timed out.";
            return result;
        }

        juce::Thread::sleep(80);
    }

    processOutput += process.readAllProcessOutput();
    if (config.cancelFile != juce::File())
        config.cancelFile.deleteFile();

    if (cancelRequested)
    {
        result.cancelled = true;
        result.status = "Analysis cancelled.";
        return result;
    }

    const auto exitCode = process.getExitCode();

    if (exitCode != 0)
    {
        result.status = "Bridge process failed. Exit code: " + juce::String(exitCode);
        result.errorMessage = processOutput.trim();
        return result;
    }

    if (! config.outputJson.existsAsFile())
    {
        result.status = "Bridge completed but result.json was not created.";
        result.errorMessage = processOutput.trim();
        return result;
    }

    if (! parseResultJson(config.outputJson, config.sourceKey, result))
        return result;

    result.hasResult = true;
    result.succeeded = true;
    result.status = "Analysis complete.";
    return result;
}

bool QQDeBreathBridgeAnalysis::parseResultJson(const juce::File& jsonFile,
                                               const juce::String& sourceKey,
                                               QQDeBreathBridgeAnalysisResult& result)
{
    juce::var parsed = juce::JSON::parse(jsonFile);

    if (! parsed.isObject())
    {
        result.status = "result.json is not a JSON object.";
        return false;
    }

    const auto status = propString(parsed, "status");
    if (status != "ok")
    {
        result.status = "result.json reported error status.";
        const auto error = parsed.getProperty("error", {});
        result.errorMessage = error.isObject() ? propString(error, "message") : status;
        return false;
    }

    result.schemaVersion = static_cast<int>(parsed.getProperty("schema_version", 0));
    if (result.schemaVersion != 1)
    {
        result.status = "Unsupported result.json schema_version: " + juce::String(result.schemaVersion);
        return false;
    }

    const auto input = parsed.getProperty("input", {});
    if (! input.isObject())
    {
        result.status = "result.json is missing input object.";
        return false;
    }

    result.sampleRate = static_cast<int>(input.getProperty("sample_rate", 0));
    result.channels = static_cast<int>(input.getProperty("channels", 0));
    result.numSamples = propInt64(input, "num_samples");
    result.durationSeconds = propDouble(input, "duration");

    const auto regions = parsed.getProperty("regions", {});
    if (! regions.isArray())
    {
        result.status = "result.json is missing regions array.";
        return false;
    }

    result.regions.clear();
    result.breathCount = 0;
    result.noizeCount = 0;

    if (auto* regionArray = regions.getArray())
    {
        for (const auto& item : *regionArray)
        {
            if (! item.isObject())
                continue;

            QQDeBreathBridgeRegion region;
            region.type = qqNormalizedRegionType(propString(item, "type"));
            region.startSample = propInt64(item, "start_sample");
            region.endSample = propInt64(item, "end_sample");
            region.startTime = propDouble(item, "start_time");
            region.endTime = propDouble(item, "end_time");
            region.gainDb = juce::jlimit(-30.0, 30.0, propDouble(item, "region_gain_db"));
            QQDeBreathEqState regionEq;
            if (deserializeBreathEqState(propString(item, "region_eq"), regionEq))
                region.eqState = regionEq;
            result.regions.add(region);

            if (region.type.equalsIgnoreCase("Breath"))
                ++result.breathCount;
            else if (region.type.equalsIgnoreCase("Noise"))
                ++result.noizeCount;
            else if (region.type.equalsIgnoreCase("Sibilance"))
                ++result.sibilanceCount;
            else if (region.type.equalsIgnoreCase("Others"))
                ++result.othersCount;
        }
    }

    const auto files = parsed.getProperty("files", {});
    if (! files.isObject())
    {
        result.status = "result.json is missing files object.";
        return false;
    }

    const auto baseDir = jsonFile.getParentDirectory();
    result.vocalOnlyFile = fileFromJsonPath(propString(files, "vocal_only"), baseDir);
    result.breathFile = fileFromJsonPath(propString(files, "breath"), baseDir);
    result.noizeFile = fileFromJsonPath(propString(files, "noize"), baseDir);
    result.resultJson = jsonFile;
    result.sourceKey = sourceKey;

    if (! result.vocalOnlyFile.existsAsFile() || ! result.breathFile.existsAsFile() || ! result.noizeFile.existsAsFile())
    {
        result.status = "One or more stem files listed in result.json are missing.";
        return false;
    }

    return true;
}

juce::String QQDeBreathBridgeAnalysis::serializeResult(const QQDeBreathBridgeAnalysisResult& result)
{
    if (! result.hasResult)
        return {};

    auto root = new juce::DynamicObject();
    root->setProperty("version", 1);
    root->setProperty("has_result", result.hasResult);
    root->setProperty("succeeded", result.succeeded);
    root->setProperty("cancelled", result.cancelled);
    root->setProperty("source_key", result.sourceKey);
    root->setProperty("status", result.status);
    root->setProperty("error_message", result.errorMessage);
    root->setProperty("schema_version", result.schemaVersion);
    root->setProperty("sample_rate", result.sampleRate);
    root->setProperty("channels", result.channels);
    root->setProperty("num_samples", static_cast<double>(result.numSamples));
    root->setProperty("duration_seconds", result.durationSeconds);
    root->setProperty("breath_count", result.breathCount);
    root->setProperty("noize_count", result.noizeCount);
    root->setProperty("sibilance_count", result.sibilanceCount);
    root->setProperty("others_count", result.othersCount);
    root->setProperty("result_json", result.resultJson.getFullPathName());
    root->setProperty("vocal_only_file", result.vocalOnlyFile.getFullPathName());
    root->setProperty("breath_file", result.breathFile.getFullPathName());
    root->setProperty("noize_file", result.noizeFile.getFullPathName());

    juce::Array<juce::var> regionArray;
    for (const auto& region : result.regions)
    {
        auto object = new juce::DynamicObject();
        object->setProperty("type", region.type);
        object->setProperty("start_sample", static_cast<double>(region.startSample));
        object->setProperty("end_sample", static_cast<double>(region.endSample));
        object->setProperty("start_time", region.startTime);
        object->setProperty("end_time", region.endTime);
        object->setProperty("region_gain_db", region.gainDb);
        object->setProperty("region_eq", serializeBreathEqState(region.eqState));
        regionArray.add(juce::var(object));
    }
    root->setProperty("regions", regionArray);

    return juce::JSON::toString(juce::var(root), false);
}

bool QQDeBreathBridgeAnalysis::deserializeResult(const juce::String& text, QQDeBreathBridgeAnalysisResult& result)
{
    result = {};

    if (text.trim().isEmpty())
        return true;

    auto parsed = juce::JSON::parse(text);
    if (! parsed.isObject())
        return false;

    if (static_cast<int>(parsed.getProperty("version", 0)) != 1)
        return false;

    result.hasResult = static_cast<bool>(parsed.getProperty("has_result", false));
    result.succeeded = static_cast<bool>(parsed.getProperty("succeeded", false));
    result.cancelled = static_cast<bool>(parsed.getProperty("cancelled", false));
    result.sourceKey = propString(parsed, "source_key");
    result.status = propString(parsed, "status");
    result.errorMessage = propString(parsed, "error_message");
    result.schemaVersion = static_cast<int>(parsed.getProperty("schema_version", 0));
    result.sampleRate = static_cast<int>(parsed.getProperty("sample_rate", 0));
    result.channels = static_cast<int>(parsed.getProperty("channels", 0));
    result.numSamples = propInt64(parsed, "num_samples");
    result.durationSeconds = propDouble(parsed, "duration_seconds");
    result.breathCount = static_cast<int>(parsed.getProperty("breath_count", 0));
    result.noizeCount = static_cast<int>(parsed.getProperty("noize_count", 0));
    result.sibilanceCount = static_cast<int>(parsed.getProperty("sibilance_count", 0));
    result.othersCount = static_cast<int>(parsed.getProperty("others_count", 0));
    result.resultJson = juce::File(propString(parsed, "result_json"));
    result.vocalOnlyFile = juce::File(propString(parsed, "vocal_only_file"));
    result.breathFile = juce::File(propString(parsed, "breath_file"));
    result.noizeFile = juce::File(propString(parsed, "noize_file"));

    result.regions.clear();
    const auto regions = parsed.getProperty("regions", {});
    if (auto* array = regions.getArray())
    {
        for (const auto& item : *array)
        {
            if (! item.isObject())
                continue;

            QQDeBreathBridgeRegion region;
            region.type = qqNormalizedRegionType(propString(item, "type"));
            region.startSample = propInt64(item, "start_sample");
            region.endSample = propInt64(item, "end_sample");
            region.startTime = propDouble(item, "start_time");
            region.endTime = propDouble(item, "end_time");
            region.gainDb = juce::jlimit(-30.0, 30.0, propDouble(item, "region_gain_db"));
            QQDeBreathEqState regionEq;
            if (deserializeBreathEqState(propString(item, "region_eq"), regionEq))
                region.eqState = regionEq;
            result.regions.add(region);
        }
    }

    if (result.status.isEmpty())
        result.status = result.hasResult ? "Analysis restored from project." : "No analysis.";

    return true;
}
