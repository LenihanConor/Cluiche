#ifndef __MATRIX_HEADER_INCLUDED__
#define __MATRIX_HEADER_INCLUDED__

/*----------------------------------------------------------------------------------------------------------

	Interface of our matrix type

	Matrix representation of linear transforms:-

	Due to the design of the PS2 vector units, transforms are stored in matrices as follows:-

			| r00  r01  r02  0 |
			| r10  r11  r12  0 |
			| r20  r21  r22  0 |
			|  Tx   Ty   Tz  1 |

	where r is the rotational part & T is the translation
	
	This corresponds to row-major format rather than column-major which seems to be more commonly
	used in books on 3d graphics. But that's not really important!

----------------------------------------------------------------------------------------------------------*/

#include "Vector.h"
#include "Quaternion.h"

//----------------------------------------------------------------------------------------------------------

class Matrix
{
public:
	float	m[4][4];

	// construction
	Matrix(void);
	Matrix(const Matrix& rOther);

	// array type access - allows for Matrix mat[r][c] type access
	Vector&		  	operator[](int i);
	const Vector& 	operator[](int i) const;

	// named versions of the [] operator
	Vector&			row0(void);
	Vector&			row1(void);
	Vector&			row2(void);
	Vector&			row3(void);

	const Vector&	row0(void) const;
	const Vector&	row1(void) const;
	const Vector&	row2(void) const;
	const Vector&	row3(void) const;

	// assignment
	Matrix&			operator=(const Matrix& rOther);	// copy by assignment

	// assignment operators
	Matrix& 		operator+=(const Matrix& rOther);
	Matrix& 		operator-=(const Matrix& rOther);
	Matrix&			operator*=(const Matrix& rOther);

	// scalar multiplication
	Matrix&			operator*=(float fScalar);
	
	// binary operators that do not change this
	Matrix			operator+(const Matrix& rOther) const;
	Matrix			operator-(const Matrix& rOther) const;
	Matrix			operator*(const Matrix& rOther) const;

	// scalar multiplication
	Matrix			operator*(float fScalar) const;

	// m[r][c] = m[c][r]
	void			Transpose(void);
	Matrix			Transpose(void) const;

	// sum of the diagonal elements
	float			Trace(void) const;

	// determinant
	float			Det4x4(void) const;
	float			Det3x3(void) const;
	float			Det2x2(void) const;
	float			Det3x3(float a1, float a2, float a3, 
				   		   float b1, float b2, float b3, 
				   		   float c1, float c2, float c3) const;
	float			Det2x2(float a, float b, 
						   float c, float d) const;

	void			Adjoint(void);
	Matrix 			Adjoint(void) const;

	// full matrix inverse - expensive!
	void    		Invert(void);
	Matrix			Inverse(void) const;
	Matrix 			InverseNoVu0(void) const;


	// reverse signs of rOther and store in this
	void			Negate(const Matrix& rOther); 			
	void			Negate(void);
	
	// Normalise the axes 
	Matrix			Normalised() const;
	void			Normalise();

	// -------------------------------------
	// methods relating to linear transforms
	// -------------------------------------

	// setting this to the unit/identity transform
	void			SetUnit(void);
	void			SetIdentity(void);

	// this lot set this to the rotation transformations
	void 			SetRotX(float fAngle);
	void 			SetRotY(float fAngle);
	void 			SetRotZ(float fAngle);
	void 			SetRotXYZ(const Vector& vAngles);	// rotate around x, then y, then z
	void			SetRotZXY(const Vector& vAngles);	// rotate around z, then x, then y - do we ever use this!?

	void 			SetXAxis(const Vector& vAxis);
	void 			SetYAxis(const Vector& vAxis);
	void 			SetZAxis(const Vector& vAxis);

	void			SetTranslation(const Vector& vTranslation);
	void			SetTranslation(float x, float y, float z);

	void			SetScale(const Vector& vScale);
	void			SetScale(float x, float y, float z);
	void 			SetMirror( const Vector &rNormal, const float &fDistanceToPlane );
	
	// matrix decomposition functions
	Vector			GetScale(void);
//	Quaternion		GetRotation(void); //doesn't exist !
	const Vector&	GetTranslation(void) const;
	
	const Vector& 	GetXAxis(void) const;
	const Vector& 	GetYAxis(void) const;
	const Vector& 	GetZAxis(void) const;
	
	void			ApplyTranslation(const Vector& vTranslation);
	void			ApplyTranslation(float x, float y, float z);
	
	void			ApplyScale(const Vector& vScale);
	void			ApplyScale(float x, float y, float z);

