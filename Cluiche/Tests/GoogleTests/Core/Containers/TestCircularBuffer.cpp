#include <gtest/gtest.h>
#include <DiaCore/Containers/Misc/CircularBufferC.h>

using namespace Dia::Core::Containers;

TEST(CircularBuffer, PushAndOverflow_WorksCorrectly)
{
    CircularBufferC<int, 3> buffer;

    EXPECT_EQ(buffer.Size(), 0);
    EXPECT_TRUE(buffer.IsEmpty());

    buffer.PushNext(0);

    EXPECT_EQ(buffer.Size(), 1);
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Front(), 0);
    EXPECT_EQ(buffer.Back(), 0);

    buffer.PushNext(1);

    EXPECT_EQ(buffer.Size(), 2);
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Front(), 1);
    EXPECT_EQ(buffer.Back(), 0);

    buffer.PushNext(2);

    EXPECT_EQ(buffer.Size(), 3);
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Front(), 2);
    EXPECT_EQ(buffer.Back(), 0);

    buffer.PushNext(3);

    EXPECT_EQ(buffer.Size(), 3);
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Front(), 3);
    EXPECT_EQ(buffer.Back(), 1);

    buffer.PushNext(4);

    EXPECT_EQ(buffer.Size(), 3);
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Front(), 4);
    EXPECT_EQ(buffer.Back(), 2);

    buffer.RemoveAll();

    EXPECT_EQ(buffer.Size(), 0);
    EXPECT_TRUE(buffer.IsEmpty());
}

TEST(CircularBuffer, ConstIterator_IteratesCircularly)
{
    CircularBufferC<int, 3> buffer;

    EXPECT_EQ(buffer.Size(), 0);
    EXPECT_TRUE(buffer.IsEmpty());

    buffer.PushNext(1);
    buffer.PushNext(2);
    buffer.PushNext(3);

    CircularBufferC<int, 3>::ConstIterator iter(&buffer.Back(), &buffer.Back(), &buffer.Front());

    EXPECT_EQ(buffer.Size(), 3);

    EXPECT_EQ(*iter.Current(), 1);
    iter.Next();
    EXPECT_EQ(*iter.Current(), 2);
    iter.Next();
    EXPECT_EQ(*iter.Current(), 3);
    iter.Next();
    EXPECT_EQ(*iter.Current(), 1);
    iter.Next();
    EXPECT_EQ(*iter.Current(), 2);
    iter.Next();
    EXPECT_EQ(*iter.Current(), 3);
    iter.Next();
}

TEST(CircularBuffer, ConstIterator_WithStructs_IteratesCorrectly)
{
    struct Foo
    {
        Foo() : x(0), y(0) {}

        Foo(int _x, int _y)
            : x(_x)
            , y(_y)
        {}

        int x;
        int y;
    };

    CircularBufferC<Foo, 3> buffer;

    EXPECT_EQ(buffer.Size(), 0);
    EXPECT_TRUE(buffer.IsEmpty());

    buffer.PushNext(Foo(0, 9));
    buffer.PushNext(Foo(1, 8));
    buffer.PushNext(Foo(2, 7));

    CircularBufferC<Foo, 3>::ConstIterator iter(&buffer.Back(), &buffer.Back(), &buffer.Front());

    EXPECT_EQ(buffer.Size(), 3);

    EXPECT_EQ((*iter.Current()).y, 9);
    iter.Next();
    EXPECT_EQ((*iter.Current()).y, 8);
    iter.Next();
    EXPECT_EQ((*iter.Current()).y, 7);
    iter.Next();
    EXPECT_EQ((*iter.Current()).y, 9);
    iter.Previous();
    EXPECT_EQ((*iter.Current()).y, 7);
    iter.Previous();
    EXPECT_EQ((*iter.Current()).y, 8);
    iter.Previous();
    EXPECT_EQ((*iter.Current()).y, 9);
}
