#include "UnitTests/Tests/Core/UnitTestTypeTraits.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaCore/Type/TypeTraits.h>
#include <DiaCore/Core/EnumClass.h>

using namespace Dia::Core;

namespace UnitTests
{	
	UnitTestTypeTraits::UnitTestTypeTraits(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestTypeTraits::UnitTestTypeTraits(void)
		: UnitTestCore()
	{}

	void UnitTestTypeTraits::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			class Foo
			{
			public: 
				int a;
			};
			
			bool voidResult2 = Dia::Core::Types::IsVoid<void>::value;
			bool voidResult3 = Dia::Core::Types::IsVoid<void*>::value;
			bool voidResult4 = Dia::Core::Types::IsVoid<Foo>::value;
			bool voidResult5 = Dia::Core::Types::IsVoid<char>::value;
			
			UNIT_TEST_POSITIVE( voidResult2, "TypeTraits");
			UNIT_TEST_NEGATIVE( voidResult3, "TypeTraits");
			UNIT_TEST_NEGATIVE( voidResult4, "TypeTraits");
			UNIT_TEST_NEGATIVE( voidResult5, "TypeTraits");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			class Foo
			{
			public: 
				float a;
			};

			bool result1 = Dia::Core::Types::IsFloat<float>::value;
			bool result2 = Dia::Core::Types::IsFloat<double>::value;
			bool result3 = Dia::Core::Types::IsFloat<long double>::value;
			bool result4 = Dia::Core::Types::IsFloat<Foo>::value;
			bool result5 = Dia::Core::Types::IsFloat<char>::value;
			bool result6 = Dia::Core::Types::IsFloat<int>::value;
			bool result7 = Dia::Core::Types::IsFloat<long>::value;
			bool result8 = Dia::Core::Types::IsFloat<const float>::value;

			UNIT_TEST_POSITIVE( result1 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result5 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result6 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result7 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result8 == true, "TypeTraits");

		UNIT_TEST_BLOCK_END()
	
		UNIT_TEST_BLOCK_START()

			class Foo
			{
			public: 
				float a;
			};

			bool result1 = Dia::Core::Types::IsIntegral<float>::value;
			bool result2 = Dia::Core::Types::IsIntegral<double>::value;
			bool result3 = Dia::Core::Types::IsIntegral<long double>::value;
			bool result4 = Dia::Core::Types::IsIntegral<Foo>::value;
			bool result5 = Dia::Core::Types::IsIntegral<char>::value;
			bool result6 = Dia::Core::Types::IsIntegral<int>::value;
			bool result7 = Dia::Core::Types::IsIntegral<long>::value;
			bool result8 = Dia::Core::Types::IsIntegral<bool>::value;
			bool result9 = Dia::Core::Types::IsIntegral<long long>::value;
			bool result10 = Dia::Core::Types::IsIntegral<const int>::value;
			bool result11 = Dia::Core::Types::IsIntegral<int*>::value;

			UNIT_TEST_POSITIVE( result1 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result5 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result6 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result7 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result8 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result9 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result10 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result11 == false, "TypeTraits");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			class Foo
			{
			public: 
				float a;
			};

			bool result1 = Dia::Core::Types::IsArithmetic<float>::value;
			bool result2 = Dia::Core::Types::IsArithmetic<double>::value;
			bool result3 = Dia::Core::Types::IsArithmetic<long double>::value;
			bool result4 = Dia::Core::Types::IsArithmetic<Foo>::value;
			bool result5 = Dia::Core::Types::IsArithmetic<char>::value;
			bool result6 = Dia::Core::Types::IsArithmetic<int>::value;
			bool result7 = Dia::Core::Types::IsArithmetic<long>::value;
			bool result8 = Dia::Core::Types::IsArithmetic<bool>::value;
			bool result9 = Dia::Core::Types::IsArithmetic<long long>::value;
			bool result10 = Dia::Core::Types::IsArithmetic<const int>::value;
			bool result11 = Dia::Core::Types::IsArithmetic<int*>::value;

			UNIT_TEST_POSITIVE( result1 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result5 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result6 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result7 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result8 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result9 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result10 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result11 == false, "TypeTraits");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			bool result1 = Dia::Core::Types::IsPointer<char>::value;
			bool result2 = Dia::Core::Types::IsPointer<char*>::value;
			bool result3 = Dia::Core::Types::IsPointer<const char>::value;
			bool result4 = Dia::Core::Types::IsPointer<const char*>::value;

			UNIT_TEST_POSITIVE( result1 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == true, "TypeTraits");
	
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			bool result1 = Dia::Core::Types::IsReference<char>::value;
			bool result2 = Dia::Core::Types::IsReference<char&>::value;
			bool result3 = Dia::Core::Types::IsReference<const char>::value;
			bool result4 = Dia::Core::Types::IsReference<const char&>::value;

			UNIT_TEST_POSITIVE( result1 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == true, "TypeTraits");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			class Foo
			{
			public: 
				float a;
			};

			bool result1 = Dia::Core::Types::IsClass<float>::value;
			bool result2 = Dia::Core::Types::IsClass<Foo>::value;
			bool result3 = Dia::Core::Types::IsClass<char>::value;
			bool result4 = Dia::Core::Types::IsClass<const int>::value;
			bool result5 = Dia::Core::Types::IsClass<int*>::value;
			bool result6 = Dia::Core::Types::IsClass<Foo*>::value;

			UNIT_TEST_POSITIVE( result1 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result5 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result6 == false, "TypeTraits");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			class Foo
			{
			public: 
				Foo(){};
				float a;
			};

			class Moo : public Foo
			{
			public:
				Moo() {};
				float a;
			};
			
			class Too
			{
			public:
				Too() {};
				virtual void Do(){};
			};
			
			class Soo : public Too
			{
			public:
				Soo() {};
				virtual void Do(){}
			};

			class Zoo
			{
			public:
				Zoo() {};
				virtual void Do()=0;
			};

			class Xoo
			{
			public:
				Xoo() {};
				virtual void Do(){}
			};

			bool result1 = Dia::Core::Types::IsAbstract<char>::value;
			bool result2 = Dia::Core::Types::IsAbstract<float>::value;
			bool result3 = Dia::Core::Types::IsAbstract<Foo>::value;
			bool result4 = Dia::Core::Types::IsAbstract<Moo>::value;
			bool result5 = Dia::Core::Types::IsAbstract<Too>::value;
			bool result6 = Dia::Core::Types::IsAbstract<Soo>::value;
			bool result7 = Dia::Core::Types::IsAbstract<Zoo>::value;
			bool result8 = Dia::Core::Types::IsAbstract<Xoo>::value;

			UNIT_TEST_POSITIVE( result1 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result5 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result6 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result7 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result8 == false, "TypeTraits");

		UNIT_TEST_BLOCK_END()
	
		UNIT_TEST_BLOCK_START()

			bool result1 = Dia::Core::Types::IsConstant<int>::value;
			bool result2 = Dia::Core::Types::IsConstant<const int>::value;
			
			UNIT_TEST_POSITIVE( result1 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == true, "TypeTraits");
			
		UNIT_TEST_BLOCK_END()
			
		UNIT_TEST_BLOCK_START()
			
			class Foo
			{
			public:
				Foo() {};
				int a;
			};
			
			enum EFoo
			{
				kOne = 0,
				kTwo,
			};

			CLASSEDENUM (EStyle,\
				CE_ITEMVAL(None, -1)\
				CE_ITEM(Attacking)\
				CE_ITEM(Defending),\
				None \
				);

			bool result1 = Dia::Core::Types::IsEnum<int>::value;
			bool result2 = Dia::Core::Types::IsEnum<Foo>::value;
			bool result3 = Dia::Core::Types::IsEnum<EFoo>::value;
			bool result4 = Dia::Core::Types::IsEnum<EStyle::EnumType>::value;

			UNIT_TEST_POSITIVE( result1 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == true, "TypeTraits");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			class Foo
			{
			public:
				Foo() {};
				int a;
			};

			enum EFoo
			{
				kOne = 0,
				kTwo,
			};

			bool result1 = Dia::Core::Types::IsScalar<int>::value;
			bool result2 = Dia::Core::Types::IsScalar<Foo>::value;
			bool result3 = Dia::Core::Types::IsScalar<EFoo>::value;
			bool result4 = Dia::Core::Types::IsScalar<void>::value;
			bool result5 = Dia::Core::Types::IsScalar<void*>::value;

			UNIT_TEST_POSITIVE( result1 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result5 == true, "TypeTraits");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			class Foo
			{
			public:
				Foo() {};
				int a;
			};

			enum EFoo
			{
				kOne = 0,
				kTwo,
			};

			bool result1 = Dia::Core::Types::IsPod<int>::value;
			bool result2 = Dia::Core::Types::IsPod<Foo>::value;
			bool result3 = Dia::Core::Types::IsPod<EFoo>::value;
			bool result4 = Dia::Core::Types::IsPod<void>::value;
			bool result5 = Dia::Core::Types::IsPod<void*>::value;

			UNIT_TEST_POSITIVE( result1 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result5 == true, "TypeTraits");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			class Foo
			{
			public:
				Foo() {};
				int a;
			};

			enum EFoo
			{
				kOne = 0,
				kTwo,
			};

			union DATATYPE    // Declare union type
			{
				char   ch;
				int    i;
				long   l;
				float  f;
				double d;
			}; 

			bool result1 = Dia::Core::Types::IsUnion<int>::value;
			bool result2 = Dia::Core::Types::IsUnion<Foo>::value;
			bool result3 = Dia::Core::Types::IsUnion<EFoo>::value;
			bool result4 = Dia::Core::Types::IsUnion<DATATYPE>::value;
			bool result5 = Dia::Core::Types::IsUnion<void*>::value;

			UNIT_TEST_POSITIVE( result1 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result3 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == true, "TypeTraits");
			UNIT_TEST_POSITIVE( result5 == false, "TypeTraits");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			class Foo
			{
			public:
				Foo() {};
				Foo(const Foo& rhs) {};
				~Foo() {};
				
				Foo& operator=	(const Foo& rhs){return *this;};

				int a;
			};
			
			class Moo
			{
			public:
			};

			class Too
			{
			public:
				Too() {};
				virtual ~Too() {};
			};
				
			bool result1 = Dia::Core::Types::HasTrivialConstructor<Foo>::value;
			bool result2 = Dia::Core::Types::HasTrivialConstructor<Moo>::value;
			
			bool result3 = Dia::Core::Types::HasTrivialConstructor<Foo>::value;
			bool result4 = Dia::Core::Types::HasTrivialConstructor<Moo>::value;

			bool result5 = Dia::Core::Types::HasTrivialDestructor<Foo>::value;
			bool result6 = Dia::Core::Types::HasTrivialDestructor<Moo>::value;

			bool result7 = Dia::Core::Types::HasTrivialAssign<Foo>::value;
			bool result8 = Dia::Core::Types::HasTrivialAssign<Moo>::value;

			bool result9 = Dia::Core::Types::HasVirtualDestructor<Moo>::value;
			bool result10 = Dia::Core::Types::HasVirtualDestructor<Too>::value;

			UNIT_TEST_POSITIVE( result1 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result2 == true, "TypeTraits");

			UNIT_TEST_POSITIVE( result3 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result4 == true, "TypeTraits");
			
			UNIT_TEST_POSITIVE( result5 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result6 == true, "TypeTraits");
			
			UNIT_TEST_POSITIVE( result7 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result8 == true, "TypeTraits");

			UNIT_TEST_POSITIVE( result9 == false, "TypeTraits");
			UNIT_TEST_POSITIVE( result10 == true, "TypeTraits");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}