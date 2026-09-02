#include "AIBudgetModule.h"

#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Counter.h>

#include <json/json.h>

namespace Dia
{
	namespace AIBudget
	{
		const Dia::Core::StringCRC AIBudgetModule::kInstanceId("AIBudgetModule");

		AIBudgetModule::AIBudgetModule()
			: SimModule(kInstanceId)
			, mScheduler()
			, mBudgetMs(1.0f)
			, mUsedUsGauge(nullptr)
			, mSystemsRunCounter(nullptr)
			, mSystemsDeferredCounter(nullptr)
		{
		}

		AIBudgetScheduler& AIBudgetModule::GetScheduler()
		{
			return mScheduler;
		}

		const AIBudgetScheduler& AIBudgetModule::GetScheduler() const
		{
			return mScheduler;
		}

		void AIBudgetModule::OnConfigure(const char* configJson)
		{
			if (!configJson || configJson[0] == '\0')
			{
				return;
			}

			Json::Value  root;
			Json::Reader reader;
			if (reader.parse(configJson, root))
			{
				if (root.isMember("budgetUs") && root["budgetUs"].isInt())
				{
					const int budgetUs = root["budgetUs"].asInt();
					DIA_ASSERT(budgetUs >= 0, "AIBudgetModule::OnConfigure — budgetUs must not be negative (got %d); using default 1000 us", budgetUs);
					if (budgetUs >= 0)
						mBudgetMs = static_cast<float>(budgetUs) / 1000.0f;
				}
			}
		}

		Dia::ApplicationFlow::StartResult AIBudgetModule::DoStart()
		{
			auto& registry = Dia::Observation::Metric::MetricRegistry::Instance();

			mUsedUsGauge            = registry.RegisterGauge(Dia::Core::StringCRC("ai.budget.used_us"));
			mSystemsRunCounter      = registry.RegisterCounter(Dia::Core::StringCRC("ai.budget.systems_run"));
			mSystemsDeferredCounter = registry.RegisterCounter(Dia::Core::StringCRC("ai.budget.systems_deferred"));

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void AIBudgetModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
		{
			mLastResult = mScheduler.Update(mBudgetMs);

			if (mUsedUsGauge)
			{
				// Convert ms back to microseconds for the gauge (metric key is "used_us")
				mUsedUsGauge->Set(static_cast<double>(mLastResult.usedMs) * 1000.0);
			}

			if (mSystemsRunCounter)
			{
				mSystemsRunCounter->Inc(static_cast<uint64_t>(mLastResult.systemsRun));
			}

			if (mSystemsDeferredCounter)
			{
				mSystemsDeferredCounter->Inc(static_cast<uint64_t>(mLastResult.systemsDeferred));
			}
		}

		const AIBudgetResult& AIBudgetModule::GetLastResult() const
		{
			return mLastResult;
		}

		float AIBudgetModule::GetBudgetMs() const
		{
			return mBudgetMs;
		}

		Dia::ApplicationFlow::StopResult AIBudgetModule::DoStop()
		{
			mUsedUsGauge            = nullptr;
			mSystemsRunCounter      = nullptr;
			mSystemsDeferredCounter = nullptr;

			return Dia::ApplicationFlow::StopResult::kDone;
		}

	} // namespace AIBudget
} // namespace Dia
