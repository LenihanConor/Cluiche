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
			static constexpr int kMaxRuns = 5;

			RunHistoryStore();
			~RunHistoryStore();

			void Initialize();
			void Shutdown();

			void RecordRun(const RunSummary& summary);

			int GetCount() const;
			const RunSummary& GetRun(int index) const;

			Json::Value ToJson() const;

		private:
			RunSummary mRuns[kMaxRuns];
			int mRunCount;
		};
	}
}
