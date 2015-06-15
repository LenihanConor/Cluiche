#pragma once

#include <DiaApplication\ApplicationProcessingUnit.h>
#include <DiaCore\CRC\StringCRC.h>

#include "Source/ApplicationModel/Modules/ApplicationTimeModule.h"

#include "Source/ApplicationModel/Phases/ApplicationBootPhase.h"
#include "Source/ApplicationModel/Phases/ApplicationCorePhase.h"

////////////////////////////////////////////////////////////////////////////////
// Class name: ApplicationProcessingUnit, Primary PU for this application
////////////////////////////////////////////////////////////////////////////////
class ApplicationProcessingUnit: public Dia::Application::ProcessingUnit
{
public:
	static const Dia::Core::StringCRC kUniqueId;

	ApplicationProcessingUnit();
	virtual ~ApplicationProcessingUnit();

	virtual void PrePhaseUpdate()override;
	virtual void PostPhaseUpdate()override;

	bool ShouldQuitApplication()const{ return mQuitApplication; }
	void RequestQuitApplication(){ mQuitApplication = true; }

private:
	// Phases
	ApplicationBootPhase mBootPhase;				// Initial Boot Phase
	ApplicationCorePhase mCorePhase;				// Essential Module Phase

	ApplicationTimeModule mApplicationTime;			// Time that is persistent from the start to the end of application

	bool mQuitApplication;
};

