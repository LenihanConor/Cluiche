////////////////////////////////////////////////////////////////////////////////
// Filename: BoundsClamp3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera3D/Behaviours/BoundsClamp3D.h"
#include "DiaCamera3D/Camera3D.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Camera3D
	{
		////////////////////////////////////////////////////////////
		BoundsClamp3D::BoundsClamp3D(const Dia::Geometry3D::AABB& bounds)
			: mBounds(bounds)
		{
		}

		////////////////////////////////////////////////////////////
		Dia::Core::StringCRC BoundsClamp3D::GetTypeId() const
		{
			return Dia::Core::StringCRC(kTypeIdStr);
		}

		////////////////////////////////////////////////////////////
		void BoundsClamp3D::Update(Camera3D& camera, float /*dt*/)
		{
			const Dia::Maths::Vector3D& mn = mBounds.GetMin();
			const Dia::Maths::Vector3D& mx = mBounds.GetMax();

			if ((mx.x - mn.x) <= 0.0f || (mx.y - mn.y) <= 0.0f || (mx.z - mn.z) <= 0.0f)
				return;

			Dia::Maths::Vector3D pos = camera.position;
			pos.x = pos.x < mn.x ? mn.x : (pos.x > mx.x ? mx.x : pos.x);
			pos.y = pos.y < mn.y ? mn.y : (pos.y > mx.y ? mx.y : pos.y);
			pos.z = pos.z < mn.z ? mn.z : (pos.z > mx.z ? mx.z : pos.z);
			camera.position = pos;
		}

	} // namespace Camera3D
} // namespace Dia
