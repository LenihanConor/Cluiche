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

#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaApplicationFlow/ApplicationPhase.h>
#include <DiaApplicationFlow/ApplicationError.h>
#include <DiaApplicationFlow/MessageBus.h>
#include <DiaApplicationFlow/HotReloadManager.h>

#include "DiaApplicationFlow/ApplicationStateObject.h"

#include <chrono>
#include <thread>

#include <DiaCore/Core/EnumClass.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Timer/TimeThreadLimiter.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/Json/external/json/json.h>

#include <atomic>
#include <mutex>
#include <vector>

namespace Dia
{
	namespace Application
	{
		class Phase;
		class ApplicationManifestLoader;
		class ApplicationTypeRegistry;
		class MetricsCollectorModule;
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
			friend class HotReloadManager;
			friend class ApplicationManifestLoader;
			friend class ApplicationIntrospector;

		public:
			static const Dia::Core::StringCRC kTypeId;

			typedef Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> PhaseTransitionList; // TODO: I am setting this to "8" could be any size to be honest.
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*, Dia::Core::StringCRCHashFunctor> ModuleTable;
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Phase*, Dia::Core::StringCRCHashFunctor> PhasesTable;
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, PhaseTransitionList, Dia::Core::StringCRCHashFunctor> PhaseTransitionTable;
			typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, ProcessingUnit*, Dia::Core::StringCRCHashFunctor> ProcessingUnitTable;

			ProcessingUnit(const Dia::Core::StringCRC& uniqueId, float hz = -1.0f, unsigned int initialModuleMapSize = 8, unsigned int initialPhaseMapSize = 8);
			virtual ~ProcessingUnit();

			void Initialize();
			
			void EnableThreadLimiting(float hz);
			void DisableThreadLimiting();

			/**
			 * @brief Adds a phase to this processing unit.
			 * @param phase Pointer to phase. Ownership remains with caller.
			 *              Caller must ensure phase lifetime exceeds ProcessingUnit lifetime.
			 *              Phase will NOT be automatically deleted.
			 */
			void AddPhase(Phase* phase);

			/**
			 * @brief Adds a module to this processing unit.
			 * @param module Pointer to module. Ownership remains with caller.
			 *               Caller must ensure module lifetime exceeds ProcessingUnit lifetime.
			 *               Module will NOT be automatically deleted.
			 */
			void AddModule(Module* module);

			/**
			 * @brief Removes a module from this processing unit.
			 * @param moduleId CRC ID of module to remove
			 * @return True if module was found and removed, false otherwise
			 * @note Module is removed from internal tables but NOT deleted.
			 *       Caller is responsible for deletion if needed.
			 */
			bool RemoveModule(const Dia::Core::StringCRC& moduleId);

			/**
			 * @brief Adds a phase to this processing unit with ownership transfer.
			 * @param phase UniquePtr to phase. ProcessingUnit takes ownership.
			 *              Phase will be automatically deleted when ProcessingUnit is destroyed.
			 */
			void AddPhaseWithOwnership(Dia::Core::UniquePtr<Phase> phase);

			/**
			 * @brief Adds a module to this processing unit with ownership transfer.
			 * @param module UniquePtr to module. ProcessingUnit takes ownership.
			 *               Module will be automatically deleted when ProcessingUnit is destroyed.
			 */
			void AddModuleWithOwnership(Dia::Core::UniquePtr<Module> module);

			void SetInitialPhase(Phase* phase);
			void AddPhaseTransiton(Phase* startPhase, Phase* endPhase);

			void TransitionPhase(const Dia::Core::StringCRC& phaseCrc);		// Transition immediately
			void QueuePhaseTransition(const Dia::Core::StringCRC& crc);		// Transition after next update (this is thread safe)

			Phase* GetCurrentPhase(){ return mCurrentPhase.load(std::memory_order_acquire); }
			const Phase* GetCurrentPhase()const{ return mCurrentPhase.load(std::memory_order_acquire); }

			bool ContainsModule(const Dia::Core::StringCRC& crc)const;

			Module* FindModule(const Dia::Core::StringCRC& crc);
			const Module* FindModule(const Dia::Core::StringCRC& crc) const;

			// Error handling
			void SetErrorCallback(ErrorCallback callback);
			void ReportError(const ErrorInfo& error);
			const std::vector<ErrorInfo>& GetErrorHistory() const { return mErrorHistory; }
			void ClearErrorHistory();

			// Message bus access
			MessageBus& GetMessageBus() { return mMessageBus; }
			const MessageBus& GetMessageBus() const { return mMessageBus; }

			// Registry access — set by ApplicationManifestLoader after construction
			ApplicationTypeRegistry* GetTypeRegistry() { return mTypeRegistry; }
			const ApplicationTypeRegistry* GetTypeRegistry() const { return mTypeRegistry; }

