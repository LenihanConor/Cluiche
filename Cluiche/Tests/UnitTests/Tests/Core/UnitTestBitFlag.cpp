
#include "UnitTests/Tests/Core/UnitTestBitFlag.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaCore/Containers/BitFlag/BitArray8.h>
#include <DiaCore/Containers/BitFlag/BitArray16.h>
#include <DiaCore/Containers/BitFlag/BitArray32.h>
#include <DiaCore/Containers/BitFlag/BitArray64.h>

namespace UnitTests
{	
	UnitTestBitArray8::UnitTestBitArray8(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestBitArray8::UnitTestBitArray8(void)
		: UnitTestCore()
	{}

	void UnitTestBitArray8::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag;	
	
			UNIT_TEST_POSITIVE(sizeof(bitflag) == 1, "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag.IsAllOn(), "BitFlag8");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1(0);	

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag8");

			for (unsigned int i = 0; i < Dia::Core::BitArray8::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag1[i], "BitFlag8");
				UNIT_TEST_POSITIVE(!bitflag1.GetBit(i), "BitFlag8");
			}
		
			Dia::Core::BitArray8 bitflag2(1);	

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag8");
			for (unsigned int i = 1; i < Dia::Core::BitArray8::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag2[i], "BitFlag8");
				UNIT_TEST_POSITIVE(!bitflag2.GetBit(i), "BitFlag8");
			}

			Dia::Core::BitArray8 bitflag3(0xFF);	

