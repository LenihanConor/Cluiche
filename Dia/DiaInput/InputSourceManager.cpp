////////////////////////////////////////////////////////////////////////////////
// Filename: DummyClass
////////////////////////////////////////////////////////////////////////////////
#include "DiaInput/InputSourceManager.h"

#include <DiaCore/Architecture/Events/EventDispatcher.h>
#include <DiaLogger/DiaLog.h>
#include "DiaInput/Events/LegacyEventConverter.h"

namespace Dia
{
	namespace Input
	{
		InputSourceManager::InputSourceManager()
		{}

		void InputSourceManager::AddInputSource(IInputSource* inputSource)
		{
			DIA_ASSERT(inputSource, "Cannot be null inputsource");

			mInputSourceList.Add(inputSource);
			DIA_LOG_INFO("Input", "Added input source (ID: %d). Total sources: %u",
				inputSource->GetUniqueID(), mInputSourceList.Size());
		}
		void InputSourceManager::RemoveInputSource(IInputSource* inputSource)
		{
			DIA_ASSERT(inputSource, "Cannot be null inputsource");

			int targetId = inputSource->GetUniqueID();
			for (unsigned int i = 0; i < mInputSourceList.Size(); i++)
			{
				if (mInputSourceList[i]->GetUniqueID() == targetId)
				{
					mInputSourceList.RemoveAt(i);
					DIA_LOG_INFO("Input", "Removed input source (ID: %d). Remaining sources: %u",
						targetId, mInputSourceList.Size());
					return;
				}
			}

			DIA_LOG_ERROR("Input", "Attempted to remove input source that was not found (ID: %d)", targetId);
			DIA_ASSERT(false, "InputSource not found in manager (ID: %d)", targetId);
		}

		void InputSourceManager::StartFrame()
		{
			DIA_LOG_TRACE("Input", "InputSourceManager::StartFrame() - polling %u sources", mInputSourceList.Size());
			for (unsigned int i = 0; i < mInputSourceList.Size(); i++)
			{
				mInputSourceList[i]->StartFrame();
			}
		}

		void InputSourceManager::Update(EventData& outData)
		{
			unsigned int initialEventCount = outData.Size();

			// Sort sources by priority (highest first) for ordered polling
			// Simple bubble sort - adequate for small lists (typically <8 sources)
			for (unsigned int i = 0; i < mInputSourceList.Size(); i++)
			{
				for (unsigned int j = i + 1; j < mInputSourceList.Size(); j++)
				{
					if (static_cast<int>(mInputSourceList[j]->GetPriority()) >
						static_cast<int>(mInputSourceList[i]->GetPriority()))
					{
						// Swap
						IInputSource* temp = mInputSourceList[i];
						mInputSourceList[i] = mInputSourceList[j];
						mInputSourceList[j] = temp;
					}
				}
			}

			// Collect events from all sources into temporary buffer (priority order)
			EventData tempBuffer;
			for (unsigned int i = 0; i < mInputSourceList.Size(); i++)
			{
				mInputSourceList[i]->Poll(tempBuffer);
			}

			// Merge consecutive mouse move events to reduce buffer pressure
			Event::EType lastType = Event::EType::kClosed;
			unsigned int lastMouseMoveIdx = 0;

			for (unsigned int i = 0; i < tempBuffer.Size(); i++)
			{
				if (tempBuffer[i].type == Event::EType::kMouseMoved)
				{
					if (lastType == Event::EType::kMouseMoved)
					{
						// Update previous mouse move coordinates instead of adding new event
						outData[lastMouseMoveIdx].mouseMove.x = tempBuffer[i].mouseMove.x;
						outData[lastMouseMoveIdx].mouseMove.y = tempBuffer[i].mouseMove.y;
					}
					else
					{
						// First mouse move or after a different event type
						lastMouseMoveIdx = outData.Size();
						outData.Add(tempBuffer[i]);
						lastType = Event::EType::kMouseMoved;
					}
				}
				else
				{
					outData.Add(tempBuffer[i]);
					lastType = tempBuffer[i].type;
				}
			}

			// Check for buffer overflow
			if (outData.IsFull())
			{
				DIA_LOG_WARNING("Input", "Event buffer is full (%u events). Some events may be lost.", outData.Size());
			}

			unsigned int newEvents = outData.Size() - initialEventCount;
			unsigned int mergedCount = tempBuffer.Size() - newEvents;
			if (newEvents > 0)
			{
				DIA_LOG_TRACE("Input", "Polled %u events from %u sources (%u merged). Total events: %u",
					newEvents, mInputSourceList.Size(), mergedCount, outData.Size());
			}
		}

		void InputSourceManager::EndFrame()
		{
			DIA_LOG_TRACE("Input", "InputSourceManager::EndFrame()");
			for (unsigned int i = 0; i < mInputSourceList.Size(); i++)
			{
				mInputSourceList[i]->EndFrame();
			}
		}

		void InputSourceManager::UpdateModern(Core::Events::EventDispatcher& dispatcher)
		{
			// Poll all sources into legacy event buffer
			EventData legacyEvents;

			for (unsigned int i = 0; i < mInputSourceList.Size(); i++)
			{
				mInputSourceList[i]->Poll(legacyEvents);
			}

			// Convert and dispatch to modern event system
			Events::LegacyEventConverter::ConvertAndDispatch(legacyEvents, dispatcher);

			DIA_LOG_TRACE("Input", "UpdateModern(): Converted %u legacy events to modern events",
				legacyEvents.Size());
		}
	}
}