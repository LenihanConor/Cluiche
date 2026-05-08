#include "DiaCore/Core/Assert.h"

// -----------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		//-----------------------------------------------------------------------------
		inline
		BitArray32::BitArray32()
			: mBits(0)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray32::BitArray32( const unsigned int value )
			: mBits(value)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray32::BitArray32( const BitArray32& rhs )
			: mBits (rhs.mBits)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray32& BitArray32::operator = ( const unsigned int rhs )
		{
			mBits = rhs;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray32& BitArray32::operator = ( const BitArray32& rhs )
		{
			mBits = rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray32::operator [] ( int index ) const
		{
			DIA_ASSERT( index >= 0, "Index outbound bitflag");

			return GetBit(static_cast<unsigned int>(index));
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray32::operator []( unsigned int index ) const
		{
			return GetBit(static_cast<unsigned int>(index));
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray32::operator ==( const BitArray32& rhs )const
		{
			return mBits == rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray32::operator != ( const BitArray32& rhs )const
		{
			return mBits != rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray32& BitArray32::operator |= ( const BitArray32& rhs )
		{
			mBits |= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray32& BitArray32::operator &= ( const BitArray32& rhs )
		{
			mBits &= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray32& BitArray32::operator ^= ( const BitArray32& rhs )
		{
			mBits ^= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray32 BitArray32::operator | ( const BitArray32& rhs )
		{
			return mBits | rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray32 BitArray32::operator & ( const BitArray32& rhs )
		{
			return mBits & rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray32 BitArray32::operator ^ ( const BitArray32& rhs )
		{
			return mBits ^ rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray32::Invert()
		{
			static const BitArray32 OnMask( 0xFFFFFFFF );

			mBits ^= OnMask.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray32::Clear()
		{
			mBits = 0;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray32::SetAllBits( const unsigned int bits )
		{
			mBits = bits;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned int BitArray32::GetAllBits()const
		{
			return mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray32::IsAllOn()const
		{
			static const BitArray32 OnMask( 0xFFFFFFFF );

			return *this == OnMask;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray32::IsAllOff()const
		{
			static const BitArray32 msOffMask = 0x00;

			return *this == msOffMask;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray32::SetBit( unsigned int index, bool value )
		{
			unsigned int mask = GetBitMaskFromOffset( index );

			if (value)
			{
				mBits |= mask;
			}
			else
			{
				mBits &= ~mask;
			}
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray32::ToggleBit( unsigned int index )
		{
			unsigned int mask = GetBitMaskFromOffset( index );
			mBits ^= mask;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned int BitArray32::GetBitMaskFromOffset( unsigned int offset )
		{
			static unsigned int lookupTable[ kMaxNumberOfBits ] = { 1u << 0,
																	1u << 1,
																	1u << 2,
																	1u << 3,
																	1u << 4,
																	1u << 5,
																	1u << 6,
																	1u << 7,
																	1u << 8,
																	1u << 9,
																	1u << 10,
																	1u << 11,
																	1u << 12,
																	1u << 13,
																	1u << 14,
																	1u << 15,
																	1u << 16,
																	1u << 17,
																	1u << 18,
																	1u << 19,
																	1u << 20,
																	1u << 21,
																	1u << 22,
																	1u << 23,
																	1u << 24,
																	1u << 25,
																	1u << 26,
																	1u << 27,
																	1u << 28,
																	1u << 29,
																	1u << 30,
																	1u << 31};

			DIA_ASSERT( offset < kMaxNumberOfBits, "Offset is bigger then Max Size");

			return lookupTable[ offset ];
		}

		//-----------------------------------------------------------------------------
		inline
		const BitArray32& BitArray32::GetBitFlagMaskFromOffset( unsigned int offset )
		{
			static BitArray32 lookupTable[ kMaxNumberOfBits ] = {	BitArray32(1 << 0), 
																	BitArray32(1 << 1),
																	BitArray32(1 << 2),
																	BitArray32(1 << 3),
																	BitArray32(1 << 4),
																	BitArray32(1 << 5),
																	BitArray32(1 << 6),
																	BitArray32(1 << 7),	
																	BitArray32(1 << 8), 
																	BitArray32(1 << 9),
																	BitArray32(1 << 10),
																	BitArray32(1 << 11),
																	BitArray32(1 << 12),
																	BitArray32(1 << 13),
																	BitArray32(1 << 14),
																	BitArray32(1 << 15),
																	BitArray32(1 << 16), 
																	BitArray32(1 << 17), 
																	BitArray32(1 << 18), 
																	BitArray32(1 << 19), 
																	BitArray32(1 << 20), 
																	BitArray32(1 << 21), 
																	BitArray32(1 << 22), 
																	BitArray32(1 << 23),
																	BitArray32(1 << 24), 
																	BitArray32(1 << 25), 
																	BitArray32(1 << 26), 
																	BitArray32(1 << 27), 
																	BitArray32(1 << 28), 
																	BitArray32(1 << 29), 
																	BitArray32(1 << 30), 
																	BitArray32(1 << 31)};

			DIA_ASSERT( offset < kMaxNumberOfBits, "Offset is bigger then Max Size");

			return lookupTable[ offset ];
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray32::GetBit( unsigned int index )const
		{
			DIA_ASSERT(index < kMaxNumberOfBits, "Outbounded bitflag");

			unsigned int mask = GetBitMaskFromOffset( index );

			return (mBits & mask) == mask;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned int BitArray32::GetNumberBitsSetOn()const
		{
			unsigned int numberOn = 0;
			for (unsigned int i = 0; i < kMaxNumberOfBits; i++)
			{
				if (GetBit(i))
				{
					numberOn++;
				}
			}

			return numberOn;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned char BitArray32::GetByte( unsigned int index )
		{
			DIA_ASSERT(index < kMaxNumberOfBytes, "Outbounding the number of bytes in Bit array");

			unsigned int dw = mBits;
			unsigned char byteResult = (dw >> (8 * index)) & 0xFFFFFFFF;
			return byteResult;
		}
	}
}