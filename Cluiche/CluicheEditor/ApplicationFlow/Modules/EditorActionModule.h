#pragma once

#include <DiaApplicationFlow/MainModule.h>
#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaEditor/EditorAPI/EditorActionQueue.h>
#include <DiaEditor/EditorAPI/EditorAPIHealthReporter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Histogram.h>

namespace Cluiche
{
	namespace Editor
	{
		// Owns the EditorActionRegistry and EditorActionQueue.
		// DoUpdate() drains the queue on the main thread each frame.
		// Exposes registry pointer so other modules can call RegisterAction().
		class EditorActionModule : public Dia::ApplicationFlow::MainModule
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit EditorActionModule(const Dia::Core::StringCRC& instanceId);

			Dia::Editor::EditorActionRegistry* GetRegistry() { return &mRegistry; }
			Dia::Editor::EditorActionQueue*    GetQueue()    { return &mQueue; }

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void                              DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
			Dia::ApplicationFlow::StopResult  DoStop() override;

		private:
			Dia::Editor::EditorActionRegistry      mRegistry;
			Dia::Editor::EditorActionQueue         mQueue;
			Dia::Editor::EditorAPIHealthReporter   mHealthReporter{ &mRegistry, &mQueue };

			Dia::Observation::Metric::Gauge*     mQueueDepthGauge       = nullptr;
			Dia::Observation::Metric::Histogram* mDispatchMsHistogram   = nullptr;
		};

	} // namespace Editor
} // namespace Cluiche
