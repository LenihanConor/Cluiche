#include "DiaGeometry2D/Transform/Transform.h"

#include "DiaMaths/Matrix/Matrix33.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		// Matrix generation
		//-----------------------------------------------------------------------------

		Dia::Maths::Matrix33 Transform::GetLocalMatrix() const
		{
			return Dia::Maths::Matrix33::FromTRS(mLocalPosition, mLocalRotation, mLocalScale);
		}

		Dia::Maths::Matrix33 Transform::GetWorldMatrix() const
		{
			Dia::Maths::Vector2D worldPos   = GetWorldPosition();
			Dia::Maths::Angle worldRot      = GetWorldRotation();
			Dia::Maths::Vector2D worldScale = GetWorldScale();

			return Dia::Maths::Matrix33::FromTRS(worldPos, worldRot, worldScale);
		}
	}
}
