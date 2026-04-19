////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationIntrospector.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplication/Introspection/ApplicationIntrospector.h"

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/ApplicationModule.h>
#include <DiaApplication/Manifest/ApplicationManifest.h>

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Application
	{
		//---------------------------------------------------------------------------------------------------------
		ApplicationIntrospector::ApplicationIntrospector(ProcessingUnit* pu)
			: mProcessingUnit(pu)
		{
			DIA_ASSERT(mProcessingUnit != nullptr, "ApplicationIntrospector requires non-null ProcessingUnit");
		}

		//---------------------------------------------------------------------------------------------------------
		Dia::Core::Containers::DynamicArrayC<Phase*, 16> ApplicationIntrospector::GetPhases() const
		{
			DIA_ASSERT(mProcessingUnit != nullptr, "ProcessingUnit is null");

			Dia::Core::Containers::DynamicArrayC<Phase*, 16> phases;

			// Iterate through the ProcessingUnit's phase hash table
			// ApplicationIntrospector is a friend of ProcessingUnit so we can access mAssociatedPhases
			const ProcessingUnit::PhasesTable& phaseTable = mProcessingUnit->mAssociatedPhases;

			for (unsigned int i = 0; i < phaseTable.Size(); ++i)
			{
				Phase* phase = phaseTable.GetItemByIndexConst(i);
				phases.Add(phase);
			}

			return phases;
		}

		//---------------------------------------------------------------------------------------------------------
		Dia::Core::Containers::DynamicArrayC<Module*, 32> ApplicationIntrospector::GetModules() const
		{
			DIA_ASSERT(mProcessingUnit != nullptr, "ProcessingUnit is null");

			Dia::Core::Containers::DynamicArrayC<Module*, 32> modules;

			// Iterate through the ProcessingUnit's module hash table
			const ProcessingUnit::ModuleTable& moduleTable = mProcessingUnit->mAssociatedModules;

			for (unsigned int i = 0; i < moduleTable.Size(); ++i)
			{
				Module* module = moduleTable.GetItemByIndexConst(i);
				modules.Add(module);
			}

			return modules;
		}

		//---------------------------------------------------------------------------------------------------------
		Phase* ApplicationIntrospector::GetCurrentPhase() const
		{
			DIA_ASSERT(mProcessingUnit != nullptr, "ProcessingUnit is null");

			return mProcessingUnit->GetCurrentPhase();
		}

		//---------------------------------------------------------------------------------------------------------
		Dia::Core::Containers::DynamicArrayC<ApplicationIntrospector::PhaseTransitionInfo, 32> ApplicationIntrospector::GetTransitions() const
		{
			DIA_ASSERT(mProcessingUnit != nullptr, "ProcessingUnit is null");

			Dia::Core::Containers::DynamicArrayC<PhaseTransitionInfo, 32> transitions;

			// Iterate through the ProcessingUnit's phase transition table
			const ProcessingUnit::PhaseTransitionTable& transitionTable = mProcessingUnit->mPhaseTransitions;

			// Use iterator to traverse the hash table
			for (auto it = transitionTable.Begin(); it != transitionTable.End(); ++it)
			{
				const Dia::Core::StringCRC& fromPhaseId = it.Key();
				const ProcessingUnit::PhaseTransitionList& toPhases = it.Value();

				// Add each transition from this phase
				for (unsigned int i = 0; i < toPhases.Size(); ++i)
				{
					PhaseTransitionInfo info(fromPhaseId, toPhases[i]);
					transitions.Add(info);
				}
			}

			return transitions;
		}

		//---------------------------------------------------------------------------------------------------------
		Dia::Core::Containers::DynamicArrayC<ApplicationIntrospector::ModulePlacement, 32> ApplicationIntrospector::GetModulePlacements() const
		{
			DIA_ASSERT(mProcessingUnit != nullptr, "ProcessingUnit is null");

			Dia::Core::Containers::DynamicArrayC<ModulePlacement, 32> placements;

			// Get all modules first
			Dia::Core::Containers::DynamicArrayC<Module*, 32> allModules = GetModules();

			// For each module, find which phases contain it
			for (unsigned int i = 0; i < allModules.Size(); ++i)
			{
				Module* module = allModules[i];
				ModulePlacement placement;
				placement.module = module;

				// Iterate through all phases and check if they contain this module
				const ProcessingUnit::PhasesTable& phaseTable = mProcessingUnit->mAssociatedPhases;
				for (unsigned int j = 0; j < phaseTable.Size(); ++j)
				{
					Phase* phase = phaseTable.GetItemByIndexConst(j);

					// Check if this phase contains the module
					if (phase->ContainsModule(module->GetUniqueId()))
					{
						placement.phases.Add(phase);
					}
				}

				placements.Add(placement);
			}

			return placements;
		}

		//---------------------------------------------------------------------------------------------------------
		Dia::Core::Containers::DynamicArrayC<Module*, 32> ApplicationIntrospector::GetModuleDependencies(Module* module) const
		{
			DIA_ASSERT(module != nullptr, "Module is null");

			Dia::Core::Containers::DynamicArrayC<Module*, 32> dependencies;

			// Module exposes GetNumberOfDependancies() and GetModuleFromIndex()
			const unsigned int numDeps = module->GetNumberOfDependancies();
			for (unsigned int i = 0; i < numDeps; ++i)
			{
				Module* dep = module->GetModuleFromIndex(i);
				if (dep != nullptr)
				{
					dependencies.Add(dep);
				}
			}

			return dependencies;
		}

		//---------------------------------------------------------------------------------------------------------
		int ApplicationIntrospector::GetModuleState(Module* module) const
		{
			DIA_ASSERT(module != nullptr, "Module is null");

			return static_cast<int>(module->GetState());
		}

		//---------------------------------------------------------------------------------------------------------
		int ApplicationIntrospector::GetPhaseState(Phase* phase) const
		{
			DIA_ASSERT(phase != nullptr, "Phase is null");

			return static_cast<int>(phase->GetState());
		}

		//---------------------------------------------------------------------------------------------------------
		void ApplicationIntrospector::ExportToManifest(ApplicationManifest& outManifest) const
		{
			DIA_ASSERT(mProcessingUnit != nullptr, "ProcessingUnit is null");

			// Use ProcessingUnit::SerializeTopology to get JSON representation
			Json::Value topology;
			mProcessingUnit->SerializeTopology(topology);

			// Create a ProcessingUnitEntry for the manifest
			ApplicationManifest::ProcessingUnitEntry puEntry;
			puEntry.instanceId = mProcessingUnit->GetUniqueId();
			puEntry.typeId = ProcessingUnit::kTypeId;
			puEntry.frequencyHz = -1.0f;  // Not exposed by ProcessingUnit API
			puEntry.dedicatedThread = false;  // Not exposed by ProcessingUnit API

			// Parse phases from JSON
			if (topology.isMember("phases") && topology["phases"].isArray())
			{
				const Json::Value& phasesArray = topology["phases"];
				for (unsigned int i = 0; i < phasesArray.size(); ++i)
				{
					const Json::Value& phaseObj = phasesArray[i];
					if (phaseObj.isMember("id"))
					{
						ApplicationManifest::PhaseEntry phaseEntry;
						phaseEntry.instanceId = Dia::Core::StringCRC(phaseObj["id"].asCString());
						phaseEntry.typeId = Phase::kTypeId;
						puEntry.phases.Add(phaseEntry);
					}
				}
			}

			// Parse transitions from JSON
			if (topology.isMember("transitions") && topology["transitions"].isArray())
			{
				const Json::Value& transitionsArray = topology["transitions"];
				for (unsigned int i = 0; i < transitionsArray.size(); ++i)
				{
					const Json::Value& transitionObj = transitionsArray[i];
					if (transitionObj.isMember("from") && transitionObj.isMember("to"))
					{
						ApplicationManifest::PhaseTransition transition;
						transition.fromPhase = Dia::Core::StringCRC(transitionObj["from"].asCString());
						transition.toPhase = Dia::Core::StringCRC(transitionObj["to"].asCString());
						puEntry.transitions.Add(transition);
					}
				}
			}

			// Parse modules from JSON
			if (topology.isMember("modules") && topology["modules"].isArray())
			{
				const Json::Value& modulesArray = topology["modules"];
				for (unsigned int i = 0; i < modulesArray.size(); ++i)
				{
					const Json::Value& moduleObj = modulesArray[i];
					if (moduleObj.isMember("id"))
					{
						ApplicationManifest::ModuleEntry moduleEntry;
						moduleEntry.instanceId = Dia::Core::StringCRC(moduleObj["id"].asCString());
						moduleEntry.typeId = Module::kTypeId;
						// Note: phase assignments and dependencies not in SerializeTopology output
						// Would need more detailed introspection to populate these
						puEntry.modules.Add(moduleEntry);
					}
				}
			}

			// Set initial phase from current phase
			if (topology.isMember("currentPhase"))
			{
				puEntry.initialPhase = Dia::Core::StringCRC(topology["currentPhase"].asCString());
			}

			// Add ProcessingUnit entry to manifest
			outManifest.processingUnits.Add(puEntry);
		}
	}
}
