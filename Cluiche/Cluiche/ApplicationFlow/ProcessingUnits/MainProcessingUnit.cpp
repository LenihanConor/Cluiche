#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"

namespace Cluiche
{
	const Dia::Core::StringCRC MainProcessingUnit::kUniqueId("MainProcessingUnit");

	MainProcessingUnit::MainProcessingUnit()
		: Dia::Application::ProcessingUnit(kUniqueId, 30.0f)
		, mRenderingPU()
		, mRunningPhase(this)
		, mBootStrapPhase(this)
		, mKernelModule(this)
	{
		// Setup Phase Transitions
		SetInitialPhase(&mRunningPhase);
		AddPhaseTransiton(&mRunningPhase, &mBootStrapPhase);

		// We call this from here to build all dependancies. We dont need to do this here, intead 
		// we could of done it in the code below if there was dynamic adding of modules or phases
		// but for this cas case this is a nicer cleaner solution.
		Initialize();
	}

	void MainProcessingUnit::PostPhaseStart(const IStartData* startData)
	{
		// Start the render thread
		RenderProcessingUnit::StartData data;
		data.mRunning = &(GetCurrentPhase()->GetModule<MainKernelModule>()->mRunning);
		data.mFrameStream = &(GetCurrentPhase()->GetModule<MainKernelModule>()->mSimToRenderFrameStream);
		data.mCanvas = GetCurrentPhase()->GetModule<MainKernelModule>()->canvas;
		
		mRenderingPU.Start(&data);
		mRenderThread = DIA_NEW(std::thread(std::ref(mRenderingPU)));
	}

	void MainProcessingUnit::PrePhaseStop()
	{
		mRenderThread->join();
		DIA_DELETE(mRenderThread);
	}

	bool MainProcessingUnit::FlaggedToStopUpdating()const
	{
		return GetCurrentPhase()->FlaggedToStopUpdating();
	}
}