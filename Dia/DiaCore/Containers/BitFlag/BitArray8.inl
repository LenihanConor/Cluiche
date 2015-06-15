#include "DiaCore/Core/Assert.h"

// -----------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		//-----------------------------------------------------------------------------
		inline
		BitArray8::BitArray8()
			: mBits(0)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray8::BitArray8( const unsigned char value )
			: mBits(value)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray8::BitArray8( const BitArray8& rhs )
			: mBits (rhs.mBits)
		{}

		//-----------------------------------------------------------------------------
		inline
		BitArray8& BitArray8::operator = ( const unsigned char rhs )
		{
			mBits = rhs;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray8& BitArray8::operator = ( const BitArray8& rhs )
		{
			mBits = rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray8::operator [] ( int index ) const
		{
			DIA_ASSERT( index >= 0, "Index outbound bitflag");

			return GetBit(static_cast<unsigned int>(index));
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray8::operator []( unsigned int index ) const
		{
			return GetBit(static_cast<unsigned int>(index));
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray8::operator ==( const BitArray8& rhs )const
		{
			return mBits == rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray8::operator != ( const BitArray8& rhs )const
		{
			return mBits != rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray8& BitArray8::operator |= ( const BitArray8& rhs )
		{
			mBits |= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray8& BitArray8::operator &= ( const BitArray8& rhs )
		{
			mBits &= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray8& BitArray8::operator ^= ( const BitArray8& rhs )
		{
			mBits ^= rhs.mBits;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray8 BitArray8::operator | ( const BitArray8& rhs )
		{
			return mBits | rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray8 BitArray8::operator & ( const BitArray8& rhs )
		{
			return mBits & rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		BitArray8 BitArray8::operator ^ ( const BitArray8& rhs )
		{
			return mBits ^ rhs.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray8::Invert()
		{
			static const BitArray8 OnMask( 0xFF );

			mBits ^= OnMask.mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray8::Clear()
		{
			mBits = 0;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray8::SetAllBits( const unsigned char bits )
		{
			mBits = bits;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned char BitArray8::GetAllBits()const
		{
			return mBits;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray8::IsAllOn()const
		{
			static const BitArray8 OnMask( 0xFF );

			return *this == OnMask;
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray8::IsAllOff()const
		{
			static const BitArray8 msOffMask = 0x00;

			return *this == msOffMask;
		}

		//-----------------------------------------------------------------------------
		inline
		void BitArray8::SetBit( unsigned int index, bool value )
		{
			unsigned char mask = GetBitMaskFromOffset( index );

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
		void BitArray8::ToggleBit( unsigned int index )
		{
			unsigned char mask = GetBitMaskFromOffset( index );
			mBits ^= mask;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned char BitArray8::GetBitMaskFromOffset( unsigned int offset )
		{
			static unsigned char lookupTable[ kMaxNumberOfBits ] = { 1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7 };

			DIA_ASSERT( offset < kMaxNumberOfBits, "Offset is bigger then Max Size");

			return lookupTable[ offset ];
		}

		//-----------------------------------------------------------------------------
		inline
		const BitArray8& BitArray8::GetBitFlagMaskFromOffset( unsigned int offset )
		{
			static BitArray8 lookupTable[ kMaxNumberOfBits ] = {	BitArray8(1 << 0), 
				BitArray8(1 << 1),
				BitArray8(1 << 2),
				BitArray8(1 << 3),
				BitArray8(1 << 4),
				BitArray8(1 << 5),
				BitArray8(1 << 6),
				BitArray8(1 << 7) };

			DIA_ASSERT( offset < kMaxNumberOfBits, "Offset is bigger then Max Size");

			return lookupTable[ offset ];
		}

		//-----------------------------------------------------------------------------
		inline
		bool BitArray8::GetBit( unsigned int index )const
		{
			DIA_ASSERT(index < kMaxNumberOfBits, "Outbounded bitflag");

			unsigned char mask = GetBitMaskFromOffset( index );

			return (mBits & mask) == mask;
		}

		//-----------------------------------------------------------------------------
		inline
		unsigned int BitArray8::GetNumberBitsSetOn()const
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
	}
}