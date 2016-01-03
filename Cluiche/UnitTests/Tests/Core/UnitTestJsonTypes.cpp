
#include "UnitTests/Tests/Core/UnitTestJsonTypes.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include "UnitTests/Tests/Core/UnitTestTypes.h"

#include "DiaCore/Type/TypeDeclarationMacros.h"
#include "DiaCore/Type/TypeParameterInput.h"
#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"
#include "DiaCore/Type/TypeFacade.h"
#include "DiaCore/Containers/Strings/StringWriter.h"
#include "DiaCore/Containers/Strings/StringReader.h"
#include "DiaCore/Core/Log.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaCore/Strings/stringutils.h"

namespace UnitTests
{	
	UnitTestJsonTypes::UnitTestJsonTypes(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestJsonTypes::UnitTestJsonTypes(void)
		: UnitTestCore()
	{}

	void UnitTestJsonTypes::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			PrimitiveTypeTest testSerialize;
			testSerialize.mBool = true;
			testSerialize.mChar = 'z';
			testSerialize.mCharUnsigned = 't';
			testSerialize.mShort = -25;
			testSerialize.mShortUnsigned = 32;
			testSerialize.mInt = -256865;
			testSerialize.mIntUnsigned = 8574578;
			testSerialize.mLong = -2548;
			testSerialize.mLongUnsigned = 68547956;
			testSerialize.mLongLong = -85485795;
			testSerialize.mLongLongUnsigned = 3524658754;
			testSerialize.mFloat = 7.5f;
			testSerialize.mDouble = 88.5;

			char bufferMemory[64*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );
		
			PrimitiveTypeTest testDeserialize;
			
			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

			UNIT_TEST_POSITIVE( testDeserialize.mBool == true, "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mChar == 'z', "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mCharUnsigned == 't', "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mShort == -25, "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mShortUnsigned == 32, "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mIntUnsigned == 8574578, "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mLong == -2548, "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mLongUnsigned == 68547956, "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mLongLong == -85485795, "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mLongLongUnsigned == 3524658754, "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mFloat == 7.5f, "PrimitiveTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mDouble == 88.5, "PrimitiveTypeTest");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
	
			ClassTypeTest testSerialize;

			testSerialize.mType1.mBool = true;
			testSerialize.mType1.mChar = 'z';
			testSerialize.mType1.mCharUnsigned = 't';
			testSerialize.mType1.mShort = -25;
			testSerialize.mType1.mShortUnsigned = 32;
			testSerialize.mType1.mInt = -256865;
			testSerialize.mType1.mIntUnsigned = 8574578;
			testSerialize.mType1.mLong = -2548;
			testSerialize.mType1.mLongUnsigned = 68547956;
			testSerialize.mType1.mLongLong = -85485795;
			testSerialize.mType1.mLongLongUnsigned = 3524658754;
			testSerialize.mType1.mFloat = 7.5f;
			testSerialize.mType1.mDouble = 88.5;

			testSerialize.mType2.mBool = false;
			testSerialize.mType2.mChar = 'x';
			testSerialize.mType2.mCharUnsigned = '$';
			testSerialize.mType2.mShort = -2455;
			testSerialize.mType2.mShortUnsigned = 8532;
			testSerialize.mType2.mInt = -255;
			testSerialize.mType2.mIntUnsigned = 4578;
			testSerialize.mType2.mLong = -258898;
			testSerialize.mType2.mLongUnsigned = 687956;
			testSerialize.mType2.mLongLong = -854855;
			testSerialize.mType2.mLongLongUnsigned = 35246754;
			testSerialize.mType2.mFloat = -10.23558f;
			testSerialize.mType2.mDouble = -45.5056;

			char bufferMemory[64*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);
			
			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );
		
			ClassTypeTest testDeserialize;

			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

			UNIT_TEST_POSITIVE( testDeserialize.mType1.mBool == true, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mChar == 'z', "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mCharUnsigned == 't', "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mShort == -25, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mShortUnsigned == 32, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mIntUnsigned == 8574578, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mLong == -2548, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mLongUnsigned == 68547956, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mLongLong == -85485795, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mLongLongUnsigned == 3524658754, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mFloat == 7.5f, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType1.mDouble == 88.5, "ClassTypeTest");

			UNIT_TEST_POSITIVE( testDeserialize.mType2.mBool == false, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mChar == 'x', "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mCharUnsigned == '$', "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mShort == -2455, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mShortUnsigned == 8532, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mIntUnsigned == 4578, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mLong == -258898, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mLongUnsigned == 687956, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mLongLong == -854855, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mLongLongUnsigned == 35246754, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mFloat == -10.23558f, "ClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mDouble == -45.5056, "ClassTypeTest");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			ClassInheritanceBaseTypeTest testSerialize;

			testSerialize.mChar = 'z';
			testSerialize.mFloat = 7.5f;

			char bufferMemory[64*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );

			ClassInheritanceBaseTypeTest testDeserialize;

			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

			UNIT_TEST_POSITIVE( testDeserialize.mChar == 'z', "ClassInheritanceBaseTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mFloat == 7.5f, "ClassInheritanceBaseTypeTest");
		
		UNIT_TEST_BLOCK_END()


		UNIT_TEST_BLOCK_START()

			ClassInheritanceDerivedTypeTest testSerialize;

			testSerialize.mChar = 'z';
			testSerialize.mFloat = 7.5f;
			testSerialize.mShort = 123;
			testSerialize.mInt = -456;

			char bufferMemory[64*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );

			ClassInheritanceDerivedTypeTest testDeserialize;

			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

			UNIT_TEST_POSITIVE( testDeserialize.mChar == 'z', "ClassInheritanceDerivedTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mFloat == 7.5f, "ClassInheritanceDerivedTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mShort == 123, "ClassInheritanceDerivedTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mInt == -456, "ClassInheritanceDerivedTypeTest");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			ClassInheritanceDerivedWithClassTypeTest testSerialize;

			testSerialize.mType1.mFloat = 1.0f;
			testSerialize.mType2.mFloat = 2.0f;
			testSerialize.mType3.mFloat = 3.0f;

			char bufferMemory[64*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );

			ClassInheritanceDerivedWithClassTypeTest testDeserialize;

			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

			UNIT_TEST_POSITIVE( testDeserialize.mType1.mFloat == 1.0f, "ClassInheritanceDerivedWithClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType2.mFloat == 2.0f, "ClassInheritanceDerivedWithClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mType3.mFloat == 3.0f, "ClassInheritanceDerivedWithClassTypeTest");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			ClassVirtualInheritanceDerivedTypeTest testSerialize;

			testSerialize.mChar = 'z';
			testSerialize.mFloat = 7.5f;
			testSerialize.mShort = 123;
			testSerialize.mInt = -456;

			char bufferMemory[64*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );

			ClassVirtualInheritanceDerivedTypeTest testDeserialize;

			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

			UNIT_TEST_POSITIVE( testDeserialize.mChar == 'z', "ClassVirtualInheritanceDerivedTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mFloat == 7.5f, "ClassVirtualInheritanceDerivedTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mShort == 123, "ClassVirtualInheritanceDerivedTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mInt == -456, "ClassVirtualInheritanceDerivedTypeTest");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			PrimitivePointerTypeTest testSerialize;
 			testSerialize.mBool = true;
 			testSerialize.mBoolPtr1 = &testSerialize.mBool;
 			testSerialize.mBoolPtr2 = &testSerialize.mBool;
 			testSerialize.mChar = 'd';
 			testSerialize.mCharPtr1 = &testSerialize.mChar;
 			testSerialize.mCharPtr2 = &testSerialize.mChar;
			testSerialize.mInt = 34;
			testSerialize.mIntPtr1 = &testSerialize.mInt;
			testSerialize.mIntPtr2 = &testSerialize.mInt;

			char bufferMemory[64*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );

			PrimitivePointerTypeTest testDeserialize;

			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

  			UNIT_TEST_POSITIVE( testDeserialize.mBool == true, "PrimitivePointerTypeTest");
  			UNIT_TEST_POSITIVE( *testSerialize.mBoolPtr1 == true, "PrimitivePointerTypeTest");
 			UNIT_TEST_POSITIVE( *testSerialize.mBoolPtr2 == true, "PrimitivePointerTypeTest");
 			UNIT_TEST_POSITIVE( testSerialize.mBoolPtr1 == testSerialize.mBoolPtr2, "PrimitivePointerTypeTest");
 			UNIT_TEST_POSITIVE( &testSerialize.mBool == testSerialize.mBoolPtr2, "PrimitivePointerTypeTest");	
			UNIT_TEST_POSITIVE( testDeserialize.mChar == 'd', "PrimitivePointerTypeTest");
			UNIT_TEST_POSITIVE( *testSerialize.mCharPtr1 == 'd', "PrimitivePointerTypeTest");
			UNIT_TEST_POSITIVE( *testSerialize.mCharPtr2 == 'd', "PrimitivePointerTypeTest");
			UNIT_TEST_POSITIVE( testSerialize.mCharPtr1 == testSerialize.mCharPtr2, "PrimitivePointerTypeTest");
			UNIT_TEST_POSITIVE( &testSerialize.mChar == testSerialize.mCharPtr2, "PrimitivePointerTypeTest");	
			UNIT_TEST_POSITIVE( testDeserialize.mInt == 34, "PrimitivePointerTypeTest");
			UNIT_TEST_POSITIVE( *testSerialize.mIntPtr1 == 34, "PrimitivePointerTypeTest");
			UNIT_TEST_POSITIVE( *testSerialize.mIntPtr2 == 34, "PrimitivePointerTypeTest");
			UNIT_TEST_POSITIVE( testSerialize.mIntPtr1 == testSerialize.mIntPtr2, "PrimitivePointerTypeTest");
			UNIT_TEST_POSITIVE( &testSerialize.mInt == testSerialize.mIntPtr2, "PrimitivePointerTypeTest");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			ClassPointerTypeTest testSerialize;
			testSerialize.mClass.mFloat = -234.0f;
			testSerialize.mClassPtr = &testSerialize.mClass;

			char bufferMemory[64*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );
			
			ClassPointerTypeTest testDeserialize;

			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

 			UNIT_TEST_POSITIVE( testSerialize.mClass.mFloat == -234.0f, "ClassPointerTypeTest");
 			UNIT_TEST_POSITIVE( testSerialize.mClassPtr->mFloat == -234.0f, "ClassPointerTypeTest");
 			UNIT_TEST_POSITIVE( testSerialize.mClassPtr == &testSerialize.mClass, "ClassPointerTypeTest");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			PointerAsObjectTypeTest testSerialize;
			testSerialize.mFloat = DIA_NEW(float);
			testSerialize.mInt = DIA_NEW(int);
			testSerialize.mClass = DIA_NEW(ClassInheritanceSimpleClassTypeTest);

			*testSerialize.mFloat = -3.987f;
			*testSerialize.mInt = -7;
			
			ClassInheritanceSimpleClassTypeTest* a = testSerialize.mClass;
			a->mFloat = 1.2345f;

			char bufferMemory[32*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32*1024);
			
			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );
			 			
			PointerAsObjectTypeTest testDeserialize;
			testDeserialize.mFloat = DIA_NEW(float);
			testDeserialize.mInt = DIA_NEW(int);
			testDeserialize.mClass = DIA_NEW(ClassInheritanceSimpleClassTypeTest);

			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );
			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );
 
 			UNIT_TEST_POSITIVE( *testSerialize.mFloat == -3.987f, "PointerAsObjectTypeTest");
 			UNIT_TEST_POSITIVE( *testSerialize.mInt == -7, "PointerAsObjectTypeTest");
 			UNIT_TEST_POSITIVE( testSerialize.mClass->mFloat == 1.2345f, "PointerAsObjectTypeTest");
 			
			DIA_DELETE( testSerialize.mFloat );
			DIA_DELETE( testSerialize.mInt );
			DIA_DELETE( testSerialize.mClass );
			DIA_DELETE( testDeserialize.mFloat );
			DIA_DELETE( testDeserialize.mInt );
			DIA_DELETE( testDeserialize.mClass );

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			StaticArrayTypeTest testSerialize;
			testSerialize.mFloatArray[0] = 1.2f;
			testSerialize.mFloatArray[1] = 3.4f;
			testSerialize.mFloatArray[2] = 5.6f;

			char bufferMemory[32*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32*1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );

			StaticArrayTypeTest testDeserialize;
			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );
			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

			UNIT_TEST_POSITIVE( Dia::Maths::Float::FEqual( testDeserialize.mFloatArray[0], 1.2f ), "StaticArrayTypeTest");
 			UNIT_TEST_POSITIVE( Dia::Maths::Float::FEqual( testDeserialize.mFloatArray[1], 3.4f ), "StaticArrayTypeTest");
 			UNIT_TEST_POSITIVE( Dia::Maths::Float::FEqual( testDeserialize.mFloatArray[2], 5.6f ), "StaticArrayTypeTest");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			StaticArrayClassTypeTest testSerialize;
			testSerialize.mClassArray[0].mShort = 1;
			testSerialize.mClassArray[0].mInt = 2;
			testSerialize.mClassArray[1].mShort = 3;
			testSerialize.mClassArray[1].mInt = 4;

			char bufferMemory[32*1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32*1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );

			StaticArrayClassTypeTest testDeserialize;
			Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );
			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

