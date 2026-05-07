#include "CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.h"

#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>

#include <cstdio>

namespace
{
	bool ReadFileToString(const char* path, char* buffer, unsigned int bufferSize)
	{
		FILE* f = nullptr;
		fopen_s(&f, path, "rb");
		if (!f)
			return false;

		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);

		if (size <= 0 || static_cast<unsigned int>(size) >= bufferSize)
		{
			fclose(f);
			return false;
		}

		fread(buffer, 1, size, f);
		buffer[size] = '\0';
		fclose(f);
		return true;
	}

	void GetDirectoryFromPath(const char* filePath, char* outDir, unsigned int outDirSize)
	{
		const char* lastSlash = nullptr;
		for (const char* p = filePath; *p; ++p)
		{
			if (*p == '/' || *p == '\\')
				lastSlash = p;
		}
		if (lastSlash)
		{
			unsigned int len = static_cast<unsigned int>(lastSlash - filePath) + 1;
			if (len >= outDirSize)
				len = outDirSize - 1;
			memcpy(outDir, filePath, len);
			outDir[len] = '\0';
		}
		else
		{
			outDir[0] = '\0';
		}
	}

	void ResolvePath(const char* baseDir, const char* relativePath, Dia::Core::Containers::String512& outPath)
	{
		if (relativePath[0] == '.' && relativePath[1] == '/')
		{
			outPath = Dia::Core::Containers::String512("%s%s", baseDir, relativePath + 2);
		}
		else if (relativePath[0] == '.' && relativePath[1] == '\0')
		{
			outPath = Dia::Core::Containers::String512("%s", baseDir);
		}
		else
		{
			outPath = Dia::Core::Containers::String512("%s%s", baseDir, relativePath);
		}
	}
}

namespace Cluiche
{
	namespace Main
	{
		const Dia::Core::StringCRC AssetServiceModule::kTypeId("Main::AssetServiceModule");

		AssetServiceModule::AssetServiceModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId)
			: Dia::Application::Module(associatedProcessingUnit, instanceId, Dia::Application::Module::RunningEnum::kIdle)
			, mPendingAcknowledgements(0)
		{}

