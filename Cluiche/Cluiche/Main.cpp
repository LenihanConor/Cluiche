#include <stdio.h>

#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"



int main( int argc, const char* argv[] )
{
	Cluiche::MainProcessingUnit mainPU;
	
	/* TODO
			Convert SimThread and RenderThread into PU
			Fix stop
			Make a UI module (Main/Sim)
			build out launch page
			Dia::SFML::RenderWindow* renderWindow; // TODO CLEAN UP
	*/

	mainPU.Start();

	// Looping call
	mainPU.Update();
		
	mainPU.Stop();
}