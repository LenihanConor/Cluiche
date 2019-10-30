////////////////////////////////////////////////////////////////////////////////
// Filename: ProcessingModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplication/ApplicationProcessingUnit.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Type/BasicTypeDefines.h>

#include "DiaApplication/ApplicationPhase.h"
#include "DiaApplication/ApplicationModule.h"

namespace Dia
{
	namespace Application
	{
		//---------------------------------------------------------------------------------------------------------
		ProcessingUnit::ProcessingUnit(const Dia::Core::StringCRC& uniqueId, 
										float hz, 
										unsigned int initialModuleMapSize, 
										unsigned int initialPhaseMapSize)
			: StateObject(uniqueId)
			, mEnableThreadLimiter(false)
			, mThreadLimiter(0.0f)
			, mCurrentPhase(NULL)
			, mAssociatedPhases(initialPhaseMapSize, initialPhaseMapSize * 2)
			, mAssociatedModules(initialModuleMapSize, initialModuleMapSize * 2)
			, mPhaseTransitions(initialModuleMapSize, initialModuleMapSize * 2)
		{
			if (hz != -1.0f)
			{
				EnableThreadLimiting(hz);
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
			mCurrentPhase = phase;
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
			DIA_ASSERT(mCurrentPhase, "Null current phase %s", GetUniqueId().AsChar());
			DIA_ASSERT(mAssociatedPhases.ContainsKey(phaseCrc), "Phase %s not associated to Processing Unit %s", phaseCrc.AsChar(), GetUniqueId().AsChar());

			Phase* endPhase = mAssociatedPhases.GetItem(phaseCrc);
			PhaseTransitionList& list = mPhaseTransitions.GetItem(mCurrentPhase->GetUniqueId());

			DIA_ASSERT(list.FindIndex(phaseCrc) != -1, "Cannot transition from %s to %s in Processing Unit %s", mCurrentPhase->GetUniqueId().AsChar(), phaseCrc.AsChar(), GetUniqueId().AsChar());

			mCurrentPhase->TransitionTo(endPhase);

			mCurrentPhase = endPhase;
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
			return mAssociatedModules.GetItem(crc);
		}

		//---------------------------------------------------------------------------------------------------------
		const Module* ProcessingUnit::GetModule(const Dia::Core::StringCRC& crc)const
		{
			return mAssociatedModules.GetItemConst(crc);
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
			DIA_ASSERT(mCurrentPhase, "For Processing Unit %s Current Phase is NULL, cannot start", GetUniqueId().AsChar());

			PrePhaseStart(startData);

			if (mCurrentPhase != NULL)
			{
				mCurrentPhase->Start(startData);
			}

			PostPhaseStart(startData);

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
			DIA_ASSERT(mCurrentPhase, "For Processing Unit %s Current Phase is NULL, cannot update", GetUniqueId().AsChar());
			
			while (!FlaggedToStopUpdating())
			{
				if (mEnableThreadLimiter)
					mThreadLimiter.Start();

				PrePhaseUpdate();

				if (mCurrentPhase != nullptr)
				{
					mCurrentPhase->Update();
				}

				PostPhaseUpdate();

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
						TransitionPhase(nextTransitionCRC);
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
			DIA_ASSERT(mCurrentPhase, "For Processing Unit %s Current Phase is NULL, cannot stop", GetUniqueId().AsChar());

			PrePhaseStop();

			if (mCurrentPhase != nullptr)
			{
				mCurrentPhase->Stop();
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
	}
}