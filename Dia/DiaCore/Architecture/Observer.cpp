#include "DiaCore/Architecture/Observer.h"

namespace Dia
{
	namespace Core
	{
		//------------------------------------------------
		void ObserverSubject::AttachToObserver(Observer* val)
		{
			DIA_ASSERT(val, "Null obserer");

			std::lock_guard<std::mutex> lock(mMutex);

			mObservers.Add(val);
		}

		//------------------------------------------------
		void ObserverSubject::DetachFromObserver(Observer* val)
		{
			std::lock_guard<std::mutex> lock(mMutex);

			int count = mObservers.Size();
			int i;

			for (i = 0; i < count; i++) 
			{
				if (mObservers[i] == val)
				{
					break;
				}
			}

			if (i < count)
			{
				mObservers.RemoveAt(count);
			}
			else
			{
				DIA_ASSERT(0, "Could not find obserers");
			}
		}

		//------------------------------------------------
		void ObserverSubject::NotifyObservers(int message)
		{
			std::lock_guard<std::mutex> lock(mMutex);

			int count = mObservers.Size();

			for (int i = 0; i < count; i++)
			{
				(mObservers[i])->ObserverNotification(this, message);
			}
		}

		//------------------------------------------------
		void ObserverSubject::NotifyObservers(int message)const
		{
			std::lock_guard<std::mutex> lock(mMutex);

			int count = mObservers.Size();

			for (int i = 0; i < count; i++)
			{
				(mObservers[i])->ObserverNotification(this, message);
			}
		}
	}
}