#pragma once

#include <DiaApplication/ApplicationPhase.h>

namespace Dia { namespace Application { class ProcessingUnit; } }

namespace Cluiche
{
	namespace Kernel
	{
		////////////////////////////////////////////////////
		//
		// MainPhaseBase: Base functionality for all main thread phases
		//
		////////////////////////////////////////////////////
		class MainPhaseBase : public Dia::Application::Phase
		{
		public:
			MainPhaseBase(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& uniqueId, unsigned int maxModules = 16);

			virtual bool FlaggedToStopUpdating()const override final;
		};
	}
}
