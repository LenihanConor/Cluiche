#pragma once

// Test helper: fluent builder producing dia.value_schema JSON text.
// Ships with DiaValueSchema (Testing/ convention). Setters apply to the most recently added entry.

#include <json/json.h>
#include <string>

namespace Dia::ValueSchema::Testing
{
	class SchemaFixtureBuilder
	{
	public:
		SchemaFixtureBuilder& For(const char* forFormat) { mFor = forFormat; return *this; }
		SchemaFixtureBuilder& Format(const char* format) { mFormat = format; return *this; }
		SchemaFixtureBuilder& Version(int version) { mVersion = version; return *this; }

		SchemaFixtureBuilder& Entry(const char* key, const char* kind = "parameter", const char* type = "float")
		{
			Json::Value e(Json::objectValue);
			e["key"] = key;
			e["kind"] = kind;
			e["type"] = type;
			mEntries.append(e);
			return *this;
		}
		SchemaFixtureBuilder& Set(const char* name, const Json::Value& value)
		{
			Last()[name] = value;
			return *this;
		}
		SchemaFixtureBuilder& Label(const char* v)       { return Set("label", Json::Value(v)); }
		SchemaFixtureBuilder& Description(const char* v) { return Set("description", Json::Value(v)); }
		SchemaFixtureBuilder& Unit(const char* v)        { return Set("unit", Json::Value(v)); }
		SchemaFixtureBuilder& Hint(const char* v)        { return Set("hint", Json::Value(v)); }
		SchemaFixtureBuilder& Source(const char* v)      { return Set("source", Json::Value(v)); }
		SchemaFixtureBuilder& Ref(const char* v)         { return Set("ref", Json::Value(v)); }
		SchemaFixtureBuilder& Min(double v)              { return Set("min", Json::Value(v)); }
		SchemaFixtureBuilder& Max(double v)              { return Set("max", Json::Value(v)); }
		SchemaFixtureBuilder& Default(const Json::Value& v) { return Set("default", v); }
		SchemaFixtureBuilder& Unique(bool v = true)      { return Set("unique", Json::Value(v)); }
		SchemaFixtureBuilder& Required(bool v = true)    { return Set("required", Json::Value(v)); }
		SchemaFixtureBuilder& Affects(const char* a)     { return Append("affects", a); }
		SchemaFixtureBuilder& Related(const char* a)     { return Append("related", a); }
		SchemaFixtureBuilder& EnumValueEntry(const char* id, const char* label, const char* description = "", const char* colour = nullptr)
		{
			Json::Value v(Json::objectValue);
			v["id"] = id;
			v["label"] = label;
			v["description"] = description;
			if (colour) { v["colour"] = colour; }
			Json::Value& arr = Last()["values"];
			if (!arr.isArray()) { arr = Json::Value(Json::arrayValue); }
			arr.append(v);
			return *this;
		}

		std::string Build() const
		{
			Json::Value root(Json::objectValue);
			if (!mFormat.empty()) { root["format"] = mFormat; }
			root["version"] = mVersion;
			root["for"] = mFor;
			root["entries"] = mEntries;
			Json::FastWriter w;
			return w.write(root);
		}
		// Build with the "entries" member omitted.
		std::string BuildWithoutEntries() const
		{
			Json::Value root(Json::objectValue);
			root["format"] = mFormat;
			root["version"] = mVersion;
			root["for"] = mFor;
			Json::FastWriter w;
			return w.write(root);
		}

	private:
		Json::Value& Last() { return mEntries[mEntries.size() - 1]; }
		SchemaFixtureBuilder& Append(const char* name, const char* v)
		{
			Json::Value& arr = Last()[name];
			if (!arr.isArray()) { arr = Json::Value(Json::arrayValue); }
			arr.append(v);
			return *this;
		}

		std::string	mFormat = "dia.value_schema";
		int			mVersion = 1;
		std::string	mFor = "test.format";
		Json::Value	mEntries = Json::Value(Json::arrayValue);
	};
}
