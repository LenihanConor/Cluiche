////////////////////////////////////////////////////////////////////////////////
// Filename: Follow3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera3D/Behaviours/Follow3D.h"
#include "DiaCamera3D/Registry/CameraBehaviourRegistry3D.h"
#include "DiaCamera3D/Camera3D.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Camera3D
	{
		static bool sRegistered = [] {
			CameraBehaviourRegistry3D::Get().Register(
				Dia::Core::StringCRC(Follow3D::kTypeIdStr),
				[](const void*) -> ICameraBehaviour3D* { return new Follow3D(); });
			return true;
		}();

		////////////////////////////////////////////////////////////
		Follow3D::Follow3D(Dia::Maths::Vector3D offset)
			: mOffset(offset)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC Follow3D::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void Follow3D::Update(Camera3D& camera, float /*dt*/)
		{
			if (!mHasTarget)
				return;

			camera.position = Dia::Maths::Vector3D(
				mTarget.x + mOffset.x,
				mTarget.y + mOffset.y,
				mTarget.z + mOffset.z);
		}

	} // namespace Camera3D
} // namespace Dia
