#include "DiaAssetRuntime/RuntimeManifestLoader.h"

#include "DiaCore/Json/external/json/json.h"
#include "DiaCore/FilePath/FilePath.h"

#include <DiaLogger/DiaLog.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

namespace Dia
{
    namespace AssetRuntime
    {
        namespace
        {
            static const unsigned int kMaxManifestSize = 256 * 1024;

            bool ReadFileToBuffer(const char* path, char* buffer, unsigned int bufferSize)
            {
                FILE* f = nullptr;
#if defined(_MSC_VER)
                fopen_s(&f, path, "rb");
#else
                f = fopen(path, "rb");
#endif
                if (!f)
                    return false;

                fseek(f, 0, SEEK_END);
                long fileSize = ftell(f);
                fseek(f, 0, SEEK_SET);

                if (fileSize <= 0 || static_cast<unsigned long>(fileSize) >= bufferSize)
                {
                    fclose(f);
                    return false;
                }

                size_t bytesRead = fread(buffer, 1, static_cast<size_t>(fileSize), f);
                fclose(f);
                buffer[bytesRead] = '\0';
                return true;
            }

            void GetDirectoryFromPath(const char* path, char* dirOut, unsigned int dirOutSize)
            {
                if (!path || path[0] == '\0')
                {
                    dirOut[0] = '\0';
                    return;
                }

                int len = static_cast<int>(strlen(path));
                int lastSlash = -1;
                for (int i = len - 1; i >= 0; --i)
                {
                    if (path[i] == '/' || path[i] == '\\')
                    {
                        lastSlash = i;
                        break;
                    }
                }

                if (lastSlash < 0)
                {
                    dirOut[0] = '\0';
                    return;
                }

                int copyLen = lastSlash + 1;
                if (copyLen >= static_cast<int>(dirOutSize))
                    copyLen = static_cast<int>(dirOutSize) - 1;

#if defined(_MSC_VER)
                strncpy_s(dirOut, dirOutSize, path, static_cast<size_t>(copyLen));
#else
                strncpy(dirOut, path, static_cast<size_t>(copyLen));
                dirOut[copyLen] = '\0';
#endif
            }

        } // anonymous namespace

        //------------------------------------------------------------------------------------
        // AssetEntryHashFunctor
        //------------------------------------------------------------------------------------
        unsigned int RuntimeManifestLoader::AssetEntryHashFunctor::GetHashIndex(const Key& key, const TableData* tableData) const
        {
            static const unsigned int sTranslationToTableSpace = Dia::Core::CRC::MaxCRC() / kAssetTableSize;
            DIA_ASSERT(key.Value() != 0, "Cannot hash a zero CRC key");
            return static_cast<unsigned int>(floorf(static_cast<float>(key.Value()) / static_cast<float>(sTranslationToTableSpace)));
        }

        //------------------------------------------------------------------------------------
        // StageEntryHashFunctor
        //------------------------------------------------------------------------------------
        unsigned int RuntimeManifestLoader::StageEntryHashFunctor::GetHashIndex(const Key& key, const TableData* tableData) const
        {
            static const unsigned int sTranslationToTableSpace = Dia::Core::CRC::MaxCRC() / kStageTableSize;
            DIA_ASSERT(key.Value() != 0, "Cannot hash a zero CRC key");
            return static_cast<unsigned int>(floorf(static_cast<float>(key.Value()) / static_cast<float>(sTranslationToTableSpace)));
        }

        //------------------------------------------------------------------------------------
        // RuntimeManifestLoader::Load
        //------------------------------------------------------------------------------------
        bool RuntimeManifestLoader::Load(const Dia::Core::FilePath& manifestPath,
                                         AssetTable& assetTable,
                                         StageTable& stageTable)
        {
            Dia::Core::FilePath::ResoledFilePath resolvedPath;
            manifestPath.Resolve(resolvedPath);
            return Load(resolvedPath, assetTable, stageTable);
        }

