#include <juce_core/juce_core.h>

namespace
{
constexpr int expectedSchemaVersion = 1;

enum ExitCode
{
    ok = 0,
    badArguments = 2,
    fileError = 3,
    processError = 4,
    timeoutError = 5,
    childExitError = 6,
    jsonError = 7,
    schemaError = 8,
    toolReportedError = 9
};

struct Options
{
    juce::File tool;
    juce::File input;
    juce::File outputJson;
    juce::File outDir;
    juce::File model;
    juce::File cancelFile;
    juce::String threshold = "0.86";
    juce::String fadeMs = "10";
    juce::String breathTargetDb = "-6";
    juce::String breathGainDb = "0";
    juce::String detectNoize = "0";
    int timeoutMs = 600000;
    bool validateJsonOnly = false;
};

void printLine(const juce::String& text)
{
    std::cout << text.toRawUTF8() << std::endl;
}

void printError(const juce::String& text)
{
    std::cerr << text.toRawUTF8() << std::endl;
}

juce::String usage()
{
    return "Usage: qq_debreath_bridge.exe "
           "--tool QQDeBreathTool.exe "
           "--input input.wav "
           "--output-json result.json "
           "--out-dir out "
           "[--threshold 0.86] "
           "[--fade-ms 10] "
           "[--breath-target-db -6] "
           "[--breath-gain-db 0] "
           "[--detect-noize 0] "
           "[--model breath_frame_model.joblib] "
           "[--timeout-ms 600000] "
           "[--cancel-file cancel.flag] "
           "[--validate-json-only]";
}

bool requireValue(const juce::StringArray& args, int& index, juce::String& value, juce::String& error)
{
    if (index + 1 >= args.size())
    {
        error = "Missing value for " + args[index];
        return false;
    }

    value = args[++index];
    return true;
}

bool parseOptions(const juce::StringArray& args, Options& options, juce::String& error)
{
    for (int i = 1; i < args.size(); ++i)
    {
        const auto key = args[i];
        juce::String value;

        if (key == "--tool")
        {
            if (! requireValue(args, i, value, error)) return false;
            options.tool = juce::File(value);
        }
        else if (key == "--input")
        {
            if (! requireValue(args, i, value, error)) return false;
            options.input = juce::File(value);
        }
        else if (key == "--output-json")
        {
            if (! requireValue(args, i, value, error)) return false;
            options.outputJson = juce::File(value);
        }
        else if (key == "--out-dir")
        {
            if (! requireValue(args, i, value, error)) return false;
            options.outDir = juce::File(value);
        }
        else if (key == "--model")
        {
            if (! requireValue(args, i, value, error)) return false;
            options.model = juce::File(value);
        }
        else if (key == "--threshold")
        {
            if (! requireValue(args, i, options.threshold, error)) return false;
        }
        else if (key == "--fade-ms")
        {
            if (! requireValue(args, i, options.fadeMs, error)) return false;
        }
        else if (key == "--breath-target-db")
        {
            if (! requireValue(args, i, options.breathTargetDb, error)) return false;
        }
        else if (key == "--breath-gain-db")
        {
            if (! requireValue(args, i, options.breathGainDb, error)) return false;
        }
        else if (key == "--detect-noize")
        {
            if (! requireValue(args, i, options.detectNoize, error)) return false;
        }
        else if (key == "--timeout-ms")
        {
            if (! requireValue(args, i, value, error)) return false;
            options.timeoutMs = value.getIntValue();
        }
        else if (key == "--cancel-file")
        {
            if (! requireValue(args, i, value, error)) return false;
            options.cancelFile = juce::File(value);
        }
        else if (key == "--validate-json-only")
        {
            options.validateJsonOnly = true;
        }
        else if (key == "--help" || key == "-h")
        {
            error = usage();
            return false;
        }
        else
        {
            error = "Unknown argument: " + key;
            return false;
        }
    }

    if (options.outputJson == juce::File()) { error = "Missing required --output-json"; return false; }
    if (options.validateJsonOnly)           { return true; }

    if (options.tool == juce::File())       { error = "Missing required --tool"; return false; }
    if (options.input == juce::File())      { error = "Missing required --input"; return false; }
    if (options.outDir == juce::File())     { error = "Missing required --out-dir"; return false; }
    if (options.timeoutMs <= 0)             { error = "--timeout-ms must be greater than zero"; return false; }

    return true;
}

bool ensureWritableDirectory(const juce::File& dir, juce::String& error)
{
    if (! dir.exists())
    {
        if (! dir.createDirectory())
        {
            error = "Could not create out-dir: " + dir.getFullPathName();
            return false;
        }
    }

    if (! dir.isDirectory())
    {
        error = "out-dir is not a directory: " + dir.getFullPathName();
        return false;
    }

    const auto probe = dir.getChildFile("qq_debreath_bridge_write_probe.tmp");
    if (! probe.replaceWithText("probe", false, false, "\n"))
    {
        error = "out-dir is not writable: " + dir.getFullPathName();
        return false;
    }

    probe.deleteFile();
    return true;
}

juce::StringArray buildChildArgs(const Options& options)
{
    juce::StringArray args {
        options.tool.getFullPathName(),
        "analyze-for-plugin",
        "--input", options.input.getFullPathName(),
        "--output-json", options.outputJson.getFullPathName(),
        "--out-dir", options.outDir.getFullPathName(),
        "--threshold", options.threshold,
        "--fade-ms", options.fadeMs,
        "--breath-target-db", options.breathTargetDb,
        "--breath-gain-db", options.breathGainDb,
        "--detect-noize", options.detectNoize
    };

    if (options.model != juce::File())
        args.addArray({ "--model", options.model.getFullPathName() });

    return args;
}

juce::var getRequiredProperty(const juce::var& object, const juce::Identifier& key, juce::String& error)
{
    if (! object.isObject())
    {
        error = "Expected JSON object while reading " + key.toString();
        return {};
    }

    auto value = object.getProperty(key, {});
    if (value.isVoid())
        error = "Missing required JSON field: " + key.toString();

    return value;
}

juce::String asString(const juce::var& value)
{
    return value.toString();
}

int countRegionsOfType(const juce::Array<juce::var>* regions, const juce::String& type)
{
    int count = 0;

    if (regions == nullptr)
        return 0;

    for (const auto& region : *regions)
    {
        const auto regionType = region.getProperty("type", {}).toString();
        if (regionType.equalsIgnoreCase(type))
            ++count;
    }

    return count;
}

int validateAndPrintResult(const juce::File& resultJson)
{
    if (! resultJson.existsAsFile())
    {
        printError("ERROR: result.json does not exist: " + resultJson.getFullPathName());
        return jsonError;
    }

    auto parsed = juce::JSON::parse(resultJson);
    if (! parsed.isObject())
    {
        printError("ERROR: result.json is not a JSON object.");
        return jsonError;
    }

    juce::String error;
    const auto schemaVersion = getRequiredProperty(parsed, "schema_version", error);
    if (error.isNotEmpty()) { printError("ERROR: " + error); return schemaError; }

    if (static_cast<int>(schemaVersion) != expectedSchemaVersion)
    {
        printError("ERROR: schema_version mismatch. Expected "
                   + juce::String(expectedSchemaVersion)
                   + ", got "
                   + schemaVersion.toString());
        return schemaError;
    }

    const auto status = getRequiredProperty(parsed, "status", error);
    if (error.isNotEmpty()) { printError("ERROR: " + error); return schemaError; }

    if (status.toString() == "error")
    {
        const auto errorObject = parsed.getProperty("error", {});
        const auto code = errorObject.getProperty("code", {}).toString();
        const auto message = errorObject.getProperty("message", {}).toString();
        printError("ERROR: tool returned status=error code=" + code + " message=" + message);
        return toolReportedError;
    }

    if (status.toString() != "ok")
    {
        printError("ERROR: unexpected result status: " + status.toString());
        return schemaError;
    }

    const auto input = getRequiredProperty(parsed, "input", error);
    if (error.isNotEmpty()) { printError("ERROR: " + error); return schemaError; }

    const auto regionsValue = getRequiredProperty(parsed, "regions", error);
    if (error.isNotEmpty()) { printError("ERROR: " + error); return schemaError; }

    const auto files = getRequiredProperty(parsed, "files", error);
    if (error.isNotEmpty()) { printError("ERROR: " + error); return schemaError; }

    auto* regions = regionsValue.getArray();
    if (regions == nullptr)
    {
        printError("ERROR: regions field is not an array.");
        return schemaError;
    }

    const auto vocalOnly = files.getProperty("vocal_only", {});
    const auto breath = files.getProperty("breath", {});
    const auto noize = files.getProperty("noize", {});
    if (vocalOnly.isVoid() || breath.isVoid() || noize.isVoid())
    {
        printError("ERROR: files field must contain vocal_only, breath, and noize.");
        return schemaError;
    }

    printLine("sample_rate: " + input.getProperty("sample_rate", {}).toString());
    printLine("duration: " + input.getProperty("duration", {}).toString());
    printLine("breath_count: " + juce::String(countRegionsOfType(regions, "Breath")));
    printLine("noize_count: " + juce::String(countRegionsOfType(regions, "Noize")));
    printLine("vocal_only: " + asString(vocalOnly));
    printLine("breath: " + asString(breath));
    printLine("noize: " + asString(noize));
    return ok;
}
} // namespace

