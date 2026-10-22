// TestSchemaReader.cpp
// Unit tests for SchemaReader — loads component schema from .diaschema files.

#include <gtest/gtest.h>
#include <DiaEntityTemplateEditor/SchemaReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <stdio.h>
#include <string.h>
#include <string>

using namespace Dia::EntityTemplateEditor;
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

// Schema where first component has default_values, second does not.
static const char* kSchemaWithDefaults =
    "{"
    "  \"version\": { \"major\": 1, \"minor\": 0 },"
    "  \"components\": ["
    "    { \"type_id\": \"cluichetest.transform\", \"debug_name\": \"TransformComponent\","
    "      \"fields\": [ {\"name\": \"x\", \"kind\": \"primitive\"} ],"
    "      \"default_values\": { \"x\": 0, \"scale\": 1.0 } },"
    "    { \"type_id\": \"cluichetest.visual\", \"debug_name\": \"VisualComponent\","
    "      \"fields\": [] }"
    "  ]"
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

TEST(SchemaReader, LoadFromFile_WithDefaultValues_DefaultValuesPresent)
{
    WriteSchemaFile(kTestSchemaPath, kSchemaWithDefaults);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_GE(reader.GetComponentCount(), 1u);
    const Json::Value& dv = reader.GetDefaultValues(0);
    EXPECT_FALSE(dv.isNull());
    EXPECT_TRUE(dv.isMember("x"));

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_WithDefaultValues_ValueCorrect)
{
    WriteSchemaFile(kTestSchemaPath, kSchemaWithDefaults);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_GE(reader.GetComponentCount(), 1u);
    const Json::Value& dv = reader.GetDefaultValues(0);
    ASSERT_FALSE(dv.isNull());
    EXPECT_FLOAT_EQ(dv["scale"].asFloat(), 1.0f);

    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_NoDefaultValues_IsNull)
{
    WriteSchemaFile(kTestSchemaPath, kSchemaWithDefaults);

    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_GE(reader.GetComponentCount(), 2u);
    EXPECT_TRUE(reader.GetDefaultValues(1).isNull());

    DeleteFile(kTestSchemaPath);
}

// ==============================================================================
// Field metadata (label/description/unit/hint/min/max/default)
// ==============================================================================

static const char* kSchemaDescribedField =
    "{"
    "  \"version\": { \"major\": 1, \"minor\": 0 },"
    "  \"components\": ["
    "    { \"type_id\": \"t.c\", \"debug_name\": \"C\","
    "      \"fields\": ["
    "        {\"name\": \"speed\", \"kind\": \"primitive\", \"label\": \"Move Speed\","
    "         \"description\": \"100% of units per second\", \"unit\": \"m/s\", \"hint\": \"slider\","
    "         \"min\": 0, \"max\": 12.5, \"default\": 3.5 },"
    "        {\"name\": \"plain\", \"kind\": \"primitive\"}"
    "      ] }"
    "  ]"
    "}";

TEST(SchemaReader, LoadFromFile_DescribedField_ParsesAllMetadata)
{
    WriteSchemaFile(kTestSchemaPath, kSchemaDescribedField);
    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_EQ(reader.GetComponentCount(), 1u);
    const SchemaFieldEntry& f = reader.GetComponent(0).fields[0];
    EXPECT_STREQ(f.label, "Move Speed");
    EXPECT_STREQ(f.description, "100% of units per second");
    EXPECT_STREQ(f.unit, "m/s");
    EXPECT_STREQ(f.hint, "slider");
    EXPECT_TRUE(f.hasMin);
    EXPECT_TRUE(f.hasMax);
    EXPECT_DOUBLE_EQ(f.min, 0.0);
    EXPECT_DOUBLE_EQ(f.max, 12.5);
    EXPECT_TRUE(f.hasDefault);
    EXPECT_STREQ(f.defaultJson, "3.5");
    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_PlainField_HasNoMetadata)
{
    WriteSchemaFile(kTestSchemaPath, kSchemaDescribedField);
    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_EQ(reader.GetComponentCount(), 1u);
    const SchemaFieldEntry& f = reader.GetComponent(0).fields[1];
    EXPECT_STREQ(f.name, "plain");
    EXPECT_STREQ(f.kind, "primitive");
    EXPECT_STREQ(f.label, "");
    EXPECT_STREQ(f.description, "");
    EXPECT_STREQ(f.unit, "");
    EXPECT_STREQ(f.hint, "");
    EXPECT_FALSE(f.hasMin);
    EXPECT_FALSE(f.hasMax);
    EXPECT_FALSE(f.hasDefault);
    DeleteFile(kTestSchemaPath);
}

TEST(SchemaReader, LoadFromFile_OverlongDescription_TruncatedSafely)
{
    std::string longText(5000, 'a');
    std::string json = "{\"version\":{\"major\":1,\"minor\":0},\"components\":[{\"type_id\":\"t.c\","
        "\"debug_name\":\"C\",\"fields\":[{\"name\":\"f\",\"kind\":\"primitive\",\"description\":\""
        + longText + "\"}]}]}";
    WriteSchemaFile(kTestSchemaPath, json.c_str());
    SchemaReader reader;
    reader.LoadFromFile(kTestSchemaPath);
    ASSERT_EQ(reader.GetComponentCount(), 1u);
    const SchemaFieldEntry& f = reader.GetComponent(0).fields[0];
    EXPECT_EQ(strlen(f.description), sizeof(f.description) - 1);
    EXPECT_GE(sizeof(f.description), 1024u);
    DeleteFile(kTestSchemaPath);
}
