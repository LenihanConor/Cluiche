
#include "UnitTests/Tests/Application/UnitTestApplication.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

//#include <DiaApplication/ApplicationProcessingUnit.h>
//#include <DiaApplication/ApplicationPhase.h>
//#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaCore/Timer/TimerSystem.h>

#include <string>
#include <iostream>
#include <thread>
#include <future>        

namespace UnitTests
{	
/*	class MyModuleA: public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MyModuleA(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Module::RunningEnum::kUpdate)
		{}

		void DoUpdate()override
		{
			int x = 0;
			x++;
		}
	};
	const Dia::Core::StringCRC MyModuleA::kUniqueId("MyModuleA");


	class MyModuleB: public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MyModuleB(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Module::RunningEnum::kIdle)
		{}
		
		void DoBuildDependancies(IBuildDependencyData* buildDependencies)override
		{
			AddDependancy(GetAssociatedProcessingUnit()->GetModule(MyModuleA::kUniqueId));
		}

		StateObject::OpertionResponse DoStart() override
		{
		
			std::future<void> fut = std::async(MyModuleB::ThreadedStartTask, this);

			return StateObject::OpertionResponse::kAsync;
		}

	private:
		
		static void ThreadedStartTask(Dia::Application::Module* pModule)
		{
			std::chrono::milliseconds dura(2000);
			std::this_thread::sleep_for(dura);

			// Ok this is not thread safe but that not the point, it would be the system that 
			//	lives in here that should manage its threading.
			pModule->NotifyReadyToStartAsync();
		}

	};
	const Dia::Core::StringCRC MyModuleB::kUniqueId("MyModuleB");

	class MyModuleC: public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MyModuleC(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Module::RunningEnum::kIdle)
		{}

		void DoBuildDependancies(IBuildDependencyData* buildDependencies)override
		{
			AddDependancy(GetAssociatedProcessingUnit()->GetModule(MyModuleA::kUniqueId));
		}
	};
	const Dia::Core::StringCRC MyModuleC::kUniqueId("MyModuleC");

	class MyModuleD : public Dia::Application::Module
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MyModuleD(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Module::RunningEnum::kIdle)
		{}

		void DoBuildDependancies()override
		{
			AddDependancy(GetAssociatedProcessingUnit()->GetModule(MyModuleB::kUniqueId));
		}
	};
	const Dia::Core::StringCRC MyModuleD::kUniqueId("MyModuleD");

	class MyPhaseA: public Dia::Application::Phase
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MyPhaseA(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
		{}

		void DoBuildDependancies(IBuildDependencyData* buildDependencies)override
		{
			AddModule(GetAssociatedProcessingUnit()->GetModule(MyModuleB::kUniqueId));
			AddModule(GetAssociatedProcessingUnit()->GetModule(MyModuleC::kUniqueId));
		}
	};
	const Dia::Core::StringCRC MyPhaseA::kUniqueId("MyPhaseA");

	class MyPhaseB: public Dia::Application::Phase
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MyPhaseB(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
		{}

		void DoBuildDependancies()override
		{
			AddModule(GetAssociatedProcessingUnit()->GetModule(MyModuleB::kUniqueId));
			AddModule(GetAssociatedProcessingUnit()->GetModule(MyModuleD::kUniqueId));
		}
	};
	const Dia::Core::StringCRC MyPhaseB::kUniqueId("MyPhaseB");

	class MyPhaseC: public Dia::Application::Phase
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MyPhaseC(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
		{}

		void DoBuildDependancies(IBuildDependencyData* buildDependencies)override
		{
			AddModule(GetAssociatedProcessingUnit()->GetModule(MyModuleA::kUniqueId));
		}
	};
	const Dia::Core::StringCRC MyPhaseC::kUniqueId("MyPhaseC");

	class MyProcessingUnitA: public Dia::Application::ProcessingUnit
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MyProcessingUnitA()
			: Dia::Application::ProcessingUnit(kUniqueId)
			, myPhaseA(this)
			, myPhaseB(this)
			, myPhaseC(this)
			, myModuleA(this)
			, myModuleB(this)
			, myModuleC(this)
			, myModuleD(this)
		{
			// Add Phases/Modules
			AddPhase(&myPhaseA);
			AddPhase(&myPhaseB);
			AddPhase(&myPhaseC);

			AddModule(&myModuleA);
			AddModule(&myModuleB);
			AddModule(&myModuleC);
			AddModule(&myModuleD);

			// Setup Phase Transitions
			SetInitialPhase(&myPhaseA);
			AddPhaseTransiton(&myPhaseA, &myPhaseB);

			// We call this from here to build all dependancies. We dont need to do this here, intead 
			// we could of done it in the code below if there was dynamic adding of modules or phases
			// but for this cas case this is a nicer cleaner solution.
			Initialize();
		}


	private:
		MyModuleA myModuleA;
		MyModuleB myModuleB;
		MyModuleC myModuleC;
		MyModuleD myModuleD;

		MyPhaseA myPhaseA;
		MyPhaseB myPhaseB;
		MyPhaseB myPhaseC;
	};

	const Dia::Core::StringCRC MyProcessingUnitA::kUniqueId("MyProcessingUnitA");
	*/
	UnitTestApplication::UnitTestApplication(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestApplication::UnitTestApplication(void)
		: UnitTestCore()
	{}

	void UnitTestApplication::DoTest()
	{
/*		UNIT_TEST_BLOCK_START()

			MyProcessingUnitA pu;
			
			pu.Start();
			pu.Update();
			pu.Stop();
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			MyProcessingUnitA pu;

			pu.Start();
			pu.Update();
			pu.TransitionPhase(MyPhaseB::kUniqueId);
			pu.Stop();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			MyProcessingUnitA pu;

			pu.Start();
			pu.QueuePhaseTransition(MyPhaseB::kUniqueId);
			pu.Update();
			pu.Stop();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			MyProcessingUnitA pu;

			pu.Start();
			pu.Update();
			UNIT_TEST_ASSERT_EXPECTED_START();
				pu.TransitionPhase(MyPhaseC::kUniqueId);
			UNIT_TEST_ASSERT_EXPECTED_END();
			pu.Stop();

		UNIT_TEST_BLOCK_END()
		*/
		mState = kFinished;
	}
}