int runBridge(const juce::StringArray& args)
{
    Options options;
    juce::String error;

    if (! parseOptions(args, options, error))
    {
        printError(error);
        printError(usage());
        return badArguments;
    }

    if (options.validateJsonOnly)
        return validateAndPrintResult(options.outputJson);

    if (! options.tool.existsAsFile())
    {
        printError("ERROR: tool does not exist: " + options.tool.getFullPathName());
        return fileError;
    }

    if (! options.input.existsAsFile())
    {
        printError("ERROR: input does not exist: " + options.input.getFullPathName());
        return fileError;
    }

    if (! ensureWritableDirectory(options.outDir, error))
    {
        printError("ERROR: " + error);
        return fileError;
    }

    if (auto parent = options.outputJson.getParentDirectory(); parent != juce::File())
    {
        if (! ensureWritableDirectory(parent, error))
        {
            printError("ERROR: output-json parent is not writable: " + error);
            return fileError;
        }
    }

    options.outputJson.deleteFile();

    juce::ChildProcess process;
    const auto childArgs = buildChildArgs(options);

    printLine("Running QQDeBreathTool analyze-for-plugin...");
    if (! process.start(childArgs, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        printError("ERROR: failed to start tool: " + options.tool.getFullPathName());
        return processError;
    }

    const auto startTime = juce::Time::getMillisecondCounter();
    juce::String childOutput;

    while (process.isRunning())
    {
        childOutput += process.readAllProcessOutput();

        if (options.cancelFile != juce::File() && options.cancelFile.existsAsFile())
        {
            process.kill();
            printError("ERROR: subprocess cancelled.");
            return processError;
        }

        if (juce::Time::getMillisecondCounter() - startTime > static_cast<uint32_t>(options.timeoutMs))
        {
            process.kill();
            printError("ERROR: subprocess timed out after " + juce::String(options.timeoutMs) + " ms.");
            return timeoutError;
        }

        juce::Thread::sleep(100);
    }

    childOutput += process.readAllProcessOutput();

    const auto exitCode = static_cast<int>(process.getExitCode());
    if (exitCode != 0)
    {
        if (options.outputJson.existsAsFile())
            return validateAndPrintResult(options.outputJson);

        printError("ERROR: subprocess exit code: " + juce::String(exitCode));
        if (childOutput.isNotEmpty())
            printError(childOutput);
        return childExitError;
    }

    return validateAndPrintResult(options.outputJson);
}

#if JUCE_WINDOWS
int wmain(int argc, wchar_t* argv[])
{
    juce::StringArray args;

    for (int i = 0; i < argc; ++i)
        args.add(juce::String(argv[i]));

    return runBridge(args);
}
#else
int main(int argc, char* argv[])
{
    juce::StringArray args;

    for (int i = 0; i < argc; ++i)
        args.add(juce::String::fromUTF8(argv[i]));

    return runBridge(args);
}
#endif
