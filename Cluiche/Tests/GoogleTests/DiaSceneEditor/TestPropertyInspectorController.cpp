// Tests for PropertyInspectorController field metadata passthrough
// (DiaValueSchema Feature 7, Scene Editor propRow): label/description/unit/hint/min/max/default
// from registeredtypes.diaschema are copied into components[].fields[] only when present.

#include <gtest/gtest.h>
#include <DiaSceneEditor/PropertyInspectorController.h>
#include <DiaEntityTemplateEditor/SchemaReader.h>
#include <DiaCore/Json/external/json/json.h>

#include <stdio.h>
#include <string>

using namespace Dia::SceneEditor;
using namespace Dia::EntityTemplateEditor;

namespace
{
	const char* kPicSchemaPath   = "TestPIC_temp.diaschema";
	const char* kPicTemplatePath = "pic_hero.diaentitytemplate";

	void WriteText(const char* path, const std::string& content)
	{
		FILE* f = nullptr;
		fopen_s(&f, path, "w");
		if (f) { fputs(content.c_str(), f); fclose(f); }
	}

	std::string SchemaText(const std::string& description)
	{
		return std::string(
			"{ \"version\": { \"major\": 1, \"minor\": 0 }, \"components\": ["
			"  { \"type_id\": \"t.meta\", \"debug_name\": \"Meta\", \"fields\": ["
			"    {\"name\": \"speed\", \"kind\": \"primitive\", \"label\": \"Speed\","
			"     \"description\": \"") + description + "\", \"unit\": \"m/s\", \"hint\": \"slider\","
			"     \"min\": 0, \"max\": 12.5, \"default\": 3.5 },"
			"    {\"name\": \"plain\", \"kind\": \"primitive\"}"
			"  ] } ] }";
	}

	const char* kTemplateText =
		"{ \"entity_template\": { \"id\": \"pic_hero\", \"components\": ["
		"  { \"type\": \"t.meta\", \"fields\": { \"speed\": 1, \"plain\": 7 } },"
		"  { \"type\": \"t.other\", \"fields\": { \"x\": 2 } }"
		"] } }";

	// Fields come from the raw template JSON (alphabetical), so look them up by name.
	const Json::Value& FieldNamed(const Json::Value& comp, const char* name)
	{
		static const Json::Value kNone;
		for (unsigned int i = 0; i < comp["fields"].size(); ++i)
			if (comp["fields"][i]["name"].asString() == name) return comp["fields"][i];
		return kNone;
	}

	Json::Value SceneWithEntity()
	{
		Json::Value root;
		Json::Value ent(Json::objectValue);
		ent["id"] = "e1";
		ent["blueprint"] = "pic_hero";
		root["scene2d"]["entities"].append(ent);
		return root;
	}

	// Returns the full property JSON; schema may be null (no schema attached).
	Json::Value BuildProps(const std::string& schemaText, bool attachSchema)
	{
		WriteText(kPicTemplatePath, kTemplateText);
		WriteText(kPicSchemaPath, schemaText);
		SchemaReader schema;
		schema.LoadFromFile(kPicSchemaPath);
		PropertyInspectorController ctrl;
		if (attachSchema) ctrl.SetSchema(&schema);
		Json::Value result = ctrl.BuildPropertyJson(SceneWithEntity(), "entity", "e1", ".");
		ctrl.SetSchema(nullptr);
		remove(kPicSchemaPath);
		remove(kPicTemplatePath);
		return result;
	}
}

TEST(PropertyInspectorMetadata, DescribedField_IncludesMetadata)
{
	Json::Value r = BuildProps(SchemaText("100% fast"), true);
	const Json::Value& f = FieldNamed(r["components"][0], "speed");
	ASSERT_EQ(f["name"].asString(), "speed");
	EXPECT_EQ(f["label"].asString(), "Speed");
	EXPECT_EQ(f["description"].asString(), "100% fast");
	EXPECT_EQ(f["unit"].asString(), "m/s");
	EXPECT_EQ(f["hint"].asString(), "slider");
	ASSERT_TRUE(f["min"].isNumeric());
	EXPECT_DOUBLE_EQ(f["min"].asDouble(), 0.0);
	EXPECT_DOUBLE_EQ(f["max"].asDouble(), 12.5);
	ASSERT_TRUE(f.isMember("default"));
	EXPECT_DOUBLE_EQ(f["default"].asDouble(), 3.5);
	EXPECT_EQ(f["value"].asInt(), 1);
}

TEST(PropertyInspectorMetadata, UndescribedFieldInSchema_OutputUnchanged)
{
	Json::Value withSchema = BuildProps(SchemaText("d"), true);
	Json::Value without    = BuildProps(SchemaText("d"), false);
	const Json::Value& f = FieldNamed(withSchema["components"][0], "plain");
	ASSERT_EQ(f["name"].asString(), "plain");
	EXPECT_EQ(f.getMemberNames().size(), 4u);   // name, overridden, value, kind
	EXPECT_EQ(f, FieldNamed(without["components"][0], "plain"));
}

TEST(PropertyInspectorMetadata, ComponentNotInSchema_OutputUnchanged)
{
	Json::Value withSchema = BuildProps(SchemaText("d"), true);
	Json::Value without    = BuildProps(SchemaText("d"), false);
	EXPECT_EQ(withSchema["components"][1], without["components"][1]);
}

TEST(PropertyInspectorMetadata, NoSchema_NoMetadataKeys)
{
	Json::Value r = BuildProps(SchemaText("d"), false);
	const Json::Value& f = FieldNamed(r["components"][0], "speed");
	EXPECT_EQ(f.getMemberNames().size(), 4u);
	EXPECT_FALSE(f.isMember("description"));
}

TEST(PropertyInspectorMetadata, OverlongDescription_TruncatedNotCrash)
{
	Json::Value r = BuildProps(SchemaText(std::string(3000, 'a')), true);
	const Json::Value& f = FieldNamed(r["components"][0], "speed");
	ASSERT_TRUE(f.isMember("description"));
	EXPECT_GT(f["description"].asString().size(), 0u);
	EXPECT_LE(f["description"].asString().size(), 1023u);
}

TEST(PropertyInspectorMetadata, BlueprintDefaults_IncludesMetadata)
{
	WriteText(kPicTemplatePath, kTemplateText);
	WriteText(kPicSchemaPath, SchemaText("fast"));
	SchemaReader schema;
	schema.LoadFromFile(kPicSchemaPath);
	PropertyInspectorController ctrl;
	ctrl.SetSchema(&schema);
	Json::Value r = ctrl.BuildBlueprintDefaultsJson("pic_hero", "entity", ".");
	ctrl.SetSchema(nullptr);
	remove(kPicSchemaPath);
	remove(kPicTemplatePath);
	EXPECT_EQ(FieldNamed(r["components"][0], "speed")["description"].asString(), "fast");
}
