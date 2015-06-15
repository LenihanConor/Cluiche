#ifndef DIA_BITFLAG8_H
#define DIA_BITFLAG8_H

namespace Dia
{
	namespace Core
	{
		class BitArray8
		{
		public:
			static const unsigned int kMaxNumberOfBits = 8;
			static const BitArray8&	GetBitFlagMaskFromOffset( unsigned int offset );
			static unsigned char	GetBitMaskFromOffset( unsigned int offset );

			BitArray8();
			BitArray8( const unsigned char value );
			BitArray8( const BitArray8& rhs );

			BitArray8&		operator =			( const unsigned char rhs );
			BitArray8&		operator =			( const BitArray8& rhs );
			
			BitArray8&		operator |=			( const BitArray8& rhs );
			BitArray8&		operator &=			( const BitArray8& rhs );
			BitArray8&		operator ^=			( const BitArray8& rhs );
			
			BitArray8		operator |			( const BitArray8& rhs );
			BitArray8		operator &			( const BitArray8& rhs );
			BitArray8		operator ^			( const BitArray8& rhs );

			bool			operator []			( int index ) const;
			bool			operator []			( unsigned int index ) const;

			bool			operator ==			( const BitArray8& rhs )const;	
			bool			operator !=			( const BitArray8& rhs )const;
 			
			void			Invert();
			void			Clear();

			void			SetAllBits			( const unsigned char bits );
			unsigned char	GetAllBits			()const;
				
			bool			IsAllOn				()const;
			bool			IsAllOff			()const;
			
			void			SetBit				( unsigned int index, bool value );
			void			ToggleBit			( unsigned int index );
			bool			GetBit				( unsigned int index )const;	
	
			unsigned int	GetNumberBitsSetOn	()const;

		private:
			unsigned char mBits;
		};
	}
}

#include "DiaCore/Containers/BitFlag/BitArray8.inl"

#endif 