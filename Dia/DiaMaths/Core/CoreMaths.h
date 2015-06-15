#pragma once

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Basic functions
		//---------------------------------------------------------------------------------------------------------------------------------
		
		template <class T> T Clamp ( const T& x, const T& min, const T& max );
		template <class T> T Sign ( const T& x );
		template <class T> T Abs ( const T& x )	;
		template <class T> T Negative ( const T& x );		
		template <class T> T Min( const T& a, const T& b )	;
		template <class T> T Max( const T& a, const T& b )	;	
		template <class T> T Power(const T& number, int exponent);
		template <class T> T Square(const T& number);
		template <class T> void Swap (T& a, T& b);
		
		float				SquareRoot(const float& number);
		int					Factorial(const int& number);
	}
}

#include "DiaMaths/Core/CoreMaths.inl"