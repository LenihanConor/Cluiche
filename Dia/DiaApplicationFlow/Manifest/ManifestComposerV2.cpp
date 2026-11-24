#include "ManifestComposerV2.h"
#include "ApplicationManifestLoaderV2.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <memory>
#include <string>
#include <string.h>

namespace Dia { namespace ApplicationFlow {

    //-----------------------------------------------------------------------------
    // Internal constants
    //-----------------------------------------------------------------------------

    static const unsigned int kFileBufferSize = 32768;
    static const unsigned int kPathBufferSize = 256;

    //-----------------------------------------------------------------------------
    // ManifestComposerV2::ReadFile
    // Opens path with fstream and reads up to bufferSize-1 bytes into buffer.
    // Returns false if the file cannot be opened or is too large for the buffer.
    //-----------------------------------------------------------------------------

    bool ManifestComposerV2::ReadFile(const char* path, char* buffer, unsigned int bufferSize)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return false;

        file.seekg(0, std::ios::end);
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        if (fileSize <= 0 || static_cast<unsigned int>(fileSize) >= bufferSize)
            return false;

        file.read(buffer, fileSize);
        if (!file)
            return false;

        buffer[static_cast<unsigned int>(fileSize)] = '\0';
        return true;
    }

    //-----------------------------------------------------------------------------
    // ManifestComposerV2::BuildPath
    // Combines baseDir + "/" + relPath into outPath (max outSize chars).
    //-----------------------------------------------------------------------------

    void ManifestComposerV2::BuildPath(const char* baseDir, const char* relPath, char* outPath, unsigned int outSize)
    {
        unsigned int dirLen = 0;
        while (baseDir[dirLen] != '\0' && dirLen < outSize - 1)
        {
            outPath[dirLen] = baseDir[dirLen];
            ++dirLen;
        }

        // Append separator if needed and there is room
        if (dirLen > 0 && baseDir[dirLen - 1] != '/' && baseDir[dirLen - 1] != '\\' && dirLen < outSize - 1)
        {
            outPath[dirLen] = '/';
            ++dirLen;
        }

        unsigned int relIdx = 0;
        while (relPath[relIdx] != '\0' && dirLen < outSize - 1)
        {
            outPath[dirLen] = relPath[relIdx];
            ++dirLen;
            ++relIdx;
        }

        outPath[dirLen] = '\0';
    }

    //-----------------------------------------------------------------------------
    // ManifestComposerV2::LoadDiagame
    // Reads the .diagame file, loads the base manifest (type "manifest"), then
    // kicks off a MergeStage call for every "stage" type import.
    // baseDir is populated with the directory containing the .diagame file.
    //-----------------------------------------------------------------------------

    ComposeResult ManifestComposerV2::LoadDiagame(const char* path, ApplicationManifestV3& outManifest, char* baseDir)
    {
        char fileBuffer[kFileBufferSize];
        if (!ReadFile(path, fileBuffer, kFileBufferSize))
        {
            DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: failed to read .diagame file '%s'", path);
            return ComposeResult::kFileNotFound;
        }

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(fileBuffer, root))
        {
            DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: failed to parse .diagame JSON '%s'", path);
            return ComposeResult::kParseError;
        }

        // Build baseDir from the .diagame file path (everything up to and including last separator)
        const char* lastSlash = nullptr;
        for (const char* p = path; *p != '\0'; ++p)
        {
            if (*p == '/' || *p == '\\')
                lastSlash = p;
        }

        if (lastSlash != nullptr)
        {
            unsigned int dirLen = static_cast<unsigned int>(lastSlash - path) + 1;
            if (dirLen >= kPathBufferSize)
                dirLen = kPathBufferSize - 1;
            for (unsigned int i = 0; i < dirLen; ++i)
                baseDir[i] = path[i];
            // Strip trailing separator so BuildPath can add its own
            if (dirLen > 0 && (baseDir[dirLen - 1] == '/' || baseDir[dirLen - 1] == '\\'))
                --dirLen;
            baseDir[dirLen] = '\0';
        }
        else
        {
            // No directory component — use empty string (relative to cwd)
            baseDir[0] = '\0';
        }

        // Capture the full config block from .diagame
        if (root.isMember("config") && root["config"].isObject())
        {
            delete outManifest.diagameConfig;
            outManifest.diagameConfig = new Json::Value(root["config"]);
        }

