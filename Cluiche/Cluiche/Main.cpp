#include <stdio.h>

#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"



int main( int argc, const char* argv[] )
{
	Cluiche::MainProcessingUnit mainPU;
	
	/* TODO
			Convert SimThread and RenderThread into PU
			Fix stop
			Expand PU to do the thread limitation
			Make a UI module (Main/Sim)
			static_cast<MainKernelModule*>(GetModule(MainKernelModule::kUniqueId))->mAwesomiumUISystem->LoadPage(launchUIPage); Templatize this
			build out launch page
	*/

	mainPU.Start();

	// Looping call
	mainPU.Update();
		
	mainPU.Stop();
}