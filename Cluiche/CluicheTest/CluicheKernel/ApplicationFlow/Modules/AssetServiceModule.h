#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntime/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String512.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "CluicheKernel/ApplicationFlow/Modules/SFMLTextureHandler.h"
#include "CluicheKernel/ApplicationFlow/Modules/UltralightUIHandler.h"

namespace Cluiche
{
	namespace Main
	{
		class DefaultAssetHandler : public Dia::AssetRuntime::IAssetTypeHandler
		{
		public:
			virtual void Load(const Dia::Core::StringCRC& assetId,
			                  const Dia::Core::Containers::String512& resolvedPath,
			                  Dia::AssetRuntime::IAssetLoadCallback* callback) override;

			virtual void Unload(const Dia::Core::StringCRC& assetId) override;
		};

		class AssetServiceModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			AssetServiceModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId = kTypeId);

			void RequestGlobalLoad();
			void RequestStageLoad(const Dia::Core::StringCRC& stageId);
			void RequestStageUnload(const Dia::Core::StringCRC& stageId);

			bool IsLoadComplete() const;

			const Dia::AssetRuntime::AssetRuntime& GetRuntime() const;
			const Dia::Core::Containers::String512& GetUltralightResourcePrefix() const;

		private:
			virtual StateObject::OpertionResponse DoStart(const IStartData* startData) override;
			virtual void DoStop() override;

			void RegisterGlobalAliases();
			void RegisterStageAliases(const char* diastagePath);
			void UnregisterStageAliases();
			void EnsureHandlerDependencies();

			struct PathAliasEntry
			{
				Dia::Core::Containers::String32 mAlias;
				Dia::Core::Containers::String512 mResolvedPath;
			};

			struct StagePathEntry
			{
				Dia::Core::StringCRC mStageId;
				Dia::Core::Containers::String512 mDiastagePath;
			};

			Dia::AssetRuntime::AssetRuntime mRuntime;
			DefaultAssetHandler mDefaultHandler;
			SFMLTextureHandler mTextureHandler;
			UltralightUIHandler mUIHandler;
			Dia::Core::StringCRC mCurrentLoadStageId;

			Dia::Core::Containers::DynamicArrayC<PathAliasEntry, 16> mGlobalAliases;
			Dia::Core::Containers::DynamicArrayC<PathAliasEntry, 16> mStageAliases;
			Dia::Core::Containers::DynamicArrayC<StagePathEntry, 8> mStagePathMap;
			Dia::Core::Containers::String512 mUltralightResourcePrefix;
			Dia::Core::Containers::String512 mDeployRoot;
			bool mHandlerDependenciesWired;
		};
	}
}
