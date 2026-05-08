#include "CluicheKernel/ApplicationFlow/Modules/AssetServiceModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>
#include <DiaGame/DiaGameManifest.h>
#include <DiaGame/DiaGameManifestLoader.h>
#include <DiaSFML/RenderWindow.h>
#include <DiaUIUltralight/UltralightUISystem.h>

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>

#include <cstdio>

namespace
{
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
}

namespace Cluiche
{
	namespace Main
	{
		const Dia::Core::StringCRC AssetServiceModule::kTypeId("Main::AssetServiceModule");

		AssetServiceModule::AssetServiceModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId)
			: Dia::Application::Module(associatedProcessingUnit, instanceId, Dia::Application::Module::RunningEnum::kIdle)
		{}

		Dia::Application::StateObject::OpertionResponse AssetServiceModule::DoStart(const IStartData* startData)
		{
			std::string exePath;
			Dia::Core::Path::ExePath(exePath);

			mDeployRoot = Dia::Core::Containers::String512("%s/assets/", exePath.c_str());

			const char* deployRoot = mDeployRoot.AsCStr();

			// Load runtime manifest
			Dia::Core::FilePath::ResoledFilePath manifestPath("%sassets.runtime.json", deployRoot);
			mRuntime.LoadManifest(manifestPath);


			// Load .diagame via DiaGameManifestLoader
			Dia::Core::Containers::String512 diagamePath("%scluichetest.diagame", deployRoot);
			Dia::Game::DiaGameManifest gameManifest;

			auto loadResult = Dia::Game::DiaGameManifestLoader::LoadGameFile(diagamePath.AsCStr(), gameManifest);
			if (loadResult != Dia::Application::ManifestValidationResult::kSuccess)
			{
				DIA_LOG_ERROR("AssetService", "Failed to load .diagame file: %s", diagamePath.AsCStr());
			}
			else
			{
				char diagameDir[512];
				GetDirectoryFromPath(diagamePath.AsCStr(), diagameDir, sizeof(diagameDir));

				// Build stageId-to-diastage path map from typed imports
				for (unsigned int i = 0; i < gameManifest.imports.Size(); ++i)
				{
					const Dia::Application::TypedImport& import = gameManifest.imports[i];
					if (import.type != Dia::Application::TypedImport::ImportType::kStage)
						continue;

					Dia::Core::Containers::String512 resolvedStagePath;
					Dia::Core::Path::ResolveRelative(diagameDir, import.path.AsCStr(), resolvedStagePath);

					Dia::Game::DiaStageManifest stageManifest;
					auto stageResult = Dia::Game::DiaGameManifestLoader::LoadStageFile(resolvedStagePath.AsCStr(), stageManifest);
					if (stageResult != Dia::Application::ManifestValidationResult::kSuccess)
					{
						DIA_LOG_ERROR("AssetService", "Failed to load .diastage file: %s", resolvedStagePath.AsCStr());
						continue;
					}

					// Convert PascalCase name to snake_case stage ID
					const char* rawName = stageManifest.name.AsCStr();
					Dia::Core::Containers::String512 stageIdStr("stage.");
					for (unsigned int c = 0; rawName[c] != '\0'; ++c)
					{
						char ch = rawName[c];
						if (ch >= 'A' && ch <= 'Z')
						{
							if (c > 0)
								stageIdStr.Append('_');
							stageIdStr.Append(static_cast<char>(ch + 32));
						}
						else
						{
							stageIdStr.Append(ch);
						}
					}

					StagePathEntry stageEntry;
					stageEntry.mStageId = Dia::Core::StringCRC(stageIdStr.AsCStr());
					stageEntry.mDiastagePath = resolvedStagePath;
					mStagePathMap.Add(stageEntry);
				}

				// Parse config from rawConfig (path_aliases, ultralight)
				if (gameManifest.rawConfig)
				{
					const Json::Value& config = *gameManifest.rawConfig;

					if (config.isMember("path_aliases") && config["path_aliases"].isObject())
					{
						const Json::Value& aliases = config["path_aliases"];
						Json::Value::Members members = aliases.getMemberNames();
						for (unsigned int i = 0; i < members.size(); ++i)
						{
							const std::string& aliasName = members[i];
							const char* relativePath = aliases[aliasName].asCString();

							PathAliasEntry entry;
							entry.mAlias = Dia::Core::Containers::String32(aliasName.c_str());
							Dia::Core::Path::ResolveRelative(diagameDir, relativePath, entry.mResolvedPath);

							mGlobalAliases.Add(entry);
						}
					}

					if (config.isMember("ultralight") && config["ultralight"].isObject())
					{
						const Json::Value& ul = config["ultralight"];
						if (ul.isMember("resource_path_prefix") && ul["resource_path_prefix"].isString())
						{
							mUltralightResourcePrefix = Dia::Core::Containers::String512(
								"%s%s", deployRoot, ul["resource_path_prefix"].asCString());
						}
					}
				}
			}

			RegisterGlobalAliases();

			RequestGlobalLoad();

			// AC9: kImmediate is correct while all handlers complete synchronously.
			// Switch to kDeferred + SignalStartComplete() when async handlers are added.
			return StateObject::OpertionResponse::kImmediate;
		}

