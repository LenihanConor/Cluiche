////////////////////////////////////////////////////////////////////////////////
// Filename: CameraBehaviourRegistry.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Behaviour/CameraBehaviourRegistry.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Camera2D
	{
		////////////////////////////////////////////////////////////
		CameraBehaviourRegistry& CameraBehaviourRegistry::Get()
		{
			static CameraBehaviourRegistry sInstance;
			return sInstance;
		}

		////////////////////////////////////////////////////////////
		void CameraBehaviourRegistry::Register(Dia::Core::StringCRC typeId, BehaviourFactory factory)
		{
			DIA_ASSERT(!IsRegistered(typeId), "CameraBehaviourRegistry: type already registered");
			DIA_ASSERT(mCount < kMaxBehaviourTypes, "CameraBehaviourRegistry: capacity exceeded");
			mTypeIds[mCount]  = typeId;
			mFactories[mCount] = factory;
			++mCount;
		}

		////////////////////////////////////////////////////////////
		ICameraBehaviour* CameraBehaviourRegistry::Create(Dia::Core::StringCRC typeId, const void* config) const
		{
			for (unsigned int i = 0; i < mCount; ++i)
			{
				if (mTypeIds[i] == typeId)
					return mFactories[i](config);
			}
			return nullptr;
		}

		////////////////////////////////////////////////////////////
		bool CameraBehaviourRegistry::IsRegistered(Dia::Core::StringCRC typeId) const
		{
			for (unsigned int i = 0; i < mCount; ++i)
			{
				if (mTypeIds[i] == typeId)
					return true;
			}
			return false;
		}

	} // namespace Camera2D
} // namespace Dia
