#ifndef DIA_BITFLAG16_H
#define DIA_BITFLAG16_H

namespace Dia
{
	namespace Core
	{
		class BitArray16
		{
		public:
			static const unsigned int kMaxNumberOfBits = 16;
			static const unsigned int kMaxNumberOfBytes = 2;

			static const BitArray16&	GetBitFlagMaskFromOffset( unsigned int offset );
			static unsigned short		GetBitMaskFromOffset( unsigned int offset );

			BitArray16();
			BitArray16( const unsigned short value );
			BitArray16( const BitArray16& rhs );

			BitArray16&		operator =			( const unsigned short rhs );
			BitArray16&		operator =			( const BitArray16& rhs );
			
			BitArray16&		operator |=			( const BitArray16& rhs );
			BitArray16&		operator &=			( const BitArray16& rhs );
			BitArray16&		operator ^=			( const BitArray16& rhs );
			
			BitArray16		operator |			( const BitArray16& rhs );
			BitArray16		operator &			( const BitArray16& rhs );
			BitArray16		operator ^			( const BitArray16& rhs );

			bool			operator []			( int index ) const;
			bool			operator []			( unsigned int index ) const;

			bool			operator ==			( const BitArray16& rhs )const;	
			bool			operator !=			( const BitArray16& rhs )const;
 			
			void			Invert();
			void			Clear();

			void			SetAllBits			( const unsigned short bits );
			unsigned short	GetAllBits			()const;
				
			bool			IsAllOn				()const;
			bool			IsAllOff			()const;
			
			void			SetBit				( unsigned int index, bool value );
			void			ToggleBit			( unsigned int index );
			bool			GetBit				( unsigned int index )const;	

			unsigned char	GetByte				( unsigned int index );
			unsigned int	GetNumberBitsSetOn	()const;
		
		private:
			unsigned short mBits;
		};
	}
}

#include "DiaCore/Containers/BitFlag/BitArray16.inl"

#endif 