#include "JsonApplicationManifestSerializer.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>
#include <cstring>

namespace Dia
{
	namespace Application
	{
		static const char* kSchemaVersion = "1.0";

		const char* JsonApplicationManifestSerializer::GetVersion() const
		{
			return kSchemaVersion;
		}

		// ---------------------------------------------------------------------------
		// Load
		// ---------------------------------------------------------------------------

		static bool ParsePhase(const Json::Value& phaseJson, ApplicationManifest::PhaseEntry& out)
		{
			if (!phaseJson.isMember("type_id") || !phaseJson["type_id"].isString()) return false;
			if (!phaseJson.isMember("instance_id") || !phaseJson["instance_id"].isString()) return false;
			out.typeId     = Dia::Core::StringCRC(phaseJson["type_id"].asCString());
			out.instanceId = Dia::Core::StringCRC(phaseJson["instance_id"].asCString());
			if (phaseJson.isMember("config"))
				out.config = new Json::Value(phaseJson["config"]);
			return true;
		}

		static bool ParseModule(const Json::Value& modJson, ApplicationManifest::ModuleEntry& out)
		{
			if (!modJson.isMember("type_id") || !modJson["type_id"].isString()) return false;
			if (!modJson.isMember("instance_id") || !modJson["instance_id"].isString()) return false;
			out.typeId     = Dia::Core::StringCRC(modJson["type_id"].asCString());
			out.instanceId = Dia::Core::StringCRC(modJson["instance_id"].asCString());

			if (modJson.isMember("phase_ids") && modJson["phase_ids"].isArray())
			{
				const Json::Value& ids = modJson["phase_ids"];
				for (unsigned int i = 0; i < ids.size(); ++i)
					if (ids[i].isString())
						out.phaseIds.Add(Dia::Core::StringCRC(ids[i].asCString()));
			}

			if (modJson.isMember("dependencies") && modJson["dependencies"].isArray())
			{
				const Json::Value& deps = modJson["dependencies"];
				for (unsigned int i = 0; i < deps.size(); ++i)
					if (deps[i].isString())
						out.dependencies.Add(Dia::Core::StringCRC(deps[i].asCString()));
			}

			if (modJson.isMember("config"))
				out.config = new Json::Value(modJson["config"]);

			return true;
		}

		static bool ParseTransition(const Json::Value& tJson, ApplicationManifest::PhaseTransition& out)
		{
			if (!tJson.isMember("from") || !tJson["from"].isString()) return false;
			if (!tJson.isMember("to")   || !tJson["to"].isString())   return false;
			out.fromPhase = Dia::Core::StringCRC(tJson["from"].asCString());
			out.toPhase   = Dia::Core::StringCRC(tJson["to"].asCString());
			return true;
		}

		static bool ParseProcessingUnit(const Json::Value& puJson, ApplicationManifest::ProcessingUnitEntry& out)
		{
			if (!puJson.isMember("type_id") || !puJson["type_id"].isString()) return false;
			if (!puJson.isMember("instance_id") || !puJson["instance_id"].isString()) return false;
			out.typeId     = Dia::Core::StringCRC(puJson["type_id"].asCString());
			out.instanceId = Dia::Core::StringCRC(puJson["instance_id"].asCString());
			out.frequencyHz    = puJson.get("frequency_hz",    -1.0f).asFloat();
			out.dedicatedThread = puJson.get("dedicated_thread", false).asBool();
			out.root            = puJson.get("root", false).asBool();

			if (puJson.isMember("config"))
				out.config = new Json::Value(puJson["config"]);

			if (!puJson.isMember("initial_phase") || !puJson["initial_phase"].isString()) return false;
			out.initialPhase = Dia::Core::StringCRC(puJson["initial_phase"].asCString());

			if (puJson.isMember("phases") && puJson["phases"].isArray())
			{
				const Json::Value& phases = puJson["phases"];
				for (unsigned int i = 0; i < phases.size(); ++i)
				{
					ApplicationManifest::PhaseEntry entry;
					if (!ParsePhase(phases[i], entry))
						return false;
					out.phases.Add(entry);
				}
			}

			if (puJson.isMember("modules") && puJson["modules"].isArray())
			{
				const Json::Value& modules = puJson["modules"];
				for (unsigned int i = 0; i < modules.size(); ++i)
				{
					ApplicationManifest::ModuleEntry entry;
					if (!ParseModule(modules[i], entry))
						return false;
					out.modules.Add(entry);
				}
			}

			if (puJson.isMember("transitions") && puJson["transitions"].isArray())
			{
				const Json::Value& transitions = puJson["transitions"];
				for (unsigned int i = 0; i < transitions.size(); ++i)
				{
					ApplicationManifest::PhaseTransition t;
					if (!ParseTransition(transitions[i], t))
						return false;
					out.transitions.Add(t);
				}
			}

			return true;
		}

