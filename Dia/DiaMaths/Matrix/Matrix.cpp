/*----------------------------------------------------------------------------------------------------------

	Implementation of our matrix type

----------------------------------------------------------------------------------------------------------*/

#include <stdio.h>
#include "Maths.h"
#include "Matrix.h"

//----------------------------------------------------------------------------------------------------------

Matrix& Matrix::operator+=(const Matrix& rOther)
{
	asm volatile("
		.set noreorder
		lqc2		vf20, 0x0(%0)		# Fetch first couple of chunks of each matrix
		lqc2		vf24, 0x0(%1)
		lqc2		vf21, 0x10(%0)
		lqc2		vf25, 0x10(%1)
		vadd.xyzw	vf20, vf20, vf24	# First add
		lqc2		vf22, 0x20(%0)		# Third fetch
		lqc2		vf26, 0x20(%1)
		vadd.xyzw	vf21, vf21, vf25	# Second add
		lqc2		vf23, 0x30(%0)		# Fourth fetch
		lqc2		vf24, 0x30(%1)
		vadd.xyzw	vf22, vf22, vf26
		vnop
		vadd.xyzw	vf23, vf23, vf24	# Fourth add
		sqc2		vf20, 0x0(%0)		# Start storing results
		sqc2		vf21, 0x10(%0)
		sqc2		vf22, 0x20(%0)
		sqc2		vf23, 0x30(%0)
		.set reorder
		" : : "r" (this), "r" (&rOther) : "vf20","vf21","vf22","vf23","vf24","vf25","vf26" );
	return *this;
}	

//----------------------------------------------------------------------------------------------------------

Matrix& Matrix::operator-=(const Matrix& rOther)
{
	asm volatile("
		.set noreorder
		lqc2		vf20, 0x0(%0)		# Fetch first couple of chunks of each matrix
		lqc2		vf24, 0x0(%1)
		lqc2		vf21, 0x10(%0)
		lqc2		vf25, 0x10(%1)
		vsub.xyzw	vf20, vf20, vf24	# First subtract
		lqc2		vf22, 0x20(%0)		# Third fetch
		lqc2		vf26, 0x20(%1)
		vsub.xyzw	vf21, vf21, vf25	# Second subtract
		lqc2		vf23, 0x30(%0)		# Fourth fetch
		lqc2		vf24, 0x30(%1)
		vsub.xyzw	vf22, vf22, vf26	# Third subtract
		vnop
		vsub.xyzw	vf23, vf23, vf24	# Fourth subtract
		sqc2		vf20, 0x0(%0)		# Start storing results
		sqc2		vf21, 0x10(%0)
		sqc2		vf22, 0x20(%0)
		sqc2		vf23, 0x30(%0)
		.set reorder
		" : : "r" (this), "r" (&rOther) : "vf20","vf21","vf22","vf23","vf24","vf25","vf26" );
	return *this;
}

//----------------------------------------------------------------------------------------------------------

// A = A * B
Matrix& Matrix::operator*=(const Matrix& rOther)
{
	asm volatile("
		.set noreorder
		lqc2	vf20, 0x0(%1)		# load rOther
		lqc2	vf21, 0x10(%1)
		lqc2	vf22, 0x20(%1)
		lqc2	vf23, 0x30(%1)

		lqc2	vf28, 0x00(%0)		# load this
		lqc2	vf29, 0x10(%0)
		lqc2	vf30, 0x20(%0)
		lqc2	vf31, 0x30(%0)

		vmulax.xyzw		ACCxyzw, vf20xyzw, vf28x
		vmadday.xyzw	ACCxyzw, vf21xyzw, vf28y
		vmaddaz.xyzw	ACCxyzw, vf22xyzw, vf28z
		vmaddw.xyzw		vf24xyzw, vf23xyzw, vf28w

		vmulax.xyzw		ACCxyzw, vf20xyzw, vf29x
		vmadday.xyzw	ACCxyzw, vf21xyzw, vf29y
		vmaddaz.xyzw	ACCxyzw, vf22xyzw, vf29z
		vmaddw.xyzw		vf25xyzw, vf23xyzw, vf29w

		vmulax.xyzw		ACCxyzw, vf20xyzw, vf30x
		vmadday.xyzw	ACCxyzw, vf21xyzw, vf30y
		vmaddaz.xyzw	ACCxyzw, vf22xyzw, vf30z
		vmaddw.xyzw		vf26xyzw, vf23xyzw, vf30w

		vmulax.xyzw		ACCxyzw, vf20xyzw, vf31x
		vmadday.xyzw	ACCxyzw, vf21xyzw, vf31y
		vmaddaz.xyzw	ACCxyzw, vf22xyzw, vf31z
		vmaddw.xyzw		vf27xyzw, vf23xyzw, vf31w

		sqc2	vf24, 0x00(%0)
		sqc2	vf25, 0x10(%0)
		sqc2	vf26, 0x20(%0)
		sqc2	vf27, 0x30(%0)
		.set reorder
		" : : "r" (this), "r" (&rOther) : "vf20","vf21","vf22","vf23",	// clobbered registers
										  "vf24","vf25","vf26","vf27",	// probably don't need this
										  "vf28","vf29","vf30","vf31" );
	return *this;
}

//----------------------------------------------------------------------------------------------------------

// multiply this by a scalar
Matrix& Matrix::operator*=(float fScalar)
{
	asm volatile("
		mfc1	$8, %0			# load scale factor from fpu
		qmtc2	$8, vf5			# store to vu0 vf5

		lqc2	vf1, 0x0(%1)	# load this matrix
		lqc2	vf2, 0x10(%1)
		lqc2	vf3, 0x20(%1)
		lqc2	vf4, 0x30(%1)

		vmulx.xyzw	vf1, vf1, vf5x	# scale the matrix
		vmulx.xyzw	vf2, vf2, vf5x
		vmulx.xyzw	vf3, vf3, vf5x
		vmulx.xyzw	vf4, vf4, vf5x

		sqc2	vf1, 0x0(%1)	# store to this
		sqc2	vf2, 0x10(%1)
		sqc2	vf3, 0x20(%1)
		sqc2	vf4, 0x30(%1)
		" : : "f" (fScalar), "r" (this) : "vf1","vf2","vf3","vf4","vf5" );
	return *this;
}

//----------------------------------------------------------------------------------------------------------

Matrix Matrix::operator+(const Matrix& rOther) const
{
	Matrix mResult;
	asm volatile("
		.set noreorder
		lqc2		vf20, 0x0(%0)		# Fetch first couple of chunks of each matrix
		lqc2		vf24, 0x0(%1)
		lqc2		vf21, 0x10(%0)
		lqc2		vf25, 0x10(%1)
		vadd.xyzw	vf20, vf20, vf24	# First add
		lqc2		vf22, 0x20(%0)		# Third fetch
		lqc2		vf26, 0x20(%1)
		vadd.xyzw	vf21, vf21, vf25	# Second add
		lqc2		vf23, 0x30(%0)		# Fourth fetch
		lqc2		vf24, 0x30(%1)
		vadd.xyzw	vf22, vf22, vf26	# Third add
		vnop
		vadd.xyzw	vf23, vf23, vf24	# Fourth add
		sqc2		vf20, 0x0(%2)		# Start storing results
		sqc2		vf21, 0x10(%2)
		sqc2		vf22, 0x20(%2)
		sqc2		vf23, 0x30(%2)
		.set reorder
		" : : "r" (this), "r" (&rOther), "r" (&mResult) : "vf20","vf21","vf22","vf23","vf24","vf25","vf26" );
	return mResult;
}

//----------------------------------------------------------------------------------------------------------

Matrix Matrix::operator-(const Matrix& rOther) const
{
	Matrix mResult;
	asm volatile("
		.set noreorder
		lqc2		vf20, 0x0(%0)		# Fetch first couple of chunks of each matrix
		lqc2		vf24, 0x0(%1)
		lqc2		vf21, 0x10(%0)
		lqc2		vf25, 0x10(%1)
		vsub.xyzw	vf20, vf20, vf24	# First subtract
		lqc2		vf22, 0x20(%0)		# Third fetch
		lqc2		vf26, 0x20(%1)
		vsub.xyzw	vf21, vf21, vf25	# Second subtract
		lqc2		vf23, 0x30(%0)		# Fourth fetch
		lqc2		vf24, 0x30(%1)
		vsub.xyzw	vf22, vf22, vf26	# Third subtract
		vnop
		vsub.xyzw	vf23, vf23, vf24	# Fourth subtract
		sqc2		vf20, 0x0(%2)		# Start storing results
		sqc2		vf21, 0x10(%2)
		sqc2		vf22, 0x20(%2)
		sqc2		vf23, 0x30(%2)
		.set reorder
		" : : "r" (this), "r" (&rOther), "r" (&mResult) : "vf20","vf21","vf22","vf23","vf24","vf25","vf26" );
	return mResult;
}

//----------------------------------------------------------------------------------------------------------

// R = A * B
Matrix Matrix::operator*(const Matrix& rOther) const
{
	Matrix mResult;
	asm volatile ("
		.set noreorder
		lqc2	vf20, 0x00(%1)		# load rOther
		lqc2	vf21, 0x10(%1)
		lqc2	vf22, 0x20(%1)
		lqc2	vf23, 0x30(%1)

		lqc2	vf28, 0x00(%0)		# load this
		lqc2	vf29, 0x10(%0)
		lqc2	vf30, 0x20(%0)
		lqc2	vf31, 0x30(%0)

		vmulax.xyzw		ACCxyzw, vf20xyzw, vf28x
		vmadday.xyzw	ACCxyzw, vf21xyzw, vf28y
		vmaddaz.xyzw	ACCxyzw, vf22xyzw, vf28z
		vmaddw.xyzw		vf24xyzw, vf23xyzw, vf28w

		vmulax.xyzw		ACCxyzw, vf20xyzw, vf29x
		vmadday.xyzw	ACCxyzw, vf21xyzw, vf29y
		vmaddaz.xyzw	ACCxyzw, vf22xyzw, vf29z
		vmaddw.xyzw		vf25xyzw, vf23xyzw, vf29w

		vmulax.xyzw		ACCxyzw, vf20xyzw, vf30x
		vmadday.xyzw	ACCxyzw, vf21xyzw, vf30y
		vmaddaz.xyzw	ACCxyzw, vf22xyzw, vf30z
		vmaddw.xyzw		vf26xyzw, vf23xyzw, vf30w

		vmulax.xyzw		ACCxyzw, vf20xyzw, vf31x
		vmadday.xyzw	ACCxyzw, vf21xyzw, vf31y
		vmaddaz.xyzw	ACCxyzw, vf22xyzw, vf31z
		vmaddw.xyzw		vf27xyzw, vf23xyzw, vf31w

		sqc2	vf24, 0x00(%2)
		sqc2	vf25, 0x10(%2)
		sqc2	vf26, 0x20(%2)
		sqc2	vf27, 0x30(%2)
		.set reorder
		" : : "r" (this), "r" (&rOther), "r" (&mResult) : "vf20","vf21","vf22","vf23",	// clobbered registers
														  "vf24","vf25","vf26","vf27",	// probably don't need this
														  "vf28","vf29","vf30","vf31" );
	return mResult;
}

//----------------------------------------------------------------------------------------------------------

Matrix Matrix::operator*(float fScalar) const
{
	Matrix mResult;
	asm volatile("
		mfc1	$8, %0			# load scale factor from fpu
		qmtc2	$8, vf5			# store to vu0 vf5

		lqc2	vf1, 0x0(%1)	# load this matrix
		lqc2	vf2, 0x10(%1)
		lqc2	vf3, 0x20(%1)
		lqc2	vf4, 0x30(%1)

		vmulx.xyzw	vf1, vf1, vf5x	# scale the matrix
		vmulx.xyzw	vf2, vf2, vf5x
		vmulx.xyzw	vf3, vf3, vf5x
		vmulx.xyzw	vf4, vf4, vf5x

		sqc2	vf1, 0x0(%2)	# store to this
		sqc2	vf2, 0x10(%2)
		sqc2	vf3, 0x20(%2)
		sqc2	vf4, 0x30(%2)
		" : : "f" (fScalar), "r" (this), "r" (&mResult) : "vf1","vf2","vf3","vf4","vf5" );	
	return mResult;
}

//----------------------------------------------------------------------------------------------------------

void Matrix::Transpose(void)
{
	asm volatile("
		.set noreorder
		
		lq 		$24,  0(%0)		# $24 = ABCD
		lq 		$25, 16(%0)		# $25 = EFGH
		lq 		$10, 32(%0)		# $10 = IJKL
		lq 		$11, 48(%0)		# $11 = MNOP

		PEXTLW 	$12, $25, $24	# $12 = AEBF
		PEXTUW 	$13, $25, $24	# $13 = CGDH
		PEXTLW 	$14, $11, $10	# $14 = IMJN
		PEXTUW 	$15, $11, $10	# $15 = KOLP

		PCPYLD 	$24, $14, $12	# $24 = AEIM
		PCPYUD 	$25, $12, $14	# $25 = BFJN
		PCPYLD 	$10, $15, $13	# $10 = CGKO
		PCPYUD 	$11, $13, $15	# $11 = DHLP

		sq 		$24,  0(%0)
		sq 		$25, 16(%0)
		sq 		$10, 32(%0)
		sq 		$11, 48(%0)
		
		.set reorder

	" : : "r" (this) : "$24", "$25", "$10", "$11", "$12", "$13", "$14", "$15" );
}

//----------------------------------------------------------------------------------------------------------

Matrix Matrix::Transpose(void) const
{
	Matrix mResult;
	asm volatile("
		.set noreorder
		
		lq 		$24,  0(%0)		# $24 = ABCD
		lq 		$25, 16(%0)		# $25 = EFGH
		lq 		$10, 32(%0)		# $10 = IJKL
		lq 		$11, 48(%0)		# $11 = MNOP

		PEXTLW 	$12, $25, $24	# $12 = AEBF
		PEXTUW 	$13, $25, $24	# $13 = CGDH
		PEXTLW 	$14, $11, $10	# $14 = IMJN
		PEXTUW 	$15, $11, $10	# $15 = KOLP

		PCPYLD 	$24, $14, $12	# $24 = AEIM
		PCPYUD 	$25, $12, $14	# $25 = BFJN
		PCPYLD 	$10, $15, $13	# $10 = CGKO
		PCPYUD 	$11, $13, $15	# $11 = DHLP

		sq 		$24,  0(%1)
		sq 		$25, 16(%1)
		sq 		$10, 32(%1)
		sq 		$11, 48(%1)
		
		.set reorder

	" : : "r" (this), "r" (&mResult) : "$24", "$25", "$10", "$11", "$12", "$13", "$14", "$15" );
	return mResult;
}

//----------------------------------------------------------------------------------------------------------

// do we need the determinant of a 4x4 matrix?
float Matrix::Det4x4(void) const
{
	float a1 = m[0][0];	
	float b1 = m[0][1];	
	float c1 = m[0][2];	
	float d1 = m[0][3];	
	
	float a2 = m[1][0];	
	float b2 = m[1][1];	
	float c2 = m[1][2];		
	float d2 = m[1][3];	
	
	float a3 = m[2][0];	
	float b3 = m[2][1];	
	float c3 = m[2][2];	
	float d3 = m[2][3];	
	
	float a4 = m[3][0];	
	float b4 = m[3][1];	
	float c4 = m[3][2];	
	float d4 = m[3][3];	

    return  a1 * Det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4) -
			b1 * Det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4) +
			c1 * Det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4) -
			d1 * Det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
}

//----------------------------------------------------------------------------------------------------------

float Matrix::Det3x3(void) const
{
	return Det3x3(m[0][0],m[0][1],m[0][2],
				  m[1][0],m[1][1],m[1][2],
				  m[2][0],m[2][1],m[2][2]);
}

//----------------------------------------------------------------------------------------------------------

float Matrix::Det2x2(void) const
{
	return Det2x2(m[0][0],m[0][1],m[1][0],m[1][1]);
}

//----------------------------------------------------------------------------------------------------------

float Matrix::Det3x3(float a1, float a2, float a3, float b1, float b2, float b3, float c1, float c2, float c3) const
{
	return ( (a1 * Det2x2(b2, b3, c2, c3)) - (b1 * Det2x2(a2, a3, c2, c3)) + (c1 * Det2x2(a2, a3, b2, b3)) );
}

//----------------------------------------------------------------------------------------------------------

// For full inverse you need to calculate the determinants.
float Matrix::Det2x2(float a, float b, float c, float d) const
{
	return (a * d - b * c);
}

//----------------------------------------------------------------------------------------------------------

// Hmmm, not sure about this
void Matrix::Adjoint(void)
{
	
	float a1 = m[0][0];	
	float b1 = m[0][1];	
	float c1 = m[0][2];	
	float d1 = m[0][3];	
	
	float a2 = m[1][0];	
	float b2 = m[1][1];	
	float c2 = m[1][2];	
	float d2 = m[1][3];	
	
	float a3 = m[2][0];	
	float b3 = m[2][1];	
	float c3 = m[2][2];	
	float d3 = m[2][3];	
	
	float a4 = m[3][0];	
	float b4 = m[3][1];	
	float c4 = m[3][2];	
	float d4 = m[3][3];	

	// row column labeling reversed since we transpose rows & columns
	m[0][0]	=  Det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
	m[0][1]	= -Det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
	m[0][2]	=  Det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
	m[0][3]	= -Det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
	
	m[1][0]	= -Det3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
	m[1][1]	=  Det3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
	m[1][2]	= -Det3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
	m[1][3]	=  Det3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4);
	
	m[2][0]	=  Det3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
	m[2][1]	= -Det3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
	m[2][2]	=  Det3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
	m[2][3]	= -Det3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4);
	
	m[3][0]	= -Det3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
	m[3][1]	=  Det3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
	m[3][2]	= -Det3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
	m[3][3]	=  Det3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

Matrix Matrix::Adjoint(void) const
{
	Matrix result;
	
	float a1 = m[0][0];	
	float b1 = m[0][1];	
	float c1 = m[0][2];	
	float d1 = m[0][3];	
	
	float a2 = m[1][0];	
	float b2 = m[1][1];	
	float c2 = m[1][2];	
	float d2 = m[1][3];	
	
	float a3 = m[2][0];	
	float b3 = m[2][1];	
	float c3 = m[2][2];	
	float d3 = m[2][3];	
	
	float a4 = m[3][0];	
	float b4 = m[3][1];	
	float c4 = m[3][2];	
	float d4 = m[3][3];	

	// row column labeling reversed since we transpose rows & columns
	result.m[0][0]	=  Det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
	result.m[0][1]	= -Det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
	result.m[0][2]	=  Det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
	result.m[0][3]	= -Det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
	
	result.m[1][0]	= -Det3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
	result.m[1][1]	=  Det3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
	result.m[1][2]	= -Det3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
	result.m[1][3]	=  Det3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4);
	
	result.m[2][0]	=  Det3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
	result.m[2][1]	= -Det3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
	result.m[2][2]	=  Det3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
	result.m[2][3]	= -Det3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4);
	
	result.m[3][0]	= -Det3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
	result.m[3][1]	=  Det3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
	result.m[3][2]	= -Det3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
	result.m[3][3]	=  Det3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
	
	return result;
}


//----------------------------------------------------------------------------------------------------------

// TODO - check this isn't horribly rubbish!
void Matrix::Invert(void)
{
	*this = Inverse();
}


//----------------------------------------------------------------------------------------------------------
//
//	Thread safe version of the matrix Inverse function.
//
Matrix Matrix::InverseNoVu0(void) const
{	
	Matrix result 	= Adjoint();
	
	result.Transpose();
	
	float fScalar 	= 1.0f/Det4x4();
	
	for ( u_int i=0; i<4; i++ )
	{
		for ( u_int j=0; j<4; j++ )
		{
			result.m[i][j] *= fScalar;
		}
	}
		
	return result;
}
//----------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------
Matrix Matrix::Inverse(void) const
{
	Matrix mResult;
	asm volatile("
		.set noreorder
		
		/* Set up constants vf16 and vf17 */

		VMULx.xyzw vf16, vf0, vf0x  # clear vf16
		VMULx.xyzw vf17, vf0, vf0x  # and vf17
		VNOP						# rest here awhile,
		VNOP						# yawn, zzzz
		VSUBw.xz vf16, vf16, vf0w   # set the -1's
		VSUBw.yw vf17, vf17, vf0w   # here,
		VADDw.yw vf16, vf16, vf0w   # and the 1's
		VADDw.xz vf17, vf17, vf0w   # as well.

		/* Get input matrix from memory (from the 'this' pointer; implicit first argument) */
		LQC2 vf2, 16( %0 )
		LQC2 vf1,  0( %0 )
		LQC2 vf3, 32( %0 )
		LQC2 vf4, 48( %0 )
	
		/* 
		// vf1  - vf4  is our input matrix.
		// vf5  - vf8  are our 2x2 cofactors per iteration,
		//             multiplied by the corresponding term in the 3x3 matrices,
		// vf11 - vf14 is the store for the sum of cofactors,
		// vf21 - vf24 are the continually rotating versions of vf1-vf4
		//             (we need to rotate to get the cross product for each set of three axis)
		// vf16 , vf17 are constants which make up the sign matrix
		//
		// +1 -1 +1 -1 = vf17
		// -1 +1 -1 +1 = vf16
		// +1 -1 +1 -1 = vf17
		// -1 +1 -1 +1 = vf16
		//
		// but because there are only two unique rows we only need two vectors.

		// Use outer product on XYZ to get three 2x2 determinants per pair of instructions
		// (twelve 2x2's altogether) 
		*/
	
		VOPMULA ACC, vf1, vf2
		VOPMSUB vf8, vf2, vf1
		VOPMULA ACC, vf2, vf3
		VOPMSUB vf5, vf3, vf2
		VOPMULA ACC, vf3, vf4
		VOPMSUB vf6, vf4, vf3
		VOPMULA ACC, vf4, vf1
		VOPMSUB vf7, vf1, vf4

		/* Now multiply by the other row to get scaled determinants, */

		VMUL.xyz vf8, vf8, vf3
		VMUL.xyz vf5, vf5, vf4
		VMUL.xyz vf6, vf6, vf1
		VMUL.xyz vf7, vf7, vf2

		/* Rotate so WXY become the next XYZ (we can only do outer products on XYZ) */

		VMR32 vf23, vf3
		VMR32 vf24, vf4
		VMR32 vf21, vf1
		VMR32 vf22, vf2

		/* Sum the terms (can we do this any faster, somehow?) */

		VADDy.x vf8, vf8, vf8
		VADDy.x vf5, vf5, vf5
		VADDx.y vf6, vf6, vf6
		VADDx.z vf7, vf7, vf7
		VADDz.x vf8, vf8, vf8
		VADDz.x vf14, vf5, vf5
		VADDz.y vf14, vf6, vf6
		VADDy.z vf14, vf7, vf7
		VMULx.w vf14, vf0, vf8 	/* multiply vf8 by 1.0 to move from X to W */

		VOPMULA ACC, vf21, vf22	/* Now a load more outer products, */
		VOPMSUB vf8, vf22, vf21
		VOPMULA ACC, vf22, vf23
		VOPMSUB vf5, vf23, vf22
		VOPMULA ACC, vf23, vf24
		VOPMSUB vf6, vf24, vf23
		VOPMULA ACC, vf24, vf21
		VOPMSUB vf7, vf21, vf24

		VMUL.xyz vf8, vf8, vf23
		VMUL.xyz vf5, vf5, vf24
		VMUL.xyz vf6, vf6, vf21
		VMUL.xyz vf7, vf7, vf22

		VMR32 vf23, vf23
		VMR32 vf24, vf24
		VMR32 vf21, vf21
		VMR32 vf22, vf22

		VADDy.x vf8, vf8, vf8
		VADDy.x vf5, vf5, vf5
		VADDx.y vf6, vf6, vf6
		VADDx.z vf7, vf7, vf7
		VADDz.x vf8, vf8, vf8
		VADDz.x vf11, vf5, vf5
		VADDz.y vf11, vf6, vf6
		VADDy.z vf11, vf7, vf7
		VMULx.w vf11, vf0, vf8

		VOPMULA ACC, vf21, vf22 /* And again, */
		VOPMSUB vf8, vf22, vf21
		VOPMULA ACC, vf22, vf23
		VOPMSUB vf5, vf23, vf22
		VOPMULA ACC, vf23, vf24
		VOPMSUB vf6, vf24, vf23
		VOPMULA ACC, vf24, vf21
		VOPMSUB vf7, vf21, vf24

		VMUL.xyz vf8, vf8, vf23
		VMUL.xyz vf5, vf5, vf24
		VMUL.xyz vf6, vf6, vf21
		VMUL.xyz vf7, vf7, vf22

		VMR32 vf23, vf23
		VMR32 vf24, vf24
		VMR32 vf21, vf21
		VMR32 vf22, vf22

		VADDy.x vf8, vf8, vf8
		VADDy.x vf5, vf5, vf5
		VADDx.y vf6, vf6, vf6
		VADDx.z vf7, vf7, vf7
		VADDz.x vf8, vf8, vf8
		VADDz.x vf12, vf5, vf5
		VADDz.y vf12, vf6, vf6
		VADDy.z vf12, vf7, vf7
		VMULx.w vf12, vf0, vf8

		VOPMULA ACC, vf21, vf22 /* And for the final time. (no VMR32 this time, we don't need it) */
		VOPMSUB vf8, vf22, vf21
		VOPMULA ACC, vf22, vf23
		VOPMSUB vf5, vf23, vf22
		VOPMULA ACC, vf23, vf24
		VOPMSUB vf6, vf24, vf23
		VOPMULA ACC, vf24, vf21
		VOPMSUB vf7, vf21, vf24

		VMUL.xyz vf8, vf8, vf23
		VMUL.xyz vf5, vf5, vf24
		VMUL.xyz vf6, vf6, vf21
		VMUL.xyz vf7, vf7, vf22

		VADDy.x vf8, vf8, vf8
		VADDy.x vf5, vf5, vf5
		VADDx.y vf6, vf6, vf6
		VADDx.z vf7, vf7, vf7
		VADDz.x vf8, vf8, vf8
		VADDz.x vf13, vf5, vf5
		VADDz.y vf13, vf6, vf6
		VADDy.z vf13, vf7, vf7
		VMULx.w vf13, vf0, vf8

		/* Multiply element M(n,m) by -1 ** (n+m) to get adjoint. */

		VMUL.xyzw vf14, vf14, vf17
		VMUL.xyzw vf11, vf11, vf16
		VMUL.xyzw vf12, vf12, vf17
		VMUL.xyzw vf13, vf13, vf16

		/*  Do first row of matrix * adjoint to find what determinant should be. */

		VMULAx.xyzw  ACC, vf1, vf11x
		VMADDAy.xyzw ACC, vf2, vf11y
		VMADDAz.xyzw ACC, vf3, vf11z
		VMADDw.xyzw vf5, vf4, vf11w

		/* vf5x should now contain the determinant. */
	
		VNOP
		VNOP
		VNOP

		VDIV Q, vf0w, vf5x		/* get the reciprocal of the determinant here, */
		VNOP
		VWAITQ					/* 7-cycle divide - such primitive hardware. */

		/* so divide by the determinant. (multiply by its reciprocal) */

		VMULQ.xyzw vf11, vf11, Q
		VMULQ.xyzw vf12, vf12, Q
		VMULQ.xyzw vf13, vf13, Q
		VMULQ.xyzw vf14, vf14, Q
	
		/* Store relative to a1 pointer, which is our first explicit argument, the destination matrix. */

		SQC2 vf11,  0(%1)
		SQC2 vf12, 16(%1)
		SQC2 vf13, 32(%1)
		SQC2 vf14, 48(%1)
		
		.set reorder
			
	" : : "r" (this), "r" (&mResult) );
	return mResult;
}


//----------------------------------------------------------------------------------------------------------

void Matrix::SetRotX(float fAngle)
{
	float sin = Maths::AccurateSin(fAngle);
	float cos = Maths::AccurateCos(fAngle);

	m[0][0]	= 1.0f;
	m[1][0]	= 0.0f;
	m[2][0]	= 0.0f;
	m[3][0]	= 0.0f;

	m[0][1]	= 0.0f;
	m[1][1]	= cos;
	m[2][1]	= -sin;
	m[3][1]	= 0.0f;

	m[0][2] = 0.0f;
	m[1][2]	= sin;
	m[2][2]	= cos;
	m[3][2]	= 0.0f;

	m[0][3]	= 0.0f;
	m[1][3]	= 0.0f;
	m[2][3]	= 0.0f;
	m[3][3]	= 1.0f;
}

//----------------------------------------------------------------------------------------------------------

void Matrix::SetRotY(float fAngle)
{
	float sin = Maths::AccurateSin(fAngle);
	float cos = Maths::AccurateCos(fAngle);
	
	m[0][0] = cos;
	m[1][0] = 0.0f;
	m[2][0] = sin;
	m[3][0] = 0.0f;
	
	m[0][1] = 0.0f;
	m[1][1] = 1.0f;
	m[2][1] = 0.0f;
	m[3][1] = 0.0f;
	
	m[0][2] = -sin;
	m[1][2] = 0.0f;
	m[2][2] = cos;
	m[3][2] = 0.0f;
	
	m[0][3] = 0.0f;
	m[1][3] = 0.0f;
	m[2][3] = 0.0f;
	m[3][3] = 1.0f;
}

//----------------------------------------------------------------------------------------------------------

void Matrix::SetRotZ(float fAngle)
{
	float sin = Maths::AccurateSin(fAngle);
	float cos = Maths::AccurateCos(fAngle);
	
	m[0][0] = cos;
	m[1][0] = -sin;
	m[2][0] = 0.0f;
	m[3][0] = 0.0f;
	
	m[0][1] = sin;
	m[1][1] = cos;
	m[2][1] = 0.0f;
	m[3][1] = 0.0f;
	
	m[0][2] = 0.0f;
	m[1][2] = 0.0f;
	m[2][2] = 1.0f;
	m[3][2] = 0.0f;
	
	m[0][3] = 0.0f;
	m[1][3] = 0.0f;
	m[2][3] = 0.0f;
	m[3][3] = 1.0f;
}

//----------------------------------------------------------------------------------------------------------

// builds a rotation matrix, rotating around x, then y, then z
void Matrix::SetRotXYZ(const Vector& vAngles)
{
	float	cosines[4], sines[4];
	Maths::AccurateSinCos( vAngles.x, sines[0], cosines[0] );
	Maths::AccurateSinCos( vAngles.y, sines[1], cosines[1] );
	Maths::AccurateSinCos( vAngles.z, sines[2], cosines[2] );
	asm volatile("
		.set noreorder
		lq		$t0,0x0(%0)			# Fetch sines
		lq		$t1,0x0(%1)			# Fetch cosines.
		pxor	$t2,$t2,$t2			# Clear t2
		pxor	$t6,$t6,$t6			# Clear t6
		pnor	$t2,$t2,$zero		# All bits set
		psrlw	$t3,$t2,1			# Top bit not set in t3
		pxor	$t2,$t2,$t3			# Top bit set in t2
		pxor	$t3,$t0,$t2			# t3 = (..,-sZ,-sY,-sX)
		pexcw	$t3,$t3				# t3 = (..,-sY,-sZ,-sX)
		addiu	$t6,$zero,0xffff	# 1st sixteen bits set
		dsll	$t7,$t6,0x10		# t7 second sixteen bits set
		or		$t6,$t6,$t7			# t6 : (0,0,0,F)
		or		$t7,$t6,$t6			# t7 : (0,0,0,F)
		prot3w	$t6,$t6				# t6 : (0,F,0,0)
		pand	$t3,$t3,$t6			# t3 : (0,-sY,0,0)
		
		mtc1	$t0,$f1				# $f1 = sX
		mtc1	$t1,$f2				# $f2 = cX
		prot3w	$t0,$t0				# t0 = (sX,sZ,sY)
		prot3w	$t1,$t1				# t1 = (cX,cZ,cY)
		mtc1	$t0,$f3				# $f3 = sY
		mtc1	$t1,$f4				# $f4 = cY
		mul.s	$f5,$f2,$f3			# $f5 = cxXsy
		mul.s	$f6,$f1,$f3			# $f6 = sxXsy
		prot3w	$t0,$t0				# t0 = (sY,sX,sZ)
		prot3w	$t1,$t1				# t1 = (cY,cX,cZ)
		mtc1	$t0,$f7				# $f7 = sZ
		mtc1	$t1,$f8				# $f8 = cZ
		mul.s	$f9,$f4,$f8			# $f9 = cY.cZ
		mul.s	$f10,$f4,$f7		# $f10 = cY.sZ

		pxor	$t4,$t4,$t4
		pxor	$t5,$t5,$t5
		mfc1	$t4,$f9				# t4 = cY.cZ
		mfc1	$t5,$f10			# t5 = cY.sZ

		# Clean any sign nonsense out of bits > 32 in t4,t5
		pand	$t4,$t4,$t7
		pand	$t5,$t5,$t7
		pextlw	$t5,$t5,$t4			# t5 = (0,0,cY.sZ,cY.cZ)
		por		$t5,$t3,$t5			# t5 = (0,-sY,cY.sZ,cY.cZ)
		sq		$t5,0x0(%2)			# First row complete

		mula.s	$f6,$f8				# ACC = sxXsy.cZ
		msub.s	$f9,$f7,$f2			# $f9 = sxXsy.cZ - sZ.cX
		mula.s	$f6,$f7				# ACC = sxXsy.sZ
		madd.s	$f10,$f2,$f8		# $f10 = sxXsy.sZ + cX.cZ
		mul.s	$f11,$f1,$f4		# $f11 = sX.cY
		pxor	$t4,$t4,$t5
		pxor	$t5,$t5,$t5
		pxor	$t6,$t6,$t6
		mfc1	$t4,$f9				# t4 = (0,0,0,$f9)
		mfc1	$t5,$f10			# t5 = (0,0,0,$f10)
		mfc1	$t6,$f11			# t6 = (0,0,0,$f11)
		
		# Clean up signs in t4,t5,t6
		pand	$t4,$t4,$t7
		pand	$t5,$t5,$t7
		pand	$t6,$t6,$t7
		pextlw	$t5,$t5,$t4			# t5 = (0,0,$f10,$f9)
		prot3w	$t6,$t6				# t6 = (0,$f11,0,0)
		por		$t5,$t5,$t6			# t5 = (0,$f11,$f10,$f9)
		sq		$t5,0x10(%2)		# Second row complete

		mula.s	$f5,$f8				# ACC = cxXsy.cZ
		madd.s	$f9,$f1,$f7			# $f9 = cxXsy.cZ + sX.sZ
		mula.s	$f5,$f7				# ACC = csXsy.sZ
		msub.s	$f10,$f1,$f8		# $f10 = csXsy.sZ - sX.cZ
		mul.s	$f11,$f2,$f4		# $f11 = cX.cY
		pxor	$t4,$t4,$t4
		pxor	$t5,$t5,$t5
		pxor	$t6,$t6,$t6
		mfc1	$t4,$f9
		mfc1	$t5,$f10
		mfc1	$t6,$f11

		# Clean up signs in t4,t5,t6
		pand	$t4,$t4,$t7
		pand	$t5,$t5,$t7
		pand	$t6,$t6,$t7
		pextlw	$t5,$t5,$t4
		prot3w	$t6,$t6
		por		$t5,$t5,$t6
		sq		$t5,0x20(%2)		# Third row complete

		pxor	$t0,$t0
		lui		$t0,0x3f80			# t0 (0,0,0,1.0f)
		prot3w	$t0,$t0
		prot3w	$t0,$t0
		pcpyld	$t0,$t0,$zero		# t0 (1.0f,0,0,0)
		sq		$t0,0x30(%2)		# Final row complete.
		.set reorder
	
		" : : "r"(sines), "r"(cosines), "r"(this) : "t0","t1","t2","t3","t4","t5","t6","t7" );
}

//----------------------------------------------------------------------------------------------------------

// don't know why we'd need more than the xyz ordering
// but since zxy was already defined, i've kept it
// HOWEVER i haven't checked this correct!
void Matrix::SetRotZXY(const Vector& rAngles)
{
	float	cx,cy,cz,sx,sy,sz;

	Maths::AccurateSinCos(rAngles.x, sx, cx);
	Maths::AccurateSinCos(rAngles.y, sy, cy);
	Maths::AccurateSinCos(rAngles.z, sz, cz);

	float szXsx	= sz*sx;
	float sxXcz	= sx*cz;
	
	m[0][0] = cy*cz - szXsx*sy;
	m[0][1] = sz*cx;
	m[0][2] = -cz*sy - szXsx*cy;
	m[0][3] = 0.0f;
			    	  
	m[1][0] = -sz*cy - sxXcz*sy;
	m[1][1] = cz*cx;
	m[1][2] = sy*sz - sxXcz*cy;
	m[1][3] = 0.0f;
			    	  
	m[2][0] = cx*sy;
	m[2][1] = sx;
	m[2][2] = cx*cy;
	m[2][3] = 0.0f;
			    	  
	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = 0.0f;
	m[3][3] = 1.0f;
}

//----------------------------------------------------------------------------------------------------------

void Matrix::SetMirror( const Vector &rNormal, const float &fDistanceToPlane )
{
/*
Mirror Matrix
-------------

| 1-2*nx*nx    -2*nx*ny     -2*nx*nz     0   |

|  -2*ny*nx   1-2*ny*ny     -2*ny*nz     0   |

|  -2*nz*nx    -2*nz*ny    1-2*nz*nz     0   |

|   -2*nx*k     -2*ny*k    -2*nz*k       1   |
*/

	m[0][0]	= 1.0f-2.0f*rNormal.x*rNormal.x;
	m[0][1]	= -2.0f*rNormal.x*rNormal.y;
	m[0][2]	= -2.0f*rNormal.x*rNormal.z;
	m[0][3]	= 0.0f;

	m[1][0]	= -2.0f*rNormal.y*rNormal.x;
	m[1][1]	= 1.0f-2.0f*rNormal.y*rNormal.y;
	m[1][2]	= -2.0f*rNormal.y*rNormal.z;
	m[1][3]	= 0.0f;

	m[2][0]	= -2.0f*rNormal.z*rNormal.x;
	m[2][1]	= -2.0f*rNormal.z*rNormal.y;
	m[2][2]	= 1.0f-2.0f*rNormal.z*rNormal.z;
	m[2][3]	= 0.0f;

	m[3][0]	= -2.0f*rNormal.x*fDistanceToPlane;
	m[3][1]	= -2.0f*rNormal.y*fDistanceToPlane;
	m[3][2]	= -2.0f*rNormal.z*fDistanceToPlane;
	m[3][3]	= 1.0f;
}

//----------------------------------------------------------------------------------------------------------

// TODO - check this actually works!
void Matrix::SetOrientFromDir(Vector& rDir)
{
	Vector	xV;
	Vector	yV;

	rDir.Negate();
	
	xV.Set(1.0f, 0.0f, 0.0f, 0.0f);			// world x

	if (rDir == xV)
	{
		yV.Set(0.0f, 1.0f, 0.0f, 0.0f);		// world y
		xV = rDir.Cross(yV);				// generate local x
		yV = xV.Cross(rDir);				// generate local y
	}
	else
	{
		yV = rDir.Cross(xV);				// generate local y
		xV = yV.Cross(rDir);				// generate local x
	}

	rDir.Negate();
	
	m[0][0]	= xV.x;
	m[1][0]	= xV.y;
	m[2][0]	= xV.z;
	
	m[0][1]	= yV.x;
	m[1][1]	= yV.y;
	m[2][1]	= yV.z;
	
	m[0][2] = rDir.x;
	m[1][2]	= rDir.y;
	m[2][2]	= rDir.z;
}
//----------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------
//
//	Sets a martrix froma position and direction
//
void Matrix::SetPositionDirection( const Vector& rvPosition, const Vector& rvDirection )
{
	Vector xAxis;
	Vector yAxis( 0.01f, 1.0f, 0.0f, 0.0f );
	Vector zAxis( rvDirection );

	xAxis = yAxis.Cross( zAxis );
	yAxis = zAxis.Cross( xAxis );

	*(Vector*)&m[0] = xAxis.GetNormalised();
	*(Vector*)&m[1] = yAxis.GetNormalised();
	*(Vector*)&m[2] = zAxis.GetNormalised();
	*(Vector*)&m[3] = rvPosition;

	m[0][3] = m[1][3] = m[2][3] = 0.0f;
	m[3][3] = 1.0f;	
}
//----------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------

void Matrix::SetOrientFromUpAndDir(const Vector& vUp, const Vector& vDir )
{
	Vector	zAxis( vDir );
	zAxis.Normalise();
	
	Vector	xAxis  = zAxis.Cross( vUp );
	xAxis.Normalise();
	
	Vector	yAxis  = xAxis.Cross( zAxis );
	yAxis.Normalise();
	
	SetIdentity();
	SetXAxis( xAxis );
	SetYAxis( yAxis );
	SetZAxis( zAxis );
}

//----------------------------------------------------------------------------------------------------------


/*
SetOrientFromQuat - VU0 macro-mode version:

2*(ss+xx) - 1	xy + zs			zx - ys			0
xy - zs			2*(ss+yy) - 1	zy + xs			0
zx + ys			zy - xs			2*(ss+zz) - 1	0
0				0				0				1

;// given vector vfQuat containing x, y, z, s
;// calculate rotation matrix
;// based on algorithm in Matrix.cpp
*/

#define vfDiagonal 	vf1
#define vfQuat		vf2
#define vfQuatSq   	vf3
#define vfOuter1   	vf4
#define vfOuter2   	vf5
#define vfRow1	   	vf6
#define vfRow2	   	vf7
#define vfRow3		vf8


asm void Matrix::SetOrientFromQuat( const Quaternion& quat )
{
	LQC2			vfQuat,		0(a1)

	VADDw.xyz 		vfDiagonal, vf0,		vfQuat		;// s, s, s

	VMUL.xyzw  		vfDiagonal, vfDiagonal, vfDiagonal 	;// ss, ss, ss
	VMUL.xyzw  		vfQuatSq,   vfQuat,     vfQuat		;// xx, yy, zz

	VADD.xyz   		vfDiagonal, vfDiagonal, vfQuatSq	;// (ss+xx, ss+yy, ss+zz)
	VADD.xyz 		vfDiagonal, vfDiagonal, vfDiagonal 	;// 2 * above

	VSUBw.xyz		vfDiagonal, vfDiagonal, vf0w		;// (2 * (ss+xx,ss+yy,...))-1

	VOPMULA.xyz		ACCxyz,   	vfQuat, 	vfQuat		;// (yz, zx, xy)
	VMSUBw.xyz  	vfOuter1, 	vfQuat, 	vfQuat		;// (yz - xs, zx - ys, xy - zs)
	VMADDw.xyz  	vfOuter2, 	vfQuat, 	vfQuat		;// (yz + xs, zx + ys, xy + zs)

	VADD.xyz		vfOuter1, 	vfOuter1, 	vfOuter1	;// *2
	VADD.xyz		vfOuter2, 	vfOuter2, 	vfOuter2	;// *2

// now we have:

// A: 2(ss+xx)-1	B: 2(ss+yy)-1	C: 2(ss+zz)-1		vfDiagonal
// D: (yz-xs)		E: (zx-ys)		F: (xy-zs)			vfOuter1
// G: (yz+xs)		H: (zx+ys)		I: (xy+zs)			vfOuter2

// and I want:

//	A				I				E			0
//	F				B				G			0
//	H				D				C			0
//  0				0				0			1

	VSUB.w			vfDiagonal, vf0, 		vf0			;// set diagonal w to 0
	
	VMULw.xyzw		vfRow1,		vfDiagonal, vf0			;// initialise rows to diagonal,
	VMULw.xyzw		vfRow2, 	vfDiagonal, vf0			;//
	VMULw.xyzw		vfRow3,		vfDiagonal, vf0			;// 

	VADDy.z			vfRow1,		vf0,		vfOuter1	;// 0,1 : copy across other values
	VADDz.y			vfRow1,		vf0,		vfOuter2	;// 0,2
	VADDz.x			vfRow2, 	vf0,		vfOuter1	;// 1,0
	VADDx.z			vfRow2,		vf0,		vfOuter2	;// 1,2
	VADDx.y			vfRow3,		vf0,		vfOuter1	;// 2,0
	VADDy.x			vfRow3,		vf0,		vfOuter2	;// 2,1

	SQC2			vfRow1,		0(a0)
	SQC2			vfRow2,		16(a0)
	SQC2			vfRow3,		32(a0)
	SQC2			vf0,		48(a0)

	jr ra
	NOP
}


#if 0
void Matrix::SetOrientFromQuat(const Quaternion& Quat)
{
	float xy,zx,zy,zs,xs,ys,ss,dd;

	ss = Quat.GetS();
	ss *= ss;
	dd = Quat.GetX();
	dd *= dd;
	m[0][0] = 2.0f *(ss + dd) - 1.0f;

	dd = Quat.GetY();
	dd *= dd;
	m[1][1] = 2.0f*(ss + dd) - 1.0f;

	dd = Quat.GetZ();
	dd *= dd;
	m[2][2] = 2.0f*(ss + dd) - 1.0f;
	
	xy = 2.0f*Quat.GetX()*Quat.GetY();
	zs = 2.0f*Quat.GetZ()*Quat.GetS();

	m[1][0] = xy - zs;
	m[0][1] = xy + zs;

	zy = 2.0f*Quat.GetZ()*Quat.GetY();
	xs = 2.0f*Quat.GetX()*Quat.GetS();

	m[2][1] = zy - xs;
	m[1][2] = zy + xs;

	zx = 2.0f*Quat.GetZ()*Quat.GetX();
	ys = 2.0f*Quat.GetY()*Quat.GetS();

	m[0][2] = zx - ys;
	m[2][0] = zx + ys;

	// set last row to zero
	*(u_long128*)&m[3] = 0; 

	// set last column to zero (m[3][3] element is already zero from previous instruction) 
	m[0][3] = 0.0f;
	m[1][3] = 0.0f;
	m[2][3] = 0.0f;
	m[3][3] = 1.0f;
}
#endif