		Dia::Application::StateObject::OpertionResponse AssetServiceModule::DoStart(const IStartData* startData)
		{
			std::string exePath;
			Dia::Core::Path::ExePath(exePath);

			mDeployRoot = Dia::Core::Containers::String512("%s/assets/", exePath.c_str());

			// Load runtime manifest
			Dia::Core::FilePath::ResoledFilePath manifestPath("%sassets.runtime.json", mDeployRoot.AsCStr());
			mRuntime.LoadManifest(manifestPath);
			mRuntime.RegisterListener(this);

			// Parse .diagame for config (path_aliases, ultralight)
			Dia::Core::Containers::String512 diagamePath("%sglobal/cluichetest.diagame", mDeployRoot.AsCStr());

			static const unsigned int kMaxFileSize = 4096;
			char fileBuffer[kMaxFileSize];
			if (ReadFileToString(diagamePath.AsCStr(), fileBuffer, kMaxFileSize))
			{
				Json::Value root;
				Json::Reader reader;
				if (reader.parse(fileBuffer, root, false))
				{
					// Parse stage imports to build stageId→diastage path map
					if (root.isMember("imports") && root["imports"].isArray())
					{
						char diagameDir[512];
						GetDirectoryFromPath(diagamePath.AsCStr(), diagameDir, sizeof(diagameDir));

						const Json::Value& imports = root["imports"];
						for (unsigned int i = 0; i < imports.size(); ++i)
						{
							const Json::Value& entry = imports[i];
							if (entry.isMember("type") && entry["type"].asString() == "stage" && entry.isMember("path"))
							{
								Dia::Core::Containers::String512 resolvedStagePath;
								ResolvePath(diagameDir, entry["path"].asCString(), resolvedStagePath);

								// Read the .diastage to get the stage name for the ID
								char stageFileBuffer[kMaxFileSize];
								if (ReadFileToString(resolvedStagePath.AsCStr(), stageFileBuffer, kMaxFileSize))
								{
									Json::Value stageRoot;
									if (reader.parse(stageFileBuffer, stageRoot, false) && stageRoot.isMember("name"))
									{
										Dia::Core::Containers::String512 stageIdStr("stage.%s",
											stageRoot["name"].asCString());
										// Lowercase the stage name for the ID
										for (unsigned int c = 6; c < stageIdStr.Length(); ++c)
										{
											char ch = stageIdStr.AsCStr()[c];
											if (ch >= 'A' && ch <= 'Z')
												const_cast<char*>(stageIdStr.AsCStr())[c] = ch + 32;
										}

										StagePathEntry stageEntry;
										stageEntry.mStageId = Dia::Core::StringCRC(stageIdStr.AsCStr());
										stageEntry.mDiastagePath = resolvedStagePath;
										mStagePathMap.Add(stageEntry);
									}
								}
							}
						}
					}

					if (root.isMember("config"))
					{
						const Json::Value& config = root["config"];

						// Parse path_aliases
						if (config.isMember("path_aliases") && config["path_aliases"].isObject())
						{
							char diagameDir2[512];
							GetDirectoryFromPath(diagamePath.AsCStr(), diagameDir2, sizeof(diagameDir2));

							const Json::Value& aliases = config["path_aliases"];
							Json::Value::Members members = aliases.getMemberNames();
							for (unsigned int i = 0; i < members.size(); ++i)
							{
								const std::string& aliasName = members[i];
								const char* relativePath = aliases[aliasName].asCString();

								PathAliasEntry entry;
								entry.mAlias = Dia::Core::Containers::String32(aliasName.c_str());
								ResolvePath(diagameDir2, relativePath, entry.mResolvedPath);

								mGlobalAliases.Add(entry);
							}
						}

						// Parse ultralight config
						if (config.isMember("ultralight") && config["ultralight"].isObject())
						{
							const Json::Value& ul = config["ultralight"];
							if (ul.isMember("resource_path_prefix") && ul["resource_path_prefix"].isString())
							{
								mUltralightResourcePrefix = Dia::Core::Containers::String512(
									"%s%s", mDeployRoot.AsCStr(), ul["resource_path_prefix"].asCString());
							}
						}
					}
				}
			}

			RegisterGlobalAliases();

			return StateObject::OpertionResponse::kImmediate;
		}

		void AssetServiceModule::DoStop()
		{
			mRuntime.UnregisterListener(this);
			mRuntime.Reset();
		}

		void AssetServiceModule::RegisterGlobalAliases()
		{
			for (unsigned int i = 0; i < mGlobalAliases.Size(); ++i)
			{
				const PathAliasEntry& entry = mGlobalAliases[i];
				Dia::Core::Path::Alias alias(entry.mAlias.AsCStr());
				Dia::Core::Path::String pathStr(entry.mResolvedPath.AsCStr());
				Dia::Core::PathStore::RegisterToStore(alias, pathStr);
			}
		}

		void AssetServiceModule::RegisterStageAliases(const char* diastagePath)
		{
			UnregisterStageAliases();

			static const unsigned int kMaxFileSize = 4096;
			char fileBuffer[kMaxFileSize];
			if (!ReadFileToString(diastagePath, fileBuffer, kMaxFileSize))
			{
				DIA_LOG_ERROR("AssetService", "Failed to read .diastage: %s", diastagePath);
				return;
			}

			Json::Value root;
			Json::Reader reader;
			if (!reader.parse(fileBuffer, root, false) || !root.isMember("config"))
				return;

			const Json::Value& config = root["config"];
			if (!config.isMember("path_aliases") || !config["path_aliases"].isObject())
				return;

			char stageDir[512];
			GetDirectoryFromPath(diastagePath, stageDir, sizeof(stageDir));

			const Json::Value& aliases = config["path_aliases"];
			Json::Value::Members members = aliases.getMemberNames();
			for (unsigned int i = 0; i < members.size(); ++i)
			{
				const std::string& aliasName = members[i];
				const char* relativePath = aliases[aliasName].asCString();

				PathAliasEntry entry;
				entry.mAlias = Dia::Core::Containers::String32(aliasName.c_str());
				ResolvePath(stageDir, relativePath, entry.mResolvedPath);

				mStageAliases.Add(entry);

				Dia::Core::Path::Alias alias(entry.mAlias.AsCStr());
				Dia::Core::Path::String pathStr(entry.mResolvedPath.AsCStr());
				Dia::Core::PathStore::RegisterToStore(alias, pathStr);
			}
		}