		Dia::Serializer::SerializeResult JsonApplicationManifestSerializer::Load(const char* data, ApplicationManifest& outManifest) const
		{
			Json::Value root;
			Json::Reader reader;

			if (!reader.parse(data, root))
			{
				DIA_LOG_WARNING("Application", "JsonApplicationManifestSerializer: JSON parse error");
				return Dia::Serializer::SerializeResult::Failure("json parse error");
			}

			if (!root.isMember("version"))
				return Dia::Serializer::SerializeResult::Failure("missing version");

			outManifest.version = root["version"].asUInt();

			if (!root.isMember("processing_units") || !root["processing_units"].isArray())
				return Dia::Serializer::SerializeResult::Failure("missing processing_units array");

			const Json::Value& pus = root["processing_units"];
			for (unsigned int i = 0; i < pus.size(); ++i)
			{
				// AddDefault then populate in-place to avoid MemoryCopy sharing phase/module
				// config pointers between temporary and the target slot (double-free on temp dtor).
				outManifest.processingUnits.AddDefault();
				ApplicationManifest::ProcessingUnitEntry& entry =
					outManifest.processingUnits[outManifest.processingUnits.Size() - 1];
				if (!ParseProcessingUnit(pus[i], entry))
				{
					DIA_LOG_WARNING("Application", "JsonApplicationManifestSerializer: error parsing processing_unit[%u]", i);
					return Dia::Serializer::SerializeResult::Failure("malformed processing_unit entry");
				}
			}

			if (root.isMember("imports") && root["imports"].isArray())
			{
				const Json::Value& importsJson = root["imports"];
				for (unsigned int i = 0; i < importsJson.size() && !outManifest.imports.IsFull(); ++i)
				{
					if (importsJson[i].isObject() && importsJson[i].isMember("path"))
					{
						TypedImport import;
						import.path = importsJson[i]["path"].asCString();
						const std::string typeStr = importsJson[i].get("type", "manifest").asString();
						import.type = (typeStr == "stage")
							? TypedImport::ImportType::kStage
							: TypedImport::ImportType::kManifest;
						outManifest.imports.Add(import);
					}
					else if (importsJson[i].isString())
					{
						DIA_LOG_WARNING("Application",
							"Deprecated: flat-string import \"%s\" — use { \"path\": \"...\", \"type\": \"manifest\" } format",
							importsJson[i].asCString());
						TypedImport import(importsJson[i].asCString(), TypedImport::ImportType::kManifest);
						outManifest.imports.Add(import);
					}
				}
			}

			if (root.isMember("metadata"))
			{
				outManifest.metadata = new Json::Value(root["metadata"]);
				// For stage manifests, fold top-level stage data into metadata
				// so MergeStageManifest can access it uniformly
				if (root.isMember("stage_phases"))
					(*outManifest.metadata)["stage_phases"] = root["stage_phases"];
				if (root.isMember("stage_transitions"))
					(*outManifest.metadata)["stage_transitions"] = root["stage_transitions"];
				if (root.isMember("stage_modules"))
					(*outManifest.metadata)["stage_modules"] = root["stage_modules"];
			}

			return Dia::Serializer::SerializeResult::Success();
		}

		// ---------------------------------------------------------------------------
		// Save
		// ---------------------------------------------------------------------------

		static Json::Value SerialisePhase(const ApplicationManifest::PhaseEntry& entry)
		{
			Json::Value j;
			j["type_id"]     = entry.typeId.AsChar();
			j["instance_id"] = entry.instanceId.AsChar();
			if (entry.config)
				j["config"] = *entry.config;
			return j;
		}

