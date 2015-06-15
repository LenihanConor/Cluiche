#pragma once

namespace Dia
{
	namespace Maths
	{
		class Vector2D;
		class Vector3D;
		class Vector4D;

		namespace VectorUtils
		{
			
			//---------------------------------------------------------------------------------------------------------------------------------
			// Standard Vector Conversions
			//---------------------------------------------------------------------------------------------------------------------------------
			
			Vector2D&		ToVector2DFromVector3D			(Vector2D& lhs, const Vector3D& rhs);
			Vector2D&		ToVector2DFromVector4D			(Vector2D& lhs, const Vector4D& rhs);
		
			Vector3D&		ToVector3DFromVector2D			(Vector3D& lhs, const Vector2D& rhs);
			Vector3D&		ToVector3DFromVector4D			(Vector3D& lhs, const Vector4D& rhs);

			Vector4D&		ToVector4DPointFromVector2D		(Vector4D& lhs, const Vector2D& rhs);
			Vector4D&		ToVector4DVectorFromVector2D	(Vector4D& lhs, const Vector2D& rhs);
			Vector4D&		ToVector4DPointFromVector3D		(Vector4D& lhs, const Vector3D& rhs);
			Vector4D&		ToVector4DVectorFromVector3D	(Vector4D& lhs, const Vector3D& rhs);

			//---------------------------------------------------------------------------------------------------------------------------------
			// Vector2D Conversions
			//---------------------------------------------------------------------------------------------------------------------------------
			
			Vector2D&		Vector2DFromVector3DXY			(Vector2D& lhs, const Vector3D& rhs);
			Vector2D&		Vector2DFromVector3DXZ			(Vector2D& lhs, const Vector3D& rhs);
			Vector2D&		Vector2DFromVector3DZY			(Vector2D& lhs, const Vector3D& rhs);
			
			Vector2D&		Vector2DFromVector4DXY			(Vector2D& lhs, const Vector4D& rhs);
			Vector2D&		Vector2DFromVector4DXZ			(Vector2D& lhs, const Vector4D& rhs);
			Vector2D&		Vector2DFromVector4DZY			(Vector2D& lhs, const Vector4D& rhs);
			
			Vector3D&		Vector3DXYFromVector2D			(Vector3D& lhs, const Vector2D& rhs);
			Vector3D&		Vector3DXZFromVector2D			(Vector3D& lhs, const Vector2D& rhs);
			Vector3D&		Vector3DYZFromVector2D			(Vector3D& lhs, const Vector2D& rhs);
			
			Vector4D&		Vector4DXYFromVector2D			(Vector4D& lhs, const Vector2D& rhs);
			Vector4D&		Vector4DXZFromVector2D			(Vector4D& lhs, const Vector2D& rhs);
			Vector4D&		Vector4DYZFromVector2D			(Vector4D& lhs, const Vector2D& rhs);
			
			//---------------------------------------------------------------------------------------------------------------------------------
			// Vector Swizzeling
			//---------------------------------------------------------------------------------------------------------------------------------
			
			Vector2D&		Vector2DSwizzleYX				(Vector2D& YX, const Vector2D& XY);
	
			Vector3D&		Vector3DSwizzleXZY				(Vector3D& XZY, const Vector3D& XYZ);
			Vector3D&		Vector3DSwizzleYXZ				(Vector3D& YXZ, const Vector3D& XYZ);
			Vector3D&		Vector3DSwizzleYZX				(Vector3D& YZX, const Vector3D& XYZ);
			Vector3D&		Vector3DSwizzleZXY				(Vector3D& ZXY, const Vector3D& XYZ);
			Vector3D&		Vector3DSwizzleZYX				(Vector3D& ZYX, const Vector3D& XYZ);

			Vector4D&		Vector4DSwizzleXZY				(Vector4D& XZY, const Vector4D& XYZ);
			Vector4D&		Vector4DSwizzleYXZ				(Vector4D& YXZ, const Vector4D& XYZ);
			Vector4D&		Vector4DSwizzleYZX				(Vector4D& YZX, const Vector4D& XYZ);
			Vector4D&		Vector4DSwizzleZXY				(Vector4D& ZXY, const Vector4D& XYZ);
			Vector4D&		Vector4DSwizzleZYX				(Vector4D& ZYX, const Vector4D& XYZ);
		};
	}
}
