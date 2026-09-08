#include "DiaPipelineEditor/RunHistoryStore.h"

using namespace Dia::PipelineEditor;

RunHistoryStore::RunHistoryStore()
	: mRunCount(0)
{
}

RunHistoryStore::~RunHistoryStore()
{
}

void RunHistoryStore::Initialize()
{
	mRunCount = 0;
}

void RunHistoryStore::Shutdown()
{
}

void RunHistoryStore::RecordRun(const RunSummary& summary)
{
	if (mRunCount >= kMaxRuns)
	{
		for (int i = kMaxRuns - 1; i > 0; --i)
			mRuns[i] = mRuns[i - 1];
	}
	else
	{
		for (int i = mRunCount; i > 0; --i)
			mRuns[i] = mRuns[i - 1];
		++mRunCount;
	}

	mRuns[0] = summary;
}

int RunHistoryStore::GetCount() const
{
	return mRunCount;
}

const RunSummary& RunHistoryStore::GetRun(int index) const
{
	return mRuns[index];
}

Json::Value RunHistoryStore::ToJson() const
{
	Json::Value arr(Json::arrayValue);
	for (int i = 0; i < mRunCount; ++i)
	{
		const RunSummary& run = mRuns[i];
		Json::Value val;
		val["target"] = run.target.AsChar();
		val["config"] = run.config.AsChar();
		val["passCount"] = run.passCount;
		val["failCount"] = run.failCount;
		val["totalDurationMs"] = run.totalDurationMs;
		val["startTimestamp"] = static_cast<double>(run.startTimestamp);
		val["interrupted"] = run.interrupted;
		arr.append(val);
	}
	return arr;
}
