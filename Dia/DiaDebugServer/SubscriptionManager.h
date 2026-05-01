#ifndef DIA_DEBUG_SERVER_SUBSCRIPTION_MANAGER_H
#define DIA_DEBUG_SERVER_SUBSCRIPTION_MANAGER_H

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Threading/Mutex.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace DebugServer
	{
		struct Subscription
		{
			int connectionId;
			Dia::Core::StringCRC dataType;
			Json::Value filter;
		};

		class SubscriptionManager
		{
		public:
			void Subscribe(int connectionId, const Dia::Core::StringCRC& dataType, const Json::Value& filter);
			void Unsubscribe(int connectionId, const Dia::Core::StringCRC& dataType);
			void UnsubscribeAll(int connectionId);

			Dia::Core::Containers::DynamicArrayC<int, 16> GetSubscribers(const Dia::Core::StringCRC& dataType) const;
			bool IsSubscribed(int connectionId, const Dia::Core::StringCRC& dataType) const;
			int GetSubscriptionCount() const;

		private:
			mutable Dia::Core::Mutex mMutex;
			Dia::Core::Containers::DynamicArrayC<Subscription, 64> mSubscriptions;
		};
	}
}

#endif // DIA_DEBUG_SERVER_SUBSCRIPTION_MANAGER_H
