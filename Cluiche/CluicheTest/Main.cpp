#include <stdio.h>

#include <DiaApplication/Loader/ApplicationLoader.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/ApplicationProcessingUnit.h>



int main( int argc, const char* argv[] )
{
	Dia::Application::ApplicationTypeRegistry registry;
	registry.DrainPendingRegistrations();

	Dia::Application::ManifestValidationResult result;
	Dia::Application::ProcessingUnit* mainPU =
		Dia::Application::ApplicationLoader::LoadFromGameFile(registry, "assets/cluichetest.diagame", result);

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