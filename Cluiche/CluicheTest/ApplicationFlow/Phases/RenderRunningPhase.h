#pragma once

#include <DiaApplication/ApplicationPhase.h>

namespace Dia { namespace Application { class ProcessingUnit; } }

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// RenderRunningPhase: Running phase for the render thread
	//
	////////////////////////////////////////////////////
	class RenderRunningPhase : public Dia::Application::Phase
	{
	public:
		static const Dia::Core::StringCRC kTypeId;

		RenderRunningPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId = kTypeId);

		virtual bool FlaggedToStopUpdating(void)const override { return true; }
		virtual void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)override;
	};
}
