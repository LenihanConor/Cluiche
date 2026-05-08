#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Dia
{
	namespace PipelineEditor
	{
		class PipelineLogTailer;

		class PipelineBuildManager
		{
		public:
			PipelineBuildManager();
			~PipelineBuildManager();

			void Initialize(PipelineLogTailer* tailer, const char* repoRoot);
			void Shutdown();

			int Start(const char* config, const char* target,
			          const char* stages, bool force);

			void Cancel();

			void Update();

			bool IsBuildRunning() const;
			int GetLastExitCode() const;

		private:
			void CleanupProcess();

			PipelineLogTailer* mTailer;
			char mRepoRoot[512];
			HANDLE mProcessHandle;
			HANDLE mThreadHandle;
			bool mBuildRunning;
			int mLastExitCode;
		};
	}
}
