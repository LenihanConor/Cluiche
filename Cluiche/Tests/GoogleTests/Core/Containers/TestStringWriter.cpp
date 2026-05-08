#include <gtest/gtest.h>
#include <DiaCore/Containers/Strings/StringWriter.h>
#include <DiaCore/Strings/String8.h>

using namespace Dia::Core::Containers;

TEST(StringWriter, DefaultConstruction_MethodsAssert)
{
    StringWriter buffer;

    EXPECT_DEATH({ buffer.AsCStr(); }, "");
    EXPECT_DEATH({ buffer.Capacity(); }, "");
    EXPECT_DEATH({ buffer.CurrentLength(); }, "");
    EXPECT_DEATH({ buffer << "BLAH"; }, "");
    EXPECT_DEATH({ buffer << 'b'; }, "");
}

TEST(StringWriter, ConstructWithBuffer_InitializesCorrectly)
{
    char bufferMemory[8];
    StringWriter buffer(&bufferMemory[0], 8);

    EXPECT_EQ(buffer.Capacity(), 8);
    EXPECT_EQ(buffer.CurrentLength(), 0);
}

TEST(StringWriter, WriteChar_AddsToBuffer)
{
    char bufferMemory[8];
    StringWriter buffer(&bufferMemory[0], 8);

    buffer << 'c';

    EXPECT_EQ(String8(buffer.AsCStr()), "c");
    EXPECT_EQ(buffer.Capacity(), 8);
    EXPECT_EQ(buffer.CurrentLength(), 1);
}

TEST(StringWriter, WriteString_AppendsToBuffer)
{
    char bufferMemory[8];
    StringWriter buffer(&bufferMemory[0], 8);

    buffer << 'c';
    buffer << "ats";

    EXPECT_EQ(String8(buffer.AsCStr()), "cats");
    EXPECT_EQ(buffer.Capacity(), 8);
    EXPECT_EQ(buffer.CurrentLength(), 4);
}

TEST(StringWriter, WriteOverflow_Asserts)
{
    char bufferMemory[8];
    StringWriter buffer(&bufferMemory[0], 8);

    buffer << "cats";

    EXPECT_DEATH({ buffer << "are crap animals"; }, "");
}

TEST(StringWriter, AssignBuffer_AfterConstruction_Asserts)
{
    char bufferMemory[8];
    StringWriter buffer(&bufferMemory[0], 8);

    EXPECT_DEATH({ buffer.AssignBuffer(&bufferMemory[0], 8); }, "");
}

TEST(StringWriter, AssignBuffer_AfterDefaultConstruction_Works)
{
    char bufferMemory[8];
    StringWriter buffer;

    buffer.AssignBuffer(&bufferMemory[0], 8);

    EXPECT_EQ(buffer.Capacity(), 8);
    EXPECT_EQ(buffer.CurrentLength(), 0);
}

TEST(StringWriter, AssignBuffer_ThenWrite_Works)
{
    char bufferMemory[8];
    StringWriter buffer;

    buffer.AssignBuffer(&bufferMemory[0], 8);

    buffer << 'c';

    EXPECT_EQ(String8(buffer.AsCStr()), "c");
    EXPECT_EQ(buffer.Capacity(), 8);
    EXPECT_EQ(buffer.CurrentLength(), 1);

    buffer << "ats";

    EXPECT_EQ(String8(buffer.AsCStr()), "cats");
    EXPECT_EQ(buffer.Capacity(), 8);
    EXPECT_EQ(buffer.CurrentLength(), 4);
}

TEST(StringWriter, AssignBuffer_WriteOverflow_Asserts)
{
    char bufferMemory[8];
    StringWriter buffer;

    buffer.AssignBuffer(&bufferMemory[0], 8);
    buffer << "cats";

    EXPECT_DEATH({ buffer << "are crap animals"; }, "");
}

TEST(StringWriter, AssignBuffer_CalledTwice_Asserts)
{
    char bufferMemory[8];
    StringWriter buffer;

    buffer.AssignBuffer(&bufferMemory[0], 8);

    EXPECT_DEATH({ buffer.AssignBuffer(&bufferMemory[0], 8); }, "");
}
