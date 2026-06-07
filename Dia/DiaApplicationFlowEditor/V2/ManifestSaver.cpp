#include <DiaApplicationFlowEditor/V2/ManifestSaver.h>
#include <DiaCore/Json/external/json/json.h>

#include <windows.h>
#include <cstdio>
#include <cstring>

using namespace Dia::ApplicationFlow;
using namespace Dia::ApplicationFlow::Editor;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static const char* OverflowPolicyToString(OverflowPolicy policy)
{
    switch (policy)
    {
    case OverflowPolicy::kDropNewest: return "drop-newest";
    case OverflowPolicy::kBlock:      return "block";
    case OverflowPolicy::kFailLoud:   return "fail-loud";
    default:                          return "drop-oldest";
    }
}

static void SerializeStreamDeclaration(const StreamDeclaration& stream, Json::Value& outJson)
{
    outJson["id"]          = stream.id.AsChar();
    outJson["kind"]        = stream.kind.AsChar();
    outJson["payload_type"]= stream.payloadType.AsChar();
    outJson["from"]        = stream.fromPU.AsChar();
    outJson["to"]          = stream.toPU.AsChar();

    if (stream.multiWriter)
        outJson["multi_writer"] = true;

    if (stream.capacity > 0)
        outJson["capacity"] = stream.capacity;
    if (stream.maxReaders > 0)
        outJson["max_readers"] = stream.maxReaders;

    static const Dia::Core::StringCRC kEventStream("EventStream");
    if (stream.kind == kEventStream)
    {
        outJson["overflow"]         = OverflowPolicyToString(stream.overflowPolicy);
        outJson["block_timeout_ms"] = stream.blockTimeoutMs;
    }
}

static void SerializeModuleDeclaration(const ModuleDeclaration& module, Json::Value& outJson)
{
    outJson["instance_id"]       = module.instanceId.AsChar();
    outJson["type_id"]           = module.typeId.AsChar();
    outJson["start_timeout_ms"]  = module.startTimeoutMs;
    outJson["stop_timeout_ms"]   = module.stopTimeoutMs;

    if (module.stages.Size() > 0)
    {
        Json::Value& stagesJson = outJson["stages"] = Json::Value(Json::arrayValue);
        for (unsigned int i = 0; i < module.stages.Size(); ++i)
            stagesJson.append(module.stages[i].AsChar());
    }

    if (module.dependencies.Size() > 0)
    {
        Json::Value& depsJson = outJson["dependencies"] = Json::Value(Json::arrayValue);
        for (unsigned int i = 0; i < module.dependencies.Size(); ++i)
            depsJson.append(module.dependencies[i].AsChar());
    }

    if (module.channels.Size() > 0)
    {
        Json::Value& channelsJson = outJson["channels"] = Json::Value(Json::arrayValue);
        for (unsigned int i = 0; i < module.channels.Size(); ++i)
        {
            Json::Value ch;
            ch["id"]   = module.channels[i].id.AsChar();
            ch["role"] = module.channels[i].role.AsChar();
            channelsJson.append(ch);
        }
    }

    if (module.configJson.Length() > 0)
        outJson["config_json"] = module.configJson.AsCStr();
}

static void SerializeProcessingUnitDeclaration(const ProcessingUnitDeclaration& pu, Json::Value& outJson)
{
    // TODO: StringCRC::AsChar() returns the original string stored at construction time.
    // When CRC values come from a loader that passed string literals, AsChar() is valid.
    // If CRCs were constructed from raw uint32 values (no string), AsChar() returns empty.
    // Full string-round-trip fidelity requires commands to always construct StringCRC from strings.
    outJson["instance_id"]      = pu.instanceId.AsChar();
    outJson["frequency_hz"]     = pu.frequencyHz;
    outJson["dedicated_thread"] = pu.dedicatedThread;

    Json::Value& modulesJson = outJson["modules"] = Json::Value(Json::arrayValue);
    for (unsigned int i = 0; i < pu.modules.Size(); ++i)
    {
        Json::Value moduleJson;
        SerializeModuleDeclaration(pu.modules[i], moduleJson);
        modulesJson.append(moduleJson);
    }
}

