#include "ApplicationManifestLoaderV2.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>

#include <fstream>
#include <sstream>
#include <cstring>

namespace Dia { namespace ApplicationFlow {

    // ---------------------------------------------------------------------------
    // LoadFromFile
    // ---------------------------------------------------------------------------

    LoadResult ApplicationManifestLoaderV2::LoadFromFile(const char* filePath, ApplicationManifestV2& outManifest)
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

    LoadResult ApplicationManifestLoaderV2::LoadFromString(const char* jsonString, ApplicationManifestV2& outManifest)
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

    LoadResult ApplicationManifestLoaderV2::ParseJson(const char* jsonString, ApplicationManifestV2& outManifest)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(jsonString, root))
        {
            DIA_LOG_WARNING("ApplicationFlow", "ApplicationManifestLoaderV2::ParseJson — JSON parse error");
            return LoadResult::kParseError;
        }

        // --- Version check ---
        if (!root.isMember("version") || root["version"].asInt() != 2)
        {
            DIA_LOG_WARNING("ApplicationFlow", "ApplicationManifestLoaderV2::ParseJson — version mismatch (expected 2, got %d)",
                root.isMember("version") ? root["version"].asInt() : -1);
            return LoadResult::kVersionMismatch;
        }
        outManifest.version = 2;

        // --- stages ---
        if (root.isMember("stages") && root["stages"].isArray())
        {
            const Json::Value& stagesJson = root["stages"];
            for (unsigned int i = 0; i < stagesJson.size(); ++i)
            {
                StageDeclaration decl;
                const Json::Value& entry = stagesJson[i];
                if (entry.isString())
                {
                    // Plain string form: treat as name with empty manifestPath
                    decl.name = Dia::Core::StringCRC(entry.asCString());
                    // manifestPath stays default-constructed (empty)
                }
                else if (entry.isObject() && entry.isMember("name"))
                {
                    decl.name = Dia::Core::StringCRC(entry["name"].asCString());
                    if (entry.isMember("manifestPath") && entry["manifestPath"].isString())
                    {
                        decl.manifestPath = entry["manifestPath"].asCString();
                    }
                }
                else
                {
                    DIA_LOG_WARNING("ApplicationFlow", "ApplicationManifestLoaderV2::ParseJson — stages[%u] is neither a string nor an object with 'name'", i);
                    continue;
                }
                outManifest.stages.Add(decl);
            }
        }

        // --- initial_stage ---
        if (root.isMember("initial_stage") && root["initial_stage"].isString())
        {
            outManifest.initialStage = Dia::Core::StringCRC(root["initial_stage"].asCString());
        }

        // --- auto_stages ---
        if (root.isMember("auto_stages") && root["auto_stages"].isArray())
        {
            const Json::Value& autoStagesJson = root["auto_stages"];
            for (unsigned int i = 0; i < autoStagesJson.size(); ++i)
            {
                if (autoStagesJson[i].isString())
                {
                    outManifest.autoStages.Add(Dia::Core::StringCRC(autoStagesJson[i].asCString()));
                }
            }
        }

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
                if (s.isMember("type") && s["type"].isString())
                    stream.type = Dia::Core::StringCRC(s["type"].asCString());
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

                        // reads
                        if (modJson.isMember("reads") && modJson["reads"].isArray())
                        {
                            const Json::Value& readsArr = modJson["reads"];
                            for (unsigned int k = 0; k < readsArr.size(); ++k)
                            {
                                if (readsArr[k].isString())
                                    mod.reads.Add(Dia::Core::StringCRC(readsArr[k].asCString()));
                            }
                        }

                        // writes
                        if (modJson.isMember("writes") && modJson["writes"].isArray())
                        {
                            const Json::Value& writesArr = modJson["writes"];
                            for (unsigned int k = 0; k < writesArr.size(); ++k)
                            {
                                if (writesArr[k].isString())
                                    mod.writes.Add(Dia::Core::StringCRC(writesArr[k].asCString()));
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
