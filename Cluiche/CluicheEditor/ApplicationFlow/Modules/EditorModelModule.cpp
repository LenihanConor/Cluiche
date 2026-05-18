#include "EditorModelModule.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>
#include <DiaLogger/LogLevel.h>
#include <DiaLogger/DiaLog.h>
#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <string.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorModelModule::kTypeId("EditorModelModule");

		namespace
		{
			// Parse command-line arguments from GetCommandLineW():
			//   - Bare path (no prefix)       -> outProjectPath  (.cluicheproj)
			//   - --project=<path>            -> outDiagamePath  (.diagame)
			// Strips the exe argv[0] first.
			void ParseCommandLine(char* outProjectPath, unsigned int projectPathSize,
			                      char* outDiagamePath, unsigned int diagamePathSize)
			{
				if (projectPathSize > 0) outProjectPath[0] = '\0';
				if (diagamePathSize > 0) outDiagamePath[0]  = '\0';

				LPWSTR fullCmd = GetCommandLineW();
				if (fullCmd == nullptr || fullCmd[0] == L'\0') return;

				// Skip exe argv[0].
				LPWSTR p = fullCmd;
				if (*p == L'"')
				{
					++p;
					while (*p && *p != L'"') ++p;
					if (*p == L'"') ++p;
				}
				else
				{
					while (*p && *p != L' ' && *p != L'\t') ++p;
				}

				while (*p == L' ' || *p == L'\t') ++p;

				// Process each argument.
				while (*p != L'\0')
				{
					// Skip leading whitespace between args.
					while (*p == L' ' || *p == L'\t') ++p;
					if (*p == L'\0') break;

					// Extract the argument token into a wide buffer.
					wchar_t arg[512] = {0};
					wchar_t* dest = arg;
					wchar_t* end  = arg + (sizeof(arg) / sizeof(arg[0])) - 1;

					if (*p == L'"')
					{
						++p;
						while (*p && *p != L'"' && dest < end) *dest++ = *p++;
						if (*p == L'"') ++p;
					}
					else
					{
						while (*p && *p != L' ' && *p != L'\t' && dest < end) *dest++ = *p++;
					}
					*dest = L'\0';

					// Check for --project=<path>
					static const wchar_t kProjectPrefix[] = L"--project=";
					static const int kProjectPrefixLen = 10;
					if (wcsncmp(arg, kProjectPrefix, kProjectPrefixLen) == 0)
					{
						WideCharToMultiByte(CP_UTF8, 0, arg + kProjectPrefixLen, -1,
						                    outDiagamePath, static_cast<int>(diagamePathSize), nullptr, nullptr);
					}
					else if (arg[0] != L'-' && outProjectPath[0] == '\0')
					{
						// First non-flag argument is the .cluicheproj path.
						WideCharToMultiByte(CP_UTF8, 0, arg, -1,
						                    outProjectPath, static_cast<int>(projectPathSize), nullptr, nullptr);
					}
				}
			}

			// Apply editor-logger.json (if present): sets default level + per-sink thresholds/channels.
			void ApplyLoggerConfig(const char* configPath, Dia::Logger::ISink** sinks, unsigned int sinkCount)
			{
				if (configPath == nullptr) return;

				std::ifstream file(configPath);
				if (!file.is_open()) return;

				Json::Value root;
				Json::Reader reader;
				if (!reader.parse(file, root)) return;

				Dia::Logger::LogLevel defaultLevel = Dia::Logger::LogLevel::kInfo;
				if (root.isMember("default_level") && root["default_level"].isString())
					defaultLevel = Dia::Logger::LogLevelFromString(root["default_level"].asCString());

				if (root.isMember("sinks") && root["sinks"].isArray())
				{
					const Json::Value& sinksConfig = root["sinks"];
					for (Json::ArrayIndex i = 0; i < sinksConfig.size(); ++i)
					{
						const Json::Value& sinkConfig = sinksConfig[i];
						if (!sinkConfig.isMember("name") || !sinkConfig["name"].isString())
							continue;

						const char* sinkName = sinkConfig["name"].asCString();
						for (unsigned int s = 0; s < sinkCount; ++s)
						{
							if (strcmp(sinks[s]->GetName(), sinkName) != 0) continue;

							if (sinkConfig.isMember("level_threshold") && sinkConfig["level_threshold"].isString())
								sinks[s]->SetLevelThreshold(Dia::Logger::LogLevelFromString(sinkConfig["level_threshold"].asCString(), defaultLevel));
							else
								sinks[s]->SetLevelThreshold(defaultLevel);

							if (sinkConfig.isMember("channels") && sinkConfig["channels"].isArray())
							{
								sinks[s]->ClearChannelFilter();
								const Json::Value& channels = sinkConfig["channels"];
								for (Json::ArrayIndex c = 0; c < channels.size(); ++c)
								{
									if (channels[c].isString())
										sinks[s]->SetChannelFilter(Dia::Core::StringCRC(channels[c].asCString()), true);
								}
							}
							break;
						}
					}
				}
				else
				{
					for (unsigned int i = 0; i < sinkCount; ++i)
						sinks[i]->SetLevelThreshold(defaultLevel);
				}
			}
		} // anonymous namespace

		EditorModelModule::EditorModelModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
			, mRecentCount(0)
		{
			mProjectPath[0]         = '\0';
			mDiagamePath[0]         = '\0';
			mCluicheproj[0]         = '\0';
			mRestoredLastProject[0] = '\0';
		}

		void EditorModelModule::SetProjectPath(const char* path)
		{
			if (path == nullptr)
			{
				mProjectPath[0] = '\0';
				return;
			}
			strncpy_s(mProjectPath, kMaxProjectPathLength, path, _TRUNCATE);
		}

		const char* EditorModelModule::GetProjectPath() const
		{
			return mProjectPath;
		}

		Dia::ApplicationFlow::StartResult EditorModelModule::DoStart()
		{
			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
			logger.RegisterThreadBuffer();
			logger.RegisterSink(&mDebugOutputSink);

			Dia::Logger::ISink* sinks[] = { &mDebugOutputSink };
			ApplyLoggerConfig("assets/configs/editor-logger.json", sinks, 1);

			ParseCommandLine(mProjectPath, kMaxProjectPathLength, mDiagamePath, kMaxProjectPathLength);

			// Subscribe to game-project changes to maintain editor_state persistence.
			mModel.OnDiagameProjectChanged([](const Dia::Editor::ProjectContext& ctx, void* ud)
			{
				auto* self = static_cast<EditorModelModule*>(ud);
				self->OnDiagameProjectChanged(ctx);
			}, this);

			// Restore last_project from editor_state if no --project arg was supplied.
			if (mDiagamePath[0] == '\0' && mProjectPath[0] != '\0')
				ReadEditorState(mProjectPath);

			// Apply --project arg (takes precedence over restored last_project).
			if (mDiagamePath[0] != '\0')
			{
				if (!mModel.LoadDiagameProject(mDiagamePath))
					DIA_LOG_WARNING("Editor", "EditorModelModule: --project path not found or invalid: '%s'", mDiagamePath);
			}
			else if (mRestoredLastProject[0] != '\0')
			{
				if (!mModel.LoadDiagameProject(mRestoredLastProject))
					DIA_LOG_WARNING("Editor", "EditorModelModule: last_project not found, starting with no project");
			}

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void EditorModelModule::DoUpdate(float /*deltaTime*/)
		{
			Dia::Logger::Logger::Instance().FlushBuffers();
		}

		Dia::ApplicationFlow::StopResult EditorModelModule::DoStop()
		{
			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
			logger.UnregisterSink(&mDebugOutputSink);
			logger.UnregisterThreadBuffer();
			mModel.Reset();
			return Dia::ApplicationFlow::StopResult::kDone;
		}

		void EditorModelModule::ReadEditorState(const char* cluicheprojPath)
		{
			strncpy_s(mCluicheproj, kMaxProjectPathLength, cluicheprojPath, _TRUNCATE);
			mRestoredLastProject[0] = '\0';

			std::ifstream file(cluicheprojPath);
			if (!file.is_open()) return;

			Json::Value root;
			Json::CharReaderBuilder builder;
			std::string errors;
			if (!Json::parseFromStream(builder, file, &root, &errors)) return;

			const Json::Value& state = root["editor_state"];
			if (!state.isObject()) return;

			if (state.isMember("last_project") && state["last_project"].isString())
				strncpy_s(mRestoredLastProject, kMaxProjectPathLength, state["last_project"].asCString(), _TRUNCATE);

			// Load recent list into mRecentProjects.
			mRecentCount = 0;
			const Json::Value& recent = state["recent_projects"];
			if (recent.isArray())
			{
				for (Json::ArrayIndex i = 0; i < recent.size() && mRecentCount < kMaxRecent; ++i)
				{
					if (recent[i].isString())
					{
						strncpy_s(mRecentProjects[mRecentCount], kMaxProjectPathLength,
						          recent[i].asCString(), _TRUNCATE);
						++mRecentCount;
					}
				}
			}
		}

		void EditorModelModule::WriteEditorState()
		{
			if (mCluicheproj[0] == '\0') return;

			// Read existing file so we preserve version/name/manifests.
			Json::Value root;
			{
				std::ifstream in(mCluicheproj);
				if (in.is_open())
				{
					Json::CharReaderBuilder builder;
					std::string errors;
					if (!Json::parseFromStream(builder, in, &root, &errors))
						root = Json::Value(Json::objectValue);
				}
				else
				{
					root = Json::Value(Json::objectValue);
				}
			}

			Json::Value& state = root["editor_state"];
			if (!state.isObject()) state = Json::Value(Json::objectValue);

			const Dia::Editor::ProjectContext& ctx = mModel.GetDiagameProject();
			if (ctx.IsValid())
				state["last_project"] = ctx.diagamePath;
			else
				state.removeMember("last_project");

			Json::Value recentArr(Json::arrayValue);
			for (unsigned int i = 0; i < mRecentCount; ++i)
				recentArr.append(mRecentProjects[i]);
			state["recent_projects"] = recentArr;

			root["editor_state"] = state;

			Json::StyledWriter writer;
			std::string output = writer.write(root);
			std::ofstream out(mCluicheproj);
			if (!out.is_open())
			{
				DIA_LOG_WARNING("Editor", "EditorModelModule: failed to write editor_state to '%s'", mCluicheproj);
				return;
			}
			out << output;
		}

		void EditorModelModule::OnDiagameProjectChanged(const Dia::Editor::ProjectContext& ctx)
		{
			if (ctx.IsValid())
			{
				// Update recent list: add to front, keep last kMaxRecent unique paths.
				const char* path = ctx.diagamePath;
				// Remove existing entry for this path.
				unsigned int write = 0;
				for (unsigned int i = 0; i < mRecentCount; ++i)
				{
					if (strcmp(mRecentProjects[i], path) != 0)
					{
						if (write != i)
							strncpy_s(mRecentProjects[write], kMaxProjectPathLength, mRecentProjects[i], _TRUNCATE);
						++write;
					}
				}
				// Shift right and insert at front (limited to kMaxRecent).
				unsigned int newCount = (write < kMaxRecent) ? write + 1 : kMaxRecent;
				for (unsigned int i = newCount - 1; i > 0; --i)
				{
					if (i - 1 < write)
						strncpy_s(mRecentProjects[i], kMaxProjectPathLength, mRecentProjects[i - 1], _TRUNCATE);
				}
				strncpy_s(mRecentProjects[0], kMaxProjectPathLength, path, _TRUNCATE);
				mRecentCount = newCount;
			}

			// Mirror recent list into EditorModel so IEditorContext can expose it.
			const char* ptrs[kMaxRecent];
			for (unsigned int i = 0; i < mRecentCount; ++i)
				ptrs[i] = mRecentProjects[i];
			mModel.SetRecentProjects(ptrs, mRecentCount);

			WriteEditorState();
		}

		unsigned int EditorModelModule::GetRecentProjectCount() const
		{
			return mRecentCount;
		}

		const char* EditorModelModule::GetRecentProject(unsigned int index) const
		{
			if (index >= mRecentCount) return nullptr;
			return mRecentProjects[index];
		}
	}
}

namespace { using EditorModelModule_ = Cluiche::Editor::EditorModelModule; }
DIA_MODULE(EditorModelModule_);
