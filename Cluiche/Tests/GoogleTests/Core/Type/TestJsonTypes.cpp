#include <gtest/gtest.h>
#include "TypeTestClasses.h"
#include <DiaCore/Type/TypeDeclarationMacros.h>
#include <DiaCore/Type/TypeParameterInput.h>
#include <DiaCore/Type/TypeDefinition.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String8.h>
#include <DiaCore/Type/TypeDefinitionMacros.h>
#include <DiaCore/Type/TypeFacade.h>
#include <DiaCore/Containers/Strings/StringWriter.h>
#include <DiaCore/Containers/Strings/StringReader.h>
#include <DiaCore/Core/Log.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <DiaCore/Strings/stringutils.h>
#include <DiaCore/Json/external/json/json.h>

using namespace UnitTests;

TEST(JsonTypes, PrimitiveTypes_SerializeAndDeserialize)
{
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

	EXPECT_TRUE( testDeserialize.mBool );
	EXPECT_EQ( testDeserialize.mChar, 'z' );
	EXPECT_EQ( testDeserialize.mCharUnsigned, 't' );
	EXPECT_EQ( testDeserialize.mShort, -25 );
	EXPECT_EQ( testDeserialize.mShortUnsigned, 32 );
	EXPECT_EQ( testDeserialize.mIntUnsigned, 8574578 );
	EXPECT_EQ( testDeserialize.mLong, -2548 );
	EXPECT_EQ( testDeserialize.mLongUnsigned, 68547956 );
	EXPECT_EQ( testDeserialize.mLongLong, -85485795 );
	EXPECT_EQ( testDeserialize.mLongLongUnsigned, 3524658754 );
	EXPECT_EQ( testDeserialize.mFloat, 7.5f );
	EXPECT_EQ( testDeserialize.mDouble, 88.5 );
}

TEST(JsonTypes, ClassTypes_SerializeAndDeserialize)
{
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

	EXPECT_TRUE( testDeserialize.mType1.mBool );
	EXPECT_EQ( testDeserialize.mType1.mChar, 'z' );
	EXPECT_EQ( testDeserialize.mType1.mCharUnsigned, 't' );
	EXPECT_EQ( testDeserialize.mType1.mShort, -25 );
	EXPECT_EQ( testDeserialize.mType1.mShortUnsigned, 32 );
	EXPECT_EQ( testDeserialize.mType1.mIntUnsigned, 8574578 );
	EXPECT_EQ( testDeserialize.mType1.mLong, -2548 );
	EXPECT_EQ( testDeserialize.mType1.mLongUnsigned, 68547956 );
	EXPECT_EQ( testDeserialize.mType1.mLongLong, -85485795 );
	EXPECT_EQ( testDeserialize.mType1.mLongLongUnsigned, 3524658754 );
	EXPECT_EQ( testDeserialize.mType1.mFloat, 7.5f );
	EXPECT_EQ( testDeserialize.mType1.mDouble, 88.5 );

	EXPECT_FALSE( testDeserialize.mType2.mBool );
	EXPECT_EQ( testDeserialize.mType2.mChar, 'x' );
	EXPECT_EQ( testDeserialize.mType2.mCharUnsigned, '$' );
	EXPECT_EQ( testDeserialize.mType2.mShort, -2455 );
	EXPECT_EQ( testDeserialize.mType2.mShortUnsigned, 8532 );
	EXPECT_EQ( testDeserialize.mType2.mIntUnsigned, 4578 );
	EXPECT_EQ( testDeserialize.mType2.mLong, -258898 );
	EXPECT_EQ( testDeserialize.mType2.mLongUnsigned, 687956 );
	EXPECT_EQ( testDeserialize.mType2.mLongLong, -854855 );
	EXPECT_EQ( testDeserialize.mType2.mLongLongUnsigned, 35246754 );
	EXPECT_EQ( testDeserialize.mType2.mFloat, -10.23558f );
	EXPECT_EQ( testDeserialize.mType2.mDouble, -45.5056 );
}

TEST(JsonTypes, ClassInheritanceBase_SerializesAndDeserializes)
{
	ClassInheritanceBaseTypeTest testSerialize;

	testSerialize.mChar = 'z';
	testSerialize.mFloat = 7.5f;

	char bufferMemory[64*1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );

	ClassInheritanceBaseTypeTest testDeserialize;

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

	EXPECT_EQ( testDeserialize.mChar, 'z' );
	EXPECT_EQ( testDeserialize.mFloat, 7.5f );
}

TEST(JsonTypes, ClassInheritanceDerived_SerializesBaseAndDerived)
{
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

	EXPECT_EQ( testDeserialize.mChar, 'z' );
	EXPECT_EQ( testDeserialize.mFloat, 7.5f );
	EXPECT_EQ( testDeserialize.mShort, 123 );
	EXPECT_EQ( testDeserialize.mInt, -456 );
}

TEST(JsonTypes, ClassInheritanceDerivedWithClass_SerializesNested)
{
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

	EXPECT_EQ( testDeserialize.mType1.mFloat, 1.0f );
	EXPECT_EQ( testDeserialize.mType2.mFloat, 2.0f );
	EXPECT_EQ( testDeserialize.mType3.mFloat, 3.0f );
}

TEST(JsonTypes, ClassVirtualInheritanceDerived_SerializesCorrectly)
{
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

	EXPECT_EQ( testDeserialize.mChar, 'z' );
	EXPECT_EQ( testDeserialize.mFloat, 7.5f );
	EXPECT_EQ( testDeserialize.mShort, 123 );
	EXPECT_EQ( testDeserialize.mInt, -456 );
}

