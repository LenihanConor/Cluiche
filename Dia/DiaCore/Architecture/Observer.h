#ifndef OBSERVER
#define OBSERVER

#include <mutex>

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace Core
	{
		class ObserverSubject;

		// the observer is the recieving class
		class Observer
		{
		public:
			Observer() {};
			~Observer() {};
			virtual void ObserverNotification(const ObserverSubject* theChangeSubject, int message) = 0;
		};

		// the observer subject is the class that wants to communicate 
		class ObserverSubject
		{
		public:
			ObserverSubject() {};
			
			void AttachToObserver(Observer*);
			void DetachFromObserver(Observer*);
			void NotifyObservers(int message);
			void NotifyObservers(int message)const;
		
		private:
			mutable std::mutex mMutex;
			Containers::DynamicArrayC<Observer*, 16> mObservers;
		};
	}
}

#endif