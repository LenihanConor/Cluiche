#include <gtest/gtest.h>
#include <DiaUI/BoundMethod.h>

using namespace Dia::UI;
using namespace Dia::Core::Containers;

// ==============================================================================
// BoundMethodValue Tests
// ==============================================================================

TEST(BoundMethodValue, DefaultConstruct_TypeIsBoolean)
{
    BoundMethodValue v;
    EXPECT_EQ(v.GetType(), BoundMethodValue::EType::kBoolean);
}

TEST(BoundMethodValue, BoolConstruct_TypeAndValueCorrect)
{
    BoundMethodValue v(true);
    EXPECT_TRUE(v.IsBoolean());
    EXPECT_TRUE(v.GetBoolean());
}

TEST(BoundMethodValue, BoolConstruct_False)
{
    BoundMethodValue v(false);
    EXPECT_TRUE(v.IsBoolean());
    EXPECT_FALSE(v.GetBoolean());
}

TEST(BoundMethodValue, IntConstruct_TypeAndValueCorrect)
{
    BoundMethodValue v(42);
    EXPECT_TRUE(v.IsInteger());
    EXPECT_EQ(v.GetInteger(), 42);
}

TEST(BoundMethodValue, DoubleConstruct_TypeAndValueCorrect)
{
    BoundMethodValue v(3.14);
    EXPECT_TRUE(v.IsDouble());
    EXPECT_DOUBLE_EQ(v.GetDouble(), 3.14);
}

TEST(BoundMethodValue, StringConstruct_TypeAndValueCorrect)
{
    String64 str("hello");
    BoundMethodValue v(str);
    EXPECT_TRUE(v.IsString());
    EXPECT_STREQ(v.GetString().AsCStr(), "hello");
}

TEST(BoundMethodValue, CopyConstruct_PreservesTypeAndValue)
{
    String64 str("world");
    BoundMethodValue original(str);
    BoundMethodValue copy(original);
    EXPECT_TRUE(copy.IsString());
    EXPECT_STREQ(copy.GetString().AsCStr(), "world");
}

TEST(BoundMethodValue, AssignmentOp_PreservesTypeAndValue)
{
    String64 str("assign");
    BoundMethodValue a(str);
    BoundMethodValue b;
    b = a;
    EXPECT_TRUE(b.IsString());
    EXPECT_STREQ(b.GetString().AsCStr(), "assign");
}

TEST(BoundMethodValue, IsBoolean_FalseForOtherTypes)
{
    BoundMethodValue v(1);
    EXPECT_FALSE(v.IsBoolean());
    EXPECT_FALSE(v.IsDouble());
    EXPECT_FALSE(v.IsString());
}

// ==============================================================================
// BoundMethodArgs Tests
// ==============================================================================

TEST(BoundMethodArgs, DefaultConstruct_SizeIsZero)
{
    BoundMethodArgs args;
    EXPECT_EQ(args.Size(), 0u);
}

TEST(BoundMethodArgs, Add_IncreasesSize)
{
    BoundMethodArgs args;
    args.Add(BoundMethodValue(1));
    args.Add(BoundMethodValue(2));
    EXPECT_EQ(args.Size(), 2u);
}

TEST(BoundMethodArgs, At_ReturnsCorrectValue)
{
    BoundMethodArgs args;
    args.Add(BoundMethodValue(10));
    args.Add(BoundMethodValue(20));
    EXPECT_EQ(args.At(0).GetInteger(), 10);
    EXPECT_EQ(args.At(1).GetInteger(), 20);
}

TEST(BoundMethodArgs, Clear_ResetsSize)
{
    BoundMethodArgs args;
    args.Add(BoundMethodValue(1));
    args.Add(BoundMethodValue(2));
    args.Clear();
    EXPECT_EQ(args.Size(), 0u);
}

// ==============================================================================
// BoundMethod Tests
// ==============================================================================

static void DummyCallback(const BoundMethodArgs&) {}
static BoundMethodValue DummyCallbackWithRet(const BoundMethodArgs&) { return BoundMethodValue(99); }

TEST(BoundMethod, CreateBoundMethod_NameAndFlagCorrect)
{
    BoundMethod::MethodPtr ptr = &DummyCallback;
    BoundMethod method = BoundMethod::CreateBoundMethod("onClick", ptr);
    EXPECT_STREQ(method.GetName().AsCStr(), "onClick");
    EXPECT_EQ(method.GetReturnValueFlag(), BoundMethod::ReturnValueFlag::kDisabled);
}

TEST(BoundMethod, CreateBoundMethodWithRetVal_NameAndFlagCorrect)
{
    BoundMethod::MethodPtrWithRetVal ptr = &DummyCallbackWithRet;
    BoundMethod method = BoundMethod::CreateBoundMethodWithRetVal("getValue", ptr);
    EXPECT_STREQ(method.GetName().AsCStr(), "getValue");
    EXPECT_EQ(method.GetReturnValueFlag(), BoundMethod::ReturnValueFlag::kEnabled);
}

TEST(BoundMethod, ConstructWithName_NoReturnValue)
{
    BoundMethod method("myMethod");
    EXPECT_STREQ(method.GetName().AsCStr(), "myMethod");
    EXPECT_EQ(method.GetReturnValueFlag(), BoundMethod::ReturnValueFlag::kDisabled);
}

// ==============================================================================
// BoundMethodList Tests
// ==============================================================================

TEST(BoundMethodList, AddMethods_SizeCorrect)
{
    BoundMethodList list;
    BoundMethod::MethodPtr ptr = &DummyCallback;
    list.Add(BoundMethod::CreateBoundMethod("a", ptr));
    list.Add(BoundMethod::CreateBoundMethod("b", ptr));
    EXPECT_EQ(list.Size(), 2u);
}
