#include <gtest/gtest.h>
#include "TypeTestClasses.h"
#include <DiaCore/Type/TypeDeclarationMacros.h>
#include <DiaCore/Type/TypeParameterInput.h>
#include <DiaCore/Type/TypeDefinition.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Type/TypeDefinitionMacros.h>
#include <DiaCore/Type/TypeFacade.h>
#include <DiaCore/Containers/Strings/StringWriter.h>
#include <DiaCore/Containers/Strings/StringReader.h>
#include <DiaCore/Core/Log.h>
#include <DiaCore/Containers/Arrays/ArrayC.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Strings/stringutils.h>

using namespace UnitTests;

// Type definitions need to be in the UnitTests namespace
namespace UnitTests
{
	DIA_TYPE_DEFINITION( PrimitiveTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mBool", mBool )
		DIA_TYPE_ADD_VARIABLE( "mChar", mChar )
		DIA_TYPE_ADD_VARIABLE( "mCharUnsigned", mCharUnsigned )
		DIA_TYPE_ADD_VARIABLE( "mShort", mShort )
		DIA_TYPE_ADD_VARIABLE( "mShortUnsigned", mShortUnsigned )
		DIA_TYPE_ADD_VARIABLE( "mInt", mInt )
		DIA_TYPE_ADD_VARIABLE( "mIntUnsigned", mIntUnsigned )
		DIA_TYPE_ADD_VARIABLE( "mLong", mLong )
		DIA_TYPE_ADD_VARIABLE( "mLongUnsigned", mLongUnsigned )
		DIA_TYPE_ADD_VARIABLE( "mLongLong", mLongLong )
		DIA_TYPE_ADD_VARIABLE( "mLongLongUnsigned", mLongLongUnsigned )
		DIA_TYPE_ADD_VARIABLE( "mFloat", mFloat )
		DIA_TYPE_ADD_VARIABLE( "mDouble", mDouble )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( ClassTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mType1", mType1 )
		DIA_TYPE_ADD_VARIABLE( "mType2", mType2 )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( ClassInheritanceBaseTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mChar", mChar )
		DIA_TYPE_ADD_VARIABLE( "mFloat", mFloat )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( ClassInheritanceDerivedTypeTest )
		DIA_TYPE_BASE_TYPE( ClassInheritanceBaseTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mShort", mShort )
		DIA_TYPE_ADD_VARIABLE( "mInt", mInt )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( ClassInheritanceSimpleClassTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mFloat", mFloat )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( ClassInheritanceBaseWithClassTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mType1", mType1 )
		DIA_TYPE_ADD_VARIABLE( "mType2", mType2 )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( ClassInheritanceDerivedWithClassTypeTest )
		DIA_TYPE_BASE_TYPE( ClassInheritanceBaseWithClassTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mType3", mType3 )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( ClassVirtualInheritanceBaseTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mChar", mChar )
		DIA_TYPE_ADD_VARIABLE( "mFloat", mFloat )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( ClassVirtualInheritanceDerivedTypeTest )
		DIA_TYPE_BASE_TYPE( ClassVirtualInheritanceBaseTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mShort", mShort )
		DIA_TYPE_ADD_VARIABLE( "mInt", mInt )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( PrimitivePointerTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mChar", mChar )
		DIA_TYPE_ADD_VARIABLE( "mCharPtr1", mCharPtr1 )
		DIA_TYPE_ADD_VARIABLE( "mCharPtr2", mCharPtr2 )
		DIA_TYPE_ADD_VARIABLE( "mBool", mBool )
		DIA_TYPE_ADD_VARIABLE( "mBoolPtr1", mBoolPtr1 )
		DIA_TYPE_ADD_VARIABLE( "mBoolPtr2", mBoolPtr2 )
		DIA_TYPE_ADD_VARIABLE( "mInt", mInt )
		DIA_TYPE_ADD_VARIABLE( "mIntPtr1", mIntPtr1 )
		DIA_TYPE_ADD_VARIABLE( "mIntPtr2", mIntPtr2 )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( ClassPointerTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mClass", mClass )
		DIA_TYPE_ADD_VARIABLE( "mClassPtr", mClassPtr )
			DIA_TYPE_ADD_VARIABLE_ATTRIBUTE( Dia::Core::Types::TypeVariableAttributesPointerAsObject )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( PointerAsObjectTypeTest )
		DIA_TYPE_ADD_VARIABLE( "mFloat", mFloat )
			DIA_TYPE_ADD_VARIABLE_ATTRIBUTE( Dia::Core::Types::TypeVariableAttributesPointerAsObject )
		DIA_TYPE_ADD_VARIABLE( "mInt", mInt )
			DIA_TYPE_ADD_VARIABLE_ATTRIBUTE( Dia::Core::Types::TypeVariableAttributesPointerAsObject )
		DIA_TYPE_ADD_VARIABLE( "mClass", mClass )
			DIA_TYPE_ADD_VARIABLE_ATTRIBUTE( Dia::Core::Types::TypeVariableAttributesPointerAsObject )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( StaticArrayTypeTest )
		DIA_TYPE_ADD_VARIABLE_ARRAY( "mFloatArray", mFloatArray, 3 )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( StaticArrayClass )
		DIA_TYPE_ADD_VARIABLE( "mShort", mShort )
		DIA_TYPE_ADD_VARIABLE( "mInt", mInt )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION( StaticArrayClassTypeTest )
		DIA_TYPE_ADD_VARIABLE_ARRAY( "mClassArray", mClassArray, 2 )
	DIA_TYPE_DEFINITION_END()

	DIA_TYPE_DEFINITION(DiaArrayTypeTest)
		DIA_TYPE_ADD_VARIABLE("mIntArray", mIntArray)
		DIA_TYPE_ADD_VARIABLE("mClassArray", mClassArray)
	DIA_TYPE_DEFINITION_END()

	void CustomSerializerTypeTest::Serialize(const Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, Json::Value& jsonData, Dia::Core::Types::TypeJsonSerializerExternalSerializeInterface& parent)
	{
		std::string str;
		unsigned int size = currentTypeVariable.GetNumberOfElements();
		for (unsigned int i = 0; i < size; i++)
		{
			char temp = currentTypeVariable.GetArithmeticValue<char>(instance, i);
			str.push_back(temp);

			if (temp == '\0')
			{
				break;
			}
		}
		str.shrink_to_fit();
		jsonData[currentTypeVariable.GetName()] = str;
	}

	void CustomSerializerTypeTest::Deserialize(Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, const Json::Value& jsonData, Dia::Core::Types::TypeJsonSerializerExternalDeserializeInterface& parent)
	{
		std::string str = jsonData.asString();

		for (unsigned int i = 0; i < str.length(); i++)
		{
			char a = str[i];
			currentTypeVariable.SetArithmeticValue(a, instance, i, false);
		}
	}

	DIA_TYPE_DEFINITION(CustomSerializerTypeTest)
			DIA_TYPE_ADD_VARIABLE_ARRAY("mArray", mArray, 16)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonSerializer, CustomSerializerTypeTest::Serialize)
				DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1(Dia::Core::Types::TypeVariableAttributesCustomJsonDeserializer, CustomSerializerTypeTest::Deserialize)
	DIA_TYPE_DEFINITION_END()
}

TEST(Types, PrimitiveTypes_TextSerializeAndDeserialize)
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

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	PrimitiveTypeTest testDeserialize;

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

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

TEST(Types, ClassTypes_TextSerializeAndDeserialize)
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

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	ClassTypeTest testDeserialize;

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

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

TEST(Types, ClassInheritanceBase_TextSerializesAndDeserializes)
{
	ClassInheritanceBaseTypeTest testSerialize;

	testSerialize.mChar = 'z';
	testSerialize.mFloat = 7.5f;

	char bufferMemory[64*1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	ClassInheritanceBaseTypeTest testDeserialize;

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

	EXPECT_EQ( testDeserialize.mChar, 'z' );
	EXPECT_EQ( testDeserialize.mFloat, 7.5f );
}

TEST(Types, ClassInheritanceDerived_TextSerializesBaseAndDerived)
{
	ClassInheritanceDerivedTypeTest testSerialize;

	testSerialize.mChar = 'z';
	testSerialize.mFloat = 7.5f;
	testSerialize.mShort = 123;
	testSerialize.mInt = -456;

	char bufferMemory[64*1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	ClassInheritanceDerivedTypeTest testDeserialize;

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

	EXPECT_EQ( testDeserialize.mChar, 'z' );
	EXPECT_EQ( testDeserialize.mFloat, 7.5f );
	EXPECT_EQ( testDeserialize.mShort, 123 );
	EXPECT_EQ( testDeserialize.mInt, -456 );
}

TEST(Types, ClassInheritanceDerivedWithClass_TextSerializesNested)
{
	ClassInheritanceDerivedWithClassTypeTest testSerialize;

	testSerialize.mType1.mFloat = 1.0f;
	testSerialize.mType2.mFloat = 2.0f;
	testSerialize.mType3.mFloat = 3.0f;

	char bufferMemory[64*1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	ClassInheritanceDerivedWithClassTypeTest testDeserialize;

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

	EXPECT_EQ( testDeserialize.mType1.mFloat, 1.0f );
	EXPECT_EQ( testDeserialize.mType2.mFloat, 2.0f );
	EXPECT_EQ( testDeserialize.mType3.mFloat, 3.0f );
}

TEST(Types, ClassVirtualInheritanceDerived_TextSerializesCorrectly)
{
	ClassVirtualInheritanceDerivedTypeTest testSerialize;

	testSerialize.mChar = 'z';
	testSerialize.mFloat = 7.5f;
	testSerialize.mShort = 123;
	testSerialize.mInt = -456;

	char bufferMemory[64*1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	ClassVirtualInheritanceDerivedTypeTest testDeserialize;

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

	EXPECT_EQ( testDeserialize.mChar, 'z' );
	EXPECT_EQ( testDeserialize.mFloat, 7.5f );
	EXPECT_EQ( testDeserialize.mShort, 123 );
	EXPECT_EQ( testDeserialize.mInt, -456 );
}

TEST(Types, PrimitivePointers_TextSerializeAndDeserialize)
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

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	PrimitivePointerTypeTest testDeserialize;

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

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

TEST(Types, DISABLED_ClassPointers_TextSerializeAndDeserialize)
{
	ClassPointerTypeTest testSerialize;
	testSerialize.mClass.mFloat = -234.0f;
	testSerialize.mClassPtr = &testSerialize.mClass;

	char bufferMemory[64*1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 64*1024);

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	ClassPointerTypeTest testDeserialize;

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );

	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

	EXPECT_EQ( testSerialize.mClass.mFloat, -234.0f );
	EXPECT_EQ( testSerialize.mClassPtr->mFloat, -234.0f );
	EXPECT_EQ( testSerialize.mClassPtr, &testSerialize.mClass );
}

TEST(Types, PointerAsObject_TextSerializeAndDeserialize)
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

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	PointerAsObjectTypeTest testDeserialize;
	testDeserialize.mFloat = DIA_NEW(float);
	testDeserialize.mInt = DIA_NEW(int);
	testDeserialize.mClass = DIA_NEW(ClassInheritanceSimpleClassTypeTest);

	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );
	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

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

TEST(Types, StaticArray_TextSerializesAndDeserializes)
{
	StaticArrayTypeTest testSerialize;
	testSerialize.mFloatArray[0] = 1.2f;
	testSerialize.mFloatArray[1] = 3.4f;
	testSerialize.mFloatArray[2] = 5.6f;

	char bufferMemory[32*1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32*1024);

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	StaticArrayTypeTest testDeserialize;
	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );
	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

	EXPECT_TRUE( Dia::Maths::Float::FEqual( testDeserialize.mFloatArray[0], 1.2f ) );
	EXPECT_TRUE( Dia::Maths::Float::FEqual( testDeserialize.mFloatArray[1], 3.4f ) );
	EXPECT_TRUE( Dia::Maths::Float::FEqual( testDeserialize.mFloatArray[2], 5.6f ) );
}

TEST(Types, StaticArrayClass_TextSerializesAndDeserializes)
{
	StaticArrayClassTypeTest testSerialize;
	testSerialize.mClassArray[0].mShort = 1;
	testSerialize.mClassArray[0].mInt = 2;
	testSerialize.mClassArray[1].mShort = 3;
	testSerialize.mClassArray[1].mInt = 4;

	char bufferMemory[32*1024];
	Dia::Core::Containers::StringWriter bufferSerial(&bufferMemory[0], 32*1024);

	Dia::Core::Types::GetTypeFacade().TextSerializer().Serialize( testSerialize, bufferSerial );

	StaticArrayClassTypeTest testDeserialize;
	Dia::Core::Containers::StringReader bufferDeserial( &bufferMemory[0] );
	Dia::Core::Types::GetTypeFacade().TextSerializer().Deserialize( testDeserialize, bufferDeserial );

	EXPECT_EQ( testDeserialize.mClassArray[0].mShort, 1 );
	EXPECT_EQ( testDeserialize.mClassArray[0].mInt, 2 );
	EXPECT_EQ( testDeserialize.mClassArray[1].mShort, 3 );
	EXPECT_EQ( testDeserialize.mClassArray[1].mInt, 4 );
}
