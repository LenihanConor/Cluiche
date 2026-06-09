#include "EditorActionModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Log/DiaLog.h>
#include <chrono>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorActionModule::kTypeId("EditorActionModule");

		EditorActionModule::EditorActionModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
		{
		}

		Dia::ApplicationFlow::StartResult EditorActionModule::DoStart()
		{
			mRegistry.Initialize();

			auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
			mQueueDepthGauge = reg.RegisterGauge(Dia::Core::StringCRC("editor.action_queue_depth"));

			static const float kDispatchBuckets[] = { 0.1f, 0.5f, 1.0f, 5.0f, 10.0f, 50.0f };
			mDispatchMsHistogram = reg.RegisterHistogram(
				Dia::Core::StringCRC("editor.action_dispatch_ms"),
				kDispatchBuckets, 6);

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void EditorActionModule::DoUpdate(float deltaTime)
		{
			DIA_TRACE_ZONE("editor.action_queue_drain", Dia::Observation::Trace::Category::kDiaApplicationFlow);

			if (mQueueDepthGauge)
				mQueueDepthGauge->Set(static_cast<double>(mQueue.GetDepth()));

			const auto before = std::chrono::high_resolution_clock::now();

			mQueue.DoUpdate(&mRegistry, deltaTime);

			const auto after   = std::chrono::high_resolution_clock::now();
			const float ms     = std::chrono::duration<float, std::milli>(after - before).count();

			if (mDispatchMsHistogram)
				mDispatchMsHistogram->Observe(ms);
		}

		Dia::ApplicationFlow::StopResult EditorActionModule::DoStop()
		{
			mRegistry.Shutdown();
			return Dia::ApplicationFlow::StopResult::kDone;
		}
	}
}

namespace { using EditorActionModule_ = Cluiche::Editor::EditorActionModule; }
DIA_MODULE(EditorActionModule_);
DIA_DESCRIBE(EditorActionModule_::kTypeId, "Owns the EditorActionRegistry and dispatches queued editor actions to the main thread each frame.");
