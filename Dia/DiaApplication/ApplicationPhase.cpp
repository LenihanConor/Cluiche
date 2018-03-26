////////////////////////////////////////////////////////////////////////////////
// Filename: ApplicationPhase.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplication/ApplicationPhase.h"

#include "DiaApplication/ApplicationProcessingUnit.h"

#include <DiaCore/Strings/String1024.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaCore/Timer/TimerSystem.h>
#include <DiaCore/Core/Log.h>

#include <DiaApplication/ApplicationProcessingUnit.h>

#include <chrono>
#include <thread>

namespace Dia
{
	namespace Application
	{
		////////////////////////////////////////////////////////////////////////////////
		// Class name: ApplicationPhase
		////////////////////////////////////////////////////////////////////////////////
		
		//-----------------------------------------------------------------------------
		Phase::Phase(ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& uniqueId, unsigned int maxModules)
			: StateObject (uniqueId)
			, mAssociatedProcessingUnit(associatedProcessingUnit)
			, mUpdatingModules(maxModules)
			, mStoppingModuleOrder(maxModules)
			, mAssociatedModules(maxModules, maxModules * 2)
		{
			mAssociatedProcessingUnit->AddPhase(this);
		}

		//---------------------------------------------------------------------------------------------------------
		void Phase::AddModule(Module* module)
		{
			if (!mAssociatedProcessingUnit->ContainsModule(module->GetUniqueId()))
			{
				DIA_ASSERT(0, "Module %s is not associated with Processing Unit %s and is being added by Phase %s", module->GetUniqueId().AsChar(), mAssociatedProcessingUnit->GetUniqueId().AsChar(), this->GetUniqueId().AsChar());
				return;
			}

			if (!mAssociatedModules.ContainsKey(module->GetUniqueId()))
			{
				mAssociatedModules.Add(module->GetUniqueId(), module);

				// Get all dependant modules and add them to the table as well
				unsigned int numberOfDependencies = module->GetNumberOfDependancies();
				for(unsigned int i = 0; i < numberOfDependencies; i++)
				{
					AddModule(module->GetModuleFromIndex(i));
				}
			}
		}
				
		//---------------------------------------------------------------------------------------------------------
		//
		// This takes a list of all modules required to start a phase it follow these steps
		//		- Creates a list of all modules that need to be started
		//		- Removes all modules that are all ready started
		//		- Iterates through all the modules finding modules that have no dependancies and starting them
		//		- If it gets blocked that it has no modules with no dependancies this should assert and fail to start
		//		- A module can flag itself as an async start in this case we will not remove it from the start list until it is finished
		//		- This means we may get stuck waiting for dependencies to end before we can continue
		StateObject::OpertionResponse Phase::DoStart(const IStartData* startData)
		{
			BeforeModulesStart();

			// Creates a list of all modules that need to be started
			Dia::Core::Containers::DynamicArrayC<Module*, 128> modulesToStart;
			for (unsigned int i = 0; i < mAssociatedModules.Size(); i++)
			{
				Module* pModule = mAssociatedModules.GetItemByIndex(i);

				if (!pModule->HasStarted())
				{
					modulesToStart.Add(pModule);
				}
			}

			// Iterates through all the modules
			unsigned int numberOfModuleToStart = modulesToStart.Size();
			while (numberOfModuleToStart > 0)
			{	
				Dia::Core::Containers::DynamicArrayC<Module*, 32> modulesFlaggedToStartAsync;

				// Finding modules that have no dependancies
				unsigned int numberOfModulesStarted = 0;
				for (unsigned int i = 0; i < modulesToStart.Size(); i++)
				{
					Module* pModule = modulesToStart.At(i);

					if (pModule != nullptr && pModule->HasAllDependanciesStarted())
					{
						StateObject::OpertionResponse response = pModule->Start(startData);

						mStoppingModuleOrder.Add(pModule);

						switch (response)
						{
						case StateObject::OpertionResponse::kImmediate:
							numberOfModulesStarted++;
							break;
						case StateObject::OpertionResponse::kAsync:
							modulesFlaggedToStartAsync.Add(pModule);
							break;
						default: DIA_ASSERT(0, "Cannot handle this response: %s", response.AsString());
							break;
						}

						// Remove the module from the list of modules that need starting
						modulesToStart[i] = nullptr;
					}
				}

				DIA_ASSERT(numberOfModulesStarted > 0 || modulesFlaggedToStartAsync.Size() > 0, "Cannot start Phase %s, due to the module dependency having circular references", GetUniqueId().AsChar());

				StartAsyncModules(modulesFlaggedToStartAsync);

				numberOfModuleToStart = 0;
				for (size_t i = 0; i < modulesToStart.Size(); i++)
				{
					if (modulesToStart[i] != nullptr)
					{
						numberOfModuleToStart++;
					}
				}
			}

			AfterModulesStart();
			
			// This will add the modules in order of starting and should make it order dependant
			for (unsigned int i = 0; i < mAssociatedModules.Size(); i++)
			{
				Module* pModule = mAssociatedModules.GetItemByIndex(i);

				if (pModule->RequiresUpdating() && mUpdatingModules.FindIndex(pModule) == -1)
				{
					mUpdatingModules.Add(pModule);
				}
			}

			// TODO: If we want Phase to start async this is what we would have to change, 
			// i dont think this is a good idea for now. Later we could do this so that we
			// can start updating the modules already started immediately instead of waiting
			// on the rest to come in.
			return StateObject::OpertionResponse::kImmediate;
		}
		
