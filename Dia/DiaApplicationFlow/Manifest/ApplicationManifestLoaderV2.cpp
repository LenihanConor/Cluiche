#include "ApplicationManifestLoaderV2.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaStreams/OverflowPolicy.h>

#include <fstream>
#include <sstream>
#include <cstring>

namespace Dia { namespace ApplicationFlow {

    // ---------------------------------------------------------------------------
    // LoadFromFile
    // ---------------------------------------------------------------------------

    LoadResult ApplicationManifestLoaderV2::LoadFromFile(const char* filePath, ApplicationManifestV3& outManifest)
    {
        if (filePath == nullptr)
        {
            DIA_LOG_WARNING("ApplicationFlow", "ApplicationManifestLoaderV2::LoadFromFile — filePath is null");
            return LoadResult::kFileNotFound;
        }

        std::ifstream file(filePath, std::ios::in);
        if (!file.is_open())
        {
            DIA_LOG_WARNING("ApplicationFlow", "ApplicationManifestLoaderV2::LoadFromFile — could not open file: %s", filePath);
            return LoadResult::kFileNotFound;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        std::string contents = ss.str();

        return LoadFromString(contents.c_str(), outManifest);
    }

    // ---------------------------------------------------------------------------
    // LoadFromString
    // ---------------------------------------------------------------------------

    LoadResult ApplicationManifestLoaderV2::LoadFromString(const char* jsonString, ApplicationManifestV3& outManifest)
    {
        if (jsonString == nullptr)
        {
            DIA_LOG_WARNING("ApplicationFlow", "ApplicationManifestLoaderV2::LoadFromString — jsonString is null");
            return LoadResult::kParseError;
        }

        return ParseJson(jsonString, outManifest);
    }

    // ---------------------------------------------------------------------------
    // ParseJson (internal)
    // ---------------------------------------------------------------------------

    LoadResult ApplicationManifestLoaderV2::ParseJson(const char* jsonString, ApplicationManifestV3& outManifest)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(jsonString, root))
        {
            DIA_LOG_WARNING("ApplicationFlow", "ApplicationManifestLoaderV2::ParseJson — JSON parse error");
            return LoadResult::kParseError;
        }

        // --- Version check ---
        if (!root.isMember("version") || root["version"].asInt() != 3)
        {
            DIA_LOG_WARNING("ApplicationFlow", "ApplicationManifestLoaderV2::ParseJson — version mismatch (expected 3, got %d)",
                root.isMember("version") ? root["version"].asInt() : -1);
            return LoadResult::kVersionMismatch;
        }
        outManifest.version = 3;

        // --- stages ---
        if (root.isMember("stages") && root["stages"].isArray())
        {
            const Json::Value& stagesJson = root["stages"];
            for (unsigned int i = 0; i < stagesJson.size(); ++i)
            {
                const Json::Value& entry = stagesJson[i];
                if (!entry.isObject() || !entry.isMember("name"))
                {
                    DIA_LOG_WARNING("ApplicationFlow",
                        "ApplicationManifestLoaderV2::ParseJson — stages[%u] is not an object with 'name' (v2 string form is no longer accepted)", i);
                    continue;
                }

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

                outManifest.stages.Add(decl);
            }
        }

        // --- initial_stage ---
        if (root.isMember("initial_stage") && root["initial_stage"].isString())
        {
            outManifest.initialStage = Dia::Core::StringCRC(root["initial_stage"].asCString());
        }

        // auto_stages: not present in v3; silently ignored if somehow present.

