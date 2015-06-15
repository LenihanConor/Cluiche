#ifndef DIA_BITFLAG64_H
#define DIA_BITFLAG64_H

namespace Dia
{
	namespace Core
	{
		class BitArray64
		{
		public:
			static const unsigned int kMaxNumberOfBits = 64;
			static const unsigned int kMaxNumberOfBytes = 8;

			static const BitArray64&	GetBitFlagMaskFromOffset( unsigned int offset );
			static unsigned long long	GetBitMaskFromOffset( unsigned int offset );

			BitArray64();
			BitArray64( const unsigned long long value );
			BitArray64( const BitArray64& rhs );

			BitArray64&		operator =			( const unsigned long long rhs );
			BitArray64&		operator =			( const BitArray64& rhs );
			
			BitArray64&		operator |=			( const BitArray64& rhs );
			BitArray64&		operator &=			( const BitArray64& rhs );
			BitArray64&		operator ^=			( const BitArray64& rhs );
			
			BitArray64		operator |			( const BitArray64& rhs );
			BitArray64		operator &			( const BitArray64& rhs );
			BitArray64		operator ^			( const BitArray64& rhs );

			bool			operator []			( int index ) const;
			bool			operator []			( unsigned int index ) const;

			bool			operator ==			( const BitArray64& rhs )const;	
			bool			operator !=			( const BitArray64& rhs )const;
 			
			void			Invert();
			void			Clear();

			void				SetAllBits			( const unsigned long long bits );
			unsigned long long	GetAllBits			()const;
				
			bool			IsAllOn				()const;
			bool			IsAllOff			()const;
			
			void			SetBit				( unsigned int index, bool value );
			void			ToggleBit			( unsigned int index );
			bool			GetBit				( unsigned int index )const;	

			unsigned char	GetByte				( unsigned int index );
			unsigned int	GetNumberBitsSetOn	()const;
		
		private:
			unsigned long long mBits;
		};
	}
}

#include "DiaCore/Containers/BitFlag/BitArray64.inl"

#endif 