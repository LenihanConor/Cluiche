////////////////////////////////////////////////////////////////////////////////
// Filename: ProcessingModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/ApplicationProcessingUnit.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Type/BasicTypeDefines.h>
#include <DiaLogger/DiaLog.h>

#include "DiaApplicationFlow/ApplicationPhase.h"
#include "DiaApplicationFlow/ApplicationModule.h"
#include "DiaApplicationFlow/Metrics/MetricsCollectorModule.h"

namespace Dia
{
	namespace Application
	{
		//---------------------------------------------------------------------------------------------------------
		// Static member initialization
		//---------------------------------------------------------------------------------------------------------
		const Dia::Core::StringCRC ProcessingUnit::kTypeId = Dia::Core::StringCRC("ProcessingUnit");

		//---------------------------------------------------------------------------------------------------------
		ProcessingUnit::ProcessingUnit(const Dia::Core::StringCRC& uniqueId,
										float hz,
										unsigned int initialModuleMapSize,
										unsigned int initialPhaseMapSize)
			: StateObject(uniqueId)
			, mEnableThreadLimiter(false)
			, mThreadLimiter(0.0f)
			, mCurrentPhase(nullptr)
			, mTransitionInProgress(false)
			, mAssociatedPhases(initialPhaseMapSize, initialPhaseMapSize * 2)
			, mAssociatedModules(initialModuleMapSize, initialModuleMapSize * 2)
			, mPhaseTransitions(initialModuleMapSize, initialModuleMapSize * 2)
			, mErrorCallback(nullptr)
			, mMetricsCollector(nullptr)
			, mFrameTimingActive(false)
			, mHotReloadManager(nullptr)
			, mParent(nullptr)
			, mChildPUs(4, 8)
		{
			if (hz != -1.0f)
			{
				EnableThreadLimiting(hz);
			}
			mErrorHistory.reserve(100);  // Reserve space for last 100 errors
		}

