////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationIntrospector.h
//
// ApplicationIntrospector provides runtime introspection of ProcessingUnit topology.
// Allows querying phases, modules, transitions, and current state.
// Supports exporting topology to ApplicationManifest for editor "save" functionality.
//
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
		class Phase;
		class Module;
		struct ApplicationManifest;

		////////////////////////////////////////////////////////////////////////////////
		// Class name: ApplicationIntrospector
		// Purpose: Query runtime application topology and state
		////////////////////////////////////////////////////////////////////////////////
		class ApplicationIntrospector
		{
		public:
			explicit ApplicationIntrospector(ProcessingUnit* pu);

			// Query structure - returns lists of phases/modules in the ProcessingUnit
			Dia::Core::Containers::DynamicArrayC<Phase*, 16> GetPhases() const;
			Dia::Core::Containers::DynamicArrayC<Module*, 32> GetModules() const;
			Phase* GetCurrentPhase() const;

			// Query topology - phase transitions
			struct PhaseTransitionInfo
			{
				Dia::Core::StringCRC fromPhase;
				Dia::Core::StringCRC toPhase;

				PhaseTransitionInfo() = default;
				PhaseTransitionInfo(const Dia::Core::StringCRC& from, const Dia::Core::StringCRC& to)
					: fromPhase(from), toPhase(to) {}
			};
			Dia::Core::Containers::DynamicArrayC<PhaseTransitionInfo, 32> GetTransitions() const;

			// Query topology - module placements (which modules in which phases)
			struct ModulePlacement
			{
				Module* module;
				Dia::Core::Containers::DynamicArrayC<Phase*, 16> phases;

				ModulePlacement() : module(nullptr) {}
			};
			Dia::Core::Containers::DynamicArrayC<ModulePlacement, 32> GetModulePlacements() const;

			// Query dependencies - returns list of modules that the given module depends on
			Dia::Core::Containers::DynamicArrayC<Module*, 32> GetModuleDependencies(Module* module) const;

			// Query state (AC10) - returns current state of module/phase
			// StateEnum values: kConstructed, kFlaggedToStart, kRunning, kFlaggedToStop, kNotRunning
			// Note: StateEnum is defined in ApplicationStateObject.h
			int GetModuleState(Module* module) const;
			int GetPhaseState(Phase* phase) const;

			// Export current topology to manifest (for editor "save")
			// Populates the given ApplicationManifest with current runtime topology
			void ExportToManifest(ApplicationManifest& outManifest) const;

		private:
			ProcessingUnit* mProcessingUnit;
		};
	}
}
