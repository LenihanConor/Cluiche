#pragma once

#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Editor
	{
		class HandlerTracker
		{
		public:
			void RegisterHandler(WebUIBridge* bridge, const Dia::Core::StringCRC& requestType,
				WebUIBridge::RequestHandler handler);
			void RegisterEvent(WebUIBridge* bridge, const Dia::Core::StringCRC& eventType,
				WebUIBridge::EventHandler handler);
			void UnregisterAll(WebUIBridge* bridge);

		private:
			static const unsigned int kMaxTracked = 32;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxTracked> mRequestHandlers;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxTracked> mEventHandlers;
		};

		inline void HandlerTracker::RegisterHandler(WebUIBridge* bridge, const Dia::Core::StringCRC& requestType,
			WebUIBridge::RequestHandler handler)
		{
			if (bridge)
			{
				bridge->RegisterRequestHandler(requestType, handler);
				mRequestHandlers.Add(requestType);
			}
		}

		inline void HandlerTracker::RegisterEvent(WebUIBridge* bridge, const Dia::Core::StringCRC& eventType,
			WebUIBridge::EventHandler handler)
		{
			if (bridge)
			{
				bridge->RegisterEventHandler(eventType, handler);
				mEventHandlers.Add(eventType);
			}
		}

		inline void HandlerTracker::UnregisterAll(WebUIBridge* bridge)
		{
			if (bridge)
			{
				for (unsigned int i = 0; i < mRequestHandlers.Size(); ++i)
					bridge->UnregisterRequestHandler(mRequestHandlers[i]);
				for (unsigned int i = 0; i < mEventHandlers.Size(); ++i)
					bridge->UnregisterEventHandler(mEventHandlers[i]);
			}
			mRequestHandlers.RemoveAll();
			mEventHandlers.RemoveAll();
		}
	}
}