		//---------------------------------------------------------------------------------------------------------
		void Phase::DoUpdate()
		{
			BeforeModulesUpdate();

			unsigned int numberOfModules = mUpdatingModules.Size();
			for(unsigned int i = 0; i < numberOfModules; i++)
			{
				mUpdatingModules[i]->Update();
			}

			AfterModulesUpdate();
		}
		
		//---------------------------------------------------------------------------------------------------------
		void Phase::DoStop()
		{
			BeforeModulesStop();

			unsigned int numberOfModules = mStoppingModuleOrder.Size();
			for (unsigned int i = numberOfModules - 1; i >= 0 && i < numberOfModules; i--)
			{
				mStoppingModuleOrder.At(i)->Stop();
			}

			mUpdatingModules.RemoveAll();
			mStoppingModuleOrder.RemoveAll();

			AfterModulesStop();
		}

		//-----------------------------------------------------------------------------
		// Called by the internal phase to request to transition to the next state
		void Phase::QueuePhaseTransition(const Dia::Core::StringCRC& crc)
		{
			mAssociatedProcessingUnit->QueuePhaseTransition(crc);
		}

		//-----------------------------------------------------------------------------
		//
		// Take us from one phase to another
		//	Modules that are in both should be retained (and notified of this)
		//	Modules that are only in the start phase need to be stopped
		//	Modules that are only in the new phase need to be started
		void Phase::TransitionTo(Phase* endPhase)
		{
			Dia::Core::Log::OutputVaradicLine("Transitioning Phase From %s to %s", GetUniqueId().AsChar(), endPhase->GetUniqueId().AsChar());

			// Get list of all the modules we need to sort out
			Dia::Core::Containers::DynamicArrayC<Module*, 128> modulesToStop;
			Dia::Core::Containers::DynamicArrayC<Module*, 128> modulesToRetain;

			for (unsigned int i = 0; i < mAssociatedModules.Size(); i++)
			{
				Module* pStartModule = mAssociatedModules.GetItemByIndex(i);

				if (endPhase->ContainsModule(pStartModule->GetUniqueId()))
				{
					modulesToRetain.Add(pStartModule);
				}
				else
				{
					modulesToStop.Add(pStartModule);
				}
			}

			// To keep order we tranverse the existing update and stopping order list and add all retaining modules
			for (unsigned int i = 0; i < mUpdatingModules.Size(); i++)
			{
				Module* module = mUpdatingModules[i];
				if (modulesToRetain.FindIndex(module) != -1)
				{
					endPhase->mUpdatingModules.Add(module);
				}
			}
			mUpdatingModules.RemoveAll();

			BeforeModulesStop();

			//	Modules that are only in the start phase need to be stopped
			const unsigned int numberOfModules = mStoppingModuleOrder.Size();
			for (unsigned int i = numberOfModules - 1; i >= 0 && i < numberOfModules; --i)
			{
				Module* moduleToStop = mStoppingModuleOrder.At(i);
				if (modulesToStop.FindIndex(moduleToStop) != -1)
				{
					moduleToStop->Stop();
				}
			}

			AfterModulesStop();

			for (unsigned int i = 0; i < mStoppingModuleOrder.Size(); i++)
			{
				Module* module = mStoppingModuleOrder[i];
				if (modulesToRetain.FindIndex(module) != -1)
				{
					endPhase->mStoppingModuleOrder.Add(module);
				}
			}
			mStoppingModuleOrder.RemoveAll();

			//	Modules that are in both should be retained (and notified of this)
			for (unsigned int i = 0; i < modulesToRetain.Size(); i++)
			{
				Module* moduleToRetain = modulesToRetain.At(i);
				moduleToRetain->RetainThroughTransition(this, endPhase);
			}

			// Set previous phase to Stop
			this->AfterPhaseTransition();

			//	Modules that are only in the new phase need to be started
			endPhase->Start();
		}

