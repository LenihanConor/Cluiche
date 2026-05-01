#include <gtest/gtest.h>
#include <DiaUI/UIDataBuffer.h>

using namespace Dia::UI;

// ==============================================================================
// UIDataBuffer Tests
// ==============================================================================

TEST(UIDataBuffer, DefaultConstruct_ZeroDimensionsNullBuffer)
{
    UIDataBuffer buf;
    EXPECT_EQ(buf.GetWidth(), 0);
    EXPECT_EQ(buf.GetHeight(), 0);
    EXPECT_EQ(buf.GetBufferSize(), 0);
    EXPECT_EQ(buf.GetBuffer(), nullptr);
}

TEST(UIDataBuffer, Create_SetsDimensionsAndCopiesData)
{
    unsigned char data[4] = { 1, 2, 3, 4 };
    UIDataBuffer buf(2, 2, data, 4);
    EXPECT_EQ(buf.GetWidth(), 2);
    EXPECT_EQ(buf.GetHeight(), 2);
    EXPECT_EQ(buf.GetBufferSize(), 4);
    EXPECT_NE(buf.GetBuffer(), nullptr);
    EXPECT_EQ(buf.GetBuffer()[0], 1);
    EXPECT_EQ(buf.GetBuffer()[3], 4);
}

TEST(UIDataBuffer, Create_DataIsCopied_NotAliased)
{
    unsigned char data[4] = { 10, 20, 30, 40 };
    UIDataBuffer buf(2, 2, data, 4);
    data[0] = 99;
    EXPECT_EQ(buf.GetBuffer()[0], 10);
}

TEST(UIDataBuffer, CopyConstruct_IndependentCopy)
{
    unsigned char data[3] = { 5, 6, 7 };
    UIDataBuffer original(3, 1, data, 3);
    UIDataBuffer copy(original);

    EXPECT_EQ(copy.GetWidth(), 3);
    EXPECT_EQ(copy.GetHeight(), 1);
    EXPECT_EQ(copy.GetBufferSize(), 3);
    EXPECT_NE(copy.GetBuffer(), original.GetBuffer());
    EXPECT_EQ(copy.GetBuffer()[1], 6);
}

TEST(UIDataBuffer, CopyConstruct_FromEmpty_RemainsEmpty)
{
    UIDataBuffer empty;
    UIDataBuffer copy(empty);
    EXPECT_EQ(copy.GetBuffer(), nullptr);
    EXPECT_EQ(copy.GetWidth(), 0);
}

TEST(UIDataBuffer, Assignment_CopiesData)
{
    unsigned char data[2] = { 11, 22 };
    UIDataBuffer src(2, 1, data, 2);
    UIDataBuffer dst;
    dst = src;

    EXPECT_EQ(dst.GetWidth(), 2);
    EXPECT_EQ(dst.GetBufferSize(), 2);
    EXPECT_EQ(dst.GetBuffer()[0], 11);
}

TEST(UIDataBuffer, Destroy_ResetsToZero)
{
    unsigned char data[2] = { 1, 2 };
    UIDataBuffer buf(2, 1, data, 2);
    buf.Destroy();
    EXPECT_EQ(buf.GetWidth(), 0);
    EXPECT_EQ(buf.GetHeight(), 0);
    EXPECT_EQ(buf.GetBufferSize(), 0);
}
