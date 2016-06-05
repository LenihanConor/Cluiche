#include <stdio.h>

#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"

int main( int argc, const char* argv[] )
{
	Cluiche::MainProcessingUnit mainPU;
	
	/* TODO
			Move the statestream into a proxy and handle module
			Fix SimPU
			look at how we communciate from main to render those ptr seem very wrong
			look at FlaggedToStopUpdating, i think it is all wrong and should be only at PU and phase level
			build out launch page
	*/

	mainPU.Start();

	// Looping call
	mainPU.Update();
		
	mainPU.Stop();
}