        bool RuntimeManifestLoader::Load(const Dia::Core::FilePath::ResoledFilePath& resolvedManifestPath,
                                         AssetTable& assetTable,
                                         StageTable& stageTable)
        {
            const char* manifestPathStr = resolvedManifestPath.AsCStr();

            char* manifestBuffer = new char[kMaxManifestSize];
            if (!ReadFileToBuffer(manifestPathStr, manifestBuffer, kMaxManifestSize))
            {
                delete[] manifestBuffer;
                DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: failed to read manifest: %s", manifestPathStr);
                return false;
            }

            // Get parent directory for relative path resolution
            char deployRoot[512];
            GetDirectoryFromPath(manifestPathStr, deployRoot, sizeof(deployRoot));

            Json::Value root;
            Json::Reader reader;
            bool parseOk = reader.parse(manifestBuffer, root, false);
            delete[] manifestBuffer;

            if (!parseOk)
            {
                DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: JSON parse error in %s", manifestPathStr);
                return false;
            }

            // Parse assets
            if (root.isMember("assets") && root["assets"].isArray())
            {
                const Json::Value& assets = root["assets"];
                for (unsigned int i = 0; i < assets.size(); ++i)
                {
                    const Json::Value& obj = assets[i];
                    if (!obj.isObject())
                        continue;

                    if (!obj.isMember("id") || !obj["id"].isString())
                    {
                        DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: asset at index %u missing 'id'", i);
                        return false;
                    }
                    if (!obj.isMember("deploy_path") || !obj["deploy_path"].isString())
                    {
                        DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: asset '%s' missing 'deploy_path'", obj["id"].asCString());
                        return false;
                    }

                    const char* idStr        = obj["id"].asCString();
                    const char* scopeStr     = obj.isMember("scope") && obj["scope"].isString() ? obj["scope"].asCString() : "stage";
                    const char* deployRelStr = obj["deploy_path"].asCString();

                    Dia::Core::StringCRC id(idStr);

                    if (assetTable.ContainsKey(id))
                    {
                        DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: duplicate asset ID '%s'", idStr);
                        return false;
                    }

                    if (assetTable.IsFull())
                    {
                        DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: asset table capacity (%u) exceeded", kMaxAssets);
                        return false;
                    }

                    // Parse scope
                    AssetScope scope = AssetScope::kStage;
                    if (strcmp(scopeStr, "global") == 0)
                        scope = AssetScope::kGlobal;
                    else if (strcmp(scopeStr, "stage") != 0)
                    {
                        DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: unknown scope '%s' for asset '%s'", scopeStr, idStr);
                        return false;
                    }

                    // Resolve absolute path: deployRoot + relative
                    RuntimeAssetEntry entry;
                    entry.mId    = id;
                    entry.mScope = scope;

                    char absPath[512];
#if defined(_MSC_VER)
                    strncpy_s(absPath, sizeof(absPath), deployRoot, _TRUNCATE);
                    strncat_s(absPath, sizeof(absPath), deployRelStr, _TRUNCATE);
#else
                    strncpy(absPath, deployRoot, sizeof(absPath) - 1);
                    absPath[sizeof(absPath) - 1] = '\0';
                    strncat(absPath, deployRelStr, sizeof(absPath) - strlen(absPath) - 1);
#endif
                    entry.mDeployPath = Dia::Core::Containers::String512(absPath);

                    assetTable.Add(id, entry);
                }
            }

            // Parse stages
            if (root.isMember("stages") && root["stages"].isArray())
            {
                const Json::Value& stages = root["stages"];
                for (unsigned int i = 0; i < stages.size(); ++i)
                {
                    const Json::Value& obj = stages[i];
                    if (!obj.isObject())
                        continue;

                    if (!obj.isMember("id") || !obj["id"].isString())
                    {
                        DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: stage at index %u missing 'id'", i);
                        return false;
                    }

                    const char* stageIdStr = obj["id"].asCString();
                    Dia::Core::StringCRC stageId(stageIdStr);

                    if (stageTable.ContainsKey(stageId))
                    {
                        DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: duplicate stage ID '%s'", stageIdStr);
                        return false;
                    }

                    if (stageTable.IsFull())
                    {
                        DIA_LOG_ERROR("AssetRuntime", "RuntimeManifestLoader: stage table capacity (%u) exceeded", kMaxStages);
                        return false;
                    }

                    RuntimeStageEntry stageEntry;
                    stageEntry.mId = stageId;

                    if (obj.isMember("assets") && obj["assets"].isArray())
                    {
                        const Json::Value& assetIds = obj["assets"];
                        if (assetIds.size() > stageEntry.mAssetIds.Capacity())
                        {
                            DIA_LOG_ERROR("AssetRuntime",
                                "RuntimeManifestLoader: stage '%s' has %u assets, exceeds capacity %u",
                                stageIdStr, assetIds.size(), stageEntry.mAssetIds.Capacity());
                            return false;
                        }
                        for (unsigned int a = 0; a < assetIds.size(); ++a)
                        {
                            if (assetIds[a].isString())
                                stageEntry.mAssetIds.Add(Dia::Core::StringCRC(assetIds[a].asCString()));
                        }
                    }

                    stageTable.Add(stageId, stageEntry);
                }
            }

            return true;
        }

    } // namespace AssetRuntime
} // namespace Dia
