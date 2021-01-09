#include <stdio.h>

#include "ApplicationFlow/ProcessingUnits/MainProcessingUnit.h"

#include "DiaCore/Containers/Graphs/Graph.h"







int main( int argc, const char* argv[] )
{
	Cluiche::MainProcessingUnit mainPU;
	
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








/*	struct ApplicationNode
	{
		int type;
		//string name;
		//int uniqueID
	};
	Dia::Core::Containers::Graph<int, 6, char, 3> graph;
	graph.AddNode(Dia::Core::Containers::Graph<int, 6, char, 3>::Node("N1", 200));
	graph.AddNode(Dia::Core::Containers::Graph<int, 6, char, 3>::Node("N2", 100));
	graph.AddEdge(Dia::Core::Containers::Graph<int, 6, char, 3>::Edge("E1", '1', graph.FindNode("N1"), graph.FindNode("N2")));*/

	mainPU.Start();

	// Looping call
	mainPU.Update();





	// Debug Calls
/*	static bool a = false;
	if (a)
	{
		mainPU.GenerateModuleDependecyGraph();
		mainPU.GeneratePhaseDependecyGraph();
	}*/




	mainPU.Stop();
}