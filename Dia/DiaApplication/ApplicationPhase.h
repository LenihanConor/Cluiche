////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationPhase.h
////////////////////////////////////////////////////////////////////////////////
#ifndef _APPLICATIONPHASE_H_
#define _APPLICATIONPHASE_H_


#include <DiaApplication/ApplicationStateObject.h>
#include <DiaApplication/ApplicationModule.h>

#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Application
	{	
		class ProcessingUnit;

		////////////////////////////////////////////////////////////////////////////////
		// Class name: Phase
		////////////////////////////////////////////////////////////////////////////////
		class Phase: public StateObject
		{
		public:
			Phase(ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& uniqueId, unsigned int maxModules = 16);

            void AddModule(Module* module);

			void QueuePhaseTransition(const Dia::Core::StringCRC& crc);

			void TransitionTo(Phase* endPhase);

			bool ContainsModule(const Dia::Core::StringCRC& crc)const;

			virtual void BeforeModulesStart(){};
			virtual void AfterModulesStart(){};

			virtual void BeforeModulesUpdate(){};
			virtual void AfterModulesUpdate(){};

			virtual void BeforeModulesStop(){};
			virtual void AfterModulesStop(){};

		protected:
			ProcessingUnit* GetAssociatedProcessingUnit();
			const ProcessingUnit* GetAssociatedProcessingUnit()const;

			template <class T> inline
			T*	GetModule() { return static_cast<T*>(GetModule(T::kUniqueId));}

			template <class T> inline
			const T* GetModule() const{ return static_cast<const T*>(GetModule(T::kUniqueId));}

			Module* GetModule(const Dia::Core::StringCRC& crc);
			const Module* GetModule(const Dia::Core::StringCRC& crc)const;

		private:
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*, Dia::Core::StringCRCHashFunctor> ModuleTable;
			typedef Dia::Core::Containers::DynamicArray<Module*> ModuleArray;

			// Inherited from StateObject
			virtual void DoBuildDependancies(IBuildDependencyData* buildDependencies)override{};
			virtual StateObject::OpertionResponse DoStart()override;
			virtual void DoUpdate()override;
			virtual void DoStop() override;

			void StartAsyncModules(Dia::Core::Containers::DynamicArrayC<Module*, 32>& modulesFlaggedToStartAsync);

			ProcessingUnit* mAssociatedProcessingUnit;
			
			ModuleTable mAssociatedModules;

			// TODO: I am updating in the order we create the modules, we may want something more   
			//	expressive that allows me dictate what must happen in what order
			ModuleArray mUpdatingModules;

			// TODO: Instead of closing all nodes in reverse order we could do something smarter.
			//	Options include creating dependancies on exit and make sure they happen in order or 
			//	potentially allowing desyncronous exits. To be honest this will probably be fine. Though it
			//	would be nice to be able to trigger a stop and then slowly transition to the exit but i may
			//	be better able to handle that with multiple phases (ie Phase_Run -> Phase_Prepare_Stop -> Phase_Stop)
			ModuleArray mStoppingModuleOrder;  
		};
	}
}

#endif