////////////////////////////////////////////////////////////////////////////////
// Filename: CameraBehaviourRegistry3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera3D/Registry/CameraBehaviourRegistry3D.h"

#include "DiaCamera3D/Behaviours/Follow3D.h"
#include "DiaCamera3D/Behaviours/SmoothDamp3D.h"
#include "DiaCamera3D/Behaviours/BoundsClamp3D.h"
#include "DiaCamera3D/Behaviours/ScreenShake3D.h"
#include "DiaCamera3D/Behaviours/Orbit.h"
#include "DiaCamera3D/Behaviours/Flythrough.h"
#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Camera3D
	{
		////////////////////////////////////////////////////////////
		CameraBehaviourRegistry3D& CameraBehaviourRegistry3D::Get()
		{
			static CameraBehaviourRegistry3D sInstance;
			static bool sBuiltInsRegistered = false;
			if (!sBuiltInsRegistered)
			{
				sBuiltInsRegistered = true;
				sInstance.Register(Dia::Core::StringCRC(Follow3D::kTypeIdStr),
					[](const void*) -> ICameraBehaviour3D* { return new Follow3D(); });
				sInstance.Register(Dia::Core::StringCRC(SmoothDamp3D::kTypeIdStr),
					[](const void*) -> ICameraBehaviour3D* { return new SmoothDamp3D(); });
				sInstance.Register(Dia::Core::StringCRC(BoundsClamp3D::kTypeIdStr),
					[](const void*) -> ICameraBehaviour3D* { return new BoundsClamp3D(); });
				sInstance.Register(Dia::Core::StringCRC(ScreenShake3D::kTypeIdStr),
					[](const void*) -> ICameraBehaviour3D* { return new ScreenShake3D(); });
				sInstance.Register(Dia::Core::StringCRC(Orbit::kTypeIdStr),
					[](const void*) -> ICameraBehaviour3D* { return new Orbit(); });
				sInstance.Register(Dia::Core::StringCRC(Flythrough::kTypeIdStr),
					[](const void*) -> ICameraBehaviour3D* { return new Flythrough(); });
			}
			return sInstance;
		}

		////////////////////////////////////////////////////////////
		void CameraBehaviourRegistry3D::Register(Dia::Core::StringCRC typeId, FactoryFn factory)
		{
			DIA_ASSERT(!IsRegistered(typeId), "CameraBehaviourRegistry3D: type already registered");
			DIA_ASSERT(mCount < kMaxBehaviourTypes, "CameraBehaviourRegistry3D: capacity exceeded");
			mEntries[mCount].typeId  = typeId;
			mEntries[mCount].factory = factory;
			++mCount;
		}

		////////////////////////////////////////////////////////////
		ICameraBehaviour3D* CameraBehaviourRegistry3D::Create(Dia::Core::StringCRC typeId, const void* config) const
		{
			for (unsigned int i = 0; i < mCount; ++i)
			{
				if (mEntries[i].typeId == typeId)
					return mEntries[i].factory(config);
			}
			return nullptr;
		}

		////////////////////////////////////////////////////////////
		bool CameraBehaviourRegistry3D::IsRegistered(Dia::Core::StringCRC typeId) const
		{
			for (unsigned int i = 0; i < mCount; ++i)
			{
				if (mEntries[i].typeId == typeId)
					return true;
			}
			return false;
		}

	} // namespace Camera3D
} // namespace Dia
