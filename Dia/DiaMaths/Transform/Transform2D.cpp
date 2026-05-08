#include "DiaMaths/Transform/Transform2D.h"
#include "DiaMaths/Matrix/Matrix33.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Matrix generation
		//-----------------------------------------------------------------------------

		Matrix33 Transform2D::GetLocalMatrix() const
		{
			return Matrix33::FromTRS(mLocalPosition, mLocalRotation, mLocalScale);
		}

		Matrix33 Transform2D::GetWorldMatrix() const
		{
			Vector2D worldPos = GetWorldPosition();
			Angle worldRot = GetWorldRotation();
			Vector2D worldScale = GetWorldScale();

			return Matrix33::FromTRS(worldPos, worldRot, worldScale);
		}
	}
}
