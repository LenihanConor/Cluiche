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
				kRegistered = 0,
				kStaged,
				kLoaded,
				kUnloading,
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
					, mState(AssetStateEnum::kRegistered)
					, mScope(AssetScopeEnum::kGlobal)
					, mRefCount(0)
					, mDeployPath()
				{}
			};

			inline const char* AssetStateEnumToString(AssetStateEnum state)
			{
				switch (state)
				{
					case AssetStateEnum::kRegistered: return "Registered";
					case AssetStateEnum::kStaged: return "Staged";
					case AssetStateEnum::kLoaded: return "Loaded";
					case AssetStateEnum::kUnloading: return "Unloading";
					default: return "Unknown";
				}
			}

			inline AssetStateEnum StringToAssetStateEnum(const char* str)
			{
				if (!str) return AssetStateEnum::kRegistered;
				if (strcmp(str, "Staged") == 0) return AssetStateEnum::kStaged;
				if (strcmp(str, "Loaded") == 0) return AssetStateEnum::kLoaded;
				if (strcmp(str, "Unloading") == 0) return AssetStateEnum::kUnloading;
				return AssetStateEnum::kRegistered;
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
