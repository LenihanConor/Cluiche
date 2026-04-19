#include <stdio.h>

#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"

#include "DiaCore/Containers/Graphs/Graph.h"







int main( int argc, const char* argv[] )
{
	/* TODO
			Save to file
			Build system to communicate debug to game
			Serialize graph
			build PU->module graph
			build PU->phase graph
			Serialize out module relationships
			Serialzie out phase timing
			Move the statestream into a proxy and handle module

			Fix SimPU
			look at how we communciate from main to render those ptr seem very wrong
			look at FlaggedToStopUpdating, i think it is all wrong and should be only at PU and phase level
			build out launch page

			Get Unit Test Page Working
	*/

	Cluiche::MainProcessingUnit* mainPU = new Cluiche::MainProcessingUnit();

	mainPU->Start();

	// Looping call
	mainPU->Update();

	mainPU->Stop();

	delete mainPU;
}