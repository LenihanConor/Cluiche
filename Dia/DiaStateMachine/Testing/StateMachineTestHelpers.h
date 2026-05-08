#pragma once

#include "DiaStateMachine/IStateMachineInspectable.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include <gtest/gtest.h>

namespace Dia
{
	namespace StateMachine
	{
		namespace Testing
		{
			inline void AssertInState(const IStateMachineInspectable& machine,
				Dia::Core::StringCRC expectedStateId)
			{
				EXPECT_EQ(machine.GetCurrentStateId(), expectedStateId)
					<< "Expected state: " << expectedStateId.AsChar()
					<< " but was: " << machine.GetCurrentStateId().AsChar();
			}

			template<typename TMachine>
			void FireAndExpect(TMachine& machine, Dia::Core::StringCRC triggerId,
				Dia::Core::StringCRC expectedStateId)
			{
				bool result = machine.Fire(triggerId);
				EXPECT_TRUE(result)
					<< "Fire(" << triggerId.AsChar() << ") returned false";
				EXPECT_EQ(machine.GetCurrentStateId(), expectedStateId)
					<< "After Fire(" << triggerId.AsChar() << "): expected "
					<< expectedStateId.AsChar()
					<< " but was: " << machine.GetCurrentStateId().AsChar();
			}

			template<typename TMachine, unsigned int N>
			void AssertTransitionSequence(TMachine& machine,
				const Dia::Core::StringCRC (&triggers)[N],
				const Dia::Core::StringCRC (&expectedStates)[N])
			{
				for (unsigned int i = 0; i < N; ++i)
				{
					bool result = machine.Fire(triggers[i]);
					EXPECT_TRUE(result)
						<< "Step " << i << ": Fire(" << triggers[i].AsChar()
						<< ") returned false";
					EXPECT_EQ(machine.GetCurrentStateId(), expectedStates[i])
						<< "Step " << i << ": expected " << expectedStates[i].AsChar()
						<< " but was: " << machine.GetCurrentStateId().AsChar();
					if (machine.GetCurrentStateId() != expectedStates[i])
					{
						return;
					}
				}
			}
		}
	}
}