        // Validate imports array
        if (!root.isMember("imports") || !root["imports"].isArray())
        {
            DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: .diagame '%s' missing 'imports' array", path);
            return ComposeResult::kParseError;
        }

        const Json::Value& imports = root["imports"];

        // --- Pass 1: load the base manifest (type == "manifest") ---
        bool baseLoaded = false;
        for (unsigned int i = 0; i < imports.size(); ++i)
        {
            const Json::Value& entry = imports[i];
            if (!entry.isMember("type") || !entry.isMember("path"))
                continue;
            if (entry["type"].asString() != "manifest")
                continue;

            char manifestPath[kPathBufferSize];
            BuildPath(baseDir, entry["path"].asCString(), manifestPath, kPathBufferSize);

            LoadResult loadResult = ApplicationManifestLoaderV2::LoadFromFile(manifestPath, outManifest);
            if (loadResult != LoadResult::kSuccess)
            {
                DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: failed to load base manifest '%s'", manifestPath);
                return (loadResult == LoadResult::kFileNotFound) ? ComposeResult::kFileNotFound : ComposeResult::kParseError;
            }

            baseLoaded = true;
            break; // Only one base manifest is expected
        }

        if (!baseLoaded)
        {
            DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: .diagame '%s' has no 'manifest' type import", path);
            return ComposeResult::kParseError;
        }

        // --- Pass 2: merge each stage import ---
        for (unsigned int i = 0; i < imports.size(); ++i)
        {
            const Json::Value& entry = imports[i];
            if (!entry.isMember("type") || !entry.isMember("path"))
                continue;
            if (entry["type"].asString() != "stage")
                continue;

            char diastagePath[kPathBufferSize];
            BuildPath(baseDir, entry["path"].asCString(), diastagePath, kPathBufferSize);

            ComposeResult result = MergeStage(diastagePath, baseDir, outManifest);
            if (result != ComposeResult::kSuccess)
            {
                DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: failed to merge stage '%s'", diastagePath);
                return result;
            }
        }

