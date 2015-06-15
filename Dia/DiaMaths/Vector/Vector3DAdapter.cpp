#include "Vector/Vector3DAdapter.h"

#include "Vector/Vector2D.h"
#include "Vector/Vector4D.h"
#include "Vector/VectorUtils.h"

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		Vector3DAdapter::Vector3DAdapter( const Vector2D& vec )
		{
			VectorUtils::ToVector3DFromVector2D(mVector, vec);
		}

		//---------------------------------------------------------------------------------------------------------------------------------
		Vector3DAdapter::Vector3DAdapter( const Vector3D& vec )
		{
			mVector = vec;
		}

		//---------------------------------------------------------------------------------------------------------------------------------
		Vector3DAdapter::Vector3DAdapter( const Vector4D& vec )
		{
			VectorUtils::ToVector3DFromVector4D(mVector, vec);
		}

		//---------------------------------------------------------------------------------------------------------------------------------
		const Vector3D& Vector3DAdapter::Data() const
		{
			return mVector;
		}
	}
}