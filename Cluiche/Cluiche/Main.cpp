#include <stdio.h>

#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"



int main( int argc, const char* argv[] )
{
	Cluiche::MainProcessingUnit mainPU;
	
	mainPU.Start();

	while (!mainPU.ShouldQuitApplication())
	{
		mainPU.Update();
	}
	
	mainPU.Stop();
}