		//---------------------------------------------------------------------------------------------------------
		ProcessingUnit::~ProcessingUnit()
		{
			// Destroy children bottom-up (vector destruction is back-to-front via clear)
			// Clear children in reverse order for bottom-up teardown
			while (!mOwnedChildPUs.empty())
			{
				mOwnedChildPUs.pop_back();
			}

			if (mHotReloadManager != nullptr)
			{
				delete mHotReloadManager;
				mHotReloadManager = nullptr;
			}
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::Initialize()
		{
			BuildDependencyData buildDependencyData( &mAssociatedPhases,
															&mPhaseTransitions,
															&mAssociatedModules);
			BuildDependancies(&buildDependencyData);
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::EnableThreadLimiting(float hz)
		{
			mEnableThreadLimiter = true;
			mThreadLimiter.Initialize(hz);
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::DisableThreadLimiting()
		{
			mEnableThreadLimiter = false;
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::AddPhase(Phase* phase)
		{
			DIA_ASSERT(phase != NULL, "Null phase being added for ProcessingUnit %s", GetUniqueId().AsChar());

			if (!mAssociatedPhases.ContainsKey(phase->GetUniqueId()))
			{
				mAssociatedPhases.Add(phase->GetUniqueId(), phase);
			}
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::AddModule(Module* module)
		{
			DIA_ASSERT(module != NULL, "Null module being added for ProcessingUnit %s", GetUniqueId().AsChar());

			if (!mAssociatedModules.ContainsKey(module->GetUniqueId()))
			{
				mAssociatedModules.Add(module->GetUniqueId(), module);
			}
		}

		//---------------------------------------------------------------------------------------------------------
		bool ProcessingUnit::RemoveModule(const Dia::Core::StringCRC& moduleId)
		{
			if (mAssociatedModules.ContainsKey(moduleId))
			{
				mAssociatedModules.Remove(moduleId);
				return true;
			}
			return false;
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::AddPhaseWithOwnership(Dia::Core::UniquePtr<Phase> phase)
		{
			DIA_ASSERT(phase != nullptr, "Null phase being added with ownership for ProcessingUnit %s", GetUniqueId().AsChar());

			Phase* rawPtr = phase.Get();

			// Add to lookup table
			if (!mAssociatedPhases.ContainsKey(rawPtr->GetUniqueId()))
			{
				mAssociatedPhases.Add(rawPtr->GetUniqueId(), rawPtr);
			}

			// Transfer ownership to owned storage
			mOwnedPhases.push_back(std::move(phase));
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::AddModuleWithOwnership(Dia::Core::UniquePtr<Module> module)
		{
			DIA_ASSERT(module != nullptr, "Null module being added with ownership for ProcessingUnit %s", GetUniqueId().AsChar());

			Module* rawPtr = module.Get();

			// Add to lookup table
			if (!mAssociatedModules.ContainsKey(rawPtr->GetUniqueId()))
			{
				mAssociatedModules.Add(rawPtr->GetUniqueId(), rawPtr);
			}

			// Transfer ownership to owned storage
			mOwnedModules.push_back(std::move(module));
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::SetInitialPhase(Phase* phase)
		{
			if (phase == NULL)
			{
				DIA_ASSERT(0, "Initial Phase can not be NULL for ProcessingUnit %s", GetUniqueId().AsChar());

				return;
			}

			if (mCurrentPhase != NULL)
			{
				DIA_ASSERT(0, "Current Phase already set for ProcessingUnit %s", GetUniqueId().AsChar());

				return;
			}
			
			if (!mAssociatedPhases.ContainsKey(phase->GetUniqueId()))
			{

				DIA_ASSERT(0, "Phase %s has not been added to ProcessingUnit %s", phase->GetUniqueId().AsChar(), GetUniqueId().AsChar());

				return;
			}
			mCurrentPhase.store(phase, std::memory_order_release);
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::AddPhaseTransiton(Phase* startPhase, Phase* endPhase)
		{
			DIA_ASSERT(startPhase, "Null start phase transition being added to %s", GetUniqueId().AsChar());
			DIA_ASSERT(endPhase, "Null end phase transition being added to %s", GetUniqueId().AsChar());
			DIA_ASSERT(mAssociatedPhases.ContainsKey(startPhase->GetUniqueId()), "Phase %s not associated to Processing Unit %s", startPhase->GetUniqueId().AsChar(), GetUniqueId().AsChar());
			DIA_ASSERT(mAssociatedPhases.ContainsKey(endPhase->GetUniqueId()), "Phase %s not associated to Processing Unit %s", endPhase->GetUniqueId().AsChar(), GetUniqueId().AsChar());

			if (mPhaseTransitions.ContainsKey(startPhase->GetUniqueId()))
			{
				PhaseTransitionList& list = mPhaseTransitions.GetItem(startPhase->GetUniqueId());
				list.Add(endPhase->GetUniqueId());	
			}
			else
			{
				// Need to create the new list of phases
				PhaseTransitionList newList;
				newList.Add(endPhase->GetUniqueId());

				mPhaseTransitions.Add(startPhase->GetUniqueId(), newList);
			}
		}

		//---------------------------------------------------------------------------------------------------------
		// A single transition is where we move from the current phase to any phases that is allowed
		void ProcessingUnit::TransitionPhase(const Dia::Core::StringCRC& phaseCrc)
		{
			Phase* currentPhase = mCurrentPhase.load(std::memory_order_acquire);
			DIA_ASSERT(currentPhase, "Null current phase %s", GetUniqueId().AsChar());
			if (!currentPhase)
			{
				DIA_LOG_ERROR("Application", "TransitionPhase: current phase is null in PU '%s'", GetUniqueId().AsChar());
				return;
			}

			DIA_ASSERT(mAssociatedPhases.ContainsKey(phaseCrc), "Phase %s not associated to Processing Unit %s", phaseCrc.AsChar(), GetUniqueId().AsChar());
			if (!mAssociatedPhases.ContainsKey(phaseCrc))
			{
				DIA_LOG_ERROR("Application", "TransitionPhase: phase '%s' not found in PU '%s'", phaseCrc.AsChar(), GetUniqueId().AsChar());
				return;
			}

			Phase* endPhase = mAssociatedPhases.GetItem(phaseCrc);
			PhaseTransitionList& list = mPhaseTransitions.GetItem(currentPhase->GetUniqueId());

			DIA_ASSERT(list.FindIndex(phaseCrc) != -1, "Cannot transition from %s to %s in Processing Unit %s", currentPhase->GetUniqueId().AsChar(), phaseCrc.AsChar(), GetUniqueId().AsChar());
			if (list.FindIndex(phaseCrc) == -1)
			{
				DIA_LOG_ERROR("Application", "TransitionPhase: invalid transition from '%s' to '%s' in PU '%s'", currentPhase->GetUniqueId().AsChar(), phaseCrc.AsChar(), GetUniqueId().AsChar());
				return;
			}

			currentPhase->TransitionTo(endPhase);

			mCurrentPhase.store(endPhase, std::memory_order_release);
		}

		//---------------------------------------------------------------------------------------------------------
		// A single transition is where we move from the current phase to any phases that is allowed
		void ProcessingUnit::QueuePhaseTransition(const Dia::Core::StringCRC& phaseCrc)
		{
			DIA_ASSERT(mCurrentPhase, "Null current phase %s", GetUniqueId().AsChar());
			DIA_ASSERT(mAssociatedPhases.ContainsKey(phaseCrc), "Phase %s not associated to Processing Unit %s", phaseCrc.AsChar(), GetUniqueId().AsChar());

			std::lock_guard<std::mutex> lock(mQueuedTransitionMutex);

			mQueuedTransition.Add(phaseCrc);
		}

		//---------------------------------------------------------------------------------------------------------
		Module* ProcessingUnit::GetModule(const Dia::Core::StringCRC& crc)
		{
			// COMMON PATTERN: Use TryGetItem() instead of GetItem() to avoid assert when key doesn't exist
			// TryGetItem() returns pointer to Module*, so need to dereference if found
			Module** pModule = mAssociatedModules.TryGetItem(crc);
			return pModule ? *pModule : nullptr;
		}

		//---------------------------------------------------------------------------------------------------------
		const Module* ProcessingUnit::GetModule(const Dia::Core::StringCRC& crc)const
		{
			// COMMON PATTERN: Const version uses TryGetItemConst() returning const Module* const*
			const Module* const * pModule = mAssociatedModules.TryGetItemConst(crc);
			return pModule ? *pModule : nullptr;
		}

		//---------------------------------------------------------------------------------------------------------
		Module* ProcessingUnit::FindModule(const Dia::Core::StringCRC& crc)
		{
			return GetModule(crc);
		}

		//---------------------------------------------------------------------------------------------------------
		const Module* ProcessingUnit::FindModule(const Dia::Core::StringCRC& crc) const
		{
			return GetModule(crc);
		}

		//---------------------------------------------------------------------------------------------------------
		bool ProcessingUnit::ContainsModule(const Dia::Core::StringCRC& crc)const
		{
			return (nullptr != GetModule(crc));
		}

		//---------------------------------------------------------------------------------------------------------
		// 
		// Make sure all modules and there dependancies are in the master module/phase list for this PU, as well
		// as iterating through each phase and module so that it knows its dependencies
		void ProcessingUnit::DoBuildDependancies(IBuildDependencyData* buildDependencies)
		{
			for (unsigned int i = 0; i < mAssociatedModules.Size(); i++)
			{
				Module* module = mAssociatedModules.GetItemByIndex(i);

				if (module->GetState() == StateEnum::kConstructed)
				{
					module->BuildDependancies(buildDependencies);

					const unsigned int numberDependencies = module->GetNumberOfDependancies();
					for (unsigned int j = 0; j < numberDependencies; j++)
					{
						Module* dependency = module->GetModuleFromIndex(j);

						AddModule(dependency);
					}
				}
			}

			for (unsigned int i = 0; i < mAssociatedPhases.Size(); i++)
			{
				Phase* phase = mAssociatedPhases.GetItemByIndex(i);
				
				if (phase->GetState() == StateEnum::kConstructed)
				{
					phase->BuildDependancies(buildDependencies);
				}
			}
		}
		
		//---------------------------------------------------------------------------------------------------------
		StateObject::OpertionResponse ProcessingUnit::DoStart(const IStartData* startData)
		{
			mTreeStopRequested.store(false, std::memory_order_release);

			Phase* currentPhase = mCurrentPhase.load(std::memory_order_acquire);
			DIA_ASSERT(currentPhase, "For Processing Unit %s Current Phase is NULL, cannot start", GetUniqueId().AsChar());

			PrePhaseStart(startData);

			if (currentPhase != nullptr)
			{
				currentPhase->Start(startData);
			}

			PostPhaseStart(startData);

			// Auto-start child PUs that haven't been manually started
			for (unsigned int i = 0; i < mChildPUs.Size(); ++i)
			{
				ProcessingUnit* child = mChildPUs.GetItemByIndex(i);
				if (child->HasStarted())
					continue;

				child->Initialize();
				child->Start(nullptr);

				std::thread* childThread = new std::thread(std::ref(*child));
				mManagedChildThreads.push_back({child, childThread});
			}

			// TODO: Currently only support immediate starts
			return StateObject::OpertionResponse::kImmediate;
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::operator()()
		{
			Update();
		}
	
		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::DoUpdate()
		{
			Phase* currentPhase = mCurrentPhase.load(std::memory_order_acquire);
			DIA_ASSERT(currentPhase, "For Processing Unit %s Current Phase is NULL, cannot update", GetUniqueId().AsChar());

			auto lastFrameStart = std::chrono::high_resolution_clock::now();

			while (!FlaggedToStopUpdating() && !mTreeStopRequested.load(std::memory_order_acquire))
			{
				if (mEnableThreadLimiter)
					mThreadLimiter.Start();

				// Process message queue before phase update
				mMessageBus.ProcessQueue();

				auto frameStart = std::chrono::high_resolution_clock::now();

				PrePhaseUpdate();

				currentPhase = mCurrentPhase.load(std::memory_order_acquire);
				if (currentPhase != nullptr)
				{
					currentPhase->Update();
				}

				PostPhaseUpdate();

				if (mMetricsCollector != nullptr)
				{
					float deltaMs = std::chrono::duration<float, std::milli>(frameStart - lastFrameStart).count();
					if (deltaMs > 0.001f)
						mMetricsCollector->ReportFrame(GetUniqueId(), GetUniqueId().AsChar(), deltaMs);
				}
				lastFrameStart = frameStart;

				{
					bool transition = false;
					Dia::Core::StringCRC nextTransitionCRC;

					{
						std::lock_guard<std::mutex> lock(mQueuedTransitionMutex);

						// If there are any transitioned phase move to them now
						if (mQueuedTransition.Size() != 0)
						{
							nextTransitionCRC = mQueuedTransition[0];

							mQueuedTransition.RemoveAt(0);

							transition = true;
						}
					}

					if (transition)
					{
						// Only transition if not already transitioning (prevents multiple simultaneous transitions)
						bool expected = false;
						if (mTransitionInProgress.compare_exchange_strong(expected, true))
						{
							TransitionPhase(nextTransitionCRC);
							mTransitionInProgress.store(false);
						}
					}
				}

				if (mEnableThreadLimiter)
				{
					mThreadLimiter.Stop();
					//	std::cout << "Main: Wait " << threadLimiter.RemainingTime().AsIntInMilliseconds() << " ms\n";
					mThreadLimiter.SleepThread();
				}
			}
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::DoStop()
		{
			Phase* currentPhase = mCurrentPhase.load(std::memory_order_acquire);
			DIA_ASSERT(currentPhase, "For Processing Unit %s Current Phase is NULL, cannot stop", GetUniqueId().AsChar());

			// Signal only auto-managed children to exit their update loops
			for (auto& entry : mManagedChildThreads)
			{
				entry.pu->mTreeStopRequested.store(true, std::memory_order_release);
			}

			// Join auto-managed child threads (reverse order for bottom-up teardown)
			for (int i = static_cast<int>(mManagedChildThreads.size()) - 1; i >= 0; --i)
			{
				if (mManagedChildThreads[i].thread && mManagedChildThreads[i].thread->joinable())
				{
					mManagedChildThreads[i].thread->join();
				}
				delete mManagedChildThreads[i].thread;
			}

			// Stop auto-managed child PUs (after threads have exited)
			for (auto& entry : mManagedChildThreads)
			{
				entry.pu->Stop();
			}
			mManagedChildThreads.clear();

			PrePhaseStop();

			if (currentPhase != nullptr)
			{
				currentPhase->Stop();
			}

			PostPhaseStop();
		}

		//---------------------------------------------------------------------------------------------------------
		BuildDependencyData::BuildDependencyData(ProcessingUnit::PhasesTable* associatedPhases,
													ProcessingUnit::PhaseTransitionTable* phaseTransitions,
													ProcessingUnit::ModuleTable* associatedModules)
			:  mAssociatedPhases(associatedPhases)
			, mPhaseTransitions(phaseTransitions)
			, mAssociatedModules(associatedModules)
		{}

		//---------------------------------------------------------------------------------------------------------
		Module* BuildDependencyData::GetModule(const Dia::Core::StringCRC& crc)
		{
			return mAssociatedModules->GetItem(crc);
		}

		//---------------------------------------------------------------------------------------------------------
		const Module* BuildDependencyData::GetModule(const Dia::Core::StringCRC& crc)const
		{
			return mAssociatedModules->GetItemConst(crc);
		}

		//---------------------------------------------------------------------------------------------------------
		Phase* BuildDependencyData::GetPhase(const Dia::Core::StringCRC& crc)
		{
			return mAssociatedPhases->GetItem(crc);
		}

		//---------------------------------------------------------------------------------------------------------
		const Phase* BuildDependencyData::GetPhase(const Dia::Core::StringCRC& crc)const
		{
			return mAssociatedPhases->GetItemConst(crc);
		}

		//---------------------------------------------------------------------------------------------------------
		// Error handling methods
		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::SetErrorCallback(ErrorCallback callback)
		{
			std::lock_guard<std::mutex> lock(mErrorMutex);
			mErrorCallback = callback;
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::ReportError(const ErrorInfo& error)
		{
			{
				std::lock_guard<std::mutex> lock(mErrorMutex);

				// Add to history (keep last 100 errors)
				mErrorHistory.push_back(error);
				if (mErrorHistory.size() > 100)
				{
					mErrorHistory.erase(mErrorHistory.begin());
				}

				// Call user callback if set
				if (mErrorCallback != nullptr)
				{
					mErrorCallback(error);
				}
			}

			// Propagate to parent
			if (mParent != nullptr)
			{
				mParent->ReportError(error);
			}
		}

		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::ClearErrorHistory()
		{
			std::lock_guard<std::mutex> lock(mErrorMutex);
			mErrorHistory.clear();
		}

		//---------------------------------------------------------------------------------------------------------
		// Metrics collector injection
		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::SetMetricsCollector(MetricsCollectorModule* collector)
		{
			mMetricsCollector = collector;
		}

		MetricsCollectorModule* ProcessingUnit::GetMetricsCollector() const
		{
			return mMetricsCollector;
		}

		//---------------------------------------------------------------------------------------------------------
		// Hot reload manager access (lazy initialization)
		//---------------------------------------------------------------------------------------------------------
		HotReloadManager* ProcessingUnit::GetHotReloadManager()
		{
			if (mHotReloadManager == nullptr)
			{
				mHotReloadManager = new HotReloadManager(this);
			}
			return mHotReloadManager;
		}

		//---------------------------------------------------------------------------------------------------------
		const HotReloadManager* ProcessingUnit::GetHotReloadManager() const
		{
			if (mHotReloadManager == nullptr)
			{
				mHotReloadManager = new HotReloadManager(const_cast<ProcessingUnit*>(this));
			}
			return mHotReloadManager;
		}

		//---------------------------------------------------------------------------------------------------------
		// Serialization methods
		//---------------------------------------------------------------------------------------------------------
		void ProcessingUnit::SerializeTopology(Json::Value& out) const
		{
			// Export basic info
			out["type"] = "ProcessingUnit";
			out["id"] = GetUniqueId().AsChar();

			// Export current phase
			Phase* currentPhase = mCurrentPhase.load(std::memory_order_acquire);
			if (currentPhase != nullptr)
			{
				out["currentPhase"] = currentPhase->GetUniqueId().AsChar();
			}

			// Export all phases
			Json::Value phasesArray(Json::arrayValue);
			for (unsigned int i = 0; i < mAssociatedPhases.Size(); i++)
			{
				Phase* phase = mAssociatedPhases.GetItemByIndexConst(i);
				Json::Value phaseObj;
				phaseObj["id"] = phase->GetUniqueId().AsChar();
				phaseObj["typeId"] = Phase::kTypeId.AsChar();
				phasesArray.append(phaseObj);
			}
			out["phases"] = phasesArray;

			// Export all modules
			Json::Value modulesArray(Json::arrayValue);
			for (unsigned int i = 0; i < mAssociatedModules.Size(); i++)
			{
				Module* module = mAssociatedModules.GetItemByIndexConst(i);
				Json::Value moduleObj;
				moduleObj["id"] = module->GetUniqueId().AsChar();
				moduleObj["typeId"] = Module::kTypeId.AsChar();
				modulesArray.append(moduleObj);
			}
			out["modules"] = modulesArray;

			// Export phase transitions using iterator
			Json::Value transitionsArray(Json::arrayValue);
			for (auto it = mPhaseTransitions.Begin(); it != mPhaseTransitions.End(); ++it)
			{
				const Dia::Core::StringCRC& fromPhaseId = it.Key();
				const PhaseTransitionList& transitions = it.Value();

				for (unsigned int j = 0; j < transitions.Size(); j++)
				{
					Json::Value transitionObj;
					transitionObj["from"] = fromPhaseId.AsChar();
					transitionObj["to"] = transitions[j].AsChar();
					transitionsArray.append(transitionObj);
				}
			}
			out["transitions"] = transitionsArray;
		}

		//---------------------------------------------------------------------------------------------------------
		bool ProcessingUnit::DeserializeTopology(const Json::Value& in)
		{
			// Base implementation is no-op
			// Subclasses can override to restore topology from JSON
			// NOTE: This is intentionally minimal - deserialization is complex and
			// typically requires factory support to reconstruct objects
			return true;
		}

		//---------------------------------------------------------------------------------------------------------
		// PU Tree methods
		//---------------------------------------------------------------------------------------------------------

		bool ProcessingUnit::AddChildProcessingUnit(Dia::Core::UniquePtr<ProcessingUnit> child)
		{
			if (!child)
				return false;

			const Dia::Core::StringCRC& childId = child->GetUniqueId();

			// Reject duplicate ID among direct children
			if (mChildPUs.ContainsKey(childId))
				return false;

			// Reject if ID matches this PU
			if (childId == GetUniqueId())
				return false;

			// Check tree-wide uniqueness by walking up to root and searching down
			ProcessingUnit* root = this;
			while (root->mParent != nullptr)
				root = root->mParent;

			if (root->FindProcessingUnitInTree(childId) != nullptr)
				return false;

			// Check max depth
			unsigned int depth = 0;
			ProcessingUnit* ancestor = this;
			while (ancestor != nullptr)
			{
				++depth;
				ancestor = ancestor->mParent;
			}
			if (depth >= kMaxTreeDepth)
				return false;

			// Wire parent pointer
			child->mParent = this;

			// Transfer ownership first (if push_back throws, hash table stays consistent)
			ProcessingUnit* rawPtr = child.Get();
			mOwnedChildPUs.push_back(std::move(child));

			// Store raw pointer in lookup table
			mChildPUs.Add(childId, rawPtr);

			return true;
		}

		//---------------------------------------------------------------------------------------------------------
		bool ProcessingUnit::RemoveChildProcessingUnit(const Dia::Core::StringCRC& childId)
		{
			if (!mChildPUs.ContainsKey(childId))
				return false;

			mChildPUs.Remove(childId);

			// Find and remove from owned storage
			for (auto it = mOwnedChildPUs.begin(); it != mOwnedChildPUs.end(); ++it)
			{
				if (*it && (*it)->GetUniqueId() == childId)
				{
					mOwnedChildPUs.erase(it);
					break;
				}
			}

			return true;
		}

		//---------------------------------------------------------------------------------------------------------
		ProcessingUnit* ProcessingUnit::GetParent()
		{
			return mParent;
		}

		//---------------------------------------------------------------------------------------------------------
		const ProcessingUnit* ProcessingUnit::GetParent() const
		{
			return mParent;
		}

		//---------------------------------------------------------------------------------------------------------
		ProcessingUnit* ProcessingUnit::FindChildProcessingUnit(const Dia::Core::StringCRC& childId)
		{
			ProcessingUnit** pp = mChildPUs.TryGetItem(childId);
			return pp ? *pp : nullptr;
		}

		//---------------------------------------------------------------------------------------------------------
		const ProcessingUnit* ProcessingUnit::FindChildProcessingUnit(const Dia::Core::StringCRC& childId) const
		{
			const ProcessingUnit* const* pp = mChildPUs.TryGetItemConst(childId);
			return pp ? *pp : nullptr;
		}

		//---------------------------------------------------------------------------------------------------------
		const ProcessingUnit::ProcessingUnitTable& ProcessingUnit::GetChildren() const
		{
			return mChildPUs;
		}

		//---------------------------------------------------------------------------------------------------------
		bool ProcessingUnit::IsRoot() const
		{
			return mParent == nullptr;
		}

		//---------------------------------------------------------------------------------------------------------
		ProcessingUnit* ProcessingUnit::FindProcessingUnitInTree(const Dia::Core::StringCRC& id)
		{
			if (GetUniqueId() == id)
				return this;

			for (unsigned int i = 0; i < mChildPUs.Size(); ++i)
			{
				ProcessingUnit* child = mChildPUs.GetItemByIndex(i);
				ProcessingUnit* found = child->FindProcessingUnitInTree(id);
				if (found)
					return found;
			}

			return nullptr;
		}

		//---------------------------------------------------------------------------------------------------------
		const ProcessingUnit* ProcessingUnit::FindProcessingUnitInTree(const Dia::Core::StringCRC& id) const
		{
			if (GetUniqueId() == id)
				return this;

			for (unsigned int i = 0; i < mChildPUs.Size(); ++i)
			{
				const ProcessingUnit* child = mChildPUs.GetItemByIndexConst(i);
				const ProcessingUnit* found = child->FindProcessingUnitInTree(id);
				if (found)
					return found;
			}

			return nullptr;
		}
	}
}