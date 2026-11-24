#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaVersion.h>

TEST(DiaValueSchema, FormatVersionIsOne)
{
	EXPECT_EQ(Dia::ValueSchema::kFormatVersion, 1);
}
