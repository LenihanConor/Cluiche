#pragma once

#include "DiaPipelineEditor/PipelineEvent.h"
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace PipelineEditor
	{
		class RunHistoryStore
		{
		public:
			static constexpr int kMaxRuns = 10;
			static constexpr int kMaxArchivedSessions = 5;

			RunHistoryStore();
			~RunHistoryStore();

			void Initialize(const char* pluginRootPath, const char* sessionId);
			void Shutdown();

			void RecordRun(const RunSummary& summary);

			int GetCount() const;
			const RunSummary& GetRun(int index) const;

			void LoadFromDisk();
			void SaveToDisk();

			Json::Value ToJson() const;

		private:
			void EnsureDirectoryExists();
			void ArchiveStaleSession();
			void WriteContext();
			void PruneSessions();
			static Json::Value SerializeRun(const RunSummary& run);
			static RunSummary DeserializeRun(const Json::Value& val);

			char mPluginRoot[512];
			char mHistoryDir[512];
			char mHistoryFilePath[512];
			char mContextFilePath[512];
			char mSessionsDir[512];
			char mSessionId[128];
			RunSummary mRuns[kMaxRuns];
			int mRunCount;
		};
	}
}
