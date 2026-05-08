#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String512.h>

#include <cstring>

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Editor
		{
			enum class AssetStateEnum
			{
				kNull = 0,
				kStaged,
				kLoading,
				kLoaded,
				kFailed,
				kUnloaded,
				kCount
			};

			enum class AssetScopeEnum
			{
				kGlobal = 0,
				kStage
			};

			struct AssetStateRow
			{
				Dia::Core::StringCRC mAssetId;
				Dia::Core::StringCRC mStageId;  // stage that owns this asset (empty for global)
				AssetStateEnum mState;
				AssetScopeEnum mScope;
				unsigned int mRefCount;
				Dia::Core::Containers::String512 mDeployPath;

				AssetStateRow()
					: mAssetId()
					, mStageId()
					, mState(AssetStateEnum::kNull)
					, mScope(AssetScopeEnum::kGlobal)
					, mRefCount(0)
					, mDeployPath()
				{}
			};

			inline const char* AssetStateEnumToString(AssetStateEnum state)
			{
				switch (state)
				{
					case AssetStateEnum::kNull: return "Null";
					case AssetStateEnum::kStaged: return "Staged";
					case AssetStateEnum::kLoading: return "Loading";
					case AssetStateEnum::kLoaded: return "Loaded";
					case AssetStateEnum::kFailed: return "Failed";
					case AssetStateEnum::kUnloaded: return "Unloaded";
					default: return "Unknown";
				}
			}

			inline AssetStateEnum StringToAssetStateEnum(const char* str)
			{
				if (!str) return AssetStateEnum::kNull;
				if (strcmp(str, "Staged") == 0) return AssetStateEnum::kStaged;
				if (strcmp(str, "Loading") == 0) return AssetStateEnum::kLoading;
				if (strcmp(str, "Loaded") == 0) return AssetStateEnum::kLoaded;
				if (strcmp(str, "Failed") == 0) return AssetStateEnum::kFailed;
				if (strcmp(str, "Unloaded") == 0) return AssetStateEnum::kUnloaded;
				return AssetStateEnum::kNull;
			}

			inline const char* AssetScopeEnumToString(AssetScopeEnum scope)
			{
				switch (scope)
				{
					case AssetScopeEnum::kGlobal: return "Global";
					case AssetScopeEnum::kStage: return "Stage";
					default: return "Unknown";
				}
			}

			inline AssetScopeEnum StringToAssetScopeEnum(const char* str)
			{
				if (!str) return AssetScopeEnum::kGlobal;
				if (strcmp(str, "Stage") == 0 || strcmp(str, "stage") == 0) return AssetScopeEnum::kStage;
				return AssetScopeEnum::kGlobal;
			}
		}
	}
}