		void AssetServiceModule::UnregisterStageAliases()
		{
			for (unsigned int i = 0; i < mStageAliases.Size(); ++i)
			{
				Dia::Core::Path::Alias alias(mStageAliases[i].mAlias.AsCStr());
				Dia::Core::PathStore::UnregisterFromStore(alias);
			}
			mStageAliases.RemoveAll();
		}

		void AssetServiceModule::RequestGlobalLoad()
		{
			mCurrentLoadStageId = Dia::Core::StringCRC("stage.global");
			mPendingAcknowledgements = 0;

			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> stageAssets;
			mRuntime.GetStageDependencies(mCurrentLoadStageId, stageAssets);
			mPendingAcknowledgements = stageAssets.Size();

			mRuntime.RequestStageLoad(mCurrentLoadStageId);
		}

		void AssetServiceModule::RequestStageLoad(const Dia::Core::StringCRC& stageId)
		{
			mCurrentLoadStageId = stageId;
			mPendingAcknowledgements = 0;

			// Look up the .diastage path from the map built at startup
			for (unsigned int i = 0; i < mStagePathMap.Size(); ++i)
			{
				if (mStagePathMap[i].mStageId == stageId)
				{
					RegisterStageAliases(mStagePathMap[i].mDiastagePath.AsCStr());
					break;
				}
			}

			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> stageAssets;
			mRuntime.GetStageDependencies(stageId, stageAssets);
			mPendingAcknowledgements = stageAssets.Size();

			mRuntime.RequestStageLoad(stageId);
		}

		void AssetServiceModule::RequestStageUnload(const Dia::Core::StringCRC& stageId)
		{
			UnregisterStageAliases();
			mRuntime.RequestStageUnload(stageId);
			mCurrentLoadStageId = Dia::Core::StringCRC();
			mPendingAcknowledgements = 0;
		}

		bool AssetServiceModule::IsLoadComplete() const
		{
			return mPendingAcknowledgements == 0;
		}

		const Dia::AssetRuntime::AssetRuntime& AssetServiceModule::GetRuntime() const
		{
			return mRuntime;
		}

		const Dia::Core::Containers::String512& AssetServiceModule::GetUltralightResourcePrefix() const
		{
			return mUltralightResourcePrefix;
		}

		void AssetServiceModule::OnAssetReady(const Dia::Core::StringCRC& assetId,
		                                      const Dia::Core::Containers::String512& resolvedPath)
		{
			mRuntime.AcknowledgeAssetLoaded(assetId);
			if (mPendingAcknowledgements > 0)
			{
				--mPendingAcknowledgements;
			}
		}

		void AssetServiceModule::OnAssetUnloading(const Dia::Core::StringCRC& assetId)
		{
			mRuntime.AcknowledgeAssetUnloaded(assetId);
		}

		void AssetServiceModule::OnAssetLoadFailed(const Dia::Core::StringCRC& assetId)
		{
			if (mPendingAcknowledgements > 0)
			{
				--mPendingAcknowledgements;
			}
		}
	}
}

namespace { using _AssetServiceModule = Cluiche::Main::AssetServiceModule; }
DIA_REGISTER_MODULE(_AssetServiceModule) {
	return new Cluiche::Main::AssetServiceModule(pu, instanceId);
}
