#include "Vector/VectorUtils.h"

#include "Vector/Vector2D.h"
#include "Vector/Vector3D.h"
#include "Vector/Vector4D.h"

namespace Dia
{
	namespace Maths
	{
		namespace VectorUtils
		{
			
			//---------------------------------------------------------------------------------------------------------------------------------
			// Standard Vector Conversions
			//---------------------------------------------------------------------------------------------------------------------------------
			
			//---------------------------------------------------------------------------------------------------------------------------------
			Vector2D& ToVector2DFromVector3D (Vector2D& lhs, const Vector3D& rhs)
			{
				return Vector2DFromVector3DXZ(lhs, rhs);
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector2D& ToVector2DFromVector4D (Vector2D& lhs, const Vector4D& rhs)
			{
				return Vector2DFromVector4DXZ(lhs, rhs);
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& ToVector3DFromVector2D (Vector3D& lhs, const Vector2D& rhs)
			{
				return Vector3DXZFromVector2D(lhs, rhs);
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& ToVector3DFromVector4D (Vector3D& lhs, const Vector4D& rhs)
			{
				lhs.Set(rhs.x, rhs.y, rhs.z);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& ToVector4DPointFromVector2D (Vector4D& lhs, const Vector2D& rhs)
			{
				Vector4DXZFromVector2D(lhs, rhs).W(1.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& ToVector4DVectorFromVector2D (Vector4D& lhs, const Vector2D& rhs)
			{
				Vector4DXZFromVector2D(lhs, rhs).W(0.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& ToVector4DPointFromVector3D (Vector4D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.x, rhs.y, rhs.z, 1.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& ToVector4DVectorFromVector3D (Vector4D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.x, rhs.y, rhs.z, 0.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			// Vector2D Conversions
			//---------------------------------------------------------------------------------------------------------------------------------
			
			//---------------------------------------------------------------------------------------------------------------------------------
			Vector2D& Vector2DFromVector3DXY (Vector2D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.x, rhs.y);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector2D& Vector2DFromVector3DXZ (Vector2D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.x, rhs.z);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector2D& Vector2DFromVector3DZY (Vector2D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.z, rhs.y);
				return lhs;
			}
			
			//---------------------------------------------------------------------------------------------------------------------------------
			Vector2D& Vector2DFromVector4DXY (Vector2D& lhs, const Vector4D& rhs)		
			{
				lhs.Set(rhs.x, rhs.y);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector2D& Vector2DFromVector4DXZ (Vector2D& lhs, const Vector4D& rhs)
			{
				lhs.Set(rhs.x, rhs.z);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector2D& Vector2DFromVector4DZY (Vector2D& lhs, const Vector4D& rhs)
			{
				lhs.Set(rhs.z, rhs.y);
				return lhs;
			}
			
			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& Vector3DXYFromVector2D (Vector3D& lhs, const Vector2D& rhs)
			{
				lhs.Set(rhs.x, rhs.y, 0.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& Vector3DXZFromVector2D (Vector3D& lhs, const Vector2D& rhs)
			{
				lhs.Set(rhs.x, 0.0f, rhs.y);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& Vector3DYZFromVector2D (Vector3D& lhs, const Vector2D& rhs)
			{
				lhs.Set(0.0f, rhs.x, rhs.y);
				return lhs;
			}
			
			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& Vector4DXYFromVector2D (Vector4D& lhs, const Vector2D& rhs)
			{
				lhs.Set(rhs.x, rhs.y, 0.0f, 0.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& Vector4DXZFromVector2D (Vector4D& lhs, const Vector2D& rhs)
			{
				lhs.Set(rhs.x, 0.0f, rhs.y, 0.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& Vector4DYZFromVector2D (Vector4D& lhs, const Vector2D& rhs)
			{
				lhs.Set(0.0f, rhs.x, rhs.y, 0.0f);
				return lhs;
			}
			
			//---------------------------------------------------------------------------------------------------------------------------------
			// Vector Swizzeling
			//---------------------------------------------------------------------------------------------------------------------------------
			
			//---------------------------------------------------------------------------------------------------------------------------------
			Vector2D& Vector2DSwizzleYX(Vector2D& lhs, const Vector2D& rhs)
			{
				lhs.Set(rhs.y, rhs.y);
				return lhs;
			}
	
			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& Vector3DSwizzleXZY(Vector3D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.x, rhs.z, rhs.y);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& Vector3DSwizzleYXZ(Vector3D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.y, rhs.x, rhs.z);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& Vector3DSwizzleYZX(Vector3D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.y, rhs.z, rhs.x);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& Vector3DSwizzleZXY(Vector3D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.z, rhs.x, rhs.y);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector3D& Vector3DSwizzleZYX(Vector3D& lhs, const Vector3D& rhs)
			{
				lhs.Set(rhs.z, rhs.y, rhs.x);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& Vector4DSwizzleXZY(Vector4D& lhs, const Vector4D& rhs)
			{
				lhs.Set(rhs.x, rhs.z, rhs.y, 0.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& Vector4DSwizzleYXZ(Vector4D& lhs, const Vector4D& rhs)
			{
				lhs.Set(rhs.y, rhs.x, rhs.z, 0.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& Vector4DSwizzleYZX(Vector4D& lhs, const Vector4D& rhs)
			{
				lhs.Set(rhs.y, rhs.z, rhs.x, 0.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& Vector4DSwizzleZXY(Vector4D& lhs, const Vector4D& rhs)
			{
				lhs.Set(rhs.z, rhs.x, rhs.y, 0.0f);
				return lhs;
			}

			//---------------------------------------------------------------------------------------------------------------------------------
			Vector4D& Vector4DSwizzleZYX(Vector4D& lhs, const Vector4D& rhs)
			{
				lhs.Set(rhs.z, rhs.y, rhs.x, 0.0f);
				return lhs;
			}
		};
	}
}
