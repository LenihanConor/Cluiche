#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntime/IAssetStateListener.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String512.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Cluiche
{
	namespace Main
	{
		class AssetServiceModule : public Dia::Application::Module,
		                           public Dia::AssetRuntime::IAssetStateListener
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

			// IAssetStateListener
			virtual void OnAssetReady(const Dia::Core::StringCRC& assetId,
			                          const Dia::Core::Containers::String512& resolvedPath) override;
			virtual void OnAssetUnloading(const Dia::Core::StringCRC& assetId) override;
			virtual void OnAssetLoadFailed(const Dia::Core::StringCRC& assetId) override;

			void RegisterGlobalAliases();
			void RegisterStageAliases(const char* diastagePath);
			void UnregisterStageAliases();

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
			Dia::Core::StringCRC mCurrentLoadStageId;
			unsigned int mPendingAcknowledgements;

			Dia::Core::Containers::DynamicArrayC<PathAliasEntry, 16> mGlobalAliases;
			Dia::Core::Containers::DynamicArrayC<PathAliasEntry, 16> mStageAliases;
			Dia::Core::Containers::DynamicArrayC<StagePathEntry, 8> mStagePathMap;
			Dia::Core::Containers::String512 mUltralightResourcePrefix;
			Dia::Core::Containers::String512 mDeployRoot;
		};
	}
}
