namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Implementation
		//---------------------------------------------------------------------------------------------------------------------------------
		
		template <class T> inline T Clamp ( const T& x, const T& min, const T& max )
		{
			if (x < min)
				return min;
			if (x > max)
				return max;

			return x;
		}

		//returns a 1 if X > 0
		//-----------------------------------------------------------------------------
		template <class T> inline T Sign ( const T& x )
		{
			if ( x < 0 )
			  return static_cast<T>(-1);
		   if ( x > 0 )
			  return static_cast<T>(1);

		   return static_cast<T>(0);
		}

		//makes the number absolute
		//-----------------------------------------------------------------------------
		template <class T> inline T Abs ( const T& x )			
		{
			if ( x < 0 )
			  return -x;
		   return x;
		}
		
		//makes the number negative
		//-----------------------------------------------------------------------------
		template <class T> inline T Negative ( const T& x )			
		{
			if ( x > 0 )
			  return -x;
		   return x;
		}

		//returns the minumum of the two numbers
		//-----------------------------------------------------------------------------
		template <class T> inline T Min( const T& a, const T& b )	
		{
			T x = b;
			if( a <= b )
				return a;
			
			return b;
		}

		//returns the minumum of the two numbers
		//-----------------------------------------------------------------------------
		template <class T> inline T Max( const T& a, const T& b )	
		{
			T x = b;
			if( a <= b )
				return b;
			
			return a;
		}

		//-----------------------------------------------------------------------------
		template <class T> inline T Power(const T& number, int exponent)
		{
			if (exponent == 0)
			{
				return 1;
			}

			T result = number;
			for (int i = 0; i < exponent - 1; i++)
			{
				result *= number;
			}
			return result;
		}

		//-----------------------------------------------------------------------------
		template <class T> inline T Square(const T& number)
		{
			return number*number;
		}

		//-----------------------------------------------------------------------------
		template <typename T> inline void Swap (T& a, T& b)
		{
			T t = a;
			a = b;
			b = t;
		}
	}
}