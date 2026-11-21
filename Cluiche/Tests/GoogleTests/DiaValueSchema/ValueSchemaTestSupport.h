#pragma once

// Shared helpers for the exhaustive DiaValueSchema core tests (Loader boundary/fuzz, Registry property,
// Clamper edge, Ramp edge). Header-only, test-private.

#include <DiaCore/Core/Assert.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <json/json.h>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace VsTest
{
	// Counts DIA_ASSERT hits instead of breaking into the debugger (Debug builds only; Release compiles asserts out).
	struct AssertCounter
	{
		static inline int sCount = 0;
		static void Handler(const char*, const char*, int, const char*, ...) { ++sCount; }

		Dia::Core::DIA_ASSERT_FUNC mPrev;
		AssertCounter() : mPrev(Dia::Core::g_pAssertFunc) { sCount = 0; Dia::Core::g_pAssertFunc = &Handler; }
		~AssertCounter() { Dia::Core::g_pAssertFunc = mPrev; }
		int Count() const { return sCount; }
	};

	inline std::string Rep(char c, size_t n) { return std::string(n, c); }

	inline Json::Value GoodEntry(const char* key = "k")
	{
		Json::Value e(Json::objectValue);
		e["key"] = key;
		e["kind"] = "parameter";
		e["label"] = "Label";
		e["description"] = "Desc";
		e["type"] = "float";
		return e;
	}

	inline Json::Value GoodDoc(const Json::Value& entry, const char* forName = "test.format")
	{
		Json::Value root(Json::objectValue);
		root["format"] = "dia.value_schema";
		root["version"] = 1;
		root["for"] = forName;
		root["entries"] = Json::Value(Json::arrayValue);
		root["entries"].append(entry);
		return root;
	}

	inline std::string Text(const Json::Value& v)
	{
		Json::FastWriter w;
		return w.write(v);
	}

	inline Dia::ValueSchema::LoadResult LoadEntry(const Json::Value& e, const char* file = "t.diavalues")
	{
		return Dia::ValueSchema::ValueSchemaLoader::LoadFromText(Text(GoodDoc(e)).c_str(), file);
	}

	inline bool Contains(const char* haystack, const char* needle)
	{
		return std::string(haystack).find(needle) != std::string::npos;
	}

	inline bool ReadFileBytes(const std::filesystem::path& p, std::string& out)
	{
		std::ifstream in(p, std::ios::binary);
		if (!in) { return false; }
		std::stringstream ss;
		ss << in.rdbuf();
		out = ss.str();
		return true;
	}

	// Walks up from the calling source file until relative (e.g. "Dia/DiaValueSchema/Data/ramps.json") exists.
	inline std::filesystem::path FindUpwards(const char* sourceFile, const std::filesystem::path& relative)
	{
		std::error_code ec;
		std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(sourceFile), ec).parent_path();
		while (!dir.empty())
		{
			const std::filesystem::path candidate = dir / relative;
			if (std::filesystem::exists(candidate, ec)) { return candidate; }
			const std::filesystem::path parent = dir.parent_path();
			if (parent == dir) { break; }
			dir = parent;
		}
		return std::filesystem::path();
	}
}
