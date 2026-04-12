#pragma once

#include <DiaApplication/ApplicationPhase.h>
#include <DiaCore/CRC/StringCRC.h>

#include <CluicheKernel/ApplicationFlow/Phases/MainPhaseBase.h>

namespace Dia { namespace Application { class ProcessingUnit; } }

namespace Cluiche
{
	namespace UnitTestLevel
	{
		////////////////////////////////////////////////////
		//
		// MainLoadPhase: Initial phase for the main thread to load dummy project
		//
		////////////////////////////////////////////////////
		class MainLoadPhase : public Cluiche::Main::MainPhaseBase
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;

			MainLoadPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);
		
		private:
			virtual void AfterModulesStart()override;
			virtual void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)override;
		};
	}
}
