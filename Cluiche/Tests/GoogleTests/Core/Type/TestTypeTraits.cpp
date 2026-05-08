#include <gtest/gtest.h>
#include <DiaCore/Type/TypeTraits.h>
#include <DiaCore/Core/EnumClass.h>
#include <string>

using namespace Dia::Core;

class TypeTraitsTestClass
{
public:
    TypeTraitsTestClass() : a(0), name("test") {}  // std::string member makes it non-POD
    int a;
    std::string name;
};

TEST(TypeTraits, IsVoid_DetectsVoidType)
{
    EXPECT_TRUE(Types::IsVoid<void>::value);
    EXPECT_FALSE(Types::IsVoid<void*>::value);
    EXPECT_FALSE(Types::IsVoid<TypeTraitsTestClass>::value);
    EXPECT_FALSE(Types::IsVoid<char>::value);
}

TEST(TypeTraits, IsFloat_DetectsFloatType)
{
    EXPECT_TRUE(Types::IsFloat<float>::value);
    EXPECT_FALSE(Types::IsFloat<double>::value);
    EXPECT_FALSE(Types::IsFloat<long double>::value);
    EXPECT_FALSE(Types::IsFloat<TypeTraitsTestClass>::value);
    EXPECT_FALSE(Types::IsFloat<char>::value);
    EXPECT_FALSE(Types::IsFloat<int>::value);
    EXPECT_FALSE(Types::IsFloat<long>::value);
    EXPECT_TRUE(Types::IsFloat<const float>::value);
}

TEST(TypeTraits, IsIntegral_DetectsIntegralTypes)
{
    EXPECT_FALSE(Types::IsIntegral<float>::value);
    EXPECT_FALSE(Types::IsIntegral<double>::value);
    EXPECT_FALSE(Types::IsIntegral<long double>::value);
    EXPECT_FALSE(Types::IsIntegral<TypeTraitsTestClass>::value);
    EXPECT_TRUE(Types::IsIntegral<char>::value);
    EXPECT_TRUE(Types::IsIntegral<int>::value);
    EXPECT_TRUE(Types::IsIntegral<long>::value);
    EXPECT_TRUE(Types::IsIntegral<bool>::value);
    EXPECT_TRUE(Types::IsIntegral<long long>::value);
    EXPECT_TRUE(Types::IsIntegral<const int>::value);
    EXPECT_FALSE(Types::IsIntegral<int*>::value);
}

TEST(TypeTraits, IsArithmetic_DetectsArithmeticTypes)
{
    EXPECT_TRUE(Types::IsArithmetic<float>::value);
    EXPECT_TRUE(Types::IsArithmetic<double>::value);
    EXPECT_TRUE(Types::IsArithmetic<long double>::value);
    EXPECT_FALSE(Types::IsArithmetic<TypeTraitsTestClass>::value);
    EXPECT_TRUE(Types::IsArithmetic<char>::value);
    EXPECT_TRUE(Types::IsArithmetic<int>::value);
    EXPECT_TRUE(Types::IsArithmetic<long>::value);
    EXPECT_TRUE(Types::IsArithmetic<bool>::value);
    EXPECT_TRUE(Types::IsArithmetic<long long>::value);
    EXPECT_TRUE(Types::IsArithmetic<const int>::value);
    EXPECT_FALSE(Types::IsArithmetic<int*>::value);
}

TEST(TypeTraits, IsPointer_DetectsPointerTypes)
{
    EXPECT_FALSE(Types::IsPointer<char>::value);
    EXPECT_TRUE(Types::IsPointer<char*>::value);
    EXPECT_FALSE(Types::IsPointer<const char>::value);
    EXPECT_TRUE(Types::IsPointer<const char*>::value);
}

TEST(TypeTraits, IsReference_DetectsReferenceTypes)
{
    EXPECT_FALSE(Types::IsReference<char>::value);
    EXPECT_TRUE(Types::IsReference<char&>::value);
    EXPECT_FALSE(Types::IsReference<const char>::value);
    EXPECT_TRUE(Types::IsReference<const char&>::value);
}

TEST(TypeTraits, IsClass_DetectsClassTypes)
{
    EXPECT_FALSE(Types::IsClass<float>::value);
    EXPECT_TRUE(Types::IsClass<TypeTraitsTestClass>::value);
    EXPECT_FALSE(Types::IsClass<char>::value);
    EXPECT_FALSE(Types::IsClass<const int>::value);
    EXPECT_FALSE(Types::IsClass<int*>::value);
    EXPECT_FALSE(Types::IsClass<TypeTraitsTestClass*>::value);
}

