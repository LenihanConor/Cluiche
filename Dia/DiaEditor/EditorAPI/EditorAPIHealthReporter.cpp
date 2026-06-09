#include "EditorAPIHealthReporter.h"

#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaEditor/EditorAPI/EditorActionQueue.h>

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorAPIHealthReporter::kName("EditorAPI");

		EditorAPIHealthReporter::EditorAPIHealthReporter(
			const EditorActionRegistry* registry,
			const EditorActionQueue* queue)
			: mRegistry(registry)
			, mQueue(queue)
		{
		}

		void EditorAPIHealthReporter::Tick()
		{
			if (mRegistry == nullptr || mQueue == nullptr)
			{
				SetFailing(Dia::Core::StringCRC("null_registry_or_queue"));
				return;
			}

			const unsigned int actionCount = mRegistry->GetManifest().GetCount();
			if (actionCount == 0)
			{
				SetFailing(Dia::Core::StringCRC("no_actions_registered"));
				mHighDepthFrames = 0;
				return;
			}

			const unsigned int depth = mQueue->GetDepth();
			if (depth > kDepthThreshold)
			{
				++mHighDepthFrames;
				if (mHighDepthFrames >= kDegradedFrameThreshold)
				{
					SetDegraded(Dia::Core::StringCRC("queue_depth_elevated"));
					return;
				}
			}
			else
			{
				mHighDepthFrames = 0;
			}

			SetOK();
		}

	} // namespace Editor
} // namespace Dia