	// orientation from direction vector
	void			SetOrientFromDir(Vector& vDir);
	void			SetOrientFromUpAndDir(const Vector& vUp, const Vector& vDir);
	void			SetPositionDirection( const Vector& rvPosition, const Vector& rvDirection );

	// orientation from quaternion
	void			SetOrientFromQuat(const Quaternion& Quat);

} __attribute__((aligned(16)));

//----------------------------------------------------------------------------------------------------------
//	inline function definitions
//----------------------------------------------------------------------------------------------------------

// default constructor
inline Matrix::Matrix(void)
{
	// does nothing!
}

//----------------------------------------------------------------------------------------------------------
// Apply Translation directly
inline void Matrix::ApplyTranslation(float x, float y, float z)
{
	m[3][0] += x;
	m[3][1] += y;
	m[3][2] += z;
}

//----------------------------------------------------------------------------------------------------------
// Apply Translation directly
inline void Matrix::ApplyTranslation(const Vector& vTranslation)
{
	m[3][0] += vTranslation.x;
	m[3][1] += vTranslation.y;
	m[3][2] += vTranslation.z;
}


//----------------------------------------------------------------------------------------------------------

inline Matrix::Matrix(const Matrix& rOther)
{
	asm volatile("
		.set noreorder
		lq	$8,  0x00(%1)		# row 0
		lq	$9,  0x10(%1)		# row 1
		lq	$10, 0x20(%1)		# row 2
		lq	$11, 0x30(%1)		# row 3
		nop
		sq	$8,  0x00(%0)		# store to this
		sq	$9,  0x10(%0)		
		sq	$10, 0x20(%0)		
		sq	$11, 0x30(%0)		
		.set reorder
		" : : "r" (this), "r" (&rOther) : "$8","$9","$10","$11" );
}

//----------------------------------------------------------------------------------------------------------

//inline Matrix::Matrix(const Quaternion& rQuat)
//{
//	SetOrientFromQuat(rQuat);
//}

//----------------------------------------------------------------------------------------------------------

inline Vector& Matrix::operator[](int i)
{
	return *(reinterpret_cast<Vector*>(m)+i);
}

inline const Vector& Matrix::operator[](int i) const
{
	return *(reinterpret_cast<const Vector*>(m)+i);
}

//----------------------------------------------------------------------------------------------------------

inline Vector& Matrix::row0(void) {	return *(reinterpret_cast<Vector*>(m)+0); }
inline Vector& Matrix::row1(void) {	return *(reinterpret_cast<Vector*>(m)+1); }
inline Vector& Matrix::row2(void) {	return *(reinterpret_cast<Vector*>(m)+2); }
inline Vector& Matrix::row3(void) {	return *(reinterpret_cast<Vector*>(m)+3); }

//----------------------------------------------------------------------------------------------------------

inline const Vector& Matrix::row0(void) const { return *(reinterpret_cast<const Vector*>(m)+0); }
inline const Vector& Matrix::row1(void) const { return *(reinterpret_cast<const Vector*>(m)+1); }
inline const Vector& Matrix::row2(void) const { return *(reinterpret_cast<const Vector*>(m)+2); }
inline const Vector& Matrix::row3(void) const { return *(reinterpret_cast<const Vector*>(m)+3); }

//----------------------------------------------------------------------------------------------------------

// TODO - which is better / faster?
inline void Matrix::SetUnit(void)
{
#if 1
	// 1.0f = 	0x3f800000
	asm volatile("
		.set noreorder
		pxor	$t0, $t0, $t0		# All zero into t0
		lui		$t0, 0x3f80			# Top row
		prot3w	$t1, $t0			# Third row
		prot3w	$t2, $t1			# Second row
		pcpyld	$t3, $t2, $zero		# Fourth row
		sq		$t0, 0x0(%0)		# That's all we need.. store the results
		sq		$t2, 0x10(%0)
		sq		$t1, 0x20(%0)
		sq		$t3, 0x30(%0)
		.set reorder
		": : "r"(this) : "t0","t1","t2","t3" );
#else
	// this one uses the "j" constraints
	asm("
		vmove	%0, vf0
		vmr32	%1, %0
		vmr32	%2, %1
		vmr32	%3, %2
		" : "+j" ((long128)&m[3]), "+j" ((long128)&m[2]), "+j" ((long128)&m[1]), "+j" ((long128)&m[0]) );
#endif
}

//----------------------------------------------------------------------------------------------------------

inline void Matrix::SetIdentity(void)
{
	SetUnit();
}

//----------------------------------------------------------------------------------------------------------

inline Matrix& Matrix::operator=(const Matrix& rOther)
{
	asm volatile("
		.set noreorder
		lq	$8,  0x00(%1)		# row 0
		lq	$9,  0x10(%1)		# row 1
		lq	$10, 0x20(%1)		# row 2
		lq	$11, 0x30(%1)		# row 3
		nop
		sq	$8,  0x00(%0)		# store to this
		sq	$9,  0x10(%0)		
		sq	$10, 0x20(%0)		
		sq	$11, 0x30(%0)		
		.set reorder
		" : : "r" (this), "r" (&rOther) : "$8","$9","$10","$11" );
	return *this;
}

//----------------------------------------------------------------------------------------------------------

inline float Matrix::Trace(void) const
{
	return m[0][0] + m[1][1] + m[2][2] + m[3][3];
}

//----------------------------------------------------------------------------------------------------------

inline void Matrix::SetTranslation(const Vector& vTranslation)
{
	m[3][0] = vTranslation.x;
	m[3][1] = vTranslation.y;
	m[3][2] = vTranslation.z;
}

//----------------------------------------------------------------------------------------------------------

inline void Matrix::SetTranslation(float x, float y, float z)
{
	m[3][0] = x;
	m[3][1] = y;
	m[3][2] = z;
}

//----------------------------------------------------------------------------------------------------------

inline void Matrix::SetScale(const Vector& vScale)
{
	m[0][0] = vScale.x;
	m[1][1] = vScale.y;
	m[2][2] = vScale.z;
}

//----------------------------------------------------------------------------------------------------------

inline void Matrix::SetScale(float x, float y, float z)
{
	m[0][0] = x;
	m[1][1] = y;
	m[2][2] = z;
}

//----------------------------------------------------------------------------------------------------------
//Sony hadn't defined this, so here's my attempt - jw.

inline Vector Matrix::GetScale(void)
{
	Vector aVec;
	
	aVec.x = row0().Length();
	aVec.y = row1().Length();
	aVec.z = row2().Length();
	aVec.w = 1.0f;
	return aVec;
}

//----------------------------------------------------------------------------------------------------------

inline const Vector& Matrix::GetTranslation(void) const
{
	return row3();
}

//----------------------------------------------------------------------------------------------------------

inline const Vector& Matrix::GetXAxis(void) const
{
	return row0();
}

//----------------------------------------------------------------------------------------------------------

inline const Vector& Matrix::GetYAxis(void) const
{
	return row1();
}

//----------------------------------------------------------------------------------------------------------

inline const Vector& Matrix::GetZAxis(void) const
{
	return row2();
}


inline void Matrix::SetXAxis( const Vector& vAxis )
{
	row0() 	= vAxis;
	m[0][3] = 0.0f;
}

inline void Matrix::SetYAxis( const Vector& vAxis )
{
	row1() 	= vAxis;
	m[1][3] = 0.0f;
}

inline void Matrix::SetZAxis( const Vector& vAxis )
{
	row2() = vAxis;
	m[2][3] = 0.0f;
}

inline void Normalise( Matrix& rResult, const Matrix& rMatrix )
{
	asm volatile("
		.set noreorder
		
		lqc2		vf1, 0x00(%0)			# vf1 = m[0]
		vmul.xyz	vf2, vf1, vf1			# vf2 = ( x*x y*y z*z )
		vaddy.x		vf2, vf2, vf2
		vaddz.x		vf2, vf2, vf2			# vf2x = x*x + y*y + z*z 
		
		vrsqrt		Q, vf0w, vf2x			# Begin 1st divide 1/sqrt( x*x+y*y+z*z ) (13 cycles)
		
			lqc2		vf3, 0x10(%0)			# vf3 = m[1]
			vmul.xyz	vf4, vf3, vf3			# vf4 = ( x*x y*y z*z )
			vaddy.x		vf4, vf4, vf4
			vaddz.x		vf4, vf4, vf4			# vf4x = x*x + y*y + z*z 
			
		vwaitq								# Wait for 1st divide to finish
		vmulq.xyz	vf1, vf1, Q				# vf1 = ( x y z ) * 1/sqrt( x*x+y*y+z*z )
	
		vrsqrt		Q, vf0w, vf4x			# Begin 2nd divide 1/sqrt( x*x+y*y+z*z ) (13 cycles)
		
			lqc2		vf5, 0x20(%0)			# vf5 = m[2]
			sqc2		vf1, 0x00(%1)			# m[0] = vf1
			
			vmul.xyz	vf6, vf5, vf5			# vf6 = ( x*x y*y z*z )
			vaddy.x		vf6, vf6, vf6
			vaddz.x		vf6, vf6, vf6			# vf6x = x*x + y*y + z*z 
		
		vwaitq								# Wait for 2nd divide to finish
		vmulq.xyz	vf3, vf3, Q				# vf3 = ( x y z ) * 1/sqrt( x*x+y*y+z*z )
		
		vrsqrt		Q, vf0w, vf6x			# Begin 3rd divide 1/sqrt( x*x+y*y+z*z ) (13 cycles)
		
			lqc2		vf7, 0x30(%0)			# vf7 = m[3]
			sqc2		vf3, 0x10(%1)			# m[2] = vf3
			sqc2		vf7, 0x30(%1)			# m[3] = vf7
		
		vwaitq								# Wait for 3rd divide to finish
		vmulq.xyz	vf5, vf5, Q				# vf5 = ( x y z ) * 1/sqrt( x*x+y*y+z*z )
		
		sqc2		vf5, 0x20(%1)			# m[3] = vf5
		
		.set reorder
		" : : "r" (rMatrix.m), "r" (rResult.m) );
}

inline void Matrix::Normalise()
{	
	::Normalise( *this, *this );	
}

inline Matrix Matrix::Normalised() const
{
	Matrix result;
	
	::Normalise( result, *this );	
	
	return result;
	
}

//----------------------------------------------------------------------------------------------------------

inline Vector operator *( Vector vec, const Matrix& mat )
{
	long128 vOut;
	Vector vRow0 = mat.row0();
	Vector vRow1 = mat.row1();
	Vector vRow2 = mat.row2();
	Vector vRow3 = mat.row3();

	asm volatile("
		.set noreorder
		qmtc2 %1, vf1
		qmtc2 %2, vf2
		qmtc2 %3, vf3
		qmtc2 %4, vf4
		qmtc2 %5, vf5

		vmulax.xyzw   ACC, vf2, vf1
		vmadday.xyzw  ACC, vf3, vf1
		vmaddaz.xyzw  ACC, vf4, vf1
		vmaddw.xyzw   vf1, vf5, vf1

		sqc2 vf1, %0 ;//TODO avoid writing to memory here!
		.set reorder
		"
	: "+m"( vOut )
	: "r"( (long128&)vec ), "r"( (long128&)vRow0 ), "r"( (long128&)vRow1 ), "r"( (long128&)vRow2 ), "r"( (long128&)vRow3 )
	);

	return (Vector&)vOut;
}

//----------------------------------------------------------------------------------------------------------

inline Vector& operator *=( Vector& vec, const Matrix& mat )
{
	long128 out;
	Vector vRow0 = mat.row0();
	Vector vRow1 = mat.row1();
	Vector vRow2 = mat.row2();
	Vector vRow3 = mat.row3();
	asm volatile("
		.set noreorder
		qmtc2		%1, vf1
		qmtc2		%2, vf2
		qmtc2		%3, vf3
		qmtc2		%4, vf4
		qmtc2		%5, vf5

		vmulax.xyzw   ACC, vf2, vf1
		vmadday.xyzw  ACC, vf3, vf1
		vmaddaz.xyzw  ACC, vf4, vf1
		vmaddw.xyzw   vf1, vf5, vf1

		qmfc2 		%0, vf1
		.set reorder
		"
	: "=r"( out )
	: "r"( (long128&)vec ), "r"( (long128&)vRow0 ), "r"( (long128&)vRow1 ), "r"( (long128&)vRow2 ), "r"( (long128&)vRow3 )
	);
	(long128&)vec = out;
	return vec;
}

//----------------------------------------------------------------------------------------------------------

inline Vector Rotate( const Vector& vec, const Matrix& mat )
{
	long128 vOut;
	Vector vRow0 = mat.row0();
	Vector vRow1 = mat.row1();
	Vector vRow2 = mat.row2();

	asm volatile("
		.set noreorder
		qmtc2 %1, vf1
		qmtc2 %2, vf2
		qmtc2 %3, vf3
		qmtc2 %4, vf4

		vmulax.xyzw   ACC, vf2, vf1
		vmadday.xyzw  ACC, vf3, vf1
		vmaddz.xyzw  vf1, vf4, vf1

		sqc2 vf1, %0 ;//TODO avoid writing to memory here!
		.set reorder
		"
	: "+m"( vOut )
	: "r"( (long128&)vec ), "r"( (long128&)vRow0 ), "r"( (long128&)vRow1 ), "r"( (long128&)vRow2 )
	);

	return (Vector&)vOut;
}



#endif // __MATRIX_HEADER_INCLUDED__

