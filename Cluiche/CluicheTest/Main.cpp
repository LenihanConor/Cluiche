#include <stdio.h>

#include <DiaApplication/Loader/ApplicationLoader.h>
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

	Dia::Application::ProcessingUnit* mainPU =
		Dia::Application::ApplicationLoader::LoadApplication("Data/Manifests/cluiche_main.diaapp");

	mainPU->Start();
	mainPU->Update();
	mainPU->Stop();

	delete mainPU;
}