////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationProcessingUnit.h
//
// An Application Processing Unit base class is a collection of  
//		pointers to Phases. PU do not have to run on seperate 
//		threads but they can.
//
//		
//		An example of this would be a Render Processing Unit, Simulation
//			Processing Unit and Data Server Processing Unit
//
////////////////////////////////////////////////////////////////////////////////
#ifndef _APPLICATIONPROCESSINGUNIT_H_
#define _APPLICATIONPROCESSINGUNIT_H_

#include <DiaApplication/ApplicationModule.h>

#include "DiaApplication/ApplicationStateObject.h"

#include <DiaCore/Core/EnumClass.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Timer/TimeThreadLimiter.h>

#include <mutex>

namespace Dia 
{
	namespace Application
	{
		class Phase;
	}
}

namespace Dia
{
	namespace Application
	{		
		////////////////////////////////////////////////////////////////////////////////
		// Class name: ApplicationProcessingUnit
		////////////////////////////////////////////////////////////////////////////////
		class ProcessingUnit: public StateObject
		{
		public:
			typedef Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> PhaseTransitionList; // TODO: I am setting this to "8" could be any size to be honest.
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*, Dia::Core::StringCRCHashFunctor> ModuleTable;
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Phase*, Dia::Core::StringCRCHashFunctor> PhasesTable;
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, PhaseTransitionList, Dia::Core::StringCRCHashFunctor> PhaseTransitionTable;
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, ProcessingUnit*, Dia::Core::StringCRCHashFunctor> ProcessingUnitTable;

			ProcessingUnit(const Dia::Core::StringCRC& uniqueId, float hz = -1.0f, unsigned int initialModuleMapSize = 8, unsigned int initialPhaseMapSize = 8);

			void Initialize();
			
			void EnableThreadLimiting(float hz);
			void DisableThreadLimiting();

			void AddProcessingUnit(ProcessingUnit* pu); 
			void AddPhase(Phase* phase);
			void AddModule(Module* module);

			void SetInitialPhase(Phase* phase);
			void AddPhaseTransiton(Phase* startPhase, Phase* endPhase);

			void TransitionPhase(const Dia::Core::StringCRC& phaseCrc);		// Transition immediately
			void QueuePhaseTransition(const Dia::Core::StringCRC& crc);		// Transition after next update (this is thread safe)

			Phase* GetCurrentPhase(){ return mCurrentPhase; }
			const Phase* GetCurrentPhase()const{ return mCurrentPhase; }

			bool ContainsModule(const Dia::Core::StringCRC& crc)const;

			void operator()(); // Used if we are threading

			virtual const char* GetStateObjectType()const override { return "Processing Unit"; }

		protected:
			template <class T> inline
			T*	GetModule() { return static_cast<T*>(GetModule(T::kUniqueId)); }

			template <class T> inline
			const T* GetModule() const { return static_cast<const T*>(GetModule(T::kUniqueId)); }

			Module* GetModule(const Dia::Core::StringCRC& crc);
			const Module* GetModule(const Dia::Core::StringCRC& crc)const;

			virtual bool FlaggedToStopUpdating()const = 0;

		private:
			// Inherited from StateObject
			virtual void DoBuildDependancies(IBuildDependencyData* buildDependencies)override final;
			virtual StateObject::OpertionResponse DoStart(const IStartData* startData) override final;
			virtual void DoUpdate()override final;
			virtual void DoStop() override final;

			// Virtual functions that allow us to hook into different times
			virtual void PrePhaseStart(const IStartData* startData) {}	// Before the phase and the new modules have started
			virtual void PostPhaseStart(const IStartData* startData) {} // After the current phase and all the modules have started

			virtual void PrePhaseUpdate() {}
			virtual void PostPhaseUpdate() {}

			virtual void PrePhaseStop() {}
			virtual void PostPhaseStop() {}

			// PU frequency variables
			bool mEnableThreadLimiter;
			Dia::Core::TimeThreadLimiter mThreadLimiter; // The thread limiter is used if we want to control how often the PU is updated. It will sleep the thread if it finishes 

			// PU associated application phases 
			Phase* mCurrentPhase;
			std::mutex mQueuedTransitionMutex;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> mQueuedTransition;	// FIFO List of phase transition
			PhasesTable mAssociatedPhases;
			PhaseTransitionTable mPhaseTransitions;
		
			// List of all modules that are associated to this PU. Phase can choose from these to enable/disable as they see fir
			ModuleTable mAssociatedModules;

			ProcessingUnitTable mAssociatedProcessingUnites;
		};

		////////////////////////////////////////////////////////////////////////////////
		// Class name: BuildDependencyData
		////////////////////////////////////////////////////////////////////////////////
		class BuildDependencyData: public Dia::Application::IBuildDependencyData
		{
		public: 
			BuildDependencyData(ProcessingUnit::ProcessingUnitTable* associatedProcessingUnites,
									ProcessingUnit::PhasesTable* associatedPhases,
									ProcessingUnit::PhaseTransitionTable* phaseTransitions,
									ProcessingUnit::ModuleTable* associatedModules);

			virtual Module* GetModule(const Dia::Core::StringCRC& crc) override;
			virtual const Module* GetModule(const Dia::Core::StringCRC& crc)const override;

			virtual Phase* GetPhase(const Dia::Core::StringCRC& crc) override;
			virtual const Phase* GetPhase(const Dia::Core::StringCRC& crc)const override;

			virtual ProcessingUnit* GetProcessingUnit(const Dia::Core::StringCRC& crc) override;
			virtual const ProcessingUnit* GetProcessingUnit(const Dia::Core::StringCRC& crc)const override;
		private:
			ProcessingUnit::ProcessingUnitTable* mAssociatedProcessingUnites;
			ProcessingUnit::PhasesTable* mAssociatedPhases;
			ProcessingUnit::PhaseTransitionTable* mPhaseTransitions;
			ProcessingUnit::ModuleTable* mAssociatedModules;
		};
	}
}

#endif