			UNIT_TEST_POSITIVE( testDeserialize.mClassArray[0].mShort == 1, "StaticArrayClassTypeTest");
 			UNIT_TEST_POSITIVE( testDeserialize.mClassArray[0].mInt == 2, "StaticArrayClassTypeTest");
 			UNIT_TEST_POSITIVE( testDeserialize.mClassArray[1].mShort == 3, "StaticArrayClassTypeTest");
			UNIT_TEST_POSITIVE( testDeserialize.mClassArray[1].mInt == 4, "StaticArrayClassTypeTest");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			DiaArrayTypeTest testSerialize;
			testSerialize.mIntArray[0] = 1;
			testSerialize.mIntArray[1] = 2;

			testSerialize.mClassArray[0].mShort = 3;
			testSerialize.mClassArray[0].mInt = 4;
			testSerialize.mClassArray[1].mShort = 5;
			testSerialize.mClassArray[1].mInt = 9;

			char bufferMemory[32 * 1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32 * 1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(testSerialize, bufferSerial);

			DiaArrayTypeTest testDeserialize;
			Dia::Core::Containers::StringReader bufferDeserial(&bufferMemory[0]);
			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize(testDeserialize, bufferDeserial);

			UNIT_TEST_POSITIVE(testDeserialize.mIntArray[0] == 1, "DiaArrayTypeTest");
			UNIT_TEST_POSITIVE(testDeserialize.mIntArray[1] == 2, "DiaArrayTypeTest");
			UNIT_TEST_POSITIVE(testDeserialize.mClassArray[0].mShort == 3, "DiaArrayTypeTest");
			UNIT_TEST_POSITIVE(testDeserialize.mClassArray[0].mInt == 4, "DiaArrayTypeTest");
			UNIT_TEST_POSITIVE(testDeserialize.mClassArray[1].mShort == 5, "DiaArrayTypeTest");
			UNIT_TEST_POSITIVE(testDeserialize.mClassArray[1].mInt == 9, "DiaArrayTypeTest");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			CustomSerializerTypeTest testSerialize;
			Dia::Core::StringFormat(testSerialize.mArray, 8, "Test");
			
			char bufferMemory[32 * 1024];
			Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32 * 1024);

			Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(testSerialize, bufferSerial);

			CustomSerializerTypeTest testDeserialize;
			Dia::Core::Containers::StringReader bufferDeserial(&bufferMemory[0]);
			Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize(testDeserialize, bufferDeserial);

			UNIT_TEST_POSITIVE(Dia::Core::StringCompare(testDeserialize.mArray, "Test", 8) == 0, "CustomSerializerTypeTest");

		UNIT_TEST_BLOCK_END()


		mState = kFinished;
	}
}
