#pragma once

#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Editor
	{
		class EditorActionRegistry;
		class EditorActionQueue;

		// Health reporter for the EditorAPI subsystem.
		//   OK       — >= 1 action registered and queue draining normally
		//   Degraded — queue depth > 20 for 2+ consecutive frames
		//   Failing  — no actions registered (Python module would be empty)
		class EditorAPIHealthReporter : public Dia::Observation::Health::HealthReporterBase
		{
		public:
			static const Dia::Core::StringCRC kName;  // "EditorAPI"

			EditorAPIHealthReporter(const EditorActionRegistry* registry, const EditorActionQueue* queue);

			Dia::Core::StringCRC GetReporterName() const override { return kName; }

			// Call once per frame from EditorActionModule::DoUpdate before queue drain.
			void Tick();

		private:
			const EditorActionRegistry* mRegistry = nullptr;
			const EditorActionQueue*    mQueue    = nullptr;
			unsigned int                mHighDepthFrames = 0;

			static const unsigned int kDepthThreshold = 20;
			static const unsigned int kDegradedFrameThreshold = 2;
		};

	} // namespace Editor
} // namespace Dia