TEST(TypeTraits, IsAbstract_DetectsAbstractClasses)
{
    class Foo
    {
    public:
        Foo() {};
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
        virtual void Do() {};
    };

    class Soo : public Too
    {
    public:
        Soo() {};
        virtual void Do() {}
    };

    class Zoo
    {
    public:
        Zoo() {};
        virtual void Do() = 0;
    };

    class Xoo
    {
    public:
        Xoo() {};
        virtual void Do() {}
    };

    EXPECT_FALSE(Types::IsAbstract<char>::value);
    EXPECT_FALSE(Types::IsAbstract<float>::value);
    EXPECT_FALSE(Types::IsAbstract<Foo>::value);
    EXPECT_FALSE(Types::IsAbstract<Moo>::value);
    EXPECT_FALSE(Types::IsAbstract<Too>::value);
    EXPECT_FALSE(Types::IsAbstract<Soo>::value);
    EXPECT_TRUE(Types::IsAbstract<Zoo>::value);
    EXPECT_FALSE(Types::IsAbstract<Xoo>::value);
}

TEST(TypeTraits, IsConstant_DetectsConstTypes)
{
    EXPECT_FALSE(Types::IsConstant<int>::value);
    EXPECT_TRUE(Types::IsConstant<const int>::value);
}

TEST(TypeTraits, IsEnum_DetectsEnumTypes)
{
    enum EFoo
    {
        kOne = 0,
        kTwo,
    };

    CLASSEDENUM(EStyle, \
        CE_ITEMVAL(None, -1) \
        CE_ITEM(Attacking) \
        CE_ITEM(Defending), \
        None \
    );

    EXPECT_FALSE(Types::IsEnum<int>::value);
    EXPECT_FALSE(Types::IsEnum<TypeTraitsTestClass>::value);
    EXPECT_TRUE(Types::IsEnum<EFoo>::value);
    EXPECT_TRUE(Types::IsEnum<EStyle::EnumType>::value);
}

TEST(TypeTraits, IsScalar_DetectsScalarTypes)
{
    enum EFoo
    {
        kOne = 0,
        kTwo,
    };

    EXPECT_TRUE(Types::IsScalar<int>::value);
    EXPECT_FALSE(Types::IsScalar<TypeTraitsTestClass>::value);
    EXPECT_TRUE(Types::IsScalar<EFoo>::value);
    EXPECT_FALSE(Types::IsScalar<void>::value);
    EXPECT_TRUE(Types::IsScalar<void*>::value);
}

TEST(TypeTraits, IsPod_DetectsPODTypes)
{
    enum EFoo
    {
        kOne = 0,
        kTwo,
    };

    EXPECT_TRUE(Types::IsPod<int>::value);
    EXPECT_FALSE(Types::IsPod<TypeTraitsTestClass>::value);
    EXPECT_TRUE(Types::IsPod<EFoo>::value);
    EXPECT_TRUE(Types::IsPod<void>::value);
    EXPECT_TRUE(Types::IsPod<void*>::value);
}

TEST(TypeTraits, IsUnion_DetectsUnionTypes)
{
    enum EFoo
    {
        kOne = 0,
        kTwo,
    };

    union DATATYPE
    {
        char   ch;
        int    i;
        long   l;
        float  f;
        double d;
    };

    EXPECT_FALSE(Types::IsUnion<int>::value);
    EXPECT_FALSE(Types::IsUnion<TypeTraitsTestClass>::value);
    EXPECT_FALSE(Types::IsUnion<EFoo>::value);
    EXPECT_TRUE(Types::IsUnion<DATATYPE>::value);
    EXPECT_FALSE(Types::IsUnion<void*>::value);
}

TEST(TypeTraits, TrivialTraits_DetectsTrivialProperties)
{
    class Foo
    {
    public:
        Foo() {};
        Foo(const Foo& rhs) {};
        ~Foo() {};

        Foo& operator=(const Foo& rhs) { return *this; };

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

    EXPECT_FALSE(Types::HasTrivialConstructor<Foo>::value);
    EXPECT_TRUE(Types::HasTrivialConstructor<Moo>::value);

    EXPECT_FALSE(Types::HasTrivialDestructor<Foo>::value);
    EXPECT_TRUE(Types::HasTrivialDestructor<Moo>::value);

    EXPECT_FALSE(Types::HasTrivialAssign<Foo>::value);
    EXPECT_TRUE(Types::HasTrivialAssign<Moo>::value);

    EXPECT_FALSE(Types::HasVirtualDestructor<Moo>::value);
    EXPECT_TRUE(Types::HasVirtualDestructor<Too>::value);
}
