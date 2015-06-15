
#include "UnitTests/Tests/Maths/UnitTestMatrix22.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaCore/Strings/String128.h>
#include <DiaMaths/Matrix/Matrix22.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <DiaMaths/Core/Angle.h>

using Dia::Maths::Matrix22;
using Dia::Maths::Vector2D;
using Dia::Maths::Angle;

namespace UnitTests
{	
	UnitTestMatrix22::UnitTestMatrix22(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestMatrix22::UnitTestMatrix22(void)
		: UnitTestMaths()
	{}

	void UnitTestMatrix22::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Matrix22 m; 
			UNIT_TEST_POSITIVE(m[0] == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m[1] == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m[2] == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m[3] == 1.0f, "Matrix22");
				
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			UNIT_TEST_POSITIVE(m1[0] == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1[1] == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1[2] == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1[3] == 4.0f, "Matrix22");
			
			Matrix22 m2;
			m2.Set(1.0f, 2.0f, 3.0f, 4.0f); 

			UNIT_TEST_POSITIVE(m2[0] == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2[1] == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2[2] == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2[3] == 4.0f, "Matrix22");
				
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(Vector2D(4.0f, 3.0f), Vector2D(2.0f, 1.0f)); 

			UNIT_TEST_POSITIVE(m1[0] == 4.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1[1] == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1[2] == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1[3] == 1.0f, "Matrix22");
					
			Matrix22 m2;
			m2.Set(Vector2D(4.0f, 3.0f), Vector2D(2.0f, 1.0f)); 

			UNIT_TEST_POSITIVE(m2[0] == 4.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2[1] == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2[2] == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2[3] == 1.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			Matrix22 m2(m1);

			UNIT_TEST_POSITIVE(m2[0] == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2[1] == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2[2] == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2[3] == 4.0f, "Matrix22");

			Matrix22 m3;
			m3.Set(m1);

			UNIT_TEST_POSITIVE(m3[0] == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3[1] == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3[2] == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3[3] == 4.0f, "Matrix22");
					
			Matrix22 m4;
			m4 = m1;

			UNIT_TEST_POSITIVE(m4[0] == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m4[1] == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m4[2] == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m4[3] == 4.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 	
			Matrix22 m2(1.0f, 2.0f, 3.0f, 4.0f); 
			Matrix22 m3(1.0f, 2.0f, 3.0f, 0.0f); 

			UNIT_TEST_POSITIVE(m1 == m1, "Matrix22");
			UNIT_TEST_POSITIVE(m1 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m1 != m3, "Matrix22");
				
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 	

			UNIT_TEST_ASSERT_EXPECTED_START();
			m1[4] = 3.0f; 
			UNIT_TEST_ASSERT_EXPECTED_END();			

			UNIT_TEST_ASSERT_EXPECTED_START();
			float a = m1[4];
			UNIT_TEST_ASSERT_EXPECTED_END();			
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			float a = m1.Element(4);
			UNIT_TEST_ASSERT_EXPECTED_END();	

			UNIT_TEST_ASSERT_EXPECTED_START();
			float a = m1.Element(4, 0);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			float a = m1.Element(0, 4);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_POSITIVE(m1.Element(0) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(1) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(2) == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(3) == 4.0f, "Matrix22");
			
			UNIT_TEST_POSITIVE(m1.Element(0, 0) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(0, 1) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(1, 0) == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(1, 1) == 4.0f, "Matrix22");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			m1.SetElement(4, 3.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();	

			UNIT_TEST_ASSERT_EXPECTED_START();
			m1.SetElement(4, 0, 3.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			m1.SetElement(0, 4, 3.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();

			m1.SetElement(1, 5.0f);
			m1.SetElement(1, 0, 6.0f);
			m1[3] = 7.0f;

			UNIT_TEST_POSITIVE(m1.Element(0) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(1) == 5.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(2) == 6.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(3) == 7.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			
			Vector2D xAxis, yAxis;

			m1.XAxis(xAxis);
			m1.YAxis(yAxis);
			
			UNIT_TEST_POSITIVE(m1.Element(0) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(1) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(2) == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(3) == 4.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			
			m1.Clear();

			UNIT_TEST_POSITIVE(m1.Element(0) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(1) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(2) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(3) == 0.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			
			Matrix22 m2 = -m1;

			UNIT_TEST_POSITIVE(m2.Element(0) == -1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(1) == -2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(2) == -3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(3) == -4.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f); 
			
			Matrix22 m3 = m2 + m1;
			m2 += m1;

			UNIT_TEST_POSITIVE(m3 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(0) == 6.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(1) == 8.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(2) == 10.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(3) == 12.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f); 
			
			Matrix22 m3 = m2 - m1;
			m2 -= m1;

			UNIT_TEST_POSITIVE(m3 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(0) == 4.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(1) == 4.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(2) == 4.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(3) == 4.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f); 
			
			Matrix22 m3 = m2 - m1;
			m2 -= m1;

			UNIT_TEST_POSITIVE(m3 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(0) == 4.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(1) == 4.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(2) == 4.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(3) == 4.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			Matrix22 m2(5.0f, 6.0f, 7.0f, 8.0f); 
			
			Matrix22 m3 = m2 * m1;
			m2 *= m1;

			UNIT_TEST_POSITIVE(m3 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(0) == 23.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(1) == 34.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(2) == 31.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(3) == 46.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m2(1.0f, 2.0f, 3.0f, 4.0f); 

			Matrix22 m3 = m2 * 2.0f;
			m2 *= 2.0f;

			UNIT_TEST_POSITIVE(m3 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(0) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(1) == 4.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(2) == 6.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(3) == 8.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_ASSERT_EXPECTED_START();
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			m1 /= 0.0f;
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			m1 = m1 / 0.0f;
			UNIT_TEST_ASSERT_EXPECTED_END();		

			Matrix22 m2(1.0f, 2.0f, 3.0f, 4.0f); 

			Matrix22 m3 = m2 / 2.0f;
			m2 /= 2.0f;

			UNIT_TEST_POSITIVE(m3 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(0) == 0.5f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(1) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(2) == 1.5f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(3) == 2.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()
	
		UNIT_TEST_BLOCK_START()
						
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 
			
			Matrix22 m2 = m1.AsNegative();
			m1.Negative();

			UNIT_TEST_POSITIVE(m1 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(0) == -1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(1) == -2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(2) == -3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(3) == -4.0f, "Matrix22");

			Matrix22 m3 = m1.AsNegative();
			m1.Negative();

			UNIT_TEST_POSITIVE(m3 == m1, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(0) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(1) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(2) == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(3) == 4.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
	
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 

			Matrix22 m2 = m1.AsTranspose();
			m1.Transpose();

			UNIT_TEST_POSITIVE(m1 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(0) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(1) == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(2) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(3) == 4.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 

			Matrix22 m2 = m1.UniformScale(2.0f);
			m1.AsUniformScale(2.0f);

			UNIT_TEST_POSITIVE(m1 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(0) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(1) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(2) == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(3) == 8.0f, "Matrix22");

			Matrix22 m3(1.0f, 2.0f, 3.0f, 4.0f); 

			Matrix22 m4 = m3.UniformScale(-2.0f);
			m3.AsUniformScale(-2.0f);

			UNIT_TEST_POSITIVE(m3 == m4, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(0) == -2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(1) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(2) == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(3) == -8.0f, "Matrix22");
	
			Matrix22 m5(1.0f, 2.0f, 3.0f, 4.0f); 

			Matrix22 m6 = m5.UniformScale(0.5f);
			m5.AsUniformScale(0.5f);

			UNIT_TEST_POSITIVE(m5 == m6, "Matrix22");
			UNIT_TEST_POSITIVE(m5.Element(0) == 0.5f, "Matrix22");
			UNIT_TEST_POSITIVE(m5.Element(1) == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m5.Element(2) == 3.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m5.Element(3) == 2.0f, "Matrix22");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			UNIT_TEST_ASSERT_EXPECTED_START();
			Matrix22 temp(0.0f, 0.0f, 0.0f, 1.0f); 
			temp.AsInverse();
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Matrix22 temp(0.0f, 0.0f, 0.0f, 1.0f); 
			Matrix22 temp2 = temp.Invert();
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 

			Matrix22 m2 = m1.Invert();
			m1.AsInverse();

			UNIT_TEST_POSITIVE(m1 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(0) == -2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(1) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(2) == 1.5f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(3) == -0.5f, "Matrix22");

		UNIT_TEST_BLOCK_END()
	

		UNIT_TEST_BLOCK_START()

			UNIT_TEST_ASSERT_EXPECTED_START();
			Matrix22 temp(0.0f, 0.0f, 0.0f, 1.0f); 
			temp.InvertOrthogonal();
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Matrix22 temp(0.0f, 0.0f, 0.0f, 1.0f); 
			Matrix22 temp2 = temp.AsInverseOrthogonal();
			UNIT_TEST_ASSERT_EXPECTED_END();

			Matrix22 m1(1.0f, 0.0f, 0.0f, 1.0f); 

			Matrix22 m2 = m1.Invert();
			m1.AsInverse();

			UNIT_TEST_POSITIVE(m1 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(0) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(1) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(2) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m2.Element(3) == 1.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 

			float temp = m1.Trace();

			UNIT_TEST_POSITIVE(temp == 5.0f, "Matrix22");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 

			float temp = m1.Determinant();

			UNIT_TEST_POSITIVE(temp == -2.0f, "Matrix22");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 2.0f, 3.0f, 4.0f); 

			float eigenValue1;
			float eigenValue2;
			
			m1.Eigenvalues(eigenValue1, eigenValue2);

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(eigenValue1, 5.372281323269014f), "Matrix22");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(eigenValue2, -0.3722813232690143f), "Matrix22");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 3.0f, 3.0f, 4.0f); 

			UNIT_TEST_POSITIVE(m1.IsSymmetric(), "Matrix22");
		
			Matrix22 m2(1.0f, 2.0f, 3.0f, 4.0f); 

			UNIT_TEST_POSITIVE(!m2.IsSymmetric(), "Matrix22");
		
		UNIT_TEST_BLOCK_END()
		
			
		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(0.0f, -3.0f, 3.0f, 0.0f); 

			UNIT_TEST_POSITIVE(m1.IsSkewSymmetric(), "Matrix22");
		
			Matrix22 m2(0.0f, 2.0f, 3.0f, 0.0f); 

			UNIT_TEST_POSITIVE(!m2.IsSkewSymmetric(), "Matrix22");
		
			Matrix22 m3(0.0f, 3.0f, 3.0f, 0.0f); 

			UNIT_TEST_POSITIVE(!m3.IsSkewSymmetric(), "Matrix22");
		
		UNIT_TEST_BLOCK_END()
			
		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, -3.0f, 3.0f, 4.0f); 

			UNIT_TEST_POSITIVE(!m1.IsIdentity(), "Matrix22");
		
			Matrix22 m2(1.0f, 0.0f, 0.0f, 1.0f); 

			UNIT_TEST_POSITIVE(m2.IsIdentity(), "Matrix22");
		
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 0.0f, 0.0f, 4.0f); 

			UNIT_TEST_POSITIVE(m1.IsDiagonalMatrix(), "Matrix22");
		
			Matrix22 m2(1.0f, 1.0f, 0.0f, 1.0f); 

			UNIT_TEST_POSITIVE(!m2.IsDiagonalMatrix(), "Matrix22");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Matrix22 m1(1.0f, 0.0f, 0.0f, 1.0f); 

			UNIT_TEST_POSITIVE(m1.IsOrthogonal(), "Matrix22");
		
			Matrix22 m2(1.0f, 1.0f, 0.0f, 1.0f); 

			UNIT_TEST_POSITIVE(!m2.IsOrthogonal(), "Matrix22");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Matrix22 m1(2.0f, 0.0f, 0.0f, 2.0f); 

			UNIT_TEST_POSITIVE(m1.IsScaled(), "Matrix22");

			Matrix22 m2(1.0f, 1.0f, 0.0f, 1.0f); 

			UNIT_TEST_POSITIVE(m2.IsScaled(), "Matrix22");

			Matrix22 m3(1.0f, 0.0f, 0.0f, 1.0f); 

			UNIT_TEST_POSITIVE(!m3.IsScaled(), "Matrix22");
			
			Matrix22 m4(2.0f, 0.0f, 0.0f, 1.0f); 

			UNIT_TEST_POSITIVE(m4.IsScaled(), "Matrix22");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Matrix22 m1(2.0f, 0.0f, 0.0f, 2.0f); 
			UNIT_TEST_POSITIVE(m1.IsUniformScale(), "Matrix22");

			Matrix22 m2(1.0f, 1.0f, 0.0f, 1.0f); 
			UNIT_TEST_POSITIVE(!m2.IsUniformScale(), "Matrix22");

			Matrix22 m3(1.0f, 0.0f, 0.0f, 1.0f); 
			UNIT_TEST_POSITIVE(!m3.IsUniformScale(), "Matrix22");

			Matrix22 m4(2.0f, 0.0f, 0.0f, 1.0f); 
			UNIT_TEST_POSITIVE(!m4.IsUniformScale(), "Matrix22");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Matrix22 m1(2.0f, 0.0f, 0.0f, 2.0f); 
			float ms1, ms2;
			m1.GetScale(ms1, ms2);
			UNIT_TEST_POSITIVE(ms1 == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(ms2 == 2.0f, "Matrix22");

			Matrix22 m2(1.0f, 1.0f, 0.0f, 1.0f); 
			float ms3, ms4;
			m2.GetScale(ms3, ms4);
			UNIT_TEST_POSITIVE(ms3 == 1.4142135f, "Matrix22");
			UNIT_TEST_POSITIVE(ms4 == 1.0f, "Matrix22");

			Matrix22 m3(1.0f, 0.0f, 0.0f, 1.0f); 
			float ms5, ms6;
			m3.GetScale(ms5, ms6);
			UNIT_TEST_POSITIVE(ms5 == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(ms6 == 1.0f, "Matrix22");

			Matrix22 m4(2.0f, 0.0f, 0.0f, 1.0f); 
			float ms7, ms8;
			m4.GetScale(ms7, ms8);
			UNIT_TEST_POSITIVE(ms7 == 2.0f, "Matrix22");
			UNIT_TEST_POSITIVE(ms8 == 1.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()
	
		UNIT_TEST_BLOCK_START()

			Matrix22 matrix(Matrix22::Identity);
			
			for (int i = 0; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				Matrix22 resultMatrix = matrix.AsRotateCounterClockwise(testAngle);

				Angle resultAngle;
				
				resultMatrix.GetRotationCounterClockwise(resultAngle);
				
				Dia::Core::Containers::String128 str("ResultMatrix %0.4f, %0.4f, %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", resultMatrix[0], resultMatrix[1], resultMatrix[2], resultMatrix[3], resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}
				
			matrix = Matrix22::Identity;
			
			for (int i = 1; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
	
				matrix = matrix.AsRotateCounterClockwise(Dia::Maths::Angle::Deg1);

				Angle resultAngle;
				
				matrix.GetRotationCounterClockwise(resultAngle);
				
				Dia::Core::Containers::String128 str("ResultMatrix %0.4f, %0.4f, %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", matrix[0], matrix[1], matrix[2], matrix[3], resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Matrix22 matrixBase(Matrix22::Identity);
			Matrix22 matrix(Matrix22::Identity);
			
			for (int i = 0; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				Matrix22 matrix = matrixBase;

				matrix.RotateCounterClockwise(testAngle);

				Angle resultAngle;
				
				matrix.GetRotationCounterClockwise(resultAngle);
				
				Dia::Core::Containers::String128 str("ResultMatrix %0.4f, %0.4f, %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", matrix[0], matrix[1], matrix[2], matrix[3], resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}
			
			matrix = Matrix22::Identity;
			
			for (int i = 1; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				matrix.RotateCounterClockwise(Dia::Maths::Angle::Deg1);

				Angle resultAngle;
				
				matrix.GetRotationCounterClockwise(resultAngle);
				
				Dia::Core::Containers::String128 str("ResultMatrix %0.4f, %0.4f, %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", matrix[0], matrix[1], matrix[2], matrix[3], resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Matrix22 matrix(Matrix22::Identity);
			
			for (int i = 0; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				Matrix22 resultMatrix = matrix.AsRotateClockwise(testAngle);

				Angle resultAngle;
				
				resultMatrix.GetRotationClockwise(resultAngle);
				
				Dia::Core::Containers::String128 str("Resultmatrix %0.4f, %0.4f %0.4f %0.4f- resultAngle %0.5f, testAngle %0.5f", resultMatrix[0], resultMatrix[1], resultMatrix[2], resultMatrix[3], resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}
				
			matrix = Matrix22::Identity;
			
			for (int i = 1; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				matrix = matrix.AsRotateClockwise(Dia::Maths::Angle::Deg1);

				Angle resultAngle;
				
				matrix.GetRotationClockwise(resultAngle);
				
				Dia::Core::Containers::String128 str("ResultMatrix %0.4f, %0.4f, %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", matrix[0], matrix[1], matrix[2], matrix[3], resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Matrix22 matrixBase(Matrix22::Identity);
			Matrix22 matrix(Matrix22::Identity);
			
			for (int i = 0; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				Matrix22 matrix = matrixBase;

				matrix.RotateClockwise(testAngle);

				Angle resultAngle;
				
				matrix.GetRotationClockwise(resultAngle);
				
				Dia::Core::Containers::String128 str("ResultMatrix %0.4f, %0.4f, %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", matrix[0], matrix[1], matrix[2], matrix[3], resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}

			matrix = Matrix22::Identity;
			
			for (int i = 1; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				matrix.RotateClockwise(Dia::Maths::Angle::Deg1);

				Angle resultAngle;
				
				matrix.GetRotationClockwise(resultAngle);
				
				Dia::Core::Containers::String128 str("ResultMatrix %0.4f, %0.4f, %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", matrix[0], matrix[1], matrix[2], matrix[3], resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Matrix22 m1(1.0f, 0.0f, 0.0f, 1.0f); 
			Matrix22 m2 = m1.AsReflectArbitraryAxis(Vector2D(1.0f, 0.0f));
			m1.ReflectArbitraryAxis(Vector2D(1.0f, 0.0f));
			
			UNIT_TEST_POSITIVE(m1 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(0) == -1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(1) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(2) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(3) == 1.0f, "Matrix22");
			
			Matrix22 m3(1.0f, 0.0f, 0.0f, 1.0f); 
			Matrix22 m4 = m3.AsReflectArbitraryAxis(Vector2D(0.0f, 1.0f));
			m3.ReflectArbitraryAxis(Vector2D(0.0f, 1.0f));

			UNIT_TEST_POSITIVE(m3 == m4, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(0) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(1) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(2) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m3.Element(3) == -1.0f, "Matrix22");

		UNIT_TEST_BLOCK_END()
		
			UNIT_TEST_BLOCK_START()

			Matrix22 m1(Matrix22::Identity); 
			Matrix22 m2 = m1.AsProjectArbitraryAxis(Vector2D(1.0f, 0.0f));
			m1.ProjectArbitraryAxis(Vector2D(1.0f, 0.0f));

			UNIT_TEST_POSITIVE(m1 == m2, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(0) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(1) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(2) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m1.Element(3) == 1.0f, "Matrix22");

			Matrix22 m5(Matrix22::Identity); 
			Matrix22 m6 = m5.AsProjectArbitraryAxis(Vector2D(0.0f, 1.0f));
			m5.ProjectArbitraryAxis(Vector2D(0.0f, 1.0f));

			UNIT_TEST_POSITIVE(m5 == m6, "Matrix22");
			UNIT_TEST_POSITIVE(m5.Element(0) == 1.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m5.Element(1) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m5.Element(2) == 0.0f, "Matrix22");
			UNIT_TEST_POSITIVE(m5.Element(3) == 0.0f, "Matrix22");
		
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Matrix22 matrixBase(Matrix22::Identity);
			Matrix22 matrix(Matrix22::Identity);
			
			for (int i = 0; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				Matrix22 matrix = matrixBase;

				matrix.RotateClockwise(testAngle);

				Angle resultAngle;
				
				matrix.GetRotationClockwise(resultAngle);
				
				Dia::Core::Containers::String128 str("ResultMatrix %0.4f, %0.4f, %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", matrix[0], matrix[1], matrix[2], matrix[3], resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Vector2D result;

			Matrix22 matrix1(vector1, vector3);
			Matrix22 matrix2(vector2, vector4);
			Matrix22 matrix3(vector3, vector5);
			Matrix22 matrix4(vector4, vector6);
			Matrix22 matrix5(vector5, vector7);
			Matrix22 matrix6(vector6, vector8);
			Matrix22 matrix7(vector7, vector1);
			Matrix22 matrix8(vector8, vector2);

			result = matrix1 * vector1;
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = matrix2 * vector1;
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = matrix3 * vector1;
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");
			result = matrix4 * vector1;
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");		
			result = matrix5 * vector1;
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");		
			result = matrix6 * vector1;
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = matrix7 * vector1;
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = matrix8 * vector1;
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	

			result = matrix1 * vector2;
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = matrix2 * vector2;
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			result = matrix3 * vector2;
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");
			result = matrix4 * vector2;
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");		
			result = matrix5 * vector2;
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");		
			result = matrix6 * vector2;
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = matrix7 * vector2;
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = matrix8 * vector2;
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	

			result = matrix1 * vector3;
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			result = matrix2 * vector3;
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");	
			result =  matrix3 * vector3;
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = matrix4 * vector3;
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");
			result = matrix5 * vector3;
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");		
			result = matrix6 * vector3;
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");		
			result = matrix7 * vector3;
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = matrix8 * vector3;
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	

			result = matrix1 * vector4;
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");	
			result = matrix2 * vector4;
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = matrix3 * vector4;
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = matrix4 * vector4;
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");
			result = matrix5 * vector4;
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");		
			result = matrix6 * vector4;
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");		
			result = matrix7 * vector4;
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = matrix8 * vector4;
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	

			result = matrix1 * vector5;
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = matrix2 * vector5;
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = matrix3 * vector5;
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = matrix4 * vector5;
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");
			result = matrix5 * vector5;
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");		
			result = matrix6 * vector5;
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");		
			result = matrix7 * vector5;
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			result = matrix8 * vector5;
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");

			result = matrix1 * vector6;
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = matrix2 * vector6;
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = matrix3 * vector6;
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = matrix4 * vector6;
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");
			result = matrix5 * vector6;
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");		
			result = matrix6 * vector6;
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");		
			result = matrix7 * vector6;
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");	
			result = matrix8 * vector6;
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");

			result = matrix1 * vector7;
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = matrix2 * vector7;
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = matrix3 * vector7;
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = matrix4 * vector7;
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");
			result = matrix5 * vector7;
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");		
			result = matrix6 * vector7;
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");		
			result = matrix7 * vector7;
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = matrix8 * vector7;
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");

			result = matrix1 * vector8;
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = matrix2 * vector8;
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = matrix3 * vector8;
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = matrix4 * vector8;
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");
			result = matrix5 * vector8;
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");		
			result = matrix6 * vector8;
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");		
			result = matrix7* vector8;
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = matrix8*vector8;
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Vector2D result;

			Matrix22 matrix1(vector1, vector3);
			Matrix22 matrix2(vector2, vector4);
			Matrix22 matrix3(vector3, vector5);
			Matrix22 matrix4(vector4, vector6);
			Matrix22 matrix5(vector5, vector7);
			Matrix22 matrix6(vector6, vector8);
			Matrix22 matrix7(vector7, vector1);
			Matrix22 matrix8(vector8, vector2);
	
			UNIT_TEST_POSITIVE(matrix1.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix2.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix3.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix4.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix5.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix6.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix7.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix8.IsLeftHanded(), "Vector2D");	
		
			UNIT_TEST_NEGATIVE(matrix1.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix2.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix3.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix4.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix5.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix6.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix7.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix8.IsRightHanded(), "Vector2D");

			matrix1.RightHanded();
			matrix2.RightHanded();
			matrix3.RightHanded();
			matrix4.RightHanded();
			matrix5.RightHanded();
			matrix6.RightHanded();
			matrix7.RightHanded();
			matrix8.RightHanded();
		
			UNIT_TEST_POSITIVE(matrix1.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix2.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix3.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix4.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix5.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix6.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix7.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix8.IsRightHanded(), "Vector2D");	

			UNIT_TEST_NEGATIVE(matrix1.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix2.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix3.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix4.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix5.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix6.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix7.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix8.IsLeftHanded(), "Vector2D");

			matrix1.LeftHanded();
			matrix2.LeftHanded();
			matrix3.LeftHanded();
			matrix4.LeftHanded();
			matrix5.LeftHanded();
			matrix6.LeftHanded();
			matrix7.LeftHanded();
			matrix8.LeftHanded();

			UNIT_TEST_POSITIVE(matrix1.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix2.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix3.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix4.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix5.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix6.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix7.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix8.IsLeftHanded(), "Vector2D");	

			UNIT_TEST_NEGATIVE(matrix1.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix2.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix3.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix4.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix5.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix6.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix7.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix8.IsRightHanded(), "Vector2D");

			Matrix22 matrixTest1 = matrix1.AsRightHanded();
			Matrix22 matrixTest2 = matrix2.AsRightHanded();
			Matrix22 matrixTest3 = matrix3.AsRightHanded();
			Matrix22 matrixTest4 = matrix4.AsRightHanded();
			Matrix22 matrixTest5 = matrix5.AsRightHanded();
			Matrix22 matrixTest6 = matrix6.AsRightHanded();
			Matrix22 matrixTest7 = matrix7.AsRightHanded();
			Matrix22 matrixTest8 = matrix8.AsRightHanded();

			UNIT_TEST_POSITIVE(matrixTest1.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest2.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest3.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest4.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest5.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest6.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest7.IsRightHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest8.IsRightHanded(), "Vector2D");	

			UNIT_TEST_NEGATIVE(matrixTest1.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixTest2.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixTest3.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixTest4.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixTest5.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixTest6.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixTest7.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixTest8.IsLeftHanded(), "Vector2D");

			Matrix22 matrixB1 = matrixTest1.AsLeftHanded();
			Matrix22 matrixB2 = matrixTest2.AsLeftHanded();
			Matrix22 matrixB3 = matrixTest3.AsLeftHanded();
			Matrix22 matrixB4 = matrixTest4.AsLeftHanded();
			Matrix22 matrixB5 = matrixTest5.AsLeftHanded();
			Matrix22 matrixB6 = matrixTest6.AsLeftHanded();
			Matrix22 matrixB7 = matrixTest7.AsLeftHanded();
			Matrix22 matrixB8 = matrixTest8.AsLeftHanded();

			UNIT_TEST_POSITIVE(matrixB1.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixB2.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixB3.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixB4.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixB5.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixB6.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixB7.IsLeftHanded(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixB8.IsLeftHanded(), "Vector2D");	

			UNIT_TEST_NEGATIVE(matrixB1.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixB2.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixB3.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixB4.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixB5.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixB6.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixB7.IsRightHanded(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixB8.IsRightHanded(), "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Vector2D result;

			Matrix22 matrix1(vector1, vector3);
			Matrix22 matrix2(vector2, vector4);
			Matrix22 matrix3(vector3, vector5);
			Matrix22 matrix4(vector4, vector6);
			Matrix22 matrix5(vector5, vector7);
			Matrix22 matrix6(vector6, vector8);
			Matrix22 matrix7(vector7, vector1);
			Matrix22 matrix8(vector8, vector2);

			UNIT_TEST_POSITIVE(matrix1.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix2.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix3.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix4.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix5.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix6.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix7.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix8.IsAxisNormal(), "Vector2D");	

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			Vector2D result;

			Matrix22 matrix1(vector1, vector3);
			Matrix22 matrix2(vector2, vector4);
			Matrix22 matrix3(vector3, vector5);
			Matrix22 matrix4(vector4, vector6);
			Matrix22 matrix5(vector5, vector7);
			Matrix22 matrix6(vector6, vector8);
			Matrix22 matrix7(vector7, vector1);
			Matrix22 matrix8(vector8, vector2);

			UNIT_TEST_POSITIVE(matrix1.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix2.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix3.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix4.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix5.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix6.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix7.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix8.IsAxisNormal(), "Vector2D");	

			matrix1.NormalAxis();
			matrix2.NormalAxis();
			matrix3.NormalAxis();
			matrix4.NormalAxis();
			matrix5.NormalAxis();
			matrix6.NormalAxis();
			matrix7.NormalAxis();
			matrix8.NormalAxis();
			
			UNIT_TEST_POSITIVE(matrix1.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix2.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix3.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix4.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix5.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix6.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix7.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix8.IsAxisNormal(), "Vector2D");	

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			Vector2D result;

			Matrix22 matrix1(vector1, vector3);
			Matrix22 matrix2(vector2, vector4);
			Matrix22 matrix3(vector3, vector5);
			Matrix22 matrix4(vector4, vector6);
			Matrix22 matrix5(vector5, vector7);
			Matrix22 matrix6(vector6, vector8);
			Matrix22 matrix7(vector7, vector1);
			Matrix22 matrix8(vector8, vector2);

			UNIT_TEST_POSITIVE(matrix1.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix2.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix3.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix4.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix5.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix6.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix7.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix8.IsAxisNormal(), "Vector2D");	

			Matrix22 matrixTest1 = matrix1.AsNormalAxis();
			Matrix22 matrixTest2 = matrix2.AsNormalAxis();
			Matrix22 matrixTest3 = matrix3.AsNormalAxis();
			Matrix22 matrixTest4 = matrix4.AsNormalAxis();
			Matrix22 matrixTest5 = matrix5.AsNormalAxis();
			Matrix22 matrixTest6 = matrix6.AsNormalAxis();
			Matrix22 matrixTest7 = matrix7.AsNormalAxis();
			Matrix22 matrixTest8 = matrix8.AsNormalAxis();

			UNIT_TEST_POSITIVE(matrix1.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix2.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix3.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix4.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix5.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix6.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrix7.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrix8.IsAxisNormal(), "Vector2D");	

			UNIT_TEST_POSITIVE(matrixTest1.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest2.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest3.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest4.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest5.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest6.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest7.IsAxisNormal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixTest8.IsAxisNormal(), "Vector2D");	

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector0(0.0f, 0.0f);
			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			Vector2D result;

			Matrix22 matrixToOrthogonilize1(vector1, vector0);
			Matrix22 matrixToOrthogonilize2(vector2, vector0);
			Matrix22 matrixToOrthogonilize3(vector3, vector0);
			Matrix22 matrixToOrthogonilize4(vector4, vector0);
			Matrix22 matrixToOrthogonilize5(vector5, vector0);
			Matrix22 matrixToOrthogonilize6(vector6, vector0);
			Matrix22 matrixToOrthogonilize7(vector7, vector0);
			Matrix22 matrixToOrthogonilize8(vector8, vector0);

			UNIT_TEST_NEGATIVE(matrixToOrthogonilize1.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize2.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize3.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize4.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize5.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize6.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize7.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize8.IsOrthogonal(), "Vector2D");	

			matrixToOrthogonilize1.Orthogonalize();
			matrixToOrthogonilize2.Orthogonalize();
			matrixToOrthogonilize3.Orthogonalize();
			matrixToOrthogonilize4.Orthogonalize();
			matrixToOrthogonilize5.Orthogonalize();
			matrixToOrthogonilize6.Orthogonalize();
			matrixToOrthogonilize7.Orthogonalize();
			matrixToOrthogonilize8.Orthogonalize();

			UNIT_TEST_POSITIVE(matrixToOrthogonilize1.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixToOrthogonilize2.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixToOrthogonilize3.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixToOrthogonilize4.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixToOrthogonilize5.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixToOrthogonilize6.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixToOrthogonilize7.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixToOrthogonilize8.IsOrthogonal(), "Vector2D");	

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector0(0.0f, 0.0f);
			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			Vector2D result;

			Matrix22 matrixToOrthogonilize1(vector1, vector0);
			Matrix22 matrixToOrthogonilize2(vector2, vector0);
			Matrix22 matrixToOrthogonilize3(vector3, vector0);
			Matrix22 matrixToOrthogonilize4(vector4, vector0);
			Matrix22 matrixToOrthogonilize5(vector5, vector0);
			Matrix22 matrixToOrthogonilize6(vector6, vector0);
			Matrix22 matrixToOrthogonilize7(vector7, vector0);
			Matrix22 matrixToOrthogonilize8(vector8, vector0);

			UNIT_TEST_NEGATIVE(matrixToOrthogonilize1.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize2.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize3.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize4.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize5.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize6.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize7.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_NEGATIVE(matrixToOrthogonilize8.IsOrthogonal(), "Vector2D");	

			Matrix22 matrixOrthogonilizeed1 = matrixToOrthogonilize1.AsOrthogonal();
			Matrix22 matrixOrthogonilizeed2 = matrixToOrthogonilize2.AsOrthogonal();
			Matrix22 matrixOrthogonilizeed3 = matrixToOrthogonilize3.AsOrthogonal();
			Matrix22 matrixOrthogonilizeed4 = matrixToOrthogonilize4.AsOrthogonal();
			Matrix22 matrixOrthogonilizeed5 = matrixToOrthogonilize5.AsOrthogonal();
			Matrix22 matrixOrthogonilizeed6 = matrixToOrthogonilize6.AsOrthogonal();
			Matrix22 matrixOrthogonilizeed7 = matrixToOrthogonilize7.AsOrthogonal();
			Matrix22 matrixOrthogonilizeed8 = matrixToOrthogonilize8.AsOrthogonal();

			UNIT_TEST_POSITIVE(matrixOrthogonilizeed1.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixOrthogonilizeed2.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixOrthogonilizeed3.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixOrthogonilizeed4.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixOrthogonilizeed5.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixOrthogonilizeed6.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixOrthogonilizeed7.IsOrthogonal(), "Vector2D");	
			UNIT_TEST_POSITIVE(matrixOrthogonilizeed8.IsOrthogonal(), "Vector2D");	

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Vector2D result;

			Matrix22 matrix1(vector1, vector3);
			Matrix22 matrix2(vector2, vector4);
			Matrix22 matrix3(vector3, vector5);
			Matrix22 matrix4(vector4, vector6);
			Matrix22 matrix5(vector5, vector7);
			Matrix22 matrix6(vector6, vector8);
			Matrix22 matrix7(vector7, vector1);
			Matrix22 matrix8(vector8, vector2);

			Vector2D lookFrom(1.0f, 1.0f);
			Vector2D lookAt1(3.0f, 1.0f);
			Vector2D lookAt2(3.0f, 3.0f);
			Vector2D lookAt3(1.0f, 3.0f);
			Vector2D lookAt4(-1.0f, 3.0f);
			Vector2D lookAt5(-1.0f, 1.0f);
			Vector2D lookAt6(-1.0f, -1.0f);
			Vector2D lookAt7(1.0f, -1.0f);
			Vector2D lookAt8(3.0f, -1.0f);

			Matrix22 lookAtMatrix1;
			Matrix22 lookAtMatrix2;
			Matrix22 lookAtMatrix3;
			Matrix22 lookAtMatrix4;
			Matrix22 lookAtMatrix5;
			Matrix22 lookAtMatrix6;
			Matrix22 lookAtMatrix7;
			Matrix22 lookAtMatrix8;

			lookAtMatrix1.LookAtRotation(lookFrom, lookAt1);
			lookAtMatrix2.LookAtRotation(lookFrom, lookAt2);
			lookAtMatrix3.LookAtRotation(lookFrom, lookAt3);
			lookAtMatrix4.LookAtRotation(lookFrom, lookAt4);
			lookAtMatrix5.LookAtRotation(lookFrom, lookAt5);
			lookAtMatrix6.LookAtRotation(lookFrom, lookAt6);
			lookAtMatrix7.LookAtRotation(lookFrom, lookAt7);
			lookAtMatrix8.LookAtRotation(lookFrom, lookAt8);

			UNIT_TEST_POSITIVE(matrix1 == lookAtMatrix1, "Vector2D");	
			UNIT_TEST_POSITIVE(matrix2 == lookAtMatrix2, "Vector2D");	
			UNIT_TEST_POSITIVE(matrix3 == lookAtMatrix3, "Vector2D");	
			UNIT_TEST_POSITIVE(matrix4 == lookAtMatrix4, "Vector2D");	
			UNIT_TEST_POSITIVE(matrix5 == lookAtMatrix5, "Vector2D");	
			UNIT_TEST_POSITIVE(matrix6 == lookAtMatrix6, "Vector2D");	
			UNIT_TEST_POSITIVE(matrix7 == lookAtMatrix7, "Vector2D");	
			UNIT_TEST_POSITIVE(matrix8 == lookAtMatrix8, "Vector2D");	

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
