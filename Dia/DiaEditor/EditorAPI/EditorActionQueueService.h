#pragma once

#include <DiaEditor/EditorAPI/EditorActionQueue.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Editor
	{
		class EditorActionQueueService
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;  // StringCRC("EditorActionQueueService")

			explicit EditorActionQueueService(EditorActionQueue* queue)
				: mQueue(queue)
			{}

			EditorActionQueue* GetQueue() const { return mQueue; }

		private:
			EditorActionQueue* mQueue = nullptr;
		};

	} // namespace Editor
} // namespace Dia
