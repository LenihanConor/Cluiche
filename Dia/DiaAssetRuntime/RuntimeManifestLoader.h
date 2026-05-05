#pragma once

#include "DiaAssetRuntime/RuntimeAssetEntry.h"
#include "DiaAssetRuntime/RuntimeStageEntry.h"

#include "DiaCore/FilePath/FilePath.h"
#include "DiaCore/Containers/HashTables/HashTableC.h"
#include "DiaCore/Containers/HashTables/HashTableHashFunctionData.h"

namespace Dia
{
    namespace AssetRuntime
    {
        class RuntimeManifestLoader
        {
        public:
            class AssetEntryHashFunctor
            {
            public:
                typedef Dia::Core::StringCRC Key;
                typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

                unsigned int GetHashIndex(const Key& key, const TableData* tableData) const;
            };

            class StageEntryHashFunctor
            {
            public:
                typedef Dia::Core::StringCRC Key;
                typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

                unsigned int GetHashIndex(const Key& key, const TableData* tableData) const;
            };

            static const unsigned int kMaxAssets  = 512;
            static const unsigned int kMaxStages  = 32;
            static const unsigned int kAssetTableSize  = 768; // kMaxAssets * 1.5
            static const unsigned int kStageTableSize  = 48;  // kMaxStages * 1.5

            typedef Dia::Core::Containers::HashTableC<
                Dia::Core::StringCRC,
                RuntimeAssetEntry,
                AssetEntryHashFunctor,
                kMaxAssets,
                kAssetTableSize> AssetTable;

            typedef Dia::Core::Containers::HashTableC<
                Dia::Core::StringCRC,
                RuntimeStageEntry,
                StageEntryHashFunctor,
                kMaxStages,
                kStageTableSize> StageTable;

            bool Load(const Dia::Core::FilePath& manifestPath,
                      AssetTable& assetTable,
                      StageTable& stageTable);
        };
    }
}
