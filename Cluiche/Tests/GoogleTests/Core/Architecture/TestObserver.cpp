#include <gtest/gtest.h>
#include <DiaCore/Architecture/Observer.h>

#include <thread>
#include <atomic>

using namespace Dia::Core;

class TestObserver : public Observer
{
public:
	int lastMessage = -1;
	int notifyCount = 0;
	const ObserverSubject* lastSubject = nullptr;

	void ObserverNotification(const ObserverSubject* subject, int message) override
	{
		lastMessage = message;
		lastSubject = subject;
		notifyCount++;
	}
};

TEST(Observer, DefaultConstruction_NoCrash)
{
	ObserverSubject subject;

	SUCCEED();
}

TEST(Observer, AttachSingleAndNotify_ObserverReceivesMessage)
{
	ObserverSubject subject;
	TestObserver obs;

	subject.AttachToObserver(&obs);
	subject.NotifyObservers(42);

	EXPECT_EQ(obs.lastMessage, 42);
	EXPECT_EQ(obs.lastSubject, &subject);
	EXPECT_EQ(obs.notifyCount, 1);
}

TEST(Observer, AttachMultipleAndNotify_AllReceiveMessage)
{
	ObserverSubject subject;
	TestObserver obs1;
	TestObserver obs2;
	TestObserver obs3;

	subject.AttachToObserver(&obs1);
	subject.AttachToObserver(&obs2);
	subject.AttachToObserver(&obs3);
	subject.NotifyObservers(7);

	EXPECT_EQ(obs1.lastMessage, 7);
	EXPECT_EQ(obs2.lastMessage, 7);
	EXPECT_EQ(obs3.lastMessage, 7);
	EXPECT_EQ(obs1.notifyCount, 1);
	EXPECT_EQ(obs2.notifyCount, 1);
	EXPECT_EQ(obs3.notifyCount, 1);
}

TEST(Observer, DetachObserver_DetachedNotCalledRemainingStillCalled)
{
	ObserverSubject subject;
	TestObserver obs1;
	TestObserver obs2;
	TestObserver obs3;

	subject.AttachToObserver(&obs1);
	subject.AttachToObserver(&obs2);
	subject.AttachToObserver(&obs3);
	subject.DetachFromObserver(&obs2);
	subject.NotifyObservers(99);

	EXPECT_EQ(obs1.lastMessage, 99);
	EXPECT_EQ(obs1.notifyCount, 1);
	EXPECT_EQ(obs2.lastMessage, -1);
	EXPECT_EQ(obs2.notifyCount, 0);
	EXPECT_EQ(obs3.lastMessage, 99);
	EXPECT_EQ(obs3.notifyCount, 1);
}

TEST(Observer, NotifyWithNoObservers_NoCrash)
{
	ObserverSubject subject;

	subject.NotifyObservers(10);

	SUCCEED();
}

#ifdef DEBUG
TEST(Observer, AttachNullObserver_Asserts)
{
	ObserverSubject subject;

	EXPECT_DEATH(subject.AttachToObserver(nullptr), ".*");
}

TEST(Observer, DetachNotAttached_Asserts)
{
	ObserverSubject subject;
	TestObserver obs;

	EXPECT_DEATH(subject.DetachFromObserver(&obs), ".*");
}
#endif

TEST(Observer, NotifyPassesCorrectMessageValue)
{
	ObserverSubject subject;
	TestObserver obs;

	subject.AttachToObserver(&obs);

	subject.NotifyObservers(0);
	EXPECT_EQ(obs.lastMessage, 0);

	subject.NotifyObservers(-1);
	EXPECT_EQ(obs.lastMessage, -1);

	subject.NotifyObservers(2147483647);
	EXPECT_EQ(obs.lastMessage, 2147483647);
}

TEST(Observer, NotifyPassesCorrectSubjectPointer)
{
	ObserverSubject subject;
	TestObserver obs;

	subject.AttachToObserver(&obs);
	subject.NotifyObservers(1);

	EXPECT_EQ(obs.lastSubject, &subject);
}

TEST(Observer, MultipleNotifications_NotifyCountIncrementsCorrectly)
{
	ObserverSubject subject;
	TestObserver obs;

	subject.AttachToObserver(&obs);

	subject.NotifyObservers(1);
	subject.NotifyObservers(2);
	subject.NotifyObservers(3);
	subject.NotifyObservers(4);
	subject.NotifyObservers(5);

	EXPECT_EQ(obs.notifyCount, 5);
	EXPECT_EQ(obs.lastMessage, 5);
}

TEST(Observer, ConstNotify_WorksOnConstSubject)
{
	ObserverSubject subject;
	TestObserver obs;

	subject.AttachToObserver(&obs);

	const ObserverSubject& constSubject = subject;
	constSubject.NotifyObservers(55);

	EXPECT_EQ(obs.lastMessage, 55);
	EXPECT_EQ(obs.lastSubject, &subject);
	EXPECT_EQ(obs.notifyCount, 1);
}

TEST(Observer, StressAttachMax16_AllReceiveNotification)
{
	ObserverSubject subject;
	TestObserver observers[16];

	for (int i = 0; i < 16; i++)
	{
		subject.AttachToObserver(&observers[i]);
	}

	subject.NotifyObservers(123);

	for (int i = 0; i < 16; i++)
	{
		EXPECT_EQ(observers[i].lastMessage, 123);
		EXPECT_EQ(observers[i].notifyCount, 1);
		EXPECT_EQ(observers[i].lastSubject, &subject);
	}
}

TEST(Observer, ConcurrentNotify_NoCrash)
{
	ObserverSubject subject;
	TestObserver obs;

	subject.AttachToObserver(&obs);

	std::atomic<bool> done1(false);
	std::atomic<bool> done2(false);

	std::thread t1([&]()
	{
		for (int i = 0; i < 100; i++)
		{
			subject.NotifyObservers(1);
		}
		done1.store(true);
	});

	std::thread t2([&]()
	{
		for (int i = 0; i < 100; i++)
		{
			subject.NotifyObservers(2);
		}
		done2.store(true);
	});

	t1.join();
	t2.join();

	EXPECT_TRUE(done1.load());
	EXPECT_TRUE(done2.load());
	EXPECT_EQ(obs.notifyCount, 200);
}
