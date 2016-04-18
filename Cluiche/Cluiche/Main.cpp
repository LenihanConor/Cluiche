#include <stdio.h>

#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"



int main( int argc, const char* argv[] )
{
	Cluiche::MainProcessingUnit mainPU;
	
	/* TODO
			
			Make a UI module (Main/Sim)
			Give MainKernel a proper interface
			Move the statestream into a proxy and handle module
			Convert SimThread and RenderThread into PU
			look at how we communciate from main to render those ptr seem very wrong
			Fix stop
			look at FlaggedToStopUpdating, i think it is all wrong and should be only at PU and phase level
			build out launch page
			Dia::SFML::RenderWindow* renderWindow; // TODO CLEAN UP
	*/

	mainPU.Start();

	// Looping call
	mainPU.Update();
		
	mainPU.Stop();
}