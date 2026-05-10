////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationModule.h
////////////////////////////////////////////////////////////////////////////////
#ifndef _APPLICATIONMODULE_H_
#define _APPLICATIONMODULE_H_

#include <DiaApplicationFlow/ApplicationStateObject.h>
#include <DiaApplicationFlow/ApplicationError.h>

#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/Json/external/json/json.h>

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
			static const Dia::Core::StringCRC kTypeId;

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

			template <class T> inline T* GetModule() { return static_cast<T*>(GetModule(T::kTypeId)); }
			template <class T> inline const T* GetModule() const { return static_cast<const T*>(GetModule(T::kTypeId)); }

			void AddDependancy(Module* dependancy);

			void RetainThroughTransition(const Phase* startPhase, const Phase* endPhase);

			// Hot reload support (opt-in design)
			// PATTERN: Modules must explicitly opt-in to hot reload by overriding CanHotReload() to return true
			struct ModuleVersion
			{
				int major;  // Breaking changes (incompatible with previous versions)
				int minor;  // New features (backward compatible)
				int patch;  // Bug fixes (backward compatible)

				ModuleVersion() : major(0), minor(0), patch(0) {}
				ModuleVersion(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

				// IMPORTANT: Semantic versioning - only same major version is compatible
				// Example: v1.2.3 can replace v1.0.0, but v2.0.0 cannot replace v1.x.x
				bool IsCompatibleWith(const ModuleVersion& other) const
				{
					return major == other.major;
				}
			};

			// Override to return true if this module supports hot reloading at runtime
			virtual bool CanHotReload() const { return false; }

			// Override to return this module's semantic version (major.minor.patch)
			virtual ModuleVersion GetVersion() const { return ModuleVersion(1, 0, 0); }

			// PATTERN: SaveState/RestoreState for transferring module state during hot reload
			// SaveState() should allocate and return a pointer to serialized state
			// Return nullptr if no state needs to be transferred
			// IMPORTANT: Caller is responsible for passing this pointer to RestoreState() and cleanup
			virtual void* SaveState() { return nullptr; }

			// RestoreState() receives the pointer from SaveState() and should deserialize/restore state
			// IMPORTANT: Implementation must delete the state pointer after restoring
			virtual void RestoreState(void* state) {}

			// PATTERN: Use DoStartWithError() instead of DoStart() for detailed error reporting
			// Returns ErrorInfo with isFailure flag and optional message string
			virtual ErrorInfo DoStartWithError(const IStartData* startData) { return ErrorInfo(); }

			// Configuration serialization (subclasses can override for custom config)
			virtual void SerializeConfig(Json::Value& out) const {}
			virtual bool DeserializeConfig(const Json::Value& in) { return true; }

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
			template<class T> friend class ModuleRef;

			ProcessingUnit* mAssociatedProcessingUnit;

			RunningEnum mRunningMode;
			ModuleHashTable mDependencies;
		};
	}
}

#endif