static void SerializeManifestV2(const ApplicationManifestV3& manifest, Json::Value& outJson)
{
    outJson["version"] = manifest.version;

    Json::Value& stagesJson = outJson["stages"] = Json::Value(Json::arrayValue);
    for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
    {
        const StageDeclaration& stage = manifest.stages[i];
        Json::Value stageJson;
        stageJson["name"]         = stage.name.AsChar();
        stageJson["auto_advance"] = stage.autoAdvance;

        Json::Value transitionsJson(Json::arrayValue);
        for (unsigned int t = 0; t < stage.transitions.Size(); ++t)
            transitionsJson.append(stage.transitions[t].AsChar());
        stageJson["transitions"] = transitionsJson;

        if (stage.manifestPath.Length() > 0)
            stageJson["manifestPath"] = stage.manifestPath.AsCStr();

        stagesJson.append(stageJson);
    }

    if (manifest.initialStage != StringCRC::kZero)
        outJson["initial_stage"] = manifest.initialStage.AsChar();

    Json::Value& streamsJson = outJson["streams"] = Json::Value(Json::arrayValue);
    for (unsigned int i = 0; i < manifest.streams.Size(); ++i)
    {
        Json::Value streamJson;
        SerializeStreamDeclaration(manifest.streams[i], streamJson);
        streamsJson.append(streamJson);
    }

    Json::Value& pusJson = outJson["processing_units"] = Json::Value(Json::arrayValue);
    for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
    {
        Json::Value puJson;
        SerializeProcessingUnitDeclaration(manifest.processingUnits[i], puJson);
        pusJson.append(puJson);
    }
}

// ---------------------------------------------------------------------------
// ManifestSaver
// ---------------------------------------------------------------------------

void ManifestSaver::SerializeToJson(const ManifestEditorState& state, char* outBuffer, unsigned int bufferSize)
{
    Json::Value root;
    SerializeManifestV2(state.manifest, root);

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "    ";
    builder["commentStyle"] = "None";
    builder.settings_["sortKeys"] = true;

    std::string output = Json::writeString(builder, root);

    if (bufferSize > 0)
    {
        unsigned int copyLen = static_cast<unsigned int>(output.size());
        if (copyLen >= bufferSize)
            copyLen = bufferSize - 1;
        std::memcpy(outBuffer, output.c_str(), copyLen);
        outBuffer[copyLen] = '\0';
    }
}

SaveResult ManifestSaver::Save(ManifestEditorState& state)
{
    SaveResult result;
    result.status = SaveStatus::Ok;
    result.errorMessage[0] = '\0';

    if (!state.hasManifest || state.filePath[0] == '\0')
    {
        result.status = SaveStatus::WriteError;
        strncpy_s(result.errorMessage, sizeof(result.errorMessage),
                  "Save called with no manifest loaded", _TRUNCATE);
        return result;
    }

    // Backup existing file if present
    if (GetFileAttributesA(state.filePath) != INVALID_FILE_ATTRIBUTES)
    {
        char bakPath[512 + 4];
        strncpy_s(bakPath, sizeof(bakPath), state.filePath, _TRUNCATE);
        strncat_s(bakPath, sizeof(bakPath), ".bak", _TRUNCATE);

        if (!CopyFileA(state.filePath, bakPath, FALSE))
        {
            result.status = SaveStatus::BackupError;
            strncpy_s(result.errorMessage, sizeof(result.errorMessage),
                      "Failed to create backup file", _TRUNCATE);
            return result;
        }
    }

    // Serialize to JSON
    char jsonBuffer[65536];
    SerializeToJson(state, jsonBuffer, sizeof(jsonBuffer));

    // Write to temp file
    char tmpPath[512 + 4];
    strncpy_s(tmpPath, sizeof(tmpPath), state.filePath, _TRUNCATE);
    strncat_s(tmpPath, sizeof(tmpPath), ".tmp", _TRUNCATE);

    FILE* tmpFile = nullptr;
    errno_t openErr = fopen_s(&tmpFile, tmpPath, "wt");
    if (openErr != 0 || tmpFile == nullptr)
    {
        result.status = SaveStatus::WriteError;
        strncpy_s(result.errorMessage, sizeof(result.errorMessage),
                  "Failed to open temporary file for writing", _TRUNCATE);
        return result;
    }

    fputs(jsonBuffer, tmpFile);
    fclose(tmpFile);

    // Atomic rename
    if (!MoveFileExA(tmpPath, state.filePath, MOVEFILE_REPLACE_EXISTING))
    {
        result.status = SaveStatus::WriteError;
        strncpy_s(result.errorMessage, sizeof(result.errorMessage),
                  "Failed to rename temporary file to final path", _TRUNCATE);
        return result;
    }

    state.MarkClean();
    return result;
}
