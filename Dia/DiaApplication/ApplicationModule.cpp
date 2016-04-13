////////////////////////////////////////////////////////////////////////////////
// Filename: ProcessingModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplication/ApplicationModule.h"

#include "DiaApplication/ApplicationPhase.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Type/BasicTypeDefines.h>

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Dia
{
	namespace Application
	{
		////////////////////////////////////////////////////////////////////////////////
		// Class name: Module
		////////////////////////////////////////////////////////////////////////////////
		
		//-----------------------------------------------------------------------------
		Module::Module(ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& uniqueId, RunningEnum runningMode, unsigned int initialDependencyMapSize)
			: StateObject(uniqueId)
			, mAssociatedProcessingUnit(associatedProcessingUnit)
			, mRunningMode(runningMode)
			, mDependencies(initialDependencyMapSize, initialDependencyMapSize * 2)
		{
			mAssociatedProcessingUnit->AddModule(this);
		}

		//-----------------------------------------------------------------------------
		unsigned int Module::GetNumberOfDependancies()const
		{
			return mDependencies.Size();
		}

		//-----------------------------------------------------------------------------
		Module* Module::GetDependencyFromIndex(unsigned int index)
		{
			return mDependencies.GetItemByIndex(index);
		}

		//-----------------------------------------------------------------------------
		const Module* Module::GetDependencyFromIndex(unsigned int index)const
		{
			return mDependencies.GetItemByIndexConst(index);
		}

		//-----------------------------------------------------------------------------
		Module* Module::GetDependency(const Dia::Core::StringCRC& uniqueId)
		{
			return mDependencies.GetItem(uniqueId);
		}

		//-----------------------------------------------------------------------------
		const Module* Module::GetDependency(const Dia::Core::StringCRC& uniqueId)const
		{
			return mDependencies.GetItemConst(uniqueId);
		}

		//-----------------------------------------------------------------------------
		void Module::AddDependancy(Module* dependancy)
		{
			mDependencies.Add(dependancy->GetUniqueId(), dependancy);
		}

		//-----------------------------------------------------------------------------
		//
		// This gets called when a module stays on active over a pahse Transtion
		void Module::RetainThroughTransition(const Phase* startPhase, const Phase* endPhase)
		{
			DoRetainThroughTransition(startPhase, endPhase);
		}

		//-----------------------------------------------------------------------------
		//
		// Iterates all dependencies and tests that they have started 
		//
		bool Module::HasAllDependanciesStarted()const
		{
			const unsigned int numberOfModules = mDependencies.Size();
			
			if (numberOfModules > 0)
			{
				unsigned int numberOfStartedModules = numberOfModules;

				for(unsigned int i = 0; i < numberOfModules; i++)
				{
					Module* pModule = mDependencies.GetItemByIndexConst(i);
					if (pModule->HasStarted())
					{
						numberOfStartedModules--;
					}
				}

				// If we have not gotten all dependencies started then we cant start
				if (numberOfStartedModules != 0)
				{
					return false;
				}
			}

			return true;
		}

		//-----------------------------------------------------------------------------
		bool Module::RequiresUpdating()const 
		{ 
			return (mRunningMode == RunningEnum::kUpdate);
		}

		//-----------------------------------------------------------------------------
		ProcessingUnit* Module::GetAssociatedProcessingUnit()
		{
			DIA_ASSERT(mAssociatedProcessingUnit, "mAssociatedProcessingUnit is NULL");
			return mAssociatedProcessingUnit;
		}

		//-----------------------------------------------------------------------------
		const ProcessingUnit* Module::GetAssociatedProcessingUnit()const
		{
			DIA_ASSERT(mAssociatedProcessingUnit, "mAssociatedProcessingUnit is NULL");
			return mAssociatedProcessingUnit;
		}
	}
}