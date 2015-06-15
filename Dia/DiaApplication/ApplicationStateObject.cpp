////////////////////////////////////////////////////////////////////////////////
// Filename: ProcessingModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplication/ApplicationStateObject.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Type/BasicTypeDefines.h>
#include <DiaCore/Core/Log.h>

namespace Dia
{
	namespace Application
	{
		////////////////////////////////////////////////////////////////////////////////
		// Class name: ProcessingModule
		////////////////////////////////////////////////////////////////////////////////
		
		//-----------------------------------------------------------------------------
		StateObject::StateObject(const Dia::Core::StringCRC& uniqueId)
			: mUniqueId (uniqueId)
			, mState(StateEnum::kConstructed)
		{}

		//-----------------------------------------------------------------------------
		void StateObject::BuildDependancies()
		{
			DIA_ASSERT(mState == StateEnum::kConstructed, "Starting %s but in wrong state: %s", mUniqueId.AsChar(), mState.AsString() );

			DoBuildDependancies();

			mState = StateEnum::kNotRunning;
		}

		//-----------------------------------------------------------------------------
		StateObject::OpertionResponse StateObject::Start()
		{
			Dia::Core::Log::OutputVaradicLine("Starting %s", GetUniqueId().AsChar());

			DIA_ASSERT(mState == StateEnum::kNotRunning, "Starting %s but in wrong state: %s", mUniqueId.AsChar(), mState.AsString() );

			mStartMutex.lock();
			OpertionResponse response = DoStart();

			switch (response)
			{
			case StateObject::OpertionResponse::kImmediate:
				mState = StateEnum::kRunning;
				break;
			case StateObject::OpertionResponse::kAsync:
				mState = StateEnum::kFlaggedToStart;
				break;
			default: DIA_ASSERT(0, "Cannot handle this response: %s", response.AsString());
				break;
			}
			mStartMutex.unlock();

			return response;
		}

		//-----------------------------------------------------------------------------
		//
		// This is to be called when an async start has finished
		void StateObject::NotifyReadyToStartAsync()
		{
			mStartMutex.lock();

			DIA_ASSERT(mState == StateEnum::kFlaggedToStart, "RespondToAsyncStart %s but in wrong state: %s", mUniqueId.AsChar(), mState.AsString());

			mState = StateEnum::kRunning;

			mStartMutex.unlock();
		}

		//-----------------------------------------------------------------------------
		void StateObject::Update()
		{
			DIA_ASSERT(mState == StateEnum::kRunning, "Updating %s but in wrong state: %s", mUniqueId.AsChar(), mState.AsString());

			DoUpdate();
		}

		//-----------------------------------------------------------------------------
		void StateObject::Stop()
		{
			Dia::Core::Log::OutputVaradicLine("Stoping %s", GetUniqueId().AsChar());

			DIA_ASSERT(mState == StateEnum::kRunning, "Stoping %s but in wrong state: %s", mUniqueId.AsChar(), mState.AsString());

			DoStop();
		}
	}
}