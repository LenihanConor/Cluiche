#include "DiaCore/Core/Assert.h"

// -----------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		//-----------------------------------------------------------------------------
		inline
		BitArray64::BitArray64()
			: mBits(0)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray64::BitArray64( const unsigned long long value )
			: mBits(value)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray64::BitArray64( const BitArray64& rhs )
			: mBits (rhs.mBits)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray64& BitArray64::operator = ( const unsigned long long rhs )
		{
			mBits = rhs;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray64& BitArray64::operator = ( const BitArray64& rhs )
		{
			mBits = rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray64::operator [] ( int index ) const
		{
			DIA_ASSERT( index >= 0, "Index outbound bitflag");

			return GetBit(static_cast<unsigned int>(index));
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray64::operator []( unsigned int index ) const
		{
			return GetBit(static_cast<unsigned int>(index));
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray64::operator ==( const BitArray64& rhs )const
		{
			return mBits == rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray64::operator != ( const BitArray64& rhs )const
		{
			return mBits != rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray64& BitArray64::operator |= ( const BitArray64& rhs )
		{
			mBits |= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray64& BitArray64::operator &= ( const BitArray64& rhs )
		{
			mBits &= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray64& BitArray64::operator ^= ( const BitArray64& rhs )
		{
			mBits ^= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray64 BitArray64::operator | ( const BitArray64& rhs )
		{
			return mBits | rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray64 BitArray64::operator & ( const BitArray64& rhs )
		{
			return mBits & rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray64 BitArray64::operator ^ ( const BitArray64& rhs )
		{
			return mBits ^ rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray64::Invert()
		{
			static const BitArray64 OnMask( 0xFFFFFFFFFFFFFFFF );

			mBits ^= OnMask.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray64::Clear()
		{
			mBits = 0;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray64::SetAllBits( const unsigned long long bits )
		{
			mBits = bits;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned long long BitArray64::GetAllBits()const
		{
			return mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray64::IsAllOn()const
		{
			static const BitArray64 OnMask( 0xFFFFFFFFFFFFFFFF );

			return *this == OnMask;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray64::IsAllOff()const
		{
			static const BitArray64 msOffMask = 0x00;

			return *this == msOffMask;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray64::SetBit( unsigned int index, bool value )
		{
			unsigned long long mask = GetBitMaskFromOffset( index );

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
		void BitArray64::ToggleBit( unsigned int index )
		{
			unsigned long long mask = GetBitMaskFromOffset( index );
			mBits ^= mask;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned long long BitArray64::GetBitMaskFromOffset( unsigned int offset )
		{
			static unsigned long long lookupTable[ kMaxNumberOfBits ] = {	1ull << 0,
																			1ull << 1,
																			1ull << 2,
																			1ull << 3,
																			1ull << 4,
																			1ull << 5,
																			1ull << 6,
																			1ull << 7,
																			1ull << 8,
																			1ull << 9,
																			1ull << 10,
																			1ull << 11,
																			1ull << 12,
																			1ull << 13,
																			1ull << 14,
																			1ull << 15,
																			1ull << 16,
																			1ull << 17,
																			1ull << 18,
																			1ull << 19,
																			1ull << 20,
																			1ull << 21,
																			1ull << 22,
																			1ull << 23,
																			1ull << 24,
																			1ull << 25,
																			1ull << 26,
																			1ull << 27,
																			1ull << 28,
																			1ull << 29,
																			1ull << 30,
																			1ull << 31,
																			0x100000000,
																			0x200000000,
																			0x300000000,
																			0x400000000,
																			0x500000000,
																			0x600000000,
																			0x700000000,
																			0x800000000,
																			0x10000000000,
																			0x20000000000,
																			0x30000000000,
																			0x40000000000,
																			0x50000000000,
																			0x60000000000,
																			0x70000000000,
																			0x80000000000,
																			0x1000000000000,
																			0x2000000000000,
																			0x3000000000000,
																			0x4000000000000,
																			0x5000000000000,
																			0x6000000000000,
																			0x7000000000000,
																			0x8000000000000,
																			0x100000000000000,
																			0x200000000000000,
																			0x300000000000000,
																			0x400000000000000,
																			0x500000000000000,
																			0x600000000000000,
																			0x700000000000000,
																			0x800000000000000};

			DIA_ASSERT( offset < kMaxNumberOfBits, "Offset is bigger then Max Size");

			return lookupTable[ offset ];
		}

		//-----------------------------------------------------------------------------
		inline
		const BitArray64& BitArray64::GetBitFlagMaskFromOffset( unsigned int offset )
		{
			static BitArray64 lookupTable[ kMaxNumberOfBits ] = {	BitArray64(1 << 0), 
																	BitArray64(1 << 1),
																	BitArray64(1 << 2),
																	BitArray64(1 << 3),
																	BitArray64(1 << 4),
																	BitArray64(1 << 5),
																	BitArray64(1 << 6),
																	BitArray64(1 << 7),	
																	BitArray64(1 << 8), 
																	BitArray64(1 << 9),
																	BitArray64(1 << 10),
																	BitArray64(1 << 11),
																	BitArray64(1 << 12),
																	BitArray64(1 << 13),
																	BitArray64(1 << 14),
																	BitArray64(1 << 15),
																	BitArray64(1 << 16),
																	BitArray64(1 << 17),
																	BitArray64(1 << 18),
																	BitArray64(1 << 19),
																	BitArray64(1 << 20),
																	BitArray64(1 << 21),
																	BitArray64(1 << 22),
																	BitArray64(1 << 23),
																	BitArray64(1 << 24),
																	BitArray64(1 << 25),
																	BitArray64(1 << 26),
																	BitArray64(1 << 27),
																	BitArray64(1 << 28),
																	BitArray64(1 << 29),
																	BitArray64(1 << 30),
																	BitArray64(1 << 31),																	
																	BitArray64(0x100000000), 
																	BitArray64(0x200000000),
																	BitArray64(0x300000000),
																	BitArray64(0x400000000),
																	BitArray64(0x500000000),
																	BitArray64(0x600000000),
																	BitArray64(0x700000000),
																	BitArray64(0x800000000),	
																	BitArray64(0x10000000000), 
																	BitArray64(0x20000000000),
																	BitArray64(0x30000000000),
																	BitArray64(0x40000000000),
																	BitArray64(0x50000000000),
																	BitArray64(0x60000000000),
																	BitArray64(0x70000000000),
																	BitArray64(0x80000000000),
																	BitArray64(0x1000000000000),
																	BitArray64(0x2000000000000),
																	BitArray64(0x3000000000000),
																	BitArray64(0x4000000000000),
																	BitArray64(0x5000000000000),
																	BitArray64(0x6000000000000),
																	BitArray64(0x7000000000000),
																	BitArray64(0x8000000000000),
																	BitArray64(0x100000000000000),
																	BitArray64(0x200000000000000),
																	BitArray64(0x300000000000000),
																	BitArray64(0x400000000000000),
																	BitArray64(0x500000000000000),
																	BitArray64(0x600000000000000),
																	BitArray64(0x700000000000000),
																	BitArray64(0x800000000000000)};

			DIA_ASSERT( offset < kMaxNumberOfBits, "Offset is bigger then Max Size");

			return lookupTable[ offset ];
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray64::GetBit( unsigned int index )const
		{
			DIA_ASSERT(index < kMaxNumberOfBits, "Outbounded bitflag");

			unsigned long long mask = GetBitMaskFromOffset( index );

			return (mBits & mask) == mask;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned int BitArray64::GetNumberBitsSetOn()const
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
		unsigned char BitArray64::GetByte( unsigned int index )
		{
			DIA_ASSERT(index < kMaxNumberOfBytes, "Outbounding the number of bytes in Bit array");

			unsigned long long dw = mBits;
			unsigned char byteResult = (dw >> (8 * index)) & 0xFFFFFFFFFFFFFFFF;
			return byteResult;
		}
	}
}