        // --- streams ---
        if (root.isMember("streams") && root["streams"].isArray())
        {
            const Json::Value& streamsJson = root["streams"];
            for (unsigned int i = 0; i < streamsJson.size(); ++i)
            {
                const Json::Value& s = streamsJson[i];
                StreamDeclaration stream;

                if (s.isMember("id") && s["id"].isString())
                    stream.id = Dia::Core::StringCRC(s["id"].asCString());

                // v2.1 fields
                if (s.isMember("kind") && s["kind"].isString())
                    stream.kind = Dia::Core::StringCRC(s["kind"].asCString());
                if (s.isMember("payload_type") && s["payload_type"].isString())
                    stream.payloadType = Dia::Core::StringCRC(s["payload_type"].asCString());
                if (s.isMember("capacity") && s["capacity"].isUInt())
                    stream.capacity = s["capacity"].asUInt();
                if (s.isMember("max_readers") && s["max_readers"].isUInt())
                    stream.maxReaders = s["max_readers"].asUInt();

                // F3 policy fields
                if (s.isMember("overflow") && s["overflow"].isString())
                    stream.overflowPolicy = ParseOverflowPolicy(Dia::Core::StringCRC(s["overflow"].asCString()));
                if (s.isMember("block_timeout_ms") && s["block_timeout_ms"].isUInt())
                    stream.blockTimeoutMs = s["block_timeout_ms"].asUInt();

                if (s.isMember("from") && s["from"].isString())
                    stream.fromPU = Dia::Core::StringCRC(s["from"].asCString());
                if (s.isMember("to") && s["to"].isString())
                    stream.toPU = Dia::Core::StringCRC(s["to"].asCString());
                stream.multiWriter = s.get("multi_writer", false).asBool();

                outManifest.streams.Add(stream);
            }
        }

        // --- processing_units ---
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

                // modules
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

                        // stages
                        if (modJson.isMember("stages") && modJson["stages"].isArray())
                        {
                            const Json::Value& stagesArr = modJson["stages"];
                            for (unsigned int k = 0; k < stagesArr.size(); ++k)
                            {
                                if (stagesArr[k].isString())
                                    mod.stages.Add(Dia::Core::StringCRC(stagesArr[k].asCString()));
                            }
                        }

                        // dependencies
                        if (modJson.isMember("dependencies") && modJson["dependencies"].isArray())
                        {
                            const Json::Value& depsArr = modJson["dependencies"];
                            for (unsigned int k = 0; k < depsArr.size(); ++k)
                            {
                                if (depsArr[k].isString())
                                    mod.dependencies.Add(Dia::Core::StringCRC(depsArr[k].asCString()));
                            }
                        }

                        // channels (unified reads/writes/provides/consumes)
                        if (modJson.isMember("channels") && modJson["channels"].isArray())
                        {
                            const Json::Value& channelsArr = modJson["channels"];
                            for (unsigned int k = 0; k < channelsArr.size(); ++k)
                            {
                                const Json::Value& ch = channelsArr[k];
                                if (ch.isObject() && ch.isMember("id") && ch.isMember("role"))
                                {
                                    ChannelBinding binding;
                                    binding.id   = Dia::Core::StringCRC(ch["id"].asCString());
                                    binding.role = Dia::Core::StringCRC(ch["role"].asCString());
                                    mod.channels.Add(binding);
                                }
                            }
                        }

                        // timeouts
                        mod.startTimeoutMs = modJson.get("start_timeout_ms", 10000.0f).asFloat();
                        mod.stopTimeoutMs  = modJson.get("stop_timeout_ms",  5000.0f).asFloat();

                        // config — serialize back to string and store in configJson (String256)
                        if (modJson.isMember("config"))
                        {
                            Json::FastWriter writer;
                            std::string configStr = writer.write(modJson["config"]);

                            // FastWriter appends a trailing newline — strip it
                            if (!configStr.empty() && configStr.back() == '\n')
                                configStr.pop_back();

                            static const unsigned int kMaxConfigLen = 255u;
                            if (configStr.size() > kMaxConfigLen)
                            {
                                DIA_LOG_WARNING("ApplicationFlow",
                                    "ApplicationManifestLoaderV2::ParseJson — config JSON for module '%s' exceeds 255 chars (%u), truncating",
                                    modJson.isMember("instance_id") ? modJson["instance_id"].asCString() : "?",
                                    static_cast<unsigned int>(configStr.size()));
                                configStr.resize(kMaxConfigLen);
                            }
                            mod.configJson = configStr.c_str();
                        }

                        pu.modules.Add(mod);
                    }
                }

                outManifest.processingUnits.Add(pu);
            }
        }

        return LoadResult::kSuccess;
    }

}} // namespace Dia::ApplicationFlow
