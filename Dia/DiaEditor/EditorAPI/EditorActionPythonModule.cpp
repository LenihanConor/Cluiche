#include "EditorActionPythonModule.h"

#include <DiaEditor/EditorAPI/EditorActionManifest.h>
#include <DiaPython/Module/Module.h>
#include <DiaPython/Lifecycle/Lifecycle.h>
#include <DiaPython/TypeConversion/Conversion.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace Editor
	{
		namespace
		{
			// Serialize a Json::Value to a compact JSON string.
			// Callers on the Python side use json.loads() to convert to a dict.
			std::string SerializeJson(const Json::Value& value)
			{
				Json::StreamWriterBuilder builder;
				builder["indentation"] = "";
				return Json::writeString(builder, value);
			}

			// Build a Python callback that captures the action name, dispatches through
			// the queue, and returns the result as a JSON string.
			Dia::Python::PythonCallback MakeActionCallback(
				Dia::Core::StringCRC actionName,
				EditorActionRegistry* registry,
				EditorActionQueue* queue)
			{
				return [actionName, registry, queue](const Dia::Python::PythonArgs& args) -> Dia::Python::PythonObject
				{
					// Collect positional string args into a JSON object keyed by index.
					// For typed actions the first arg is conventionally a JSON string produced
					// by the caller, e.g.  dia_editor.project.open_path('{"path":"..."}')
					Json::Value params(Json::objectValue);
					if (args.GetCount() > 0)
					{
						Dia::Python::PythonObject first = args.GetArg(0);
						if (first.IsString())
						{
							const char* raw = Dia::Python::ToString(first);
							if (raw != nullptr && raw[0] == '{')
							{
								// Try to parse as JSON object
								Json::CharReaderBuilder readerBuilder;
								std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
								std::string errors;
								const bool ok = reader->parse(raw, raw + strlen(raw), &params, &errors);
								if (!ok)
								{
									DIA_LOG_ERROR("Editor", "EditorActionPythonModule: invalid JSON param for '%s': %s",
										actionName.AsChar(), errors.c_str());
									Json::Value err(Json::objectValue);
									err["success"] = false;
									err["reason"]  = "invalid_json_params";
									return Dia::Python::ToPython(SerializeJson(err).c_str());
								}
							}
							else if (raw != nullptr && raw[0] != '\0')
							{
								// Bare string — treat as positional arg "value"
								params["value"] = raw;
							}
						}
					}

					Json::Value result = queue->DispatchAndWait(actionName, params);
					return Dia::Python::ToPython(SerializeJson(result).c_str());
				};
			}
		}

		void GeneratePythonModule(EditorActionRegistry* registry, EditorActionQueue* queue)
		{
			if (!Dia::Python::IsInitialized())
			{
				DIA_LOG_WARNING("Editor", "EditorActionPythonModule: Python not initialized, skipping module generation");
				return;
			}

			if (registry == nullptr || queue == nullptr)
			{
				DIA_LOG_ERROR("Editor", "EditorActionPythonModule: registry or queue is null");
				return;
			}

			// Refuse to register twice (CreateModule returns nullptr for duplicates).
			Dia::Python::Module* root = Dia::Python::CreateModule("dia_editor");
			if (root == nullptr)
			{
				DIA_LOG_WARNING("Editor", "EditorActionPythonModule: 'dia_editor' module already exists, skipping");
				return;
			}

			const EditorActionManifest& manifest = registry->GetManifest();
			const unsigned int count = manifest.GetCount();

			// Collect unique category names so we can create one sub-module each.
			// Max 128 actions → at most 128 distinct categories; reuse existing module pointer.
			static const unsigned int kMaxCategories = 32;
			struct CategoryEntry
			{
				const char*             name   = nullptr;
				Dia::Python::Module*    module = nullptr;
				unsigned int            count  = 0;
			};
			Dia::Core::Containers::DynamicArrayC<CategoryEntry, kMaxCategories> categories;

			auto findOrAddCategory = [&](const char* category) -> Dia::Python::Module*
			{
				if (category == nullptr || category[0] == '\0')
					category = "misc";

				for (unsigned int i = 0; i < categories.Size(); ++i)
				{
					if (strcmp(categories[i].name, category) == 0)
						return categories[i].module;
				}

				// Build sub-module name e.g. "dia_editor.project"
				char subName[128];
				snprintf(subName, sizeof(subName), "dia_editor.%s", category);

				Dia::Python::Module* sub = Dia::Python::CreateModule(subName);
				if (sub == nullptr)
				{
					DIA_LOG_ERROR("Editor", "EditorActionPythonModule: failed to create sub-module '%s'", subName);
					return nullptr;
				}

				if (!categories.IsFull())
				{
					CategoryEntry entry;
					entry.name   = category;
					entry.module = sub;
					entry.count  = 0;
					categories.Add(entry);
				}

				return sub;
			};

			unsigned int registered = 0;
			for (unsigned int i = 0; i < count; ++i)
			{
				const EditorActionEntry& entry = manifest.GetAt(i);
				if (entry.name.AsChar() == nullptr)
					continue;

				Dia::Python::Module* sub = findOrAddCategory(entry.category);
				if (sub == nullptr)
					continue;

				// Python function name is the action name with dots replaced by underscores,
				// and the category prefix stripped (since it's already the sub-module).
				// e.g. "project.open_path" in sub "dia_editor.project" → "open_path"
				const char* actionName = entry.name.AsChar();
				const char* dot = strchr(actionName, '.');
				const char* fnName = (dot != nullptr) ? dot + 1 : actionName;

				// Replace remaining dots with underscores (nested actions like "a.b.c" → "b_c")
				char safeFnName[128];
				unsigned int fi = 0;
				for (; fnName[fi] != '\0' && fi < sizeof(safeFnName) - 1; ++fi)
					safeFnName[fi] = (fnName[fi] == '.') ? '_' : fnName[fi];
				safeFnName[fi] = '\0';

				Dia::Python::PythonCallback cb = MakeActionCallback(entry.name, registry, queue);
				Dia::Python::AddFunction(sub, safeFnName, cb, entry.description);

				// Track per-category count
				for (unsigned int c = 0; c < categories.Size(); ++c)
				{
					if (categories[c].module == sub)
					{
						categories[c].count++;
						break;
					}
				}

				++registered;
			}

			// Log one line per category
			for (unsigned int c = 0; c < categories.Size(); ++c)
			{
				DIA_LOG_INFO("Editor", "EditorActionPythonModule: dia_editor.%s — %u action(s)",
					categories[c].name, categories[c].count);
			}

			DIA_LOG_INFO("Editor", "EditorActionPythonModule: generated dia_editor with %u action(s) across %u category sub-module(s)",
				registered, categories.Size());
		}

		bool EmitPythonStubs(const EditorActionRegistry* registry, const char* outputPath)
		{
			if (registry == nullptr || outputPath == nullptr || outputPath[0] == '\0')
			{
				DIA_LOG_ERROR("Editor", "EditorActionPythonModule::EmitPythonStubs: null registry or path");
				return false;
			}

			FILE* f = nullptr;
			if (fopen_s(&f, outputPath, "w") != 0 || f == nullptr)
			{
				DIA_LOG_ERROR("Editor", "EditorActionPythonModule::EmitPythonStubs: failed to open '%s' for writing", outputPath);
				return false;
			}

			fprintf(f, "# dia_editor.pyi — auto-generated by EditorActionPythonModule::EmitPythonStubs\n");
			fprintf(f, "# Do not edit manually. Regenerated each time CluicheEditor starts.\n");
			fprintf(f, "#\n");
			fprintf(f, "# Each action function accepts keyword arguments matching its parameter schema.\n");
			fprintf(f, "# All functions return a JSON string — use json.loads() to get a dict.\n");
			fprintf(f, "# Example:\n");
			fprintf(f, "#   import json, dia_editor.project\n");
			fprintf(f, "#   result = json.loads(dia_editor.project.get_state())\n");
			fprintf(f, "\n");
			fprintf(f, "import typing\n\n");

			const EditorActionManifest& manifest = registry->GetManifest();
			const unsigned int count = manifest.GetCount();

			// Collect unique categories first so we can emit grouped sub-module stubs.
			static const unsigned int kMaxCategories = 32;
			struct CatEntry { const char* name; };
			Dia::Core::Containers::DynamicArrayC<CatEntry, kMaxCategories> cats;

			auto hasCategory = [&](const char* cat) -> bool {
				for (unsigned int i = 0; i < cats.Size(); ++i)
					if (strcmp(cats[i].name, cat) == 0) return true;
				return false;
			};

			for (unsigned int i = 0; i < count; ++i)
			{
				const EditorActionEntry& e = manifest.GetAt(i);
				const char* cat = (e.category != nullptr && e.category[0] != '\0') ? e.category : "misc";
				if (!hasCategory(cat) && !cats.IsFull())
				{
					CatEntry ce; ce.name = cat;
					cats.Add(ce);
				}
			}

			// Emit one section per category
			for (unsigned int c = 0; c < cats.Size(); ++c)
			{
				const char* cat = cats[c].name;
				fprintf(f, "# ---------------------------------------------------------------------------\n");
				fprintf(f, "# dia_editor.%s\n", cat);
				fprintf(f, "# ---------------------------------------------------------------------------\n\n");

				for (unsigned int i = 0; i < count; ++i)
				{
					const EditorActionEntry& e = manifest.GetAt(i);
					const char* eCat = (e.category != nullptr && e.category[0] != '\0') ? e.category : "misc";
					if (strcmp(eCat, cat) != 0)
						continue;

					// Derive function name (strip category prefix)
					const char* actionName = e.name.AsChar();
					const char* dot = strchr(actionName, '.');
					const char* fnName = (dot != nullptr) ? dot + 1 : actionName;

					// Build param list for signature
					char paramSig[512] = "";
					char paramDoc[1024] = "";
					for (unsigned int p = 0; p < e.params.params.Size(); ++p)
					{
						const EditorActionParam& param = e.params.params[p];
						if (param.name == nullptr) continue;

						// Map type string to Python type annotation
						const char* pyType = "str";
						if (param.type != nullptr)
						{
							if (strcmp(param.type, "int") == 0)       pyType = "int";
							else if (strcmp(param.type, "float") == 0) pyType = "float";
							else if (strcmp(param.type, "bool") == 0)  pyType = "bool";
						}

						char paramEntry[128];
						if (param.required)
							snprintf(paramEntry, sizeof(paramEntry), "%s: %s", param.name, pyType);
						else
							snprintf(paramEntry, sizeof(paramEntry), "%s: typing.Optional[%s] = None", param.name, pyType);

						if (paramSig[0] != '\0')
							strncat_s(paramSig, sizeof(paramSig), ", ", 2);
						strncat_s(paramSig, sizeof(paramSig), paramEntry, sizeof(paramEntry) - 1);

						char docEntry[256];
						snprintf(docEntry, sizeof(docEntry), "\n        %s (%s%s): %s",
							param.name, pyType,
							param.required ? ", required" : ", optional",
							param.description != nullptr ? param.description : "");
						strncat_s(paramDoc, sizeof(paramDoc), docEntry, sizeof(docEntry) - 1);
					}

					fprintf(f, "def %s(%s) -> str:\n", fnName, paramSig);
					fprintf(f, "    \"\"\"%s\n\n",
						(e.description != nullptr) ? e.description : "");
					if (paramDoc[0] != '\0')
						fprintf(f, "    Args:%s\n\n", paramDoc);
					fprintf(f, "    Returns:\n");
					fprintf(f, "        str: JSON-encoded result dict. Use json.loads() to parse.\n");
					fprintf(f, "             On success: {\"success\": true, ...}\n");
					fprintf(f, "             On failure: {\"success\": false, \"reason\": str}\n");
					fprintf(f, "    \"\"\"\n    ...\n\n");
				}
			}

			fclose(f);
			DIA_LOG_INFO("Editor", "EditorActionPythonModule::EmitPythonStubs: wrote '%s' (%u actions)", outputPath, count);
			return true;
		}

	} // namespace Editor
} // namespace Dia
