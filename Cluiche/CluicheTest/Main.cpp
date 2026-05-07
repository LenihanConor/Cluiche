#include <stdio.h>

#include <DiaApplication/Loader/ApplicationLoader.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/ApplicationProcessingUnit.h>



int main( int argc, const char* argv[] )
{
	/* TODO
			Serialize out module relationships
			Serialzie out phase timing
			Move the statestream into a proxy and handle module

			Fix SimPU
			look at how we communciate from main to render those ptr seem very wrong
			look at FlaggedToStopUpdating, i think it is all wrong and should be only at PU and phase level
			build out launch page

			Get Unit Test Page Working
	*/

	Dia::Application::ApplicationTypeRegistry registry;
	registry.DrainPendingRegistrations();

	Dia::Application::ManifestValidationResult result;
	Dia::Application::ProcessingUnit* mainPU =
		Dia::Application::ApplicationLoader::LoadFromGameFile(registry, "assets/global/cluichetest.diagame", result);

	if (!mainPU)
	{
		printf("Failed to load application from .diagame (result: %d)\n", static_cast<int>(result));
		return 1;
	}

	mainPU->Start();
	mainPU->Update();
	mainPU->Stop();

	delete mainPU;
}