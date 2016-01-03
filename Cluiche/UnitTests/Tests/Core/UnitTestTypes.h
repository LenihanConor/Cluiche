#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

#include <DiaCore/Type/TypeDeclarationMacros.h>
#include <DiaCore/Type/TypeDefinitionMacros.h>
#include <DiaCore/Containers/Arrays/ArrayC.h>

namespace Json { class Value; }

namespace Dia { namespace Core { namespace Types { class TypeVariable; } } }
namespace Dia { namespace Core { namespace Types { class TypeInstance; } } }

namespace UnitTests
{
	class UnitTestTypes: public UnitTestCore
	{
	public:
		UnitTestTypes(const Dia::Core::Containers::String32& name);
		UnitTestTypes(void);
		
		void DoTest();
	};

	//Helper Classes
	//------------------------------------------------------------------------------
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

	//------------------------------------------------------------------------------
	class ClassTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		PrimitiveTypeTest mType1;
		PrimitiveTypeTest mType2;
	};

	//------------------------------------------------------------------------------
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

	//------------------------------------------------------------------------------
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

	//------------------------------------------------------------------------------
	class ClassVirtualInheritanceBaseTypeTest
	{
	public:
		DIA_TYPE_POLYMORPHIC_DECLARATION;
		virtual ~ClassVirtualInheritanceBaseTypeTest() {}
		virtual void DoSomeThing() {};

		char mChar;
		float mFloat;
	};

	class ClassVirtualInheritanceDerivedTypeTest : public ClassVirtualInheritanceBaseTypeTest
	{
	public:
		DIA_TYPE_POLYMORPHIC_DECLARATION;
		short mShort;
		int mInt;
	};

	//------------------------------------------------------------------------------
	class PrimitivePointerTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		bool mBool;
		bool* mBoolPtr1;
		bool* mBoolPtr2;
		char mChar;
		char* mCharPtr1;
		char* mCharPtr2;
		int mInt;
		int* mIntPtr1;
		int* mIntPtr2;
	};

	//------------------------------------------------------------------------------
	class ClassPointerTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		ClassInheritanceSimpleClassTypeTest mClass;
		ClassInheritanceSimpleClassTypeTest* mClassPtr;
	};

	//------------------------------------------------------------------------------
	class PointerAsObjectTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		float* mFloat;
		int* mInt;
		ClassInheritanceSimpleClassTypeTest* mClass;
	};

	//------------------------------------------------------------------------------
	class StaticArrayTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		float mFloatArray[3];
	};

	//------------------------------------------------------------------------------
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
	
	//------------------------------------------------------------------------------
	class DiaArrayTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		Dia::Core::Containers::ArrayC<int, 2> mIntArray;
		Dia::Core::Containers::ArrayC<StaticArrayClass, 2> mClassArray;
	};

	//------------------------------------------------------------------------------
	class CustomSerializerTypeTest
	{
	public:
		DIA_TYPE_DECLARATION;

		char mArray[16];

		static void Serialize(const Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, Json::Value& jsonData);
		static void Deserialize(Dia::Core::Types::TypeInstance& instance, const Dia::Core::Types::TypeVariable& currentTypeVariable, const Json::Value& jsonData);

	};
}