		//-----------------------------------------------------------------------------
		ProcessingUnit* Phase::GetAssociatedProcessingUnit()
		{
			DIA_ASSERT(mAssociatedProcessingUnit, "mAssociatedProcessingUnit is NULL");

			return mAssociatedProcessingUnit;
		}
			
		//-----------------------------------------------------------------------------
		const ProcessingUnit* Phase::GetAssociatedProcessingUnit()const
		{
			DIA_ASSERT(mAssociatedProcessingUnit, "mAssociatedProcessingUnit is NULL");

			return mAssociatedProcessingUnit;
		}

		//---------------------------------------------------------------------------------------------------------
		Module* Phase::GetModule(const Dia::Core::StringCRC& crc)
		{
			return mAssociatedModules.GetItem(crc);
		}

		//---------------------------------------------------------------------------------------------------------
		const Module* Phase::GetModule(const Dia::Core::StringCRC& crc)const
		{
			return mAssociatedModules.GetItemConst(crc);
		}

		//---------------------------------------------------------------------------------------------------------
		bool Phase::ContainsModule(const Dia::Core::StringCRC& crc)const
		{
			const Module* const * ppModule = mAssociatedModules.TryGetItemConst(crc);

			return (nullptr != ppModule);
		}

		//-----------------------------------------------------------------------------
		void Phase::StartAsyncModules(Dia::Core::Containers::DynamicArrayC<Module*, 32>& modulesFlaggedToStartAsync)
		{
			// No modules with no dependancies this should assert and fail to start
			unsigned int numberOfModulesFlaggedToStart = 0;

			for (unsigned int i = 0; i < modulesFlaggedToStartAsync.Size(); i++)
			{
				if (modulesFlaggedToStartAsync.At(i) != nullptr)
				{
					numberOfModulesFlaggedToStart++;
				}
			}

			if (numberOfModulesFlaggedToStart != 0)
			{
				Dia::Core::TimeRelative expiryTime = Dia::Core::TimeRelative::CreateFromMinutes(2);

				Dia::Core::TimerSystem timer;

				timer.Start();

				bool haveStartedNewAsyncModule = false;
				while (!haveStartedNewAsyncModule)
				{
					if (timer.IsRunningFor() > expiryTime)
					{
						Dia::Core::Containers::String1024 moduleNames;

						moduleNames = "[";
						for (unsigned int i = 0; i < modulesFlaggedToStartAsync.Size(); i++)
						{
							if (modulesFlaggedToStartAsync.At(i) != nullptr)
							{
								Module* pModule = modulesFlaggedToStartAsync.At(i);

								moduleNames += pModule->GetUniqueId().AsChar();
								
								moduleNames += ", ";
							}
						}

						moduleNames += "]";

						DIA_ASSERT(0, "Phase %s has been running for over %d waiting on %s", GetUniqueId().AsChar(), moduleNames.AsCStr());
					}
					std::chrono::milliseconds dura(1);
					std::this_thread::sleep_for(dura);

					for (unsigned int i = 0; i < modulesFlaggedToStartAsync.Size(); i++)
					{
						if (modulesFlaggedToStartAsync.At(i) != nullptr)
						{
							Module* pModule = modulesFlaggedToStartAsync.At(i);

							if (pModule->HasStarted())
							{
								modulesFlaggedToStartAsync.At(i) = nullptr;
								
								haveStartedNewAsyncModule = true;
								break;
							}
						}
					}
				}
			}
		}
	}
}