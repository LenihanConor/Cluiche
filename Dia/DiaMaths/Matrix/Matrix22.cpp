
#include "Matrix/Matrix22.h"

#include "Vector/Vector2D.h"
#include "DiaMaths/Core/Angle.h"
#include "DiaMaths/Core/Trigonometry.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"

namespace Dia
{
	namespace Maths
	{
		
 		DIA_TYPE_DEFINITION( Matrix22 )
			DIA_TYPE_ADD_VARIABLE( "e00", mElement[0] )
  			DIA_TYPE_ADD_VARIABLE( "e01", mElement[1] )
 			DIA_TYPE_ADD_VARIABLE( "e10", mElement[2] )
  			DIA_TYPE_ADD_VARIABLE( "e11", mElement[2] )
 		DIA_TYPE_DEFINITION_END()

		const Matrix22 Matrix22::Zero(0.0f, 0.0f, 0.0f, 0.0f);
		const Matrix22 Matrix22::Identity(1.0f, 0.0f, 0.0f, 1.0f);
		
		const Matrix22 Matrix22::XAxisReflection(1.0f, 0.0f, 0.0f, -1.0f);
		const Matrix22 Matrix22::YAxisReflection(-1.0f, 0.0f, 0.0f, 1.0f);
		
		const Matrix22 Matrix22::XAxisProjection(1.0f, 0.0f, 0.0f, 0.0f);
		const Matrix22 Matrix22::YAxisProjection(0.0f, 0.0f, 0.0f, 1.0f);

		//-----------------------------------------------------------------------------
		Matrix22::Matrix22() 
		{
			*this = Identity;
		}

		//-----------------------------------------------------------------------------
		Matrix22::Matrix22(float _e00, float _e01, float _e10, float _e11)
		{
			mElement[0] = _e00;
			mElement[1] = _e01;
			mElement[2] = _e10;
			mElement[3] = _e11;
		}

		//-----------------------------------------------------------------------------
		Matrix22::Matrix22(const Vector2D& xAxis, const Vector2D& yAxis)
		{
			mElement[0] = xAxis.x;
			mElement[1] = xAxis.y;
			mElement[2] = yAxis.x;
			mElement[3] = yAxis.y;
		}

