#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"

#include "DiaApplication/ApplicationPhase.h"
#include "DiaApplication/ApplicationModule.h"

namespace Cluiche
{
	const Dia::Core::StringCRC MainProcessingUnit::kUniqueId("MainProcessingUnit");

	MainProcessingUnit::MainProcessingUnit()
		: Dia::Application::ProcessingUnit(kUniqueId, 30.0f)
		, mRenderThread(nullptr)
		, mRenderingPU()
		, mSimThread(nullptr)
		, mSimPU()
		, mBootPhase(this)
		, mBootStrapPhase(this)
		, mKernelModule(this)
		, mLevelRegistryModule(this)
		, mUI(this)
	{
		// Setup Phase Transitions
		SetInitialPhase(&mBootPhase);
		AddPhaseTransiton(&mBootPhase, &mBootStrapPhase);

		// We call this from here to build all dependancies. We dont need to do this here, intead 
		// we could of done it in the code below if there was dynamic adding of modules or phases
		// but for this case this is a nicer cleaner solution.
		Initialize();
	}

	Cluiche::MainProcessingUnit* MainProcessingUnit::GetMainPU()
	{
		return this;
	}

	Cluiche::RenderProcessingUnit* MainProcessingUnit::GetRenderingPU()
	{ 
		return &mRenderingPU; 
	}

	Cluiche::SimProcessingUnit* MainProcessingUnit::GetSimPU() 
	{ 
		return &mSimPU; 
	}

	void MainProcessingUnit::PostPhaseStart(const IStartData* startData)
	{
		// Start the render thread
		{
			RenderProcessingUnit::StartData data;
			data.mRunning = &(mKernelModule.mRunning);
			data.mFrameStream = &(mKernelModule.GetSimToRenderFrameStream());
			data.mCanvas = mKernelModule.GetCanvas();

			mRenderingPU.Start(&data);
			mRenderThread = DIA_NEW(std::thread(std::ref(mRenderingPU)));
		}

		//start the sim thread
		{
			SimProcessingUnit::StartData data;
			data.mRunning = &(mKernelModule.mRunning);
			data.mFrameStream = &(mKernelModule.GetSimToRenderFrameStream());
			data.mInputToSimFrameStream = &(mKernelModule.GetInputToSimFrameStream());
			data.mMainUIModule = &mUI;

			mSimPU.Start(&data);
			mSimThread = DIA_NEW(std::thread(std::ref(mSimPU)));
		}
	}

	void MainProcessingUnit::PrePhaseStop()
	{
		mSimThread->join();
		DIA_DELETE(mSimThread);

		mRenderThread->join();
		DIA_DELETE(mRenderThread);
	}

	bool MainProcessingUnit::FlaggedToStopUpdating()const
	{
		return GetCurrentPhase()->FlaggedToStopUpdating();
	}

	void MainProcessingUnit::GenerateModuleDependecyGraph()
	{
		
	}

	void MainProcessingUnit::GeneratePhaseDependecyGraph()
	{
		 
	}
}