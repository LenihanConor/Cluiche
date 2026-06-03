// TestSchemaReader.cpp
// Unit tests for SchemaReader — loads component schema from .diaschema files.

#include <gtest/gtest.h>
#include <DiaBlueprintEditor/SchemaReader.h>
#include <DiaCore/CRC/StringCRC.h>

#include <stdio.h>
#include <string.h>

using namespace Dia::BlueprintEditor;
using Dia::Core::StringCRC;

// ==============================================================================
// Test Helpers
// ==============================================================================

static void WriteSchemaFile(const char* path, const char* content)
{
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

static void DeleteFile(const char* path)
{
    remove(path);
}

static const char* kTestSchemaPath = "TestSchemaReader_temp.diaschema";

// Valid schema: 2 components; first has 2 fields, second has none.
static const char* kValidSchema =
    "{"
    "  \"version\": { \"major\": 1, \"minor\": 3 },"
    "  \"game\": \"cluichetest\","
    "  \"components\": ["
    "    { \"type_id\": \"cluichetest.transform\", \"debug_name\": \"TransformComponent\","
    "      \"fields\": [ {\"name\": \"x\", \"kind\": \"primitive\"}, {\"name\": \"y\", \"kind\": \"primitive\"} ] },"
    "    { \"type_id\": \"cluichetest.visual\", \"debug_name\": \"VisualComponent\","
    "      \"fields\": [] }"
    "  ]"
    "}";

// Valid schema: empty components array.
static const char* kEmptyComponents =
    "{"
    "  \"version\": { \"major\": 1, \"minor\": 0 },"
    "  \"components\": []"
    "}";

// ==============================================================================
// Tests
// ==============================================================================

TEST(SchemaReader, DefaultConstruct_IsNotLoaded)
{
    SchemaReader reader;
    EXPECT_FALSE(reader.IsLoaded());
}

TEST(SchemaReader, LoadFromFile_ValidSchema_IsLoaded)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    EXPECT_TRUE(reader.IsLoaded());

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_ComponentCount_Correct)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    EXPECT_EQ(reader.GetComponentCount(), 2u);

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_ComponentTypeId_Correct)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_GE(reader.GetComponentCount(), 1u);
    EXPECT_EQ(reader.GetComponent(0).typeId, StringCRC("cluichetest.transform"));

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_ComponentDebugName_Correct)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_GE(reader.GetComponentCount(), 1u);
    EXPECT_STREQ(reader.GetComponent(0).debugName, "TransformComponent");

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_FieldCount_Correct)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_GE(reader.GetComponentCount(), 1u);
    EXPECT_EQ(reader.GetComponent(0).fields.Size(), 2u);

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_FieldName_Correct)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_GE(reader.GetComponentCount(), 1u);
    ASSERT_GE(reader.GetComponent(0).fields.Size(), 1u);
    EXPECT_STREQ(reader.GetComponent(0).fields[0].name, "x");

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_FieldKind_Correct)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_GE(reader.GetComponentCount(), 1u);
    ASSERT_GE(reader.GetComponent(0).fields.Size(), 1u);
    EXPECT_STREQ(reader.GetComponent(0).fields[0].kind, "primitive");

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_VersionMajor_Correct)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    EXPECT_EQ(reader.GetVersion().major, 1);

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_VersionMinor_Correct)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    EXPECT_EQ(reader.GetVersion().minor, 3);

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_MissingFile_EmptyList_NoCrash)
{
    SchemaReader reader;
    reader.LoadFromFile("nonexistent_schema_file_12345.diaschema");
    EXPECT_FALSE(reader.IsLoaded());
    EXPECT_EQ(reader.GetComponentCount(), 0u);
}

TEST(SchemaReader, LoadFromFile_MalformedJson_EmptyList_NoCrash)
{
    WriteSchemaFile(kTestSchemaPath, "{not valid json at all");

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    EXPECT_FALSE(reader.IsLoaded());
    EXPECT_EQ(reader.GetComponentCount(), 0u);

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_EmptyComponents_IsLoaded)
{
    WriteSchemaFile(kTestSchemaPath, kEmptyComponents);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    EXPECT_TRUE(reader.IsLoaded());
    EXPECT_EQ(reader.GetComponentCount(), 0u);

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, Clear_AfterLoad_NotLoaded)
{
    WriteSchemaFile(kTestSchemaPath, kValidSchema);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_TRUE(reader.IsLoaded());

    reader.Clear();
    EXPECT_FALSE(reader.IsLoaded());
    EXPECT_EQ(reader.GetComponentCount(), 0u);

    DeleteFile(kTestSchemaPath);
}