		//-----------------------------------------------------------------------------
		Matrix22::Matrix22(const Matrix22& matrix)
		{
			*this = matrix;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::FromAngleClockwise( const Angle& angle )
		{
			float cosAngle = Dia::Maths::Cos(angle);
			float sinAngle = Dia::Maths::Sin(angle);
			
			Vector2D xAxis(cosAngle, sinAngle);
			Vector2D yAxis(-sinAngle, cosAngle);
			
			xAxis.Normalize();
			yAxis.Normalize();

			Matrix22 m(	xAxis, yAxis );
			
			DIA_ASSERT(m.IsAxisNormal(), "Axis are not normal");

			return m;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::FromAngleCounterClockwise( const Angle& angle )
		{
			float cosAngle = Dia::Maths::Cos(angle);
			float sinAngle = Dia::Maths::Sin(angle);

			Vector2D xAxis(cosAngle, 
				-sinAngle);
			Vector2D yAxis(sinAngle, 
				cosAngle);

			xAxis.Normalize();
			yAxis.Normalize();

			Matrix22 m(	xAxis, yAxis );

			DIA_ASSERT(m.IsAxisNormal(), "Axis are not normal");

			return m;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::FromScale( const float scale )
		{
			Matrix22 m(	scale, 
				0.0f, 
				0.0f, 
				scale);

			return m;
		}
		
		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::FromShearX( const float shear )
		{
			Matrix22 m(	1.0f, 
				shear, 
				0.0f, 
				1.0f);

			return m;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::FromShearY( const float shear )
		{
			Matrix22 m(	1.0f,  
				0.0f, 
				shear,
				1.0f);

			return m;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::FromReflectionAxis( const Vector2D& axis )
		{
			float divisable = (1.0f / ((axis.x * axis.x) + (axis.y * axis.y)));
			
			Matrix22 m(	((axis.x * axis.x) - (axis.y * axis.y)),  
						2.0f * axis.x * axis.y, 
						2.0f * axis.x * axis.y,
						((axis.y * axis.y) - (axis.x * axis.x)));
			
			return (m * divisable);
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::FromProjectionAxis( const Vector2D& axis )
		{
			float divisable = (1.0f / ((axis.x * axis.x) + (axis.y * axis.y)));

			Matrix22 m(	(axis.x * axis.x),  
				axis.x * axis.y, 
				axis.x * axis.y,
				(axis.y * axis.y));

			return (m * divisable);
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::operator = (const Matrix22& rhs)
		{
			for (unsigned int i = 0; i < kNumElements; i++)
			{
				mElement[i] = rhs.mElement[i];
			}

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::operator += (const Matrix22& rhs)
		{
			mElement[0] = (mElement[0] + rhs.mElement[0]);
			mElement[1] = (mElement[1] + rhs.mElement[1]);
			mElement[2] = (mElement[2] + rhs.mElement[2]);
			mElement[3] = (mElement[3] + rhs.mElement[3]);

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::operator -=	(const Matrix22& rhs)
		{
			mElement[0] = (mElement[0] - rhs.mElement[0]);
			mElement[1] = (mElement[1] - rhs.mElement[1]);
			mElement[2] = (mElement[2] - rhs.mElement[2]);
			mElement[3] = (mElement[3] - rhs.mElement[3]);

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::operator *=	(const Matrix22& rhs)
		{
			float e00 = (mElement[0] * rhs.mElement[0]) + (mElement[1] * rhs.mElement[2]);
			float e01 = (mElement[0] * rhs.mElement[1]) + (mElement[1] * rhs.mElement[3]);
			float e10 = (mElement[2] * rhs.mElement[0]) + (mElement[3] * rhs.mElement[2]);
			float e11 = (mElement[2] * rhs.mElement[1]) + (mElement[3] * rhs.mElement[3]);
				
			mElement[0] = e00;
			mElement[1] = e01;
			mElement[2] = e10;
			mElement[3] = e11;

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::operator *=	(float scalar)
		{
			mElement[0] = scalar * mElement[0];
			mElement[1] = scalar * mElement[1];
			mElement[2] = scalar * mElement[2];
			mElement[3] = scalar * mElement[3];

			return *this;
		}
		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::operator /=	(float scalar)
		{
			DIA_ASSERT(scalar != 0.0f, "Can not divide by Zero");

			mElement[0] = mElement[0] / scalar;
			mElement[1] = mElement[1] / scalar;
			mElement[2] = mElement[2] / scalar;
			mElement[3] = mElement[3] / scalar;

			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::operator -() const
		{
			Matrix22 m(	mElement[0] * -1.0f,
						mElement[1] * -1.0f,
						mElement[2] * -1.0f,
						mElement[3] * -1.0f );
			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::operator + ( const Matrix22& rhs) const
		{
			Matrix22 m(	(mElement[0] + rhs.mElement[0]),
						(mElement[1] + rhs.mElement[1]),
						(mElement[2] + rhs.mElement[2]),
						(mElement[3] + rhs.mElement[3]));
			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::operator - ( const Matrix22& rhs) const
		{
			Matrix22 m(	(mElement[0] - rhs.mElement[0]),
						(mElement[1] - rhs.mElement[1]),
						(mElement[2] - rhs.mElement[2]),
						(mElement[3] - rhs.mElement[3]));
			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::operator * ( const Matrix22& rhs) const
		{
			Matrix22 m(	(mElement[0] * rhs.mElement[0]) + (mElement[1] * rhs.mElement[2]),
						(mElement[0] * rhs.mElement[1]) + (mElement[1] * rhs.mElement[3]),
						(mElement[2] * rhs.mElement[0]) + (mElement[3] * rhs.mElement[2]),
						(mElement[2] * rhs.mElement[1]) + (mElement[3] * rhs.mElement[3]));
			return m;		
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::operator * ( const float scalar ) const
		{
			Matrix22 m(	scalar * mElement[0],
						scalar * mElement[1],
						scalar * mElement[2],
						scalar * mElement[3]);
			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::operator / ( const float scalar) const
		{
			DIA_ASSERT(scalar != 0.0f, "Can not divide by Zero");

			Matrix22 m(	mElement[0] / scalar,
						mElement[1] / scalar,
						mElement[2] / scalar,
						mElement[3] / scalar);
			return m;
		}

		//-----------------------------------------------------------------------------
		float& Matrix22::operator [] (int index)
		{
			DIA_ASSERT(index >= 0 && index < kNumElements, "Oubound the array");

			return mElement[index];
		}

		//-----------------------------------------------------------------------------
		float Matrix22::operator []	(int index) const
		{
			DIA_ASSERT(index >= 0 && index < kNumElements, "Oubound the array");

			return mElement[index];
		}

		//-----------------------------------------------------------------------------
		bool Matrix22::operator == (const Matrix22& rhs)const
		{
			bool isEqual = true;
			for (unsigned int i = 0; i < kNumElements; i++)
			{
				isEqual = isEqual && (Dia::Maths::Float::FEqual(mElement[i], rhs.mElement[i]));
			}
			return isEqual;
		}

		//-----------------------------------------------------------------------------
		bool Matrix22::operator != (const Matrix22& m)const
		{
			return !(*this == m);
		}
			
		//-----------------------------------------------------------------------------
		Vector2D Matrix22::operator * ( const Vector2D& rhs) const
		{
			DIA_ASSERT(rhs != Vector2D::Zero(), "Cant rotate a zero vector");

			Vector2D newVector;

			newVector.x = ((mElement[0] * rhs.x) + (mElement[2] * rhs.y));
			newVector.y = ((mElement[1] * rhs.x) + (mElement[3] * rhs.y));

			return newVector;
		}

		//-----------------------------------------------------------------------------
		float Matrix22::Element ( int index ) const 
		{
			DIA_ASSERT(index >= 0 && index < kNumElements, "Oubound the array");

			return mElement[index];
		}

		//-----------------------------------------------------------------------------
		float Matrix22::Element ( int row, int coloumn ) const 
		{
			DIA_ASSERT(row >= 0 && row < kNumRows, "Oubound the array");
			DIA_ASSERT(coloumn >= 0 && coloumn < kNumColoumns, "Oubound the array");

			return mElement[LinearIndex(row, coloumn)];
		}

		//-----------------------------------------------------------------------------
		void Matrix22::SetElement ( int index, float value )
		{
			DIA_ASSERT(index >= 0 && index < kNumElements, "Oubound the array");

			mElement[index] = value;
		}

		//-----------------------------------------------------------------------------
		void Matrix22::SetElement ( int row, int coloumn, float value )
		{
			DIA_ASSERT(row >= 0 && row < kNumRows, "Oubound the array");
			DIA_ASSERT(coloumn >= 0 && coloumn < kNumColoumns, "Oubound the array");
			
			mElement[LinearIndex(row, coloumn)] = value;
		}

		//-----------------------------------------------------------------------------
		void Matrix22::XAxis (Vector2D& xAxis)const
		{
			xAxis.x = mElement[0];
			xAxis.y = mElement[1];
		}
		
		//-----------------------------------------------------------------------------
		void Matrix22::YAxis (Vector2D& yAxis)const
		{
			yAxis.x = mElement[2];
			yAxis.y = mElement[3];
		}

		//-----------------------------------------------------------------------------
		void Matrix22::Set ( float _e00, float _e01, float _e10, float _e11 )
		{
			mElement[0] = _e00;
			mElement[1] = _e01;
			mElement[2] = _e10;
			mElement[3] = _e11;
		}
		
		//-----------------------------------------------------------------------------
		void Matrix22::Set ( const Vector2D& xAxis, const Vector2D& yAxis )
		{
			mElement[0] = xAxis.x;
			mElement[1] = xAxis.y;
			mElement[2] = yAxis.x;
			mElement[3] = yAxis.y;
		}
		
		//-----------------------------------------------------------------------------
		void Matrix22::Set ( const Matrix22& matrix )
		{
			*this = matrix;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::Clear ()
		{
			mElement[0] = 0.0f; 
			mElement[1] = 0.0f;
			mElement[2] = 0.0f; 
			mElement[3] = 0.0f;

			return *this;
		}
		
		//-----------------------------------------------------------------------------
		bool Matrix22::IsValid () const
		{
			bool isValid = true;
			for (unsigned int i = 0; i < kNumElements; i++)
			{
				isValid = isValid && Maths::Float::FIsValid(mElement[i]);
			}
			return isValid;
		}
		
		//-----------------------------------------------------------------------------
		bool Matrix22::IsSymmetric()const 
		{
			return (*this == this->AsTranspose());
		}

		//-----------------------------------------------------------------------------
		bool Matrix22::IsSkewSymmetric()const
		{
			return (AsTranspose() == AsNegative());
		}
		
		//-----------------------------------------------------------------------------
		bool Matrix22::IsIdentity()const
		{
			return (*this == Identity);
		}

		//-----------------------------------------------------------------------------
		bool Matrix22::IsDiagonalMatrix()const
		{
			return (Dia::Maths::Float::FEqual(mElement[1], 0.0f) && Dia::Maths::Float::FEqual(mElement[2], 0.0f));
		}

		//-----------------------------------------------------------------------------
		bool Matrix22::IsOrthogonal()const
		{
			float det = Determinant();

			if (Dia::Maths::Float::FEqual(det, 0.0f))
			{
				return false;
			}
			
			Matrix22 a = AsTranspose();
			
			Matrix22 b(	mElement[3] / det, 
						-mElement[1] / det, 
						-mElement[2] / det, 
						mElement[0] / det);

			return (a == b);
		}
	
		//-----------------------------------------------------------------------------
		bool Matrix22::IsScaled() const
		{
			float xScale, yScale = 0.0f;
			GetScale(xScale, yScale);

			return (!Dia::Maths::Float::FEqual(xScale, 1.0f) || !Dia::Maths::Float::FEqual(yScale, 1.0f));
		}

		//-----------------------------------------------------------------------------
		bool Matrix22::IsUniformScale()const
		{
			float xScale, yScale = 0.0f;
			GetScale(xScale, yScale);

			return (!Dia::Maths::Float::FEqual(xScale, 1.0f) && 
					!Dia::Maths::Float::FEqual(yScale, 1.0f) &&
					Dia::Maths::Float::FEqual(xScale, yScale));
		}

		//-----------------------------------------------------------------------------
		bool Matrix22::IsLeftHanded () const
		{
			float det = Determinant();

			return (det > 0);
		}

		//-----------------------------------------------------------------------------
		bool Matrix22::IsRightHanded () const
		{
			float det = Determinant();

			return (det < 0);
		}

		//-----------------------------------------------------------------------------
		bool Matrix22::IsAxisNormal () const
		{
			Vector2D xAxis, yAxis;

			XAxis(xAxis);
			YAxis(yAxis);

			return (xAxis.IsNormal() && yAxis.IsNormal());
		}
		
		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::LeftHanded()
		{
			DIA_ASSERT(IsOrthogonal(), "Must be orthogonal");
			DIA_ASSERT(IsAxisNormal(), "Axis are not normal");
			
			if (IsLeftHanded())
			{
				return *this;
			}

			Vector2D xAxis, yAxis;

			XAxis(xAxis);
			YAxis(yAxis);

			this->Set(yAxis, xAxis);

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::AsLeftHanded() const
		{
			DIA_ASSERT(IsOrthogonal(), "Must be orthogonal");
			DIA_ASSERT(IsAxisNormal(), "Axis are not normal");

			if (IsLeftHanded())
			{
				return Matrix22(*this);
			}

			Vector2D xAxis, yAxis;

			XAxis(xAxis);
			YAxis(yAxis);

			Matrix22 m(yAxis, xAxis);
	
			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::RightHanded()
		{
			DIA_ASSERT(IsOrthogonal(), "Must be orthogonal");
			DIA_ASSERT(IsAxisNormal(), "Axis are not normal");

			if (IsRightHanded())
			{
				return *this;
			}

			Vector2D xAxis, yAxis;

			XAxis(xAxis);
			YAxis(yAxis);

			this->Set(yAxis, xAxis);

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::AsRightHanded() const
		{
			DIA_ASSERT(IsOrthogonal(), "Must be orthogonal");
			DIA_ASSERT(IsAxisNormal(), "Axis are not normal");

			if (IsRightHanded())
			{
				return *this;
			}

			Vector2D xAxis, yAxis;

			XAxis(xAxis);
			YAxis(yAxis);

			Matrix22 m(yAxis, xAxis);

			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::NormalAxis()
		{
			Vector2D xAxis, yAxis;

			XAxis(xAxis);
			YAxis(yAxis);

			xAxis.Normalize();
			yAxis.Normalize();
			
			this->Set(xAxis, yAxis);

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::AsNormalAxis() const
		{
			Vector2D xAxis, yAxis;

			XAxis(xAxis);
			YAxis(yAxis);

			xAxis.Normalize();
			yAxis.Normalize();

			Matrix22 m;
			m.Set(xAxis, yAxis);

			return m;
		}

		//-----------------------------------------------------------------------------
		float Matrix22::Trace()const
		{
			return (mElement[0] + mElement[3]);
		}

		//-----------------------------------------------------------------------------
		float Matrix22::Determinant()const
		{
			return ((mElement[0]* mElement[3]) - (mElement[1] * mElement[2]));
		}	

		//-----------------------------------------------------------------------------
		void Matrix22::Eigenvalues(float& eigenValue1, float& eigenValue2)const
		{
			float trace = Trace();
			float det = Determinant();

			DIA_ASSERT (det != 0.0f, "Negative Determinant");

			float variant = Dia::Maths::SquareRoot((trace * trace) - (4.0f * det)); 

			eigenValue1 = 0.5f * (trace + variant);
			eigenValue2 = 0.5f * (trace - variant);
		}

		//-----------------------------------------------------------------------------
		void Matrix22::GetScale(float& scaleX, float& scaleY)const
		{
			scaleX = Dia::Maths::SquareRoot((Dia::Maths::Square(mElement[0]) + Dia::Maths::Square(mElement[1])));
			scaleY = Dia::Maths::SquareRoot((Dia::Maths::Square(mElement[2]) + Dia::Maths::Square(mElement[3])));
		}	

		//-----------------------------------------------------------------------------
		void Matrix22::GetRotationClockwise(Angle& result)const
		{
			DIA_ASSERT(IsOrthogonal(), "Must be orthogonal");
			DIA_ASSERT(IsAxisNormal(), "Axis are not normal");

			Dia::Maths::Vector2D v1(1.0f, 0.0f);
			Dia::Maths::Vector2D v2(mElement[0], mElement[1]);

			v2.GetClockwiseAngleBetween(v1, result);
		}

		//-----------------------------------------------------------------------------
		void Matrix22::GetRotationCounterClockwise(Angle& result)const
		{
			DIA_ASSERT(IsOrthogonal(), "Must be orthogonal");
			DIA_ASSERT(IsAxisNormal(), "Axis are not normal");

			Dia::Maths::Vector2D v1(1.0f, 0.0f);
			Dia::Maths::Vector2D v2(mElement[0], mElement[1]);

			v2.GetCounterClockwiseAngleBetween(v1, result);
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::LookAtRotation(const Vector2D& lookFrom, const Vector2D& lookAt)
		{
			Vector2D xAxis = (lookAt - lookFrom).AsNormal();
			Vector2D yAxis = xAxis.AsRotated90DegreeCounterClockwise();

			this->Set(xAxis, yAxis);

			DIA_ASSERT(IsOrthogonal(), "Must be orthogonal");

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::Negative()
		{
			mElement[0] = mElement[0] * -1.0f;
			mElement[1] = mElement[1] * -1.0f;
			mElement[2] = mElement[2] * -1.0f;
			mElement[3] = mElement[3] * -1.0f;

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::AsNegative() const
		{
			Matrix22 m(	mElement[0] * -1.0f, 
						mElement[1] * -1.0f, 
						mElement[2] * -1.0f, 
						mElement[3] * -1.0f);
			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::Transpose()
		{
			float tempElement10 = mElement[1];

			mElement[0] = mElement[0];
			mElement[1] = mElement[2];
			mElement[2] = tempElement10;
			mElement[3] = mElement[3];

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::AsTranspose() const
		{
			Matrix22 m(mElement[0], mElement[2], mElement[1], mElement[3]);

			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::Invert ()
		{
			float det = Determinant();

			DIA_ASSERT(det != 0.0f, "detminant is equal to zero");

			float e00 = mElement[3] / det;
			float e01 = -mElement[1] / det;
			float e10 = -mElement[2] / det;
			float e11 = mElement[0] / det;
				
			mElement[0] = e00;
			mElement[1] = e01;
			mElement[2] = e10;
			mElement[3] = e11;

			return *this;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::InvertOrthogonal()
		{
			DIA_ASSERT(IsOrthogonal(), "Must be orthogonal to use this");

			return Transpose();	
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::AsInverse () const
		{
			float det = Determinant();

			DIA_ASSERT(det != 0.0f, "detminant is equal to zero");

			Matrix22 m(mElement[3] / det, 
						-mElement[1] / det, 
						-mElement[2] / det, 
						mElement[0] / det);

			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::AsInverseOrthogonal()const
		{
			DIA_ASSERT(IsOrthogonal(), "Must be orthogonal to use this");

			Matrix22 m(mElement[0], mElement[2], mElement[1], mElement[3]);

			return m;	
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::UniformScale(float scale)
		{
			mElement[0] *= scale;
			mElement[3] *= scale;

			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::AsUniformScale(float scale)const
		{
			Matrix22 m(	mElement[0] * scale, 
						mElement[1], 
						mElement[2], 
						mElement[3] * scale);

			return m;
		}

		//-----------------------------------------------------------------------------
		Matrix22& Matrix22::Scale(float scaleX, float scaleY)
		{
			mElement[0] *= scaleX;
			mElement[3] *= scaleY;

			return *this;
		}	
			
		//-----------------------------------------------------------------------------
		Matrix22 Matrix22::AsScale( float scaleX, float scaleY )const
		{
			Matrix22 m(	mElement[0] * scaleX, 
						mElement[1], 
						mElement[2], 
						mElement[3] * scaleY);

			return m;
		}

		//-----------------------------------------------------------------------------	
		Matrix22& Matrix22::RotateCounterClockwise(const Angle& angle)
		{
			float cosAngle = Dia::Maths::Cos(angle);
			float sinAngle = Dia::Maths::Sin(angle);

			Matrix22 m(	cosAngle, 
						-sinAngle, 
						sinAngle, 
						cosAngle);

			*this *= m;

			return *this;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::AsRotateCounterClockwise(const Angle& angle)const
		{
			float cosAngle = Dia::Maths::Cos(angle);
			float sinAngle = Dia::Maths::Sin(angle);

			Matrix22 m(	cosAngle, 
						-sinAngle, 
						sinAngle, 
						cosAngle);

			return (*this * m);
		}
	
		//-----------------------------------------------------------------------------	
		Matrix22& Matrix22::RotateClockwise(const Angle& angle)
		{
			float cosAngle = Dia::Maths::Cos(angle);
			float sinAngle = Dia::Maths::Sin(angle);

			Matrix22 m(	cosAngle, 
						sinAngle, 
						-sinAngle, 
						cosAngle);

			*this *= m;
			
			DIA_ASSERT(m.IsOrthogonal(), "Must be orthogonal to use this");
			DIA_ASSERT(m.IsAxisNormal(), "Axis are not normal");

			return *this;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::AsRotateClockwise(const Angle& angle)const
		{
			float cosAngle = Dia::Maths::Cos(angle);
			float sinAngle = Dia::Maths::Sin(angle);

			Matrix22 m(	cosAngle, 
						sinAngle, 
						-sinAngle, 
						cosAngle);

			DIA_ASSERT(m.IsOrthogonal(), "Must be orthogonal to use this");
			DIA_ASSERT(m.IsAxisNormal(), "Axis are not normal");

			return (*this * m);
		}

		//-----------------------------------------------------------------------------	
		Matrix22& Matrix22::ReflectArbitraryAxis( const Vector2D& axis )
		{
			Matrix22 reflection(1.0f - (2.0f * (axis.x * axis.x)), 
								-2.0f * axis.x * axis.y,
								-2.0f * axis.x * axis.y,
								1.0f - (2.0f * (axis.y * axis.y)));

			*this *= reflection;

			return *this;
		}
		
		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::AsReflectArbitraryAxis( const Vector2D& axis )const
		{
			Matrix22 reflection(1.0f - (2.0f * (axis.x * axis.x)), 
								-2.0f * axis.x * axis.y,
								-2.0f * axis.x * axis.y,
								1.0f - (2.0f * (axis.y * axis.y)));

			Matrix22 m(reflection * (*this));

			return m;
		}

		//-----------------------------------------------------------------------------	
		Matrix22& Matrix22::ProjectArbitraryAxis( const Vector2D& axis )
		{
			float temp = -(axis.x * axis.y);
			Matrix22 projection(1.0f - (axis.x * axis.x), 
								temp, temp,
								1.0f - (axis.y * axis.y));

			*this *= projection;

			return *this;
		}
		
		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::AsProjectArbitraryAxis( const Vector2D& axis )const
		{
			float temp = -(axis.x * axis.y);
			Matrix22 projection(1.0f - (axis.x * axis.x), 
								temp, temp,
								1.0f - (axis.y * axis.y));

			Matrix22 m(*this * projection);

			return m;
		}

		//-----------------------------------------------------------------------------	
		Matrix22& Matrix22::ShearXAxis( float shear )
		{
			Matrix22 shearMatrix(1.0f, 0.0f, shear, 1.0f);
			
			*this *= shearMatrix;

			return *this;
		}

		//-----------------------------------------------------------------------------	
		Matrix22& Matrix22::ShearYAxis( float shear )
		{
			Matrix22 shearMatrix(1.0f, shear, 0.0f, 1.0f);

			*this *= shearMatrix;

			return *this;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::AsShearXAxis( float shear )const
		{
			Matrix22 shearMatrix(1.0f, 0.0f, shear, 1.0f);
			
			Matrix22 m(*this * shearMatrix);

			return m;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::AsShearYAxis( float shear )const
		{
			Matrix22 shearMatrix(1.0f, shear, 0.0f, 1.0f);
			
			Matrix22 m(*this * shearMatrix);

			return m;
		}
		
		//-----------------------------------------------------------------------------	
		Matrix22& Matrix22::Orthogonalize()
		{
			Dia::Maths::Vector2D xAxis(mElement[0], mElement[1]);

			DIA_ASSERT( !Dia::Maths::Float::FEqual(xAxis.SquareMagnitude(), 0.0f) , 
						"Can not be a zero vector in the xAxis of the matrix");

			xAxis.Normalize();

			Dia::Maths::Vector2D yAxis = xAxis.AsRotated90DegreeCounterClockwise();
			
			this->Set(xAxis, yAxis);
		
			return *this;
		}

		//-----------------------------------------------------------------------------	
		Matrix22 Matrix22::AsOrthogonal()const
		{
			Dia::Maths::Vector2D xAxis(mElement[0], mElement[1]);

			DIA_ASSERT( !Dia::Maths::Float::FEqual(xAxis.SquareMagnitude(), 0.0f) , 
				"Can not be a zero vector in the xAxis of the matrix");

			xAxis.Normalize();

			Dia::Maths::Vector2D yAxis = xAxis.AsRotated90DegreeCounterClockwise();

			Matrix22 m(xAxis, yAxis);

			return m;
		}

		//-----------------------------------------------------------------------------
		int Matrix22::LinearIndex(int row, int coloumn)const
		{
			int index = (row * kNumRows) + coloumn;

			DIA_ASSERT(index >= 0 && index < kNumElements, "Oubound the array");

			return index;
		}
	}
}