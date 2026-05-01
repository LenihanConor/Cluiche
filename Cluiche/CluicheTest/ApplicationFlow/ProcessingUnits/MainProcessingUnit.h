#pragma once

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	class RenderProcessingUnit;
	class SimProcessingUnit;

	class MainProcessingUnit : public Dia::Application::ProcessingUnit
	{
	public:
		static const Dia::Core::StringCRC kTypeId;

		MainProcessingUnit(const Dia::Core::StringCRC& instanceId, float hz);
		~MainProcessingUnit();

		Cluiche::MainProcessingUnit* GetMainPU();
		Cluiche::RenderProcessingUnit* GetRenderingPU();
		Cluiche::SimProcessingUnit* GetSimPU();

		void GenerateModuleDependecyGraph();
		void GeneratePhaseDependecyGraph();

	private:
		virtual void PostPhaseStart(const IStartData* startData)override final;
		virtual void PrePhaseStop()override final;
		virtual bool FlaggedToStopUpdating()const override final;

		std::thread* mRenderThread;
		Cluiche::RenderProcessingUnit* mRenderingPU;

		std::thread* mSimThread;
		Cluiche::SimProcessingUnit* mSimPU;
	};
}
