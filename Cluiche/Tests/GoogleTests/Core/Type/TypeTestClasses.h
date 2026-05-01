#pragma once

#include <DiaCore/Type/TypeDeclarationMacros.h>
#include <DiaCore/Type/TypeDefinitionMacros.h>
#include <DiaCore/Containers/Arrays/ArrayC.h>

namespace Json { class Value; }

namespace Dia { namespace Core { namespace Types {
	class TypeVariable;
	class TypeInstance;
	class TypeJsonSerializerExternalSerializeInterface;
	class TypeJsonSerializerExternalDeserializeInterface;
} } }

namespace UnitTests
{
	// Primitive type test class
	class PrimitiveTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		bool mBool;
		char mChar;
		unsigned char mCharUnsigned;
		short mShort;
		unsigned short mShortUnsigned;
		int mInt;
		unsigned int mIntUnsigned;
		long mLong;
		unsigned long mLongUnsigned;
		long long mLongLong;
		unsigned long long mLongLongUnsigned;
		float mFloat;
		double mDouble;
	};

	// Class type test
	class ClassTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		PrimitiveTypeTest mType1;
		PrimitiveTypeTest mType2;
	};

	// Inheritance test classes
	class ClassInheritanceBaseTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;
		char mChar;
		float mFloat;
	};

	class ClassInheritanceDerivedTypeTest : public ClassInheritanceBaseTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;
		short mShort;
		int mInt;
	};

	// Simple class for nested tests
	class ClassInheritanceSimpleClassTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;
		float mFloat;
	};

	class ClassInheritanceBaseWithClassTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;
		ClassInheritanceSimpleClassTypeTest mType1;
		ClassInheritanceSimpleClassTypeTest mType2;
	};

	class ClassInheritanceDerivedWithClassTypeTest : public ClassInheritanceBaseWithClassTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;
		ClassInheritanceSimpleClassTypeTest mType3;
	};

	// Virtual inheritance test
	class ClassVirtualInheritanceBaseTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;
		char mChar;
		float mFloat;
	};

	class ClassVirtualInheritanceDerivedTypeTest : public ClassVirtualInheritanceBaseTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;
		short mShort;
		int mInt;
	};

	// Pointer test classes
	class PrimitivePointerTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		char mChar;
		char* mCharPtr1;
		char* mCharPtr2;
		bool mBool;
		bool* mBoolPtr1;
		bool* mBoolPtr2;
		int mInt;
		int* mIntPtr1;
		int* mIntPtr2;
	};

	class ClassPointerTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		ClassInheritanceSimpleClassTypeTest mClass;
		ClassInheritanceSimpleClassTypeTest* mClassPtr;
	};

	class PointerAsObjectTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		float* mFloat;
		int* mInt;
		ClassInheritanceSimpleClassTypeTest* mClass;
	};

	// Array test classes
	class StaticArrayTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		float mFloatArray[3];
	};

	class StaticArrayClass
	{
	public:
		DIA_TYPE_DECLARATION;

		short mShort;
		int mInt;
	};

	class StaticArrayClassTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		StaticArrayClass mClassArray[2];
	};

	class DiaArrayTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		Dia::Core::Containers::ArrayC<int, 5> mIntArray;
		Dia::Core::Containers::ArrayC<StaticArrayClass, 3> mClassArray;
	};

	// Custom serializer test
	class CustomSerializerTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		char mArray[16];

		static void Serialize(const Dia::Core::Types::TypeInstance& instance,
							const Dia::Core::Types::TypeVariable& currentTypeVariable,
							Json::Value& jsonData,
							Dia::Core::Types::TypeJsonSerializerExternalSerializeInterface& parent);

		static void Deserialize(Dia::Core::Types::TypeInstance& instance,
							   const Dia::Core::Types::TypeVariable& currentTypeVariable,
							   const Json::Value& jsonData,
							   Dia::Core::Types::TypeJsonSerializerExternalDeserializeInterface& parent);
	};
}
