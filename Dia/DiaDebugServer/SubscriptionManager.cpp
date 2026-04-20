#include "DiaDebugServer/SubscriptionManager.h"

namespace Dia
{
	namespace DebugServer
	{
		void SubscriptionManager::Subscribe(int connectionId, const Dia::Core::StringCRC& dataType, const Json::Value& filter)
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);

			for (unsigned int i = 0; i < mSubscriptions.Size(); ++i)
			{
				if (mSubscriptions[i].connectionId == connectionId && mSubscriptions[i].dataType == dataType)
				{
					mSubscriptions[i].filter = filter;
					return;
				}
			}

			Subscription sub;
			sub.connectionId = connectionId;
			sub.dataType = dataType;
			sub.filter = filter;
			mSubscriptions.Add(sub);
		}

		void SubscriptionManager::Unsubscribe(int connectionId, const Dia::Core::StringCRC& dataType)
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);

			for (unsigned int i = 0; i < mSubscriptions.Size(); ++i)
			{
				if (mSubscriptions[i].connectionId == connectionId && mSubscriptions[i].dataType == dataType)
				{
					mSubscriptions.RemoveAt(i);
					return;
				}
			}
		}

		void SubscriptionManager::UnsubscribeAll(int connectionId)
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);

			for (int i = static_cast<int>(mSubscriptions.Size()) - 1; i >= 0; --i)
			{
				if (mSubscriptions[i].connectionId == connectionId)
				{
					mSubscriptions.RemoveAt(static_cast<unsigned int>(i));
				}
			}
		}

		Dia::Core::Containers::DynamicArrayC<int, 16> SubscriptionManager::GetSubscribers(const Dia::Core::StringCRC& dataType) const
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);

			Dia::Core::Containers::DynamicArrayC<int, 16> result;
			for (unsigned int i = 0; i < mSubscriptions.Size(); ++i)
			{
				if (mSubscriptions[i].dataType == dataType)
				{
					result.Add(mSubscriptions[i].connectionId);
				}
			}
			return result;
		}

		bool SubscriptionManager::IsSubscribed(int connectionId, const Dia::Core::StringCRC& dataType) const
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);

			for (unsigned int i = 0; i < mSubscriptions.Size(); ++i)
			{
				if (mSubscriptions[i].connectionId == connectionId && mSubscriptions[i].dataType == dataType)
				{
					return true;
				}
			}
			return false;
		}

		int SubscriptionManager::GetSubscriptionCount() const
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);
			return static_cast<int>(mSubscriptions.Size());
		}
	}
}