			// Metrics collector injection
			void SetMetricsCollector(MetricsCollectorModule* collector);
			MetricsCollectorModule* GetMetricsCollector() const;

			// Hot reload manager (created on first access)
			HotReloadManager* GetHotReloadManager();
			const HotReloadManager* GetHotReloadManager() const;

			// === PU Tree API ===

			bool AddChildProcessingUnit(Dia::Core::UniquePtr<ProcessingUnit> child);
			bool RemoveChildProcessingUnit(const Dia::Core::StringCRC& childId);

			ProcessingUnit* GetParent();
			const ProcessingUnit* GetParent() const;

			ProcessingUnit* FindChildProcessingUnit(const Dia::Core::StringCRC& childId);
			const ProcessingUnit* FindChildProcessingUnit(const Dia::Core::StringCRC& childId) const;

			const ProcessingUnitTable& GetChildren() const;

			bool IsRoot() const;

			ProcessingUnit* FindProcessingUnitInTree(const Dia::Core::StringCRC& id);
			const ProcessingUnit* FindProcessingUnitInTree(const Dia::Core::StringCRC& id) const;

			// Serialization support - exports current phase/module topology
			virtual void SerializeTopology(Json::Value& out) const;
			virtual bool DeserializeTopology(const Json::Value& in);

			void operator()(); // Used if we are threading

			virtual const char* GetStateObjectType()const override { return "Processing Unit"; }

		protected:
			template <class T> inline
			T*	GetModule() { return static_cast<T*>(GetModule(T::kTypeId)); }

			template <class T> inline
			const T* GetModule() const { return static_cast<const T*>(GetModule(T::kTypeId)); }

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
			std::atomic<Phase*> mCurrentPhase;
			std::atomic<bool> mTransitionInProgress;
			std::mutex mQueuedTransitionMutex;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> mQueuedTransition;	// FIFO List of phase transition
			PhasesTable mAssociatedPhases;
			PhaseTransitionTable mPhaseTransitions;
		
			// List of all modules that are associated to this PU. Phase can choose from these to enable/disable as they see fit
			ModuleTable mAssociatedModules;

			// Smart pointer storage for owned modules/phases
			// These are automatically cleaned up when ProcessingUnit is destroyed
			std::vector<Dia::Core::UniquePtr<Module>> mOwnedModules;
			std::vector<Dia::Core::UniquePtr<Phase>> mOwnedPhases;

			// Error tracking
			ErrorCallback mErrorCallback;
			mutable std::mutex mErrorMutex;
			std::vector<ErrorInfo> mErrorHistory;	// Last 100 errors

			// Message bus
			MessageBus mMessageBus;

			// Metrics collector (optional, injected)
			MetricsCollectorModule* mMetricsCollector;
			std::chrono::time_point<std::chrono::high_resolution_clock> mFrameStartTime;
			bool mFrameTimingActive;

			// Hot reload manager (optional, created on demand)
			mutable HotReloadManager* mHotReloadManager;

			// Set by ApplicationManifestLoader after construction via friend access
			ApplicationTypeRegistry* mTypeRegistry = nullptr;

			// PU Tree members
			ProcessingUnit* mParent = nullptr;
			ProcessingUnitTable mChildPUs;
			std::vector<Dia::Core::UniquePtr<ProcessingUnit>> mOwnedChildPUs;

			struct ManagedChildThread
			{
				ProcessingUnit* pu;
				std::thread* thread;
			};
			std::vector<ManagedChildThread> mManagedChildThreads;
			std::atomic<bool> mTreeStopRequested{false};

			static const unsigned int kMaxTreeDepth = 8;
		};

		////////////////////////////////////////////////////////////////////////////////
		// Class name: BuildDependencyData
		////////////////////////////////////////////////////////////////////////////////
		class BuildDependencyData: public Dia::Application::IBuildDependencyData
		{
		public: 
			BuildDependencyData(ProcessingUnit::PhasesTable* associatedPhases,
									ProcessingUnit::PhaseTransitionTable* phaseTransitions,
									ProcessingUnit::ModuleTable* associatedModules);

			virtual Module* GetModule(const Dia::Core::StringCRC& crc) override;
			virtual const Module* GetModule(const Dia::Core::StringCRC& crc)const override;

			virtual Phase* GetPhase(const Dia::Core::StringCRC& crc) override;
			virtual const Phase* GetPhase(const Dia::Core::StringCRC& crc)const override;

		private:
			ProcessingUnit::PhasesTable* mAssociatedPhases;
			ProcessingUnit::PhaseTransitionTable* mPhaseTransitions;
			ProcessingUnit::ModuleTable* mAssociatedModules;
		};
	}
}

#endif