        return ComposeResult::kSuccess;
    }

    //-----------------------------------------------------------------------------
    // ReadPresets (internal)
    // Reads the optional "presets" array of a .diastage into the manifest's shared
    // preset table. Malformed field or preset: logged and ignored, stage still loads.
    //-----------------------------------------------------------------------------

    static std::string CompactJson(const Json::Value& v)
    {
        Json::FastWriter writer;
        std::string text = writer.write(v);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
            text.pop_back();
        return text;
    }

    static void ReadPresets(const Json::Value& stageRoot, const char* diastagePath, ApplicationManifestV3& manifest)
    {
        if (!stageRoot.isMember("presets"))
            return;

        const Json::Value& presetsJson = stageRoot["presets"];
        if (!presetsJson.isArray())
        {
            DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: .diastage '%s' 'presets' is not an array - ignoring presets", diastagePath);
            return;
        }

        if (!stageRoot.isMember("name") || !stageRoot["name"].isString())
        {
            DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: .diastage '%s' has presets but no 'name' - ignoring presets", diastagePath);
            return;
        }

        const std::string stageNameText = stageRoot["name"].asString();
        const Dia::Core::StringCRC stageName(stageNameText.c_str());
        StageDeclaration* stage = nullptr;
        for (unsigned int i = 0; i < manifest.stages.Size(); ++i)
        {
            if (manifest.stages[i].name == stageName)
            {
                stage = &manifest.stages[i];
                break;
            }
        }
        if (stage == nullptr)
        {
            DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: .diastage '%s' stage '%s' is not declared in the base manifest - ignoring presets",
                diastagePath, stageNameText.c_str());
            return;
        }
        if (stage->presetCount != 0)
        {
            DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: stage '%s' already has presets - ignoring presets from '%s'",
                stageNameText.c_str(), diastagePath);
            return;
        }

        stage->presetFirst = manifest.presets.Size();

        for (unsigned int i = 0; i < presetsJson.size(); ++i)
        {
            const Json::Value& entry = presetsJson[i];

            if (stage->presetCount >= kMaxPresetsPerStage)
            {
                DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: stage '%s' has more than %u presets - ignoring the rest (%s)",
                    stageNameText.c_str(), kMaxPresetsPerStage, diastagePath);
                break;
            }

            if (!entry.isObject() || !entry.isMember("name") || !entry["name"].isString())
            {
                DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: '%s' presets[%u] is not an object with a string 'name' - ignored", diastagePath, i);
                continue;
            }

            const std::string name = entry["name"].asString();
            if (name.empty() || name.size() > kMaxPresetNameLength)
            {
                DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: '%s' presets[%u] name '%s' is empty or longer than %u chars - ignored",
                    diastagePath, i, name.c_str(), kMaxPresetNameLength);
                continue;
            }

            std::string description;
            if (entry.isMember("description"))
            {
                if (!entry["description"].isString())
                {
                    DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: '%s' preset '%s' description is not a string - preset ignored", diastagePath, name.c_str());
                    continue;
                }
                description = entry["description"].asString();
                if (description.size() > kMaxPresetDescLength)
                {
                    DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: '%s' preset '%s' description longer than %u chars - preset ignored",
                        diastagePath, name.c_str(), kMaxPresetDescLength);
                    continue;
                }
            }

            const Dia::Core::StringCRC nameCrc(name.c_str());
            bool duplicate = false;
            for (unsigned int k = 0; k < stage->presetCount; ++k)
            {
                if (manifest.presets[stage->presetFirst + k].name == nameCrc)
                {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
            {
                DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: '%s' duplicate preset name '%s' - later one ignored", diastagePath, name.c_str());
                continue;
            }

            if (!entry.isMember("modules") || !entry["modules"].isObject())
            {
                DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: '%s' preset '%s' 'modules' is missing or not an object - preset ignored", diastagePath, name.c_str());
                continue;
            }
            const Json::Value& modulesJson = entry["modules"];

            if (modulesJson.size() > kMaxPresetModules)
            {
                DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: '%s' preset '%s' has %u modules (max %u) - preset ignored",
                    diastagePath, name.c_str(), modulesJson.size(), kMaxPresetModules);
                continue;
            }

            if (CompactJson(modulesJson).size() > kMaxPresetJsonBytes)
            {
                DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: '%s' preset '%s' JSON exceeds %u bytes - preset ignored",
                    diastagePath, name.c_str(), kMaxPresetJsonBytes);
                continue;
            }

            if (manifest.presets.Size() >= kMaxManifestPresets)
            {
                DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: manifest preset table full (%u) - preset '%s' in '%s' ignored",
                    kMaxManifestPresets, name.c_str(), diastagePath);
                continue;
            }

            // Validate module values and gather their compact text before touching the pool
            PresetDeclaration preset;
            std::string texts[kMaxPresetModules];
            bool valid = true;
            unsigned int needed = 0;
            const Json::Value::Members keys = modulesJson.getMemberNames();
            for (unsigned int k = 0; k < keys.size(); ++k)
            {
                const Json::Value& value = modulesJson[keys[k]];
                if (!value.isObject())
                {
                    DIA_LOG_WARNING("ApplicationFlow", "ManifestComposerV2: '%s' preset '%s' module '%s' override is not an object - preset ignored",
                        diastagePath, name.c_str(), keys[k].c_str());
                    valid = false;
                    break;
                }
                texts[k] = CompactJson(value);
                needed += static_cast<unsigned int>(texts[k].size()) + 1;
            }
            if (!valid)
                continue;

            if (manifest.presetOverridePoolUsed + needed > kPresetOverridePoolBytes)
            {
                DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: preset override pool full (%u/%u bytes) - preset '%s' in '%s' ignored",
                    manifest.presetOverridePoolUsed, kPresetOverridePoolBytes, name.c_str(), diastagePath);
                continue;
            }

            preset.name = nameCrc;
            memcpy(preset.description, description.c_str(), description.size() + 1);
            preset.moduleCount = static_cast<unsigned int>(keys.size());
            for (unsigned int k = 0; k < keys.size(); ++k)
            {
                preset.modules[k].instanceId = Dia::Core::StringCRC(keys[k].c_str());
                preset.modules[k].offset = manifest.presetOverridePoolUsed;
                preset.modules[k].length = static_cast<unsigned int>(texts[k].size());
                memcpy(manifest.presetOverridePool + manifest.presetOverridePoolUsed, texts[k].c_str(), texts[k].size() + 1);
                manifest.presetOverridePoolUsed += preset.modules[k].length + 1;
            }

            manifest.presets.Add(preset);
            ++stage->presetCount;
        }
    }

    //-----------------------------------------------------------------------------
    // ManifestComposerV2::MergeStage
    // Reads a .diastage file, resolves its referenced .diaapp, loads it into a
    // temporary ApplicationManifestV3, then appends each stage PU's modules to
    // the matching PU in outManifest (matched by instanceId).
    //-----------------------------------------------------------------------------

    ComposeResult ManifestComposerV2::MergeStage(const char* diastagePath, const char* baseDir, ApplicationManifestV3& outManifest)
    {
        // Read .diastage file
        char fileBuffer[kFileBufferSize];
        if (!ReadFile(diastagePath, fileBuffer, kFileBufferSize))
        {
            DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: failed to read .diastage file '%s'", diastagePath);
            return ComposeResult::kFileNotFound;
        }

        // Parse .diastage JSON
        Json::Value stageRoot;
        Json::Reader reader;
        if (!reader.parse(fileBuffer, stageRoot))
        {
            DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: failed to parse .diastage JSON '%s'", diastagePath);
            return ComposeResult::kParseError;
        }

        // Get "manifest" field — relative path to stage .diaapp
        if (!stageRoot.isMember("manifest") || !stageRoot["manifest"].isString())
        {
            DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: .diastage '%s' missing 'manifest' field", diastagePath);
            return ComposeResult::kParseError;
        }

        // Build full path to stage .diaapp relative to baseDir
        char stageManifestPath[kPathBufferSize];
        BuildPath(baseDir, stageRoot["manifest"].asCString(), stageManifestPath, kPathBufferSize);

        // Load stage .diaapp into a temporary manifest (heap-allocated — struct is large)
        std::unique_ptr<ApplicationManifestV3> stageManifestOwner(new ApplicationManifestV3());
        ApplicationManifestV3& stageManifest = *stageManifestOwner;
        LoadResult loadResult = ApplicationManifestLoaderV2::LoadFromFile(stageManifestPath, stageManifest);
        if (loadResult != LoadResult::kSuccess)
        {
            DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: failed to load stage .diaapp '%s'", stageManifestPath);
            return (loadResult == LoadResult::kFileNotFound) ? ComposeResult::kFileNotFound : ComposeResult::kParseError;
        }

        // Merge: append stage-declared streams (e.g. a stage-scoped umbrella module's
        // own EventStream/FrameStream) onto the base manifest's stream list. Mirrors
        // the modules-append below — no dedupe, same as modules.Add.
        for (unsigned int i = 0; i < stageManifest.streams.Size(); ++i)
        {
            outManifest.streams.Add(stageManifest.streams[i]);
        }

        // Merge: for each PU in stageManifest, find matching PU in outManifest and append modules
        for (unsigned int si = 0; si < stageManifest.processingUnits.Size(); ++si)
        {
            const ProcessingUnitDeclaration& stagePU = stageManifest.processingUnits[si];

            // Find matching PU in outManifest by instanceId
            bool found = false;
            for (unsigned int oi = 0; oi < outManifest.processingUnits.Size(); ++oi)
            {
                ProcessingUnitDeclaration& outPU = outManifest.processingUnits[oi];
                if (outPU.instanceId != stagePU.instanceId)
                    continue;

                found = true;

                // Append all stage modules to the matching PU
                for (unsigned int mi = 0; mi < stagePU.modules.Size(); ++mi)
                {
                    outPU.modules.Add(stagePU.modules[mi]);
                }

                break;
            }

            if (!found)
            {
                DIA_LOG_WARNING("ApplicationFlow",
                    "ManifestComposerV2: stage .diaapp '%s' references PU '%s' which does not exist in base manifest — skipping",
                    stageManifestPath, stagePU.instanceId.AsChar());
            }
        }

        ReadPresets(stageRoot, diastagePath, outManifest);

        return ComposeResult::kSuccess;
    }

    //-----------------------------------------------------------------------------
    // ManifestComposerV2::Compose
    // Public entry point. Reads the .diagame file and produces a fully merged
    // ApplicationManifestV3 combining the base manifest with all stage overlays.
    //-----------------------------------------------------------------------------

    ComposeResult ManifestComposerV2::Compose(const char* diagamePath, ApplicationManifestV3& outManifest)
    {
        char baseDir[kPathBufferSize];
        return LoadDiagame(diagamePath, outManifest, baseDir);
    }

}} // namespace Dia::ApplicationFlow
