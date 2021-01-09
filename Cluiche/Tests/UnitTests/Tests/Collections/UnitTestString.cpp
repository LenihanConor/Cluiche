
#include "UnitTests/Tests/Collections/UnitTestString.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaCore/Strings/String8.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String1024.h>

using namespace Dia::Core::Containers;

namespace UnitTests
{	
	UnitTestString8::UnitTestString8(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestString8::UnitTestString8(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestString8::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::Containers::String8 str;

			UNIT_TEST_POSITIVE(str.Length() == 0, "String8");
			UNIT_TEST_POSITIVE(str.Size() == 8, "String8");
			UNIT_TEST_POSITIVE(str.Back() == '\0', "String8");
			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 0, "String8");
			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String8");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String8 str(testStr);

			UNIT_TEST_POSITIVE(str.Length() == 7, "String8");
			UNIT_TEST_POSITIVE(str.Size() == 8, "String8");
			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 7, "String8");
			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String8");
			UNIT_TEST_POSITIVE(strcmp(str.AsCStr(), "I like ") == 0, "String8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String8 str1(testStr);
			Dia::Core::Containers::String8 str2(str1);

			UNIT_TEST_POSITIVE(str2.Length() == 7, "String8");
			UNIT_TEST_POSITIVE(str2.Size() == 8, "String8");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 7, "String8");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String8");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like ") == 0, "String8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String8 str1(testStr);
			Dia::Core::Containers::String8 str2(str1, 3, 3);
			
			UNIT_TEST_POSITIVE(str2.Length() == 3, "String8");
			UNIT_TEST_POSITIVE(str2.Size() == 8, "String8");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 3, "String8");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String8");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "ike") == 0, "String8");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String32 str1(testStr);
			Dia::Core::Containers::String8 str2(str1);

			UNIT_TEST_POSITIVE(str2.Length() == 7, "String8");
			UNIT_TEST_POSITIVE(str2.Size() == 8, "String8");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 7, "String8");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String8");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like ") == 0, "String8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String8 str1(testStr);
			Dia::Core::Containers::String8 str2(str1.IteratorAtConst(2));

			UNIT_TEST_POSITIVE(str2.Length() == 5, "String8");
			UNIT_TEST_POSITIVE(str2.Size() == 8, "String8");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String8");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "like ") == 0, "String8");

		UNIT_TEST_BLOCK_END()


		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String8 str1(testStr);
			Dia::Core::Containers::String8 str2(str1.ReverseIteratorAtConst(7));

			UNIT_TEST_POSITIVE(str2.Length() == 7, "String8");
			UNIT_TEST_POSITIVE(str2.Size() == 8, "String8");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String8");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), " ekil I") == 0, "String8");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
		
			class NoSpaces
			{
			public:
				bool Evaluate(const char& currentIter)const
				{
					return (currentIter != ' ');
				};
			};

			NoSpaces filter1;
			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String8 str1(testStr);
			Dia::Core::Containers::String8 str2(str1.IteratorAtConst(0), filter1);

			UNIT_TEST_POSITIVE(str2.Length() == 5, "String8");
			UNIT_TEST_POSITIVE(str2.Size() == 8, "String8");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String8");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "Ilike") == 0, "String8");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String8 str1("I%s%d", "like", 7);
			
			UNIT_TEST_POSITIVE(str1.Length() == 6, "String8");
			UNIT_TEST_POSITIVE(str1.Size() == 8, "String8");
			UNIT_TEST_POSITIVE(str1.IsNullTerminating(), "String8");
			UNIT_TEST_POSITIVE(strcmp(str1.AsCStr(), "Ilike7") == 0, "String8");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}



	UnitTestString32::UnitTestString32(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestString32::UnitTestString32(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestString32::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::Containers::String32 str;

			UNIT_TEST_POSITIVE(str.Length() == 0, "String32");
			UNIT_TEST_POSITIVE(str.Size() == 32, "String32");
			UNIT_TEST_POSITIVE(str.Back() == '\0', "String32");
			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 0, "String32");
			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String32");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String32 str(testStr);

			UNIT_TEST_POSITIVE(str.Length() == 21, "String32");
			UNIT_TEST_POSITIVE(str.Size() == 32, "String32");
			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 21, "String32");
			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String32");
			UNIT_TEST_POSITIVE(strcmp(str.AsCStr(), "I like lots of apples") == 0, "String32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String32 str1(testStr);
			Dia::Core::Containers::String32 str2(str1);

			UNIT_TEST_POSITIVE(str2.Length() == 21, "String32");
			UNIT_TEST_POSITIVE(str2.Size() == 32, "String32");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 21, "String32");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String32");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like lots of apples") == 0, "String32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String32 str1(testStr);
			Dia::Core::Containers::String32 str2(str1, 3, 3);
			
			UNIT_TEST_POSITIVE(str2.Length() == 3, "String32");
			UNIT_TEST_POSITIVE(str2.Size() == 32, "String32");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 3, "String32");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String32");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "ike") == 0, "String32");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String32 str1(testStr);
			Dia::Core::Containers::String32 str2(str1);

			UNIT_TEST_POSITIVE(str2.Length() == 21, "String32");
			UNIT_TEST_POSITIVE(str2.Size() == 32, "String32");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 21, "String32");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String32");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like lots of apples") == 0, "String32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String32 str1(testStr);
			Dia::Core::Containers::String32 str2(str1.IteratorAtConst(2));

			UNIT_TEST_POSITIVE(str2.Length() == 19, "String32");
			UNIT_TEST_POSITIVE(str2.Size() == 32, "String32");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String32");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "like lots of apples") == 0, "String32");

		UNIT_TEST_BLOCK_END()


		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String32 str1(testStr);
			Dia::Core::Containers::String32 str2(str1.ReverseIteratorAtConst(7));

			UNIT_TEST_POSITIVE(str2.Length() == 8, "String32");
			UNIT_TEST_POSITIVE(str2.Size() == 32, "String32");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String32");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "l ekil I") == 0, "String32");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
		
			class NoSpaces
			{
			public:
				bool Evaluate(const char& currentIter)const
				{
					return (currentIter != ' ');
				};
			};

			NoSpaces filter1;
			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String32 str1(testStr);
			Dia::Core::Containers::String32 str2(str1.IteratorAtConst(0), filter1);

			UNIT_TEST_POSITIVE(str2.Length() == 17, "String32");
			UNIT_TEST_POSITIVE(str2.Size() == 32, "String32");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String32");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "Ilikelotsofapples") == 0, "String32");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String32 str1("I%s%d", "like", 7);
			
			UNIT_TEST_POSITIVE(str1.Length() == 6, "String32");
			UNIT_TEST_POSITIVE(str1.Size() == 32, "String32");
			UNIT_TEST_POSITIVE(str1.IsNullTerminating(), "String32");
			UNIT_TEST_POSITIVE(strcmp(str1.AsCStr(), "Ilike7") == 0, "String32");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}



 	UnitTestString64::UnitTestString64(const Dia::Core::Containers::String32& name)
	 	: UnitTestCoreContainers(name)
 	{}
 
 	UnitTestString64::UnitTestString64(void)
 		: UnitTestCoreContainers()
 	{}
 
 	void UnitTestString64::DoTest()
 	{
 		UNIT_TEST_BLOCK_START()
 
 			Dia::Core::Containers::String64 str;
 
 			UNIT_TEST_POSITIVE(str.Length() == 0, "String64");
 			UNIT_TEST_POSITIVE(str.Size() == 64, "String64");
 			UNIT_TEST_POSITIVE(str.Back() == '\0', "String64");
 			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 0, "String64");
 			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String64");
 
 		UNIT_TEST_BLOCK_END()
 
 		UNIT_TEST_BLOCK_START()
 
 			const char testStr[] = "I like lots of apples";
 
 			Dia::Core::Containers::String64 str(testStr);
 
 			UNIT_TEST_POSITIVE(str.Length() == 21, "String64");
 			UNIT_TEST_POSITIVE(str.Size() == 64, "String64");
 			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 21, "String64");
 			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String64");
 			UNIT_TEST_POSITIVE(strcmp(str.AsCStr(), "I like lots of apples") == 0, "String64");
 
 		UNIT_TEST_BLOCK_END()
 
 		UNIT_TEST_BLOCK_START()
 
 			const char testStr[] = "I like lots of apples";
 
 			Dia::Core::Containers::String64 str1(testStr);
 			Dia::Core::Containers::String64 str2(str1);
 
 			UNIT_TEST_POSITIVE(str2.Length() == 21, "String64");
 			UNIT_TEST_POSITIVE(str2.Size() == 64, "String64");
 			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 21, "String64");
 			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String64");
 			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like lots of apples") == 0, "String64");
 
 		UNIT_TEST_BLOCK_END()
 
 		UNIT_TEST_BLOCK_START()
 
 			const char testStr[] = "I like lots of apples";
 
 			Dia::Core::Containers::String64 str1(testStr);
 			Dia::Core::Containers::String64 str2(str1, 3, 3);
 
 			UNIT_TEST_POSITIVE(str2.Length() == 3, "String64");
 			UNIT_TEST_POSITIVE(str2.Size() == 64, "String64");
 			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 3, "String64");
 			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String64");
 			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "ike") == 0, "String64");
 
 		UNIT_TEST_BLOCK_END()
 
 		UNIT_TEST_BLOCK_START()
 
 			const char testStr[] = "I like lots of apples";
 
 			Dia::Core::Containers::String64 str1(testStr);
 			Dia::Core::Containers::String64 str2(str1);
 
 			UNIT_TEST_POSITIVE(str2.Length() == 21, "String64");
 			UNIT_TEST_POSITIVE(str2.Size() == 64, "String64");
 			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 21, "String64");
 			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String64");
 			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like lots of apples") == 0, "String64");
 
 		UNIT_TEST_BLOCK_END()
 
 		UNIT_TEST_BLOCK_START()
 
 			const char testStr[] = "I like lots of apples";
 
 			Dia::Core::Containers::String64 str1(testStr);
 			Dia::Core::Containers::String64 str2(str1.IteratorAtConst(2));
 
 			UNIT_TEST_POSITIVE(str2.Length() == 19, "String64");
 			UNIT_TEST_POSITIVE(str2.Size() == 64, "String64");
 			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String64");
 			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "like lots of apples") == 0, "String64");
 
 		UNIT_TEST_BLOCK_END()
 
 		UNIT_TEST_BLOCK_START()
 
 			const char testStr[] = "I like lots of apples";
 
 			Dia::Core::Containers::String64 str1(testStr);
 			Dia::Core::Containers::String64 str2(str1.ReverseIteratorAtConst(7));
 
 			UNIT_TEST_POSITIVE(str2.Length() == 8, "String64");
 			UNIT_TEST_POSITIVE(str2.Size() == 64, "String64");
 			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String64");
 			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "l ekil I") == 0, "String64");
 
 		UNIT_TEST_BLOCK_END()
 
 		UNIT_TEST_BLOCK_START()
 
 			class NoSpaces
 			{
 			public:
 				bool Evaluate(const char& currentIter)const
 				{
 					return (currentIter != ' ');
 				};
 			};
 
 			NoSpaces filter1;
 			const char testStr[] = "I like lots of apples";
 
 			Dia::Core::Containers::String64 str1(testStr);
 			Dia::Core::Containers::String64 str2(str1.IteratorAtConst(0), filter1);
 
 			UNIT_TEST_POSITIVE(str2.Length() == 17, "String64");
 			UNIT_TEST_POSITIVE(str2.Size() == 64, "String64");
 			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String64");
 			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "Ilikelotsofapples") == 0, "String64");
 
 		UNIT_TEST_BLOCK_END()
 
 		UNIT_TEST_BLOCK_START()
 
 			const char testStr[] = "I like lots of apples";
 
 			Dia::Core::Containers::String64 str1("I%s%d", "like", 7);
 
 			UNIT_TEST_POSITIVE(str1.Length() == 6, "String64");
 			UNIT_TEST_POSITIVE(str1.Size() == 64, "String64");
 			UNIT_TEST_POSITIVE(str1.IsNullTerminating(), "String64");
 			UNIT_TEST_POSITIVE(strcmp(str1.AsCStr(), "Ilike7") == 0, "String64");
 
 		UNIT_TEST_BLOCK_END()
 
 		mState = kFinished;
	}


	UnitTestString128::UnitTestString128(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestString128::UnitTestString128(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestString128::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			Dia::Core::Containers::String128 str;

			UNIT_TEST_POSITIVE(str.Length() == 0, "String128");
			UNIT_TEST_POSITIVE(str.Size() == 128, "String128");
			UNIT_TEST_POSITIVE(str.Back() == '\0', "String128");
			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 0, "String128");
			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String128");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String128 str(testStr);

			UNIT_TEST_POSITIVE(str.Length() == 21, "String128");
			UNIT_TEST_POSITIVE(str.Size() == 128, "String128");
			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 21, "String128");
			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String128");
			UNIT_TEST_POSITIVE(strcmp(str.AsCStr(), "I like lots of apples") == 0, "String128");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String128 str1(testStr);
			Dia::Core::Containers::String128 str2(str1);

			UNIT_TEST_POSITIVE(str2.Length() == 21, "String128");
			UNIT_TEST_POSITIVE(str2.Size() == 128, "String128");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 21, "String128");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String128");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like lots of apples") == 0, "String128");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String128 str1(testStr);
			Dia::Core::Containers::String128 str2(str1, 3, 3);

			UNIT_TEST_POSITIVE(str2.Length() == 3, "String128");
			UNIT_TEST_POSITIVE(str2.Size() == 128, "String128");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 3, "String128");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String128");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "ike") == 0, "String128");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String128 str1(testStr);
			Dia::Core::Containers::String128 str2(str1);

			UNIT_TEST_POSITIVE(str2.Length() == 21, "String128");
			UNIT_TEST_POSITIVE(str2.Size() == 128, "String128");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 21, "String128");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String128");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like lots of apples") == 0, "String128");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String128 str1(testStr);
			Dia::Core::Containers::String128 str2(str1.IteratorAtConst(2));

			UNIT_TEST_POSITIVE(str2.Length() == 19, "String128");
			UNIT_TEST_POSITIVE(str2.Size() == 128, "String128");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String128");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "like lots of apples") == 0, "String128");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String128 str1(testStr);
			Dia::Core::Containers::String128 str2(str1.ReverseIteratorAtConst(7));

			UNIT_TEST_POSITIVE(str2.Length() == 8, "String128");
			UNIT_TEST_POSITIVE(str2.Size() == 128, "String128");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String128");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "l ekil I") == 0, "String128");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			class NoSpaces
			{
			public:
				bool Evaluate(const char& currentIter)const
				{
					return (currentIter != ' ');
				};
			};

			NoSpaces filter1;
			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String128 str1(testStr);
			Dia::Core::Containers::String128 str2(str1.IteratorAtConst(0), filter1);

			UNIT_TEST_POSITIVE(str2.Length() == 17, "String128");
			UNIT_TEST_POSITIVE(str2.Size() == 128, "String128");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String128");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "Ilikelotsofapples") == 0, "String128");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String128 str1("I%s%d", "like", 7);

			UNIT_TEST_POSITIVE(str1.Length() == 6, "String128");
			UNIT_TEST_POSITIVE(str1.Size() == 128, "String128");
			UNIT_TEST_POSITIVE(str1.IsNullTerminating(), "String128");
			UNIT_TEST_POSITIVE(strcmp(str1.AsCStr(), "Ilike7") == 0, "String128");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}




	UnitTestString1024::UnitTestString1024(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestString1024::UnitTestString1024(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestString1024::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			Dia::Core::Containers::String1024 str;

			UNIT_TEST_POSITIVE(str.Length() == 0, "String1024");
			UNIT_TEST_POSITIVE(str.Size() == 1024, "String1024");
			UNIT_TEST_POSITIVE(str.Back() == '\0', "String1024");
			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 0, "String1024");
			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String1024");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String1024 str(testStr);

			UNIT_TEST_POSITIVE(str.Length() == 21, "String1024");
			UNIT_TEST_POSITIVE(str.Size() == 1024, "String1024");
			UNIT_TEST_POSITIVE(strlen(str.AsCStr()) == 21, "String1024");
			UNIT_TEST_POSITIVE(str.IsNullTerminating(), "String1024");
			UNIT_TEST_POSITIVE(strcmp(str.AsCStr(), "I like lots of apples") == 0, "String1024");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String1024 str1(testStr);
			Dia::Core::Containers::String1024 str2(str1);

			UNIT_TEST_POSITIVE(str2.Length() == 21, "String1024");
			UNIT_TEST_POSITIVE(str2.Size() == 1024, "String1024");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 21, "String1024");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String1024");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like lots of apples") == 0, "String1024");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String1024 str1(testStr);
			Dia::Core::Containers::String1024 str2(str1, 3, 3);

			UNIT_TEST_POSITIVE(str2.Length() == 3, "String1024");
			UNIT_TEST_POSITIVE(str2.Size() == 1024, "String1024");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 3, "String1024");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String1024");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "ike") == 0, "String1024");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String1024 str1(testStr);
			Dia::Core::Containers::String1024 str2(str1);

			UNIT_TEST_POSITIVE(str2.Length() == 21, "String1024");
			UNIT_TEST_POSITIVE(str2.Size() == 1024, "String1024");
			UNIT_TEST_POSITIVE(strlen(str2.AsCStr()) == 21, "String1024");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String1024");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "I like lots of apples") == 0, "String1024");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String1024 str1(testStr);
			Dia::Core::Containers::String1024 str2(str1.IteratorAtConst(2));

			UNIT_TEST_POSITIVE(str2.Length() == 19, "String1024");
			UNIT_TEST_POSITIVE(str2.Size() == 1024, "String1024");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String1024");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "like lots of apples") == 0, "String1024");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String1024 str1(testStr);
			Dia::Core::Containers::String1024 str2(str1.ReverseIteratorAtConst(7));

			UNIT_TEST_POSITIVE(str2.Length() == 8, "String1024");
			UNIT_TEST_POSITIVE(str2.Size() == 1024, "String1024");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String1024");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "l ekil I") == 0, "String1024");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			class NoSpaces
			{
			public:
				bool Evaluate(const char& currentIter)const
				{
					return (currentIter != ' ');
				};
			};

			NoSpaces filter1;
			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String1024 str1(testStr);
			Dia::Core::Containers::String1024 str2(str1.IteratorAtConst(0), filter1);

			UNIT_TEST_POSITIVE(str2.Length() == 17, "String1024");
			UNIT_TEST_POSITIVE(str2.Size() == 1024, "String1024");
			UNIT_TEST_POSITIVE(str2.IsNullTerminating(), "String1024");
			UNIT_TEST_POSITIVE(strcmp(str2.AsCStr(), "Ilikelotsofapples") == 0, "String1024");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			const char testStr[] = "I like lots of apples";

			Dia::Core::Containers::String1024 str1("I%s%d", "like", 7);

			UNIT_TEST_POSITIVE(str1.Length() == 6, "String1024");
			UNIT_TEST_POSITIVE(str1.Size() == 1024, "String1024");
			UNIT_TEST_POSITIVE(str1.IsNullTerminating(), "String1024");
			UNIT_TEST_POSITIVE(strcmp(str1.AsCStr(), "Ilike7") == 0, "String1024");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}


	UnitTestStringCommon::UnitTestStringCommon(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestStringCommon::UnitTestStringCommon(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestStringCommon::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			String8 str1("The");
			String8 str2(" A");
			str1.Append(str2);
			UNIT_TEST_POSITIVE(str1 == "The A", "String Append");

			String8 str3("The");
			String32 str4(" B");
			str3.Append(str4);
			UNIT_TEST_POSITIVE(str3 == "The B", "String Append");

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str5("This ");
			String8 str6(" World!");
			str5.Append(str6);
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			String8 str1("The");
			String8 str2(" A Cat");
			str1.Append(str2, 2, 4);
			UNIT_TEST_POSITIVE(str1 == "The Cat", "String Append");

			String8 str3("The");
			String32 str4(" B Fat");
			str3.Append(str4, 2, 4);
			UNIT_TEST_POSITIVE(str3 == "The Fat", "String Append");

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str5("This ");
			String32 str6(" World Is BITCHIN!");
			str5.Append(str6, 2, 7);
			UNIT_TEST_ASSERT_EXPECTED_END()
			
			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str7("This ");
			String8 str8(" World Is BITCHIN!");
			str7.Append(str8, 9, 2);
			UNIT_TEST_ASSERT_EXPECTED_END()
			
			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str9("This ");
			String8 str10(" World Is BITCHIN!");
			str9.Append(str10, 1, 9);
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			String8 str1("The");
			String8 str2(" A Cat");
			str1.Append(str2.AsCStr(), 4);
			UNIT_TEST_POSITIVE(str1 == "The A C", "String Append");

			String8 str3("The");
			String32 str4(" B Fat");
			str3.Append(str4.AsCStr(), 4);
			UNIT_TEST_POSITIVE(str3 == "The B F", "String Append");

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str5("This ");
			String32 str6(" World Is BITCHIN!");
			str5.Append(str6.AsCStr(), 30);
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			String8 str1("The");
			String8 str2(" A");
			str1.Append(str2.AsCStr());
			UNIT_TEST_POSITIVE(str1 == "The A", "String Append");

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str5("This ");
			String32 str6(" World Is BITCHIN!");
			str5.Append(str6.AsCStr());
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			String8 str1("The");
			String32 str2(" WORLD IS BITCHIN");
			str1.AppendAsMuchAsCan(str2);
			UNIT_TEST_POSITIVE(str1 == "The WOR", "String Append");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			String8 str1("The");
			String32 str2(" WORLD IS BITCHIN");
			str1.AppendAsMuchAsCan(str2, 7, 9);
			UNIT_TEST_POSITIVE(str1 == "TheIS B", "String Append");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			String8 str1("The");
			String32 str2(" WORLD IS BITCHIN");
			str1.AppendAsMuchAsCan(str2.AsCStr(), 12);
			UNIT_TEST_POSITIVE(str1 == "The WOR", "String Append");
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			String8 str1("The");
			String32 str2(" WORLD IS BITCHIB");
			str1.AppendAsMuchAsCan(str2.AsCStr());
			UNIT_TEST_POSITIVE(str1 == "The WOR", "String Append");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
		
			{
				String8 str1("The");
				String8 str2("Cat");
				str2 = str1;
				UNIT_TEST_POSITIVE(str2 == "The", "String Append");
			}
			
			{
				String8 str1("The");
				String8 str2("Cat");
				str2 = str1.AsCStr();
				UNIT_TEST_POSITIVE(str2 == "The", "String Append");
			}

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			{
				String8 str1("The");
				String8 str2(" Car");
				String8 str3(str1 + str2);
				UNIT_TEST_POSITIVE(str1 == "The", "String Append");
				UNIT_TEST_POSITIVE(str2 == " Car", "String Append");
				UNIT_TEST_POSITIVE(str3 == "The Car", "String Append");
			}

			{
				String8 str1("The");
				String32 str2(" Car is hot");
				String32 str3(str2 + str1);
				UNIT_TEST_POSITIVE(str1 == "The", "String Append");
				UNIT_TEST_POSITIVE(str2 == " Car is hot", "String Append");
				UNIT_TEST_POSITIVE(str3 == " Car is hotThe", "String Append");
			}

			{
				String8 str1("The");
				String32 str2(" Car is hot");
				String64 str3(str2 + str1);
				UNIT_TEST_POSITIVE(str1 == "The", "String Append");
				UNIT_TEST_POSITIVE(str2 == " Car is hot", "String Append");
				UNIT_TEST_POSITIVE(str3 == " Car is hotThe", "String Append");
			}
			
			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The pink");
			String8 str2(" Car is");
			String8 str3(str1 + str2);
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			{
				String8 str1("The");
				String8 str2(" Car");
				str1 += str2;
				UNIT_TEST_POSITIVE(str1 == "The Car", "String Append");
				UNIT_TEST_POSITIVE(str2 == " Car", "String Append");
			}

			{
				String8 str1("The");
				String32 str2(" Car is hot");
				str2 += str1;
				UNIT_TEST_POSITIVE(str1 == "The", "String Append");
				UNIT_TEST_POSITIVE(str2 == " Car is hotThe", "String Append");
			}

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The pink");
			String8 str2(" Car is");
			str2 += str1;
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			{
				String8 str1("The");
				String8 str2("The");
				UNIT_TEST_POSITIVE(str1 == str2, "String Append");
			}
			
			{
				String8 str1("The");
				String8 str2("Car");
				UNIT_TEST_POSITIVE(str1 != str2, "String Append");
			}
			
			{
				String8 str1("The");
				String32 str2("The");
				UNIT_TEST_POSITIVE(str1 == str2, "String Append");
			}

			{
				String8 str1("The");
				String32 str2("Car");
				UNIT_TEST_POSITIVE(str1 != str2, "String Append");
			}
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			{
				String8 str1("The");
				String8 str2("The");
				UNIT_TEST_POSITIVE(str1 == str2, "String Append");
			}

			{
				String8 str1("The");
				String8 str2("Car");
				UNIT_TEST_POSITIVE(str1 != str2, "String Append");
			}

			{
				String8 str1("The");
				String32 str2("The");
				UNIT_TEST_POSITIVE(str1 == str2, "String Append");
			}

			{
				String8 str1("The");
				String32 str2("Car");
				UNIT_TEST_POSITIVE(str1 != str2, "String Append");
			}
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			String8 str1("The");
			str1.ToUpperCase();
			UNIT_TEST_POSITIVE(str1 == "THE", "String Append");
			str1.ToLowerCase();
			UNIT_TEST_POSITIVE(str1 == "the", "String Append");
			
			String8 str2("Th'1e");
			str2.ToUpperCase();
			UNIT_TEST_POSITIVE(str2 == "TH'1E", "String Append");
			str2.ToLowerCase();
			UNIT_TEST_POSITIVE(str2 == "th'1e", "String Append");
			
			String8 str3("Th'1e");
			String8 str4;
			str3.ToUpperCase(str4);
			UNIT_TEST_POSITIVE(str3 == "Th'1e", "String Append");
			UNIT_TEST_POSITIVE(str4 == "TH'1E", "String Append");
			
			String8 str5("Th'1e");
			String8 str6;
			str5.ToLowerCase(str6);
			UNIT_TEST_POSITIVE(str5 == "Th'1e", "String Append");
			UNIT_TEST_POSITIVE(str6 == "th'1e", "String Append");
		
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			String8 str1("The");
			String8 str2("tHE");
			UNIT_TEST_POSITIVE(str1.CompareNoCase(str2), "String Append");
			UNIT_TEST_POSITIVE(str1.CompareNoCase(str2.AsCStr()), "String Append");

			String8 str3("Th1'e");
			String8 str4("tH1'E");
			UNIT_TEST_POSITIVE(str3.CompareNoCase(str4), "String Append");
			UNIT_TEST_POSITIVE(str3.CompareNoCase(str4.AsCStr()), "String Append");
			
		UNIT_TEST_BLOCK_END()		

		UNIT_TEST_BLOCK_START()

			String8 str1("The Car");
			str1.Trim(4, 2);
			UNIT_TEST_POSITIVE(str1 == "Ca", "String Append");
			
			String8 str2("The Car");
			str2.Trim(4);
			UNIT_TEST_POSITIVE(str2 == "Car", "String Append");
			
			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The Car");
			str2.Trim(10, 1);
			UNIT_TEST_ASSERT_EXPECTED_END()
			
			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The Car");
			str2.Trim(1, 10);
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()		
		
		UNIT_TEST_BLOCK_START()

			String8 str1("The Car");
			String8 str2(str1.SubString(4, 2));
			UNIT_TEST_POSITIVE(str2 == "Ca", "String Append");
			UNIT_TEST_POSITIVE(str1 == "The Car", "String Append");

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The Car");
			String8 str2(str1.SubString(1, 10));
			UNIT_TEST_ASSERT_EXPECTED_END()

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The Car");
			String8 str2(str1.SubString(10, 1));
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			String8 str1("The Car");
			String8 str2(str1.SubString(4, 2));
			UNIT_TEST_POSITIVE(str2 == "Ca", "String Append");
			UNIT_TEST_POSITIVE(str1 == "The Car", "String Append");

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The Car");
			String8 str2(str1.SubString(1, 10));
			UNIT_TEST_ASSERT_EXPECTED_END()

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The Car");
			String8 str2(str1.SubString(10, 1));
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			{
				String8 str1("The Car");
				String8 str2;
				String8 str3;
				str1.Split(4, str2, str3);
				UNIT_TEST_POSITIVE(str3 == "Car", "String Append");
				UNIT_TEST_POSITIVE(str2 == "The ", "String Append");
			}

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The Car");
			String8 str2;
			String8 str3;
			str1.Split(10, str2, str3);
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			{
				String8 str1("The Car");
				String8 str2;
				String8 str3;
				str2 = String8(str1.LeftSubString(4));
				str3 = String8(str1.RightSubString(4));
				UNIT_TEST_POSITIVE(str3 == "Car", "String Append");
				UNIT_TEST_POSITIVE(str2 == "The ", "String Append");
			}

			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The Car");
			String8 str2(str1.LeftSubString(10));
			UNIT_TEST_ASSERT_EXPECTED_END()
			
			UNIT_TEST_ASSERT_EXPECTED_START()
			String8 str1("The Car");
			String8 str2 (str1.RightSubString(10));
			UNIT_TEST_ASSERT_EXPECTED_END()

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			String8 str1("The Cer");
			int index1 = str1.Find('e');
			int index2 = str1.Find('e', 4);
			int index3 = str1.FindLast('e');
			int index4 = str1.FindLast('e', 4);
			UNIT_TEST_POSITIVE(index1 == 2, "String Append");
			UNIT_TEST_POSITIVE(index2 == 5, "String Append");
			UNIT_TEST_POSITIVE(index3 == 5, "String Append");
			UNIT_TEST_POSITIVE(index4 == 2, "String Append");
			
		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
/*				char					AsChar			()const;
				short					AsShort			()const;
				unsigned short			AsUShort		()const;
				int						AsInt			()const;
				unsigned int			AsUInt			()const;
				float					AsFloat			()const;
				s64						As64Int			()const;
				u64						AsU64Int		()const;
				
				String<size>&			FromChar		(char val);
				String<size>&			FromShort		(short val);
				String<size>&			FromUShort		(unsigned short val);
				String<size>&			FromInt			(int val);
				String<size>&			FromUInt		(unsigned int val);
				String<size>&			FromFloat		(float val);
				String<size>&			FromInt64		(s64 val);
				String<size>&			FromUInt64		(u64 val);*/