// Pointer serialization is not currently supported - test disabled
TEST(JsonTypes, DISABLED_PrimitivePointers_SerializeAndDeserialize)
{
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

	EXPECT_TRUE( testDeserialize.mBool );
	EXPECT_TRUE( *testSerialize.mBoolPtr1 );
	EXPECT_TRUE( *testSerialize.mBoolPtr2 );
	EXPECT_EQ( testSerialize.mBoolPtr1, testSerialize.mBoolPtr2 );
	EXPECT_EQ( &testSerialize.mBool, testSerialize.mBoolPtr2 );
	EXPECT_EQ( testDeserialize.mChar, 'd' );
	EXPECT_EQ( *testSerialize.mCharPtr1, 'd' );
	EXPECT_EQ( *testSerialize.mCharPtr2, 'd' );
	EXPECT_EQ( testSerialize.mCharPtr1, testSerialize.mCharPtr2 );
	EXPECT_EQ( &testSerialize.mChar, testSerialize.mCharPtr2 );
	EXPECT_EQ( testDeserialize.mInt, 34 );
	EXPECT_EQ( *testSerialize.mIntPtr1, 34 );
	EXPECT_EQ( *testSerialize.mIntPtr2, 34 );
	EXPECT_EQ( testSerialize.mIntPtr1, testSerialize.mIntPtr2 );
	EXPECT_EQ( &testSerialize.mInt, testSerialize.mIntPtr2 );
}

TEST(JsonTypes, DISABLED_ClassPointers_SerializeAndDeserialize)
{
	ClassPointerTypeTest testSerialize;
	testSerialize.mClass.mFloat = -234.0f;
	testSerialize.mClassPtr = &testSerialize.mClass;

	char bufferMemory[64*1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize( testSerialize, bufferSerial );

	ClassPointerTypeTest testDeserialize;
	testDeserialize.mClassPtr = DIA_NEW(ClassInheritanceSimpleClassTypeTest);

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize( testDeserialize, bufferDeserial );

	EXPECT_EQ( testSerialize.mClass.mFloat, -234.0f );
	EXPECT_EQ( testSerialize.mClassPtr->mFloat, -234.0f );
	EXPECT_EQ( testSerialize.mClassPtr, &testSerialize.mClass );

	EXPECT_EQ( testDeserialize.mClass.mFloat, -234.0f );
	EXPECT_EQ( testDeserialize.mClassPtr->mFloat, -234.0f );

	delete testDeserialize.mClassPtr;
}

TEST(JsonTypes, DISABLED_PointerAsObject_SerializeAndDeserialize)
{
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

	EXPECT_EQ( *testSerialize.mFloat, -3.987f );
	EXPECT_EQ( *testSerialize.mInt, -7 );
	EXPECT_EQ( testSerialize.mClass->mFloat, 1.2345f );

	DIA_DELETE( testSerialize.mFloat );
	DIA_DELETE( testSerialize.mInt );
	DIA_DELETE( testSerialize.mClass );
	DIA_DELETE( testDeserialize.mFloat );
	DIA_DELETE( testDeserialize.mInt );
	DIA_DELETE( testDeserialize.mClass );
}

TEST(JsonTypes, StaticArray_SerializesAndDeserializes)
{
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

	EXPECT_TRUE( Dia::Maths::Float::FEqual( testDeserialize.mFloatArray[0], 1.2f ) );
	EXPECT_TRUE( Dia::Maths::Float::FEqual( testDeserialize.mFloatArray[1], 3.4f ) );
	EXPECT_TRUE( Dia::Maths::Float::FEqual( testDeserialize.mFloatArray[2], 5.6f ) );
}

TEST(JsonTypes, StaticArrayClass_SerializesAndDeserializes)
{
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

	EXPECT_EQ( testDeserialize.mClassArray[0].mShort, 1 );
	EXPECT_EQ( testDeserialize.mClassArray[0].mInt, 2 );
	EXPECT_EQ( testDeserialize.mClassArray[1].mShort, 3 );
	EXPECT_EQ( testDeserialize.mClassArray[1].mInt, 4 );
}

TEST(JsonTypes, DiaArray_SerializesAndDeserializes)
{
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

	EXPECT_EQ(testDeserialize.mIntArray[0], 1);
	EXPECT_EQ(testDeserialize.mIntArray[1], 2);
	EXPECT_EQ(testDeserialize.mClassArray[0].mShort, 3);
	EXPECT_EQ(testDeserialize.mClassArray[0].mInt, 4);
	EXPECT_EQ(testDeserialize.mClassArray[1].mShort, 5);
	EXPECT_EQ(testDeserialize.mClassArray[1].mInt, 9);
}

TEST(JsonTypes, CustomSerializer_SerializesAndDeserializes)
{
	CustomSerializerTypeTest testSerialize;
	Dia::Core::StringFormat(testSerialize.mArray, 8, "Test");

	char bufferMemory[32 * 1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32 * 1024);

	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(testSerialize, bufferSerial);

	CustomSerializerTypeTest testDeserialize;
	Dia::Core::Containers::StringReader bufferDeserial(&bufferMemory[0]);
	Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize(testDeserialize, bufferDeserial);

	EXPECT_EQ(Dia::Core::StringCompare(testDeserialize.mArray, "Test", 8), 0);
}

TEST(JsonTypes, String8_SerializesAndDeserializes)
{
	Dia::Core::Containers::String8 testSerialize("Test");

	char bufferMemory[32 * 1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32 * 1024);

	Dia::Core::Types::GetTypeFacade().JsonSerializer().Serialize(testSerialize, bufferSerial);

	Dia::Core::Containers::String8 testDeserialize;
	Dia::Core::Containers::StringReader bufferDeserial(&bufferMemory[0]);
	Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize(testDeserialize, bufferDeserial);

	EXPECT_EQ(testDeserialize, "Test");
}
