#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"
#include "ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h"
#include "ApplicationFlow/ProcessingUnits/SimProcessingUnit.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"

#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>
#include <DiaApplication/Loader/ApplicationLoader.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/Metrics/MetricsCollectorModule.h>
#include <DiaCore/Memory/UniquePtr.h>

namespace Cluiche
{
	const Dia::Core::StringCRC MainProcessingUnit::kTypeId("MainProcessingUnit");

	MainProcessingUnit::MainProcessingUnit(const Dia::Core::StringCRC& instanceId, float hz)
		: Dia::Application::ProcessingUnit(instanceId, hz)
		, mRenderThread(nullptr)
		, mSimThread(nullptr)
		, mRenderingPU(nullptr)
		, mSimPU(nullptr)
	{
	}

	Cluiche::MainProcessingUnit* MainProcessingUnit::GetMainPU()
	{
		return this;
	}

	Cluiche::RenderProcessingUnit* MainProcessingUnit::GetRenderingPU()
	{
		return mRenderingPU;
	}

	Cluiche::SimProcessingUnit* MainProcessingUnit::GetSimPU()
	{
		return mSimPU;
	}

	void MainProcessingUnit::PostPhaseStart(const IStartData* startData)
	{
		auto* kernel = GetModule<Cluiche::Main::KernelModule>();

		// Load render PU from manifest, add to tree for ownership
		{
			auto* rawRenderPU = static_cast<Cluiche::RenderProcessingUnit*>(
				Dia::Application::ApplicationLoader::LoadApplication(*GetTypeRegistry(), "Data/Manifests/cluiche_render.diaapp"));

			mRenderingPU = rawRenderPU;
			Dia::Core::UniquePtr<Dia::Application::ProcessingUnit> owned(rawRenderPU);
			AddChildProcessingUnit(std::move(owned));

			RenderProcessingUnit::StartData data;
			data.mRunning = &(kernel->mRunning);
			data.mFrameStream = &(kernel->GetSimToRenderFrameStream());
			data.mCanvas = kernel->GetCanvas();

			mRenderingPU->Start(&data);
			if (GetMetricsCollector())
				mRenderingPU->SetMetricsCollector(GetMetricsCollector());
			mRenderThread = DIA_NEW(std::thread(std::ref(*mRenderingPU)));
		}

		// Load sim PU from manifest, add to tree for ownership
		{
			auto* ui = GetModule<Cluiche::Main::UIModule>();

			auto* rawSimPU = static_cast<Cluiche::SimProcessingUnit*>(
				Dia::Application::ApplicationLoader::LoadApplication(*GetTypeRegistry(), "Data/Manifests/cluiche_sim.diaapp"));

			mSimPU = rawSimPU;
			Dia::Core::UniquePtr<Dia::Application::ProcessingUnit> owned(rawSimPU);
			AddChildProcessingUnit(std::move(owned));

			SimProcessingUnit::StartData data;
			data.mRunning = &(kernel->mRunning);
			data.mFrameStream = &(kernel->GetSimToRenderFrameStream());
			data.mInputToSimFrameStream = &(kernel->GetInputToSimFrameStream());
			data.mMainUIModule = ui;
			data.mCanvas = kernel->GetCanvas();

			mSimPU->Start(&data);
			if (GetMetricsCollector())
				mSimPU->SetMetricsCollector(GetMetricsCollector());
			mSimThread = DIA_NEW(std::thread(std::ref(*mSimPU)));
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

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>
namespace { using _MainProcessingUnit = Cluiche::MainProcessingUnit; }
DIA_REGISTER_PROCESSING_UNIT(_MainProcessingUnit) {
	float hz = config.get("frequency_hz", 30.0f).asFloat();
	return new Cluiche::MainProcessingUnit(instanceId, hz);
}