		void AssetServiceModule::DoStop()
		{
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
			{
				DIA_LOG_ERROR("AssetService", "Failed to parse .diastage JSON or missing 'config': %s", diastagePath);
				return;
			}

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
				Dia::Core::Path::ResolveRelative(stageDir, relativePath, entry.mResolvedPath);

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

		void AssetServiceModule::EnsureHandlersRegistered()
		{
			if (mHandlersRegistered)
				return;

			mHandlersRegistered = true;

			Dia::Application::ProcessingUnit* pu = GetAssociatedProcessingUnit();

			Cluiche::Main::KernelModule* kernel = static_cast<Cluiche::Main::KernelModule*>(
				pu->FindModule(Cluiche::Main::KernelModule::kTypeId));
			if (kernel && kernel->GetWindow())
			{
				Dia::SFML::RenderWindow* window = static_cast<Dia::SFML::RenderWindow*>(kernel->GetWindow());
				mRuntime.RegisterTypeHandler("texture", window->GetTextureHandler());
			}

			Cluiche::Main::UIModule* uiModule = static_cast<Cluiche::Main::UIModule*>(
				pu->FindModule(Cluiche::Main::UIModule::kTypeId));
			if (uiModule && uiModule->GetUISystem())
			{
				auto* uiSystem = static_cast<Dia::UI::Ultralight::UISystem*>(uiModule->GetUISystem());
				mRuntime.RegisterTypeHandler("ui", uiSystem->GetUIHandler());
			}
		}

		void AssetServiceModule::RequestGlobalLoad()
		{
			EnsureHandlersRegistered();
			mCurrentLoadStageId = Dia::Core::StringCRC("stage.global");
			mRuntime.RequestStageLoad(mCurrentLoadStageId);
		}

		void AssetServiceModule::RequestStageLoad(const Dia::Core::StringCRC& stageId)
		{
			EnsureHandlersRegistered();
			mCurrentLoadStageId = stageId;

			for (unsigned int i = 0; i < mStagePathMap.Size(); ++i)
			{
				if (mStagePathMap[i].mStageId == stageId)
				{
					RegisterStageAliases(mStagePathMap[i].mDiastagePath.AsCStr());
					break;
				}
			}

			mRuntime.RequestStageLoad(stageId);
		}

		void AssetServiceModule::RequestStageUnload(const Dia::Core::StringCRC& stageId)
		{
			UnregisterStageAliases();
			mRuntime.RequestStageUnload(stageId);
			mCurrentLoadStageId = Dia::Core::StringCRC();
		}

		bool AssetServiceModule::IsLoadComplete() const
		{
			if (mCurrentLoadStageId.Value() == 0)
				return true;

			return mRuntime.IsLoadComplete(mCurrentLoadStageId);
		}

		const Dia::AssetRuntime::AssetRuntime& AssetServiceModule::GetRuntime() const
		{
			return mRuntime;
		}

		const Dia::Core::Containers::String512& AssetServiceModule::GetUltralightResourcePrefix() const
		{
			return mUltralightResourcePrefix;
		}
	}
}

namespace { using _AssetServiceModule = Cluiche::Main::AssetServiceModule; }
DIA_REGISTER_MODULE(_AssetServiceModule) {
	return new Cluiche::Main::AssetServiceModule(pu, instanceId);
}
