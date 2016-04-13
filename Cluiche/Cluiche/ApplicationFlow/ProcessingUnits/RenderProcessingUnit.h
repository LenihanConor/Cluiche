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
			ProcessingUnit(const Dia::Core::StringCRC& uniqueId, unsigned int initialModuleMapSize = 8, unsigned int initialPhaseMapSize = 8);

			void Initialize();

			void AddProcessingUnit(ProcessingUnit* pu); 
			void AddPhase(Phase* phase);
			void AddModule(Module* module);

			void SetInitialPhase(Phase* phase);
			void AddPhaseTransiton(Phase* startPhase, Phase* endPhase);

			void TransitionPhase(const Dia::Core::StringCRC& phaseCrc);
			void QueuePhaseTransition(const Dia::Core::StringCRC& crc);

			Module* GetModule(const Dia::Core::StringCRC& crc);
			const Module* GetModule(const Dia::Core::StringCRC& crc)const;

			Phase* GetCurrentPhase(){ return mCurrentPhase; }
			const Phase* GetCurrentPhase()const{ return mCurrentPhase; }

			bool ContainsModule(const Dia::Core::StringCRC& crc)const;

			virtual void PrePhaseUpdate(){}
			virtual void PostPhaseUpdate(){}

		private:
			typedef Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> PhaseTransitionList; // TODO: I am setting this to "8" could be any size to be honest.

			// Inherited from StateObject
			virtual void DoBuildDependancies()override;
			virtual StateObject::OpertionResponse DoStart() override;
			virtual void DoUpdate()override;
			virtual void DoStop() override;

			Phase* mCurrentPhase;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> mQueuedTransition;	// FIFO List of phase transition
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*, Dia::Core::StringCRCHashFunctor> mAssociatedProcessingUnites;
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Phase*, Dia::Core::StringCRCHashFunctor> mAssociatedPhases;
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, PhaseTransitionList, Dia::Core::StringCRCHashFunctor> mPhaseTransitions;
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*, Dia::Core::StringCRCHashFunctor> mAssociatedModules;
		};
	}
}

#endif