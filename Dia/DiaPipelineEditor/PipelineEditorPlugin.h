#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

#include <DiaEditor/Plugin/EditorPluginBase.h>
#include <DiaCore/Architecture/Observer.h>

namespace Dia
{
	namespace PipelineEditor
	{
		class PipelineLogTailer;
		class PipelineBuildManager;
		class RunHistoryStore;

		class PipelineEditorPlugin : public Dia::Editor::EditorPluginBase, public Dia::Core::Observer
		{
		public:
			PipelineEditorPlugin();
			~PipelineEditorPlugin();

			void OnPluginLoad() override;
			void OnPluginUnload() override;
			void OnNavigate(const Dia::Core::StringCRC& instanceId) override;
			void OnUpdate(float deltaTime) override;
			void OnProjectChanged(const Dia::Editor::ProjectContext& context) override;

			void ObserverNotification(const Dia::Core::ObserverSubject* subject, int message) override;

		private:
			static void ExtractTarget(const char* diagamePath, char* targetOut, size_t targetSize);

			void PushEventsToUI();
			void RegisterCommands();
			void PollLaunchProcess();
			void DrainLaunchPipe();
			void CleanupLaunchProcess();

			PipelineLogTailer* mTailer;
			PipelineBuildManager* mBuildManager;
			RunHistoryStore* mHistoryStore;
			int mLastPushedEventIndex;
			bool mLastBuildRunning;
			int mLastExitCode;
			char mRepoRoot[512];

			// Launch process tracking
			HANDLE mLaunchProcess;
			HANDLE mLaunchStdoutRead;
			FILE*  mLaunchStdoutFile;
			char   mLaunchTarget[256];

			static const unsigned int kDiagamePathLength = 512;
			char mDiagamePath[kDiagamePathLength];
		};
	}
}
