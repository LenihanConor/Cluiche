#ifndef OBSERVER
#define OBSERVER

#include <mutex>

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace Core
	{
		class Subject;

		class Observer
		{
		public:
			Observer() {};
			~Observer() {};
			virtual void ObserverNotification(ObserverSubject* theChangeSubject) = 0;
			virtual void ObserverNotification(const ObserverSubject* theChangeSubject) = 0;
		};

		class ObserverSubject
		{
		public:
			ObserverSubject() {};
			
			void AttachToObserver(Observer*);
			void DetachFromObserver(Observer*);
			void NotifyObservers();
			void NotifyObservers()const;
		
		private:
			mutable std::mutex mMutex;
			Containers::DynamicArrayC<Observer*, 16> mObservers;
		};
	}
}

#include "DiaCore/Architecture/Observer.inl"

#endif