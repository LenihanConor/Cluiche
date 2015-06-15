#ifndef DIA_BITFLAG32_H
#define DIA_BITFLAG32_H

namespace Dia
{
	namespace Core
	{
		class BitArray32
		{
		public:
			static const unsigned int kMaxNumberOfBits = 32;
			static const unsigned int kMaxNumberOfBytes = 4;

			static const BitArray32&	GetBitFlagMaskFromOffset( unsigned int offset );
			static unsigned int			GetBitMaskFromOffset( unsigned int offset );

			BitArray32();
			BitArray32( const unsigned int value );
			BitArray32( const BitArray32& rhs );

			BitArray32&		operator =			( const unsigned int rhs );
			BitArray32&		operator =			( const BitArray32& rhs );
			
			BitArray32&		operator |=			( const BitArray32& rhs );
			BitArray32&		operator &=			( const BitArray32& rhs );
			BitArray32&		operator ^=			( const BitArray32& rhs );
			
			BitArray32		operator |			( const BitArray32& rhs );
			BitArray32		operator &			( const BitArray32& rhs );
			BitArray32		operator ^			( const BitArray32& rhs );

			bool			operator []			( int index ) const;
			bool			operator []			( unsigned int index ) const;

			bool			operator ==			( const BitArray32& rhs )const;	
			bool			operator !=			( const BitArray32& rhs )const;
 			
			void			Invert();
			void			Clear();

			void			SetAllBits			( const unsigned int bits );
			unsigned int	GetAllBits			()const;
				
			bool			IsAllOn				()const;
			bool			IsAllOff			()const;
			
			void			SetBit				( unsigned int index, bool value );
			void			ToggleBit			( unsigned int index );
			bool			GetBit				( unsigned int index )const;	

			unsigned char	GetByte				( unsigned int index );
			unsigned int	GetNumberBitsSetOn	()const;
		
		private:
			unsigned int mBits;
		};
	}
}

#include "DiaCore/Containers/BitFlag/BitArray32.inl"

#endif 