		static Json::Value SerialiseModule(const ApplicationManifest::ModuleEntry& entry)
		{
			Json::Value j;
			j["type_id"]     = entry.typeId.AsChar();
			j["instance_id"] = entry.instanceId.AsChar();

			Json::Value phaseIds(Json::arrayValue);
			for (unsigned int i = 0; i < entry.phaseIds.Size(); ++i)
				phaseIds.append(entry.phaseIds[i].AsChar());
			j["phase_ids"] = phaseIds;

			Json::Value deps(Json::arrayValue);
			for (unsigned int i = 0; i < entry.dependencies.Size(); ++i)
				deps.append(entry.dependencies[i].AsChar());
			j["dependencies"] = deps;

			if (entry.config)
				j["config"] = *entry.config;

			return j;
		}

		static Json::Value SerialiseProcessingUnit(const ApplicationManifest::ProcessingUnitEntry& pu)
		{
			Json::Value j;
			j["type_id"]        = pu.typeId.AsChar();
			j["instance_id"]    = pu.instanceId.AsChar();
			j["frequency_hz"]   = pu.frequencyHz;
			j["dedicated_thread"] = pu.dedicatedThread;
			if (pu.root)
				j["root"] = true;
			j["initial_phase"]  = pu.initialPhase.AsChar();

			if (pu.config)
				j["config"] = *pu.config;

			Json::Value phases(Json::arrayValue);
			for (unsigned int i = 0; i < pu.phases.Size(); ++i)
				phases.append(SerialisePhase(pu.phases[i]));
			j["phases"] = phases;

			Json::Value modules(Json::arrayValue);
			for (unsigned int i = 0; i < pu.modules.Size(); ++i)
				modules.append(SerialiseModule(pu.modules[i]));
			j["modules"] = modules;

			Json::Value transitions(Json::arrayValue);
			for (unsigned int i = 0; i < pu.transitions.Size(); ++i)
			{
				Json::Value t;
				t["from"] = pu.transitions[i].fromPhase.AsChar();
				t["to"]   = pu.transitions[i].toPhase.AsChar();
				transitions.append(t);
			}
			j["transitions"] = transitions;

			return j;
		}

		Dia::Serializer::SerializeResult JsonApplicationManifestSerializer::Save(const ApplicationManifest& manifest, char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["version"] = manifest.version;

			if (manifest.imports.Size() > 0)
			{
				Json::Value importsArray(Json::arrayValue);
				for (unsigned int i = 0; i < manifest.imports.Size(); ++i)
				{
					Json::Value entry;
					entry["path"] = manifest.imports[i].path.AsCStr();
					entry["type"] = (manifest.imports[i].type == TypedImport::ImportType::kStage) ? "stage" : "manifest";
					importsArray.append(entry);
				}
				root["imports"] = importsArray;
			}

			Json::Value pusArray(Json::arrayValue);
			for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
				pusArray.append(SerialiseProcessingUnit(manifest.processingUnits[i]));
			root["processing_units"] = pusArray;

			if (manifest.metadata)
				root["metadata"] = *manifest.metadata;

			Json::StyledWriter writer;
			std::string output = writer.write(root);

			if (output.size() + 1 > bufferSize)
			{
				DIA_LOG_WARNING("Application", "JsonApplicationManifestSerializer: output buffer too small");
				return Dia::Serializer::SerializeResult::Failure("output buffer too small");
			}

			memcpy(outBuffer, output.c_str(), output.size() + 1);
			return Dia::Serializer::SerializeResult::Success();
		}

		// ---------------------------------------------------------------------------
		// File helpers
		// ---------------------------------------------------------------------------

		Dia::Serializer::SerializeResult JsonApplicationManifestSerializer::LoadFromFile(const char* path, ApplicationManifest& outManifest) const
		{
			static const unsigned int kBufferSize = 32768;
			char* buffer = new char[kBufferSize];
			auto r = LoadFromFile(path, outManifest, buffer, kBufferSize);
			delete[] buffer;
			return r;
		}

		Dia::Serializer::SerializeResult JsonApplicationManifestSerializer::LoadFromFile(const char* path, ApplicationManifest& outManifest, char* buffer, unsigned int bufferSize) const
		{
			if (!ReadFileToBuffer(path, buffer, bufferSize))
				return Dia::Serializer::SerializeResult::Failure("file read error");
			return Load(buffer, outManifest);
		}

		Dia::Serializer::SerializeResult JsonApplicationManifestSerializer::SaveToFile(const char* path, const ApplicationManifest& manifest) const
		{
			char buffer[32768];
			auto result = Save(manifest, buffer, sizeof(buffer));
			if (!result)
				return result;
			if (!WriteBufferToFile(path, buffer, static_cast<unsigned int>(strlen(buffer))))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}
	}
}
