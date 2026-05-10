#include "ManifestComposerV2.h"
#include "ApplicationManifestLoaderV2.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/Json/external/json/json.h>

#include <fstream>

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

    ComposeResult ManifestComposerV2::LoadDiagame(const char* path, ApplicationManifestV2& outManifest, char* baseDir)
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
    // ManifestComposerV2::MergeStage
    // Reads a .diastage file, resolves its referenced .diaapp, loads it into a
    // temporary ApplicationManifestV2, then appends each stage PU's modules to
    // the matching PU in outManifest (matched by instanceId).
    //-----------------------------------------------------------------------------

    ComposeResult ManifestComposerV2::MergeStage(const char* diastagePath, const char* baseDir, ApplicationManifestV2& outManifest)
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

        // Load stage .diaapp into a temporary manifest
        ApplicationManifestV2 stageManifest;
        LoadResult loadResult = ApplicationManifestLoaderV2::LoadFromFile(stageManifestPath, stageManifest);
        if (loadResult != LoadResult::kSuccess)
        {
            DIA_LOG_ERROR("ApplicationFlow", "ManifestComposerV2: failed to load stage .diaapp '%s'", stageManifestPath);
            return (loadResult == LoadResult::kFileNotFound) ? ComposeResult::kFileNotFound : ComposeResult::kParseError;
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

        return ComposeResult::kSuccess;
    }

    //-----------------------------------------------------------------------------
    // ManifestComposerV2::Compose
    // Public entry point. Reads the .diagame file and produces a fully merged
    // ApplicationManifestV2 combining the base manifest with all stage overlays.
    //-----------------------------------------------------------------------------

    ComposeResult ManifestComposerV2::Compose(const char* diagamePath, ApplicationManifestV2& outManifest)
    {
        char baseDir[kPathBufferSize];
        return LoadDiagame(diagamePath, outManifest, baseDir);
    }

}} // namespace Dia::ApplicationFlow