			UNIT_TEST_POSITIVE(!bitflag3.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag3.IsAllOn(), "BitFlag8");
			for (unsigned int i = 0; i < Dia::Core::BitArray8::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(bitflag3[i], "BitFlag8");
				UNIT_TEST_POSITIVE(bitflag3.GetBit(i), "BitFlag8");
			}
					
			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[-1];
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[8];
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1(1);	
			Dia::Core::BitArray8 bitflag2(bitflag1);

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag8");
			
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag2[1], "BitFlag8");
			
			UNIT_TEST_POSITIVE(bitflag2 == bitflag1, "BitFlag8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1 = 1;	

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1(1);	
			Dia::Core::BitArray8 bitflag2 = bitflag1;

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag8");

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag2[1], "BitFlag8");

			UNIT_TEST_POSITIVE(bitflag2 == bitflag1, "BitFlag8");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1(1);	
			Dia::Core::BitArray8 bitflag2(1);
			Dia::Core::BitArray8 bitflag3(2);

			UNIT_TEST_POSITIVE(bitflag1 == bitflag2, "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag1 != bitflag3, "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag2 != bitflag3, "BitFlag8");
	
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1;	
			Dia::Core::BitArray8 bitflag2(bitflag1);
			
			bitflag2.Invert();

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag2.IsAllOn(), "BitFlag8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1(2);	
			
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag8");

			bitflag1.Clear();

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1;
			bitflag1.SetAllBits(0);

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag8");

			for (unsigned int i = 0; i < Dia::Core::BitArray8::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag1[i], "BitFlag8");
				UNIT_TEST_POSITIVE(!bitflag1.GetBit(i), "BitFlag8");
			}


			Dia::Core::BitArray8 bitflag2;	
			bitflag2.SetAllBits(1);

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag8");
			for (unsigned int i = 1; i < Dia::Core::BitArray8::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag2[i], "BitFlag8");
				UNIT_TEST_POSITIVE(!bitflag2.GetBit(i), "BitFlag8");
			}

			Dia::Core::BitArray8 bitflag3(0xFF);	
			bitflag3.SetAllBits(0xFF);

			UNIT_TEST_POSITIVE(!bitflag3.IsAllOff(), "BitFlag8");
			UNIT_TEST_POSITIVE(bitflag3.IsAllOn(), "BitFlag8");
			for (unsigned int i = 0; i < Dia::Core::BitArray8::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(bitflag3[i], "BitFlag8");
				UNIT_TEST_POSITIVE(bitflag3.GetBit(i), "BitFlag8");
			}

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[-1];
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[8];
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1;
					
			bitflag1.SetBit(0, true);
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag8");
		
			bitflag1.SetBit(0, false);
			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag8");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1;

			bitflag1.ToggleBit(0);
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag8");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag8");

			bitflag1.ToggleBit(0);
			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray8 bitflag1(1);
			UNIT_TEST_POSITIVE(bitflag1.GetNumberBitsSetOn() == 1, "BitFlag8");
			
			Dia::Core::BitArray8 bitflag2(0);
			UNIT_TEST_POSITIVE(bitflag2.GetNumberBitsSetOn() == 0, "BitFlag8");

			Dia::Core::BitArray8 bitflag3(0xFF);
			UNIT_TEST_POSITIVE(bitflag3.GetNumberBitsSetOn() == 8, "BitFlag8");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray8 bitflag1(1);
			Dia::Core::BitArray8 bitflag2(2);
			
			unsigned char bit3 = bit1 | bit2;
			Dia::Core::BitArray8 bitflag3 = bitflag1 | bitflag2;

			unsigned char bit4 = bit1 & bit2;
			Dia::Core::BitArray8 bitflag4 = bitflag1 & bitflag2;

			unsigned char bit5 = bit1 ^ bit2;
			Dia::Core::BitArray8 bitflag5 = bitflag1 ^ bitflag2;

			UNIT_TEST_POSITIVE(bit3 == bitflag3.GetAllBits(), "BitFlag8");
			UNIT_TEST_POSITIVE(bit4 == bitflag4.GetAllBits(), "BitFlag8");
			UNIT_TEST_POSITIVE(bit5 == bitflag5.GetAllBits(), "BitFlag8");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray8 bitflag1(1);
			Dia::Core::BitArray8 bitflag2(2);

			bit1 |= bit2;
			bitflag1 |= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray8 bitflag1(1);
			Dia::Core::BitArray8 bitflag2(2);

			bit1 &= bit2;
			bitflag1 &= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag8");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray8 bitflag1(1);
			Dia::Core::BitArray8 bitflag2(2);

			bit1 ^= bit2;
			bitflag1 ^= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag8");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}

























	UnitTestBitArray16::UnitTestBitArray16(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestBitArray16::UnitTestBitArray16(void)
		: UnitTestCore()
	{}

	void UnitTestBitArray16::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag;	

			UNIT_TEST_POSITIVE(sizeof(bitflag) == 2, "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag.IsAllOn(), "BitFlag16");

		UNIT_TEST_BLOCK_END()

			UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1(0);	

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag16");

			for (unsigned int i = 0; i < Dia::Core::BitArray16::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag1[i], "BitFlag16");
				UNIT_TEST_POSITIVE(!bitflag1.GetBit(i), "BitFlag16");
			}


			Dia::Core::BitArray16 bitflag2(1);	

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag16");
			for (unsigned int i = 1; i < Dia::Core::BitArray16::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag2[i], "BitFlag16");
				UNIT_TEST_POSITIVE(!bitflag2.GetBit(i), "BitFlag16");
			}

			Dia::Core::BitArray16 bitflag3(0xFFFF);	

			UNIT_TEST_POSITIVE(!bitflag3.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag3.IsAllOn(), "BitFlag16");
			for (unsigned int i = 0; i < Dia::Core::BitArray16::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(bitflag3[i], "BitFlag16");
				UNIT_TEST_POSITIVE(bitflag3.GetBit(i), "BitFlag16");
			}

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[-1];
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[17];
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1(1);	
			Dia::Core::BitArray16 bitflag2(bitflag1);

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag16");

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag2[1], "BitFlag16");

			UNIT_TEST_POSITIVE(bitflag2 == bitflag1, "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1 = 1;	

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1(1);	
			Dia::Core::BitArray16 bitflag2 = bitflag1;

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag16");

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag2[1], "BitFlag16");

			UNIT_TEST_POSITIVE(bitflag2 == bitflag1, "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1(1);	
			Dia::Core::BitArray16 bitflag2(1);
			Dia::Core::BitArray16 bitflag3(2);

			UNIT_TEST_POSITIVE(bitflag1 == bitflag2, "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag1 != bitflag3, "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag2 != bitflag3, "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1;	
			Dia::Core::BitArray16 bitflag2(bitflag1);

			bitflag2.Invert();

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag2.IsAllOn(), "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1(2);	

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag16");

			bitflag1.Clear();

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1;
			bitflag1.SetAllBits(0);

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag16");

			for (unsigned int i = 0; i < Dia::Core::BitArray16::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag1[i], "BitFlag16");
				UNIT_TEST_POSITIVE(!bitflag1.GetBit(i), "BitFlag16");
			}


			Dia::Core::BitArray16 bitflag2;	
			bitflag2.SetAllBits(1);

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag16");
			for (unsigned int i = 1; i < Dia::Core::BitArray16::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag2[i], "BitFlag16");
				UNIT_TEST_POSITIVE(!bitflag2.GetBit(i), "BitFlag16");
			}

			Dia::Core::BitArray16 bitflag3(0xFFFF);	
			bitflag3.SetAllBits(0xFFFF);

			UNIT_TEST_POSITIVE(!bitflag3.IsAllOff(), "BitFlag16");
			UNIT_TEST_POSITIVE(bitflag3.IsAllOn(), "BitFlag16");
			for (unsigned int i = 0; i < Dia::Core::BitArray16::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(bitflag3[i], "BitFlag16");
				UNIT_TEST_POSITIVE(bitflag3.GetBit(i), "BitFlag16");
			}

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[-1];
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[16];
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1;

			bitflag1.SetBit(0, true);
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag16");

			bitflag1.SetBit(0, false);
			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1;

			bitflag1.ToggleBit(0);
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag16");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag16");

			bitflag1.ToggleBit(0);
			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray16 bitflag1(1);
			UNIT_TEST_POSITIVE(bitflag1.GetNumberBitsSetOn() == 1, "BitFlag16");

			Dia::Core::BitArray16 bitflag2(0);
			UNIT_TEST_POSITIVE(bitflag2.GetNumberBitsSetOn() == 0, "BitFlag16");

			Dia::Core::BitArray16 bitflag3(0xFFFF);
			UNIT_TEST_POSITIVE(bitflag3.GetNumberBitsSetOn() == 16, "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray16 bitflag1(1);
			Dia::Core::BitArray16 bitflag2(2);

			unsigned char bit3 = bit1 | bit2;
			Dia::Core::BitArray16 bitflag3 = bitflag1 | bitflag2;

			unsigned char bit4 = bit1 & bit2;
			Dia::Core::BitArray16 bitflag4 = bitflag1 & bitflag2;

			unsigned char bit5 = bit1 ^ bit2;
			Dia::Core::BitArray16 bitflag5 = bitflag1 ^ bitflag2;

			UNIT_TEST_POSITIVE(bit3 == bitflag3.GetAllBits(), "BitFlag16");
			UNIT_TEST_POSITIVE(bit4 == bitflag4.GetAllBits(), "BitFlag16");
			UNIT_TEST_POSITIVE(bit5 == bitflag5.GetAllBits(), "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray16 bitflag1(1);
			Dia::Core::BitArray16 bitflag2(2);

			bit1 |= bit2;
			bitflag1 |= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray16 bitflag1(1);
			Dia::Core::BitArray16 bitflag2(2);

			bit1 &= bit2;
			bitflag1 &= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag16");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray16 bitflag1(1);
			Dia::Core::BitArray16 bitflag2(2);

			bit1 ^= bit2;
			bitflag1 ^= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag16");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray16 bitflag1(1);

			Dia::Core::BitArray8 bitarray2(bitflag1.GetByte(0));
			Dia::Core::BitArray8 bitarray3(bitflag1.GetByte(1));
				
			UNIT_TEST_POSITIVE(bitarray2[0], "BitFlag16");
			UNIT_TEST_POSITIVE(!bitarray2[1], "BitFlag16");
			UNIT_TEST_POSITIVE(bitarray3.IsAllOff(), "BitFlag16");

		UNIT_TEST_BLOCK_END()
		mState = kFinished;
	}

























	UnitTestBitArray32::UnitTestBitArray32(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestBitArray32::UnitTestBitArray32(void)
		: UnitTestCore()
	{}

	void UnitTestBitArray32::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag;	

			UNIT_TEST_POSITIVE(sizeof(bitflag) == 4, "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag.IsAllOn(), "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1(0);	

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag32");

			for (unsigned int i = 0; i < Dia::Core::BitArray32::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag1[i], "BitFlag32");
				UNIT_TEST_POSITIVE(!bitflag1.GetBit(i), "BitFlag32");
			}


			Dia::Core::BitArray32 bitflag2(1);	

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag32");
			for (unsigned int i = 1; i < Dia::Core::BitArray32::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag2[i], "BitFlag32");
				UNIT_TEST_POSITIVE(!bitflag2.GetBit(i), "BitFlag32");
			}

			Dia::Core::BitArray32 bitflag3(0xFFFFFFFF);	

			UNIT_TEST_POSITIVE(!bitflag3.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag3.IsAllOn(), "BitFlag32");
			for (unsigned int i = 0; i < Dia::Core::BitArray32::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(bitflag3[i], "BitFlag32");
				UNIT_TEST_POSITIVE(bitflag3.GetBit(i), "BitFlag32");
			}

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[-1];
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[32];
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1(1);	
			Dia::Core::BitArray32 bitflag2(bitflag1);

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag32");

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag2[1], "BitFlag32");

			UNIT_TEST_POSITIVE(bitflag2 == bitflag1, "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1 = 1;	

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1(1);	
			Dia::Core::BitArray32 bitflag2 = bitflag1;

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag32");

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag2[1], "BitFlag32");

			UNIT_TEST_POSITIVE(bitflag2 == bitflag1, "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1(1);	
			Dia::Core::BitArray32 bitflag2(1);
			Dia::Core::BitArray32 bitflag3(2);

			UNIT_TEST_POSITIVE(bitflag1 == bitflag2, "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag1 != bitflag3, "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag2 != bitflag3, "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1;	
			Dia::Core::BitArray32 bitflag2(bitflag1);

			bitflag2.Invert();

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag2.IsAllOn(), "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1(2);	

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag32");

			bitflag1.Clear();

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1;
			bitflag1.SetAllBits(0);

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag32");

			for (unsigned int i = 0; i < Dia::Core::BitArray32::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag1[i], "BitFlag32");
				UNIT_TEST_POSITIVE(!bitflag1.GetBit(i), "BitFlag32");
			}


			Dia::Core::BitArray32 bitflag2;	
			bitflag2.SetAllBits(1);

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag32");
			for (unsigned int i = 1; i < Dia::Core::BitArray32::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag2[i], "BitFlag32");
				UNIT_TEST_POSITIVE(!bitflag2.GetBit(i), "BitFlag32");
			}

			Dia::Core::BitArray32 bitflag3(0xFFFFFFFF);	
			bitflag3.SetAllBits(0xFFFFFFFF);

			UNIT_TEST_POSITIVE(!bitflag3.IsAllOff(), "BitFlag32");
			UNIT_TEST_POSITIVE(bitflag3.IsAllOn(), "BitFlag32");
			for (unsigned int i = 0; i < Dia::Core::BitArray32::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(bitflag3[i], "BitFlag32");
				UNIT_TEST_POSITIVE(bitflag3.GetBit(i), "BitFlag32");
			}

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[-1];
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[32];
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1;

			bitflag1.SetBit(0, true);
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag32");

			bitflag1.SetBit(0, false);
			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1;

			bitflag1.ToggleBit(0);
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag32");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag32");

			bitflag1.ToggleBit(0);
			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray32 bitflag1(1);
			UNIT_TEST_POSITIVE(bitflag1.GetNumberBitsSetOn() == 1, "BitFlag32");

			Dia::Core::BitArray32 bitflag2(0);
			UNIT_TEST_POSITIVE(bitflag2.GetNumberBitsSetOn() == 0, "BitFlag32");

			Dia::Core::BitArray32 bitflag3(0xFFFFFFFF);
			UNIT_TEST_POSITIVE(bitflag3.GetNumberBitsSetOn() == 32, "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray32 bitflag1(1);
			Dia::Core::BitArray32 bitflag2(2);

			unsigned char bit3 = bit1 | bit2;
			Dia::Core::BitArray32 bitflag3 = bitflag1 | bitflag2;

			unsigned char bit4 = bit1 & bit2;
			Dia::Core::BitArray32 bitflag4 = bitflag1 & bitflag2;

			unsigned char bit5 = bit1 ^ bit2;
			Dia::Core::BitArray32 bitflag5 = bitflag1 ^ bitflag2;

			UNIT_TEST_POSITIVE(bit3 == bitflag3.GetAllBits(), "BitFlag32");
			UNIT_TEST_POSITIVE(bit4 == bitflag4.GetAllBits(), "BitFlag32");
			UNIT_TEST_POSITIVE(bit5 == bitflag5.GetAllBits(), "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray32 bitflag1(1);
			Dia::Core::BitArray32 bitflag2(2);

			bit1 |= bit2;
			bitflag1 |= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray32 bitflag1(1);
			Dia::Core::BitArray32 bitflag2(2);

			bit1 &= bit2;
			bitflag1 &= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray32 bitflag1(1);
			Dia::Core::BitArray32 bitflag2(2);

			bit1 ^= bit2;
			bitflag1 ^= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray32 bitflag1(1);

			Dia::Core::BitArray8 bitarray2(bitflag1.GetByte(0));
			Dia::Core::BitArray8 bitarray3(bitflag1.GetByte(1));

			UNIT_TEST_POSITIVE(bitarray2[0], "BitFlag32");
			UNIT_TEST_POSITIVE(!bitarray2[1], "BitFlag32");
			UNIT_TEST_POSITIVE(bitarray3.IsAllOff(), "BitFlag32");

		UNIT_TEST_BLOCK_END()
			
		mState = kFinished;
	}


	UnitTestBitArray64::UnitTestBitArray64(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestBitArray64::UnitTestBitArray64(void)
		: UnitTestCore()
	{}

	void UnitTestBitArray64::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag;	

			UNIT_TEST_POSITIVE(sizeof(bitflag) == 8, "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag.IsAllOn(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1(0);	

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag64");

			for (unsigned int i = 0; i < Dia::Core::BitArray64::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag1[i], "BitFlag64");
				UNIT_TEST_POSITIVE(!bitflag1.GetBit(i), "BitFlag64");
			}

			Dia::Core::BitArray64 bitflag2(1);	

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag64");
			for (unsigned int i = 1; i < Dia::Core::BitArray64::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag2[i], "BitFlag64");
				UNIT_TEST_POSITIVE(!bitflag2.GetBit(i), "BitFlag64");
			}

			Dia::Core::BitArray64 bitflag3(0xFFFFFFFFFFFFFFFF);	

			UNIT_TEST_POSITIVE(!bitflag3.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag3.IsAllOn(), "BitFlag64");
			for (unsigned int i = 0; i < Dia::Core::BitArray64::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(bitflag3[i], "BitFlag64");
				UNIT_TEST_POSITIVE(bitflag3.GetBit(i), "BitFlag64");
			}

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[-1];
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[64];
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1(1);	
			Dia::Core::BitArray64 bitflag2(bitflag1);

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag64");

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag2[1], "BitFlag64");

			UNIT_TEST_POSITIVE(bitflag2 == bitflag1, "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1 = 1;	

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1(1);	
			Dia::Core::BitArray64 bitflag2 = bitflag1;

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag64");

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag2[1], "BitFlag64");

			UNIT_TEST_POSITIVE(bitflag2 == bitflag1, "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1(1);	
			Dia::Core::BitArray64 bitflag2(1);
			Dia::Core::BitArray64 bitflag3(2);

			UNIT_TEST_POSITIVE(bitflag1 == bitflag2, "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag1 != bitflag3, "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag2 != bitflag3, "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1;	
			Dia::Core::BitArray64 bitflag2(bitflag1);

			bitflag2.Invert();

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag2.IsAllOn(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1(2);	

			UNIT_TEST_POSITIVE(!bitflag1.IsAllOff(), "BitFlag64");

			bitflag1.Clear();

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1;
			bitflag1.SetAllBits(0);

			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1.IsAllOn(), "BitFlag64");

			for (unsigned int i = 0; i < Dia::Core::BitArray64::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag1[i], "BitFlag64");
				UNIT_TEST_POSITIVE(!bitflag1.GetBit(i), "BitFlag64");
			}


			Dia::Core::BitArray64 bitflag2;	
			bitflag2.SetAllBits(1);

			UNIT_TEST_POSITIVE(!bitflag2.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag2.IsAllOn(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag2[0], "BitFlag64");
			for (unsigned int i = 1; i < Dia::Core::BitArray64::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(!bitflag2[i], "BitFlag64");
				UNIT_TEST_POSITIVE(!bitflag2.GetBit(i), "BitFlag64");
			}

			Dia::Core::BitArray64 bitflag3(0xFFFFFFFFFFFFFFFF);	
			bitflag3.SetAllBits(0xFFFFFFFFFFFFFFFF);

			UNIT_TEST_POSITIVE(!bitflag3.IsAllOff(), "BitFlag64");
			UNIT_TEST_POSITIVE(bitflag3.IsAllOn(), "BitFlag64");
			for (unsigned int i = 0; i < Dia::Core::BitArray64::kMaxNumberOfBits; i++)
			{
				UNIT_TEST_POSITIVE(bitflag3[i], "BitFlag64");
				UNIT_TEST_POSITIVE(bitflag3.GetBit(i), "BitFlag64");
			}

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[-1];
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			bool a = bitflag3[64];
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1;

			bitflag1.SetBit(0, true);
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag64");

			bitflag1.SetBit(0, false);
			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1;

			bitflag1.ToggleBit(0);
			UNIT_TEST_POSITIVE(bitflag1[0], "BitFlag64");
			UNIT_TEST_POSITIVE(!bitflag1[1], "BitFlag64");

			bitflag1.ToggleBit(0);
			UNIT_TEST_POSITIVE(bitflag1.IsAllOff(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Core::BitArray64 bitflag1(1);
			UNIT_TEST_POSITIVE(bitflag1.GetNumberBitsSetOn() == 1, "BitFlag64");

			Dia::Core::BitArray64 bitflag2(0);
			UNIT_TEST_POSITIVE(bitflag2.GetNumberBitsSetOn() == 0, "BitFlag64");

			Dia::Core::BitArray64 bitflag3(0xFFFFFFFFFFFFFFFF);
			UNIT_TEST_POSITIVE(bitflag3.GetNumberBitsSetOn() == 64, "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray64 bitflag1(1);
			Dia::Core::BitArray64 bitflag2(2);

			unsigned char bit3 = bit1 | bit2;
			Dia::Core::BitArray64 bitflag3 = bitflag1 | bitflag2;

			unsigned char bit4 = bit1 & bit2;
			Dia::Core::BitArray64 bitflag4 = bitflag1 & bitflag2;

			unsigned char bit5 = bit1 ^ bit2;
			Dia::Core::BitArray64 bitflag5 = bitflag1 ^ bitflag2;

			UNIT_TEST_POSITIVE(bit3 == bitflag3.GetAllBits(), "BitFlag64");
			UNIT_TEST_POSITIVE(bit4 == bitflag4.GetAllBits(), "BitFlag64");
			UNIT_TEST_POSITIVE(bit5 == bitflag5.GetAllBits(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray64 bitflag1(1);
			Dia::Core::BitArray64 bitflag2(2);

			bit1 |= bit2;
			bitflag1 |= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray64 bitflag1(1);
			Dia::Core::BitArray64 bitflag2(2);

			bit1 &= bit2;
			bitflag1 &= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray64 bitflag1(1);
			Dia::Core::BitArray64 bitflag2(2);

			bit1 ^= bit2;
			bitflag1 ^= bitflag2;

			UNIT_TEST_POSITIVE(bit1 == bitflag1.GetAllBits(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			unsigned char bit1 = 1;
			unsigned char bit2 = 2;
			Dia::Core::BitArray64 bitflag1(1);

			Dia::Core::BitArray8 bitarray2(bitflag1.GetByte(0));
			Dia::Core::BitArray8 bitarray3(bitflag1.GetByte(1));

			UNIT_TEST_POSITIVE(bitarray2[0], "BitFlag64");
			UNIT_TEST_POSITIVE(!bitarray2[1], "BitFlag64");
			UNIT_TEST_POSITIVE(bitarray3.IsAllOff(), "BitFlag64");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}















