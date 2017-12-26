////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationModule.h
////////////////////////////////////////////////////////////////////////////////
#ifndef _APPLICATIONMODULE_H_
#define _APPLICATIONMODULE_H_

#include <DiaApplication/ApplicationStateObject.h>

#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>

namespace Dia
{
	namespace Application
	{		
		class ProcessingUnit;
		class Phase;

		////////////////////////////////////////////////////////////////////////////////
		// Class name: ApplicationModule
		////////////////////////////////////////////////////////////////////////////////
		class Module: public StateObject
		{
		public:
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*, Dia::Core::StringCRCHashFunctor> ModuleHashTable;

			////////////////////////////////////////////////////////////////////////////////
			// Enum name: RunningEnum, Return enum to Start call to know if we need to update
			////////////////////////////////////////////////////////////////////////////////
			CLASSEDENUM (RunningEnum,\
				CE_ITEMVAL(kIdle, 0)\
				CE_ITEM(kUpdate)\
				, kIdle \
			);

			////////////////////////////////////////////////////////////////////////////////
			// Enum name: StateEnum
			////////////////////////////////////////////////////////////////////////////////
			Module(ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& uniqueId, RunningEnum runningMode, unsigned int initialDependencyMapSize = 2);

			bool HasAllDependanciesStarted()const;
			bool RequiresUpdating()const;

			unsigned int GetNumberOfDependancies()const;
			

			Module* GetModuleFromIndex(unsigned int index);
			const Module* GetModuleFromIndex(unsigned int index)const;

			Module* GetModule(const Dia::Core::StringCRC& uniqueId);
			const Module* GetModule(const Dia::Core::StringCRC& uniqueId)const;

			template <class T> inline T* GetModule() { return static_cast<T*>(GetModule(T::kUniqueId)); }
			template <class T> inline const T* GetModule() const { return static_cast<const T*>(GetModule(T::kUniqueId)); }

			void AddDependancy(Module* dependancy);

			void RetainThroughTransition(const Phase* startPhase, const Phase* endPhase);

			virtual const char* GetStateObjectType()const override { return "Module"; }

		protected:
			// Inherited from StateObject - A specific module may override this as needed
			virtual void DoBuildDependancies(IBuildDependencyData* buildDependencies)override{};
			virtual StateObject::OpertionResponse DoStart(const IStartData* startData) override { return StateObject::OpertionResponse::kImmediate;  } // We default to immediate unless overriden by derived class
			virtual void DoUpdate() override {};
			virtual void DoStop() override {};

			// New virtual function
			virtual void DoRetainThroughTransition(const Phase* startPhase, const Phase* endPhase){};

			ProcessingUnit* GetAssociatedProcessingUnit();
			const ProcessingUnit* GetAssociatedProcessingUnit()const;

		private:
			ProcessingUnit* mAssociatedProcessingUnit;

			RunningEnum mRunningMode;
			ModuleHashTable mDependencies;
		};
	}
}

#endif