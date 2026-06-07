#include "DiaApplicationFlowEditor/V2/ManifestLoader.h"

#include <DiaCore/Json/external/json/json.h>

#include <cerrno>
#include <cstring>
#include <stdio.h>

namespace Dia { namespace ApplicationFlow { namespace Editor {

    static LoadResult MakeError(LoadStatus status, const char* message)
    {
        LoadResult r;
        r.status = status;
        strncpy_s(r.errorMessage, sizeof(r.errorMessage), message, _TRUNCATE);
        return r;
    }

    static LoadResult MakeOk()
    {
        LoadResult r;
        r.status = LoadStatus::Ok;
        r.errorMessage[0] = '\0';
        return r;
    }

    LoadResult ManifestLoader::Load(const char* path, ManifestEditorState& state)
    {
        if (!path || path[0] == '\0')
            return MakeError(LoadStatus::LockedFile, "null or empty path");

        FILE* f = nullptr;
        errno_t err = fopen_s(&f, path, "rb");
        if (!f || err != 0)
        {
            return MakeError(LoadStatus::LockedFile, "could not open file");
        }

        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fileSize <= 0)
        {
            fclose(f);
            return MakeError(LoadStatus::MalformedJson, "file is empty");
        }

        char* buffer = new char[static_cast<size_t>(fileSize) + 1];
        size_t bytesRead = fread(buffer, 1, static_cast<size_t>(fileSize), f);
        fclose(f);
        buffer[bytesRead] = '\0';

        Json::Value root;
        Json::Reader reader;
        std::string parseErrors;
        bool parsed = reader.parse(buffer, root);
        delete[] buffer;

        if (!parsed)
        {
            parseErrors = reader.getFormattedErrorMessages();
            return MakeError(LoadStatus::MalformedJson, parseErrors.c_str());
        }

        if (!root.isMember("version") || root["version"].asInt() != 3)
        {
            return MakeError(LoadStatus::WrongVersion, "expected version 3");
        }

        ApplicationManifestV3 loaded;
        loaded.version = 3;

        if (root.isMember("stages") && root["stages"].isArray())
        {
            const Json::Value& stagesJson = root["stages"];
            for (unsigned int i = 0; i < stagesJson.size(); ++i)
            {
                const Json::Value& entry = stagesJson[i];
                if (!entry.isObject() || !entry.isMember("name"))
                    continue;
                StageDeclaration decl;
                decl.name = Dia::Core::StringCRC(entry["name"].asCString());
                if (entry.isMember("manifestPath") && entry["manifestPath"].isString())
                    decl.manifestPath = entry["manifestPath"].asCString();
                if (entry.isMember("transitions") && entry["transitions"].isArray())
                {
                    const Json::Value& tx = entry["transitions"];
                    for (unsigned int t = 0; t < tx.size(); ++t)
                        if (tx[t].isString())
                            decl.transitions.Add(Dia::Core::StringCRC(tx[t].asCString()));
                }
                if (entry.isMember("auto_advance") && entry["auto_advance"].isBool())
                    decl.autoAdvance = entry["auto_advance"].asBool();
                loaded.stages.Add(decl);
            }
        }

        if (root.isMember("initial_stage") && root["initial_stage"].isString())
            loaded.initialStage = Dia::Core::StringCRC(root["initial_stage"].asCString());

        // auto_stages: not present in v3; silently ignored if present.

        if (root.isMember("streams") && root["streams"].isArray())
        {
            const Json::Value& streamsJson = root["streams"];
            for (unsigned int i = 0; i < streamsJson.size(); ++i)
            {
                const Json::Value& s = streamsJson[i];
                StreamDeclaration stream;

                if (s.isMember("id") && s["id"].isString())
                    stream.id = Dia::Core::StringCRC(s["id"].asCString());
                if (s.isMember("kind") && s["kind"].isString())
                    stream.kind = Dia::Core::StringCRC(s["kind"].asCString());
                if (s.isMember("payload_type") && s["payload_type"].isString())
                    stream.payloadType = Dia::Core::StringCRC(s["payload_type"].asCString());
                if (s.isMember("from") && s["from"].isString())
                    stream.fromPU = Dia::Core::StringCRC(s["from"].asCString());
                if (s.isMember("to") && s["to"].isString())
                    stream.toPU = Dia::Core::StringCRC(s["to"].asCString());
                if (s.isMember("capacity") && s["capacity"].isUInt())
                    stream.capacity = s["capacity"].asUInt();
                if (s.isMember("max_readers") && s["max_readers"].isUInt())
                    stream.maxReaders = s["max_readers"].asUInt();
                stream.multiWriter = s.get("multi_writer", false).asBool();

                if (s.isMember("overflow") && s["overflow"].isString())
                    stream.overflowPolicy = ParseOverflowPolicy(Dia::Core::StringCRC(s["overflow"].asCString()));
                if (s.isMember("block_timeout_ms") && s["block_timeout_ms"].isUInt())
                    stream.blockTimeoutMs = s["block_timeout_ms"].asUInt();

                loaded.streams.Add(stream);
            }
        }

