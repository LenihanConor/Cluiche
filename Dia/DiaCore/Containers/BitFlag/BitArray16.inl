#include "DiaCore/Core/Assert.h"

// -----------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		//-----------------------------------------------------------------------------
		inline
		BitArray16::BitArray16()
			: mBits(0)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray16::BitArray16( const unsigned short value )
			: mBits(value)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray16::BitArray16( const BitArray16& rhs )
			: mBits (rhs.mBits)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray16& BitArray16::operator = ( const unsigned short rhs )
		{
			mBits = rhs;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray16& BitArray16::operator = ( const BitArray16& rhs )
		{
			mBits = rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray16::operator [] ( int index ) const
		{
			DIA_ASSERT( index >= 0, "Index outbound bitflag");

			return GetBit(static_cast<unsigned int>(index));
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray16::operator []( unsigned int index ) const
		{
			return GetBit(static_cast<unsigned int>(index));
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray16::operator ==( const BitArray16& rhs )const
		{
			return mBits == rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray16::operator != ( const BitArray16& rhs )const
		{
			return mBits != rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray16& BitArray16::operator |= ( const BitArray16& rhs )
		{
			mBits |= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray16& BitArray16::operator &= ( const BitArray16& rhs )
		{
			mBits &= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray16& BitArray16::operator ^= ( const BitArray16& rhs )
		{
			mBits ^= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray16 BitArray16::operator | ( const BitArray16& rhs )
		{
			return mBits | rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray16 BitArray16::operator & ( const BitArray16& rhs )
		{
			return mBits & rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray16 BitArray16::operator ^ ( const BitArray16& rhs )
		{
			return mBits ^ rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray16::Invert()
		{
			static const BitArray16 OnMask( 0xFFFF );

			mBits ^= OnMask.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray16::Clear()
		{
			mBits = 0;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray16::SetAllBits( const unsigned short bits )
		{
			mBits = bits;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned short BitArray16::GetAllBits()const
		{
			return mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray16::IsAllOn()const
		{
			static const BitArray16 OnMask( 0xFFFF );

			return *this == OnMask;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray16::IsAllOff()const
		{
			static const BitArray16 msOffMask = 0x00;

			return *this == msOffMask;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray16::SetBit( unsigned int index, bool value )
		{
			unsigned short mask = GetBitMaskFromOffset( index );

			if (value)
			{
				mBits |= mask;
			}
			else
			{
				mask ^= mask;
				mBits &= mask;
			}
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray16::ToggleBit( unsigned int index )
		{
			unsigned short mask = GetBitMaskFromOffset( index );
			mBits ^= mask;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned short BitArray16::GetBitMaskFromOffset( unsigned int offset )
		{
			static unsigned short lookupTable[ kMaxNumberOfBits ] = { 1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7,
																	 1 << 8, 1 << 9, 1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15};

			DIA_ASSERT( offset < kMaxNumberOfBits, "Offset is bigger then Max Size");

			return lookupTable[ offset ];
		}

		//-----------------------------------------------------------------------------
		inline
		const BitArray16& BitArray16::GetBitFlagMaskFromOffset( unsigned int offset )
		{
			static BitArray16 lookupTable[ kMaxNumberOfBits ] = {	BitArray16(1 << 0), 
																	BitArray16(1 << 1),
																	BitArray16(1 << 2),
																	BitArray16(1 << 3),
																	BitArray16(1 << 4),
																	BitArray16(1 << 5),
																	BitArray16(1 << 6),
																	BitArray16(1 << 7),	
																	BitArray16(1 << 8), 
																	BitArray16(1 << 9),
																	BitArray16(1 << 10),
																	BitArray16(1 << 11),
																	BitArray16(1 << 12),
																	BitArray16(1 << 13),
																	BitArray16(1 << 14),
																	BitArray16(1 << 15) };

			DIA_ASSERT( offset < kMaxNumberOfBits, "Offset is bigger then Max Size");

			return lookupTable[ offset ];
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray16::GetBit( unsigned int index )const
		{
			DIA_ASSERT(index < kMaxNumberOfBits, "Outbounded bitflag");

			unsigned short mask = GetBitMaskFromOffset( index );

			return (mBits & mask) == mask;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned int BitArray16::GetNumberBitsSetOn()const
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
		unsigned char BitArray16::GetByte( unsigned int index )
		{
			DIA_ASSERT(index < kMaxNumberOfBytes, "Outbounding the number of bytes in Bit array");

			unsigned short dw = mBits;
			unsigned char byteResult = (dw >> (8 * index)) & 0xFFFF;
			return byteResult;
		}
	}
}