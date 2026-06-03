#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

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
			const char* GetStdoutLogPath() const;

		private:
			void CleanupProcess();
			void DrainPipe();

			PipelineLogTailer* mTailer;
			char mRepoRoot[512];
			char mStdoutLogPath[1024];
			HANDLE mProcessHandle;
			HANDLE mThreadHandle;
			HANDLE mStdoutReadHandle;
			FILE* mStdoutFile;
			bool mBuildRunning;
			int mLastExitCode;
		};
	}
}