        if (root.isMember("processing_units") && root["processing_units"].isArray())
        {
            const Json::Value& pusJson = root["processing_units"];
            for (unsigned int i = 0; i < pusJson.size(); ++i)
            {
                const Json::Value& puJson = pusJson[i];
                ProcessingUnitDeclaration pu;

                if (puJson.isMember("instance_id") && puJson["instance_id"].isString())
                    pu.instanceId = Dia::Core::StringCRC(puJson["instance_id"].asCString());
                pu.frequencyHz    = puJson.get("frequency_hz",    30.0f).asFloat();
                pu.dedicatedThread = puJson.get("dedicated_thread", false).asBool();

                if (puJson.isMember("modules") && puJson["modules"].isArray())
                {
                    const Json::Value& modulesJson = puJson["modules"];
                    for (unsigned int j = 0; j < modulesJson.size(); ++j)
                    {
                        const Json::Value& modJson = modulesJson[j];
                        ModuleDeclaration mod;

                        if (modJson.isMember("instance_id") && modJson["instance_id"].isString())
                            mod.instanceId = Dia::Core::StringCRC(modJson["instance_id"].asCString());
                        if (modJson.isMember("type_id") && modJson["type_id"].isString())
                            mod.typeId = Dia::Core::StringCRC(modJson["type_id"].asCString());

                        if (modJson.isMember("stages") && modJson["stages"].isArray())
                        {
                            const Json::Value& stagesArr = modJson["stages"];
                            for (unsigned int k = 0; k < stagesArr.size(); ++k)
                            {
                                if (stagesArr[k].isString())
                                    mod.stages.Add(Dia::Core::StringCRC(stagesArr[k].asCString()));
                            }
                        }

                        if (modJson.isMember("dependencies") && modJson["dependencies"].isArray())
                        {
                            const Json::Value& depsArr = modJson["dependencies"];
                            for (unsigned int k = 0; k < depsArr.size(); ++k)
                            {
                                if (depsArr[k].isString())
                                    mod.dependencies.Add(Dia::Core::StringCRC(depsArr[k].asCString()));
                            }
                        }

                        if (modJson.isMember("channels") && modJson["channels"].isArray())
                        {
                            const Json::Value& channelsArr = modJson["channels"];
                            for (unsigned int k = 0; k < channelsArr.size(); ++k)
                            {
                                const Json::Value& ch = channelsArr[k];
                                if (ch.isObject() && ch.isMember("id") && ch.isMember("role"))
                                {
                                    Dia::ApplicationFlow::ChannelBinding binding;
                                    binding.id   = Dia::Core::StringCRC(ch["id"].asCString());
                                    binding.role = Dia::Core::StringCRC(ch["role"].asCString());
                                    if (!mod.channels.IsFull())
                                        mod.channels.Add(binding);
                                }
                            }
                        }

                        mod.startTimeoutMs = modJson.get("start_timeout_ms", 10000.0f).asFloat();
                        mod.stopTimeoutMs  = modJson.get("stop_timeout_ms",  5000.0f).asFloat();

                        if (modJson.isMember("config"))
                        {
                            Json::FastWriter writer;
                            std::string configStr = writer.write(modJson["config"]);
                            if (!configStr.empty() && configStr.back() == '\n')
                                configStr.pop_back();

                            static const unsigned int kMaxConfigLen = 255u;
                            if (configStr.size() > kMaxConfigLen)
                                configStr.resize(kMaxConfigLen);

                            mod.configJson = configStr.c_str();
                        }

                        pu.modules.Add(mod);
                    }
                }

                loaded.processingUnits.Add(pu);
            }
        }

        state.manifest = std::move(loaded);
        strncpy_s(state.filePath, sizeof(state.filePath), path, _TRUNCATE);
        state.hasManifest = true;
        state.isDirty = false;

        return MakeOk();
    }

}}} // namespace Dia::ApplicationFlow::Editor
