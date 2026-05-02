#include "DiaStateMachine/JsonStateMachineSerializer.h"
#include "DiaStateMachine/StateMachineDefinition.h"
#include "DiaStateMachine/HierarchicalStateMachineDefinition.h"
#include "DiaStateMachine/PushdownAutomatonDefinition.h"
#include "DiaStateMachine/StateMachineMetadata.h"
#include "DiaStateMachine/CallbackRegistry.h"
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>
#include <cstring>
#include <string>

namespace Dia
{
	namespace StateMachine
	{
		const char* JsonStateMachineSerializer::kSchemaVersion = "1.0";

		// ---------------------------------------------------------------------------
		// Shared helpers
		// ---------------------------------------------------------------------------

		static void SaveMetadataToJson(const MetadataArray& arr, Json::Value& outJson)
		{
			if (arr.Size() == 0)
				return;

			Json::Value meta;
			for (unsigned int i = 0; i < arr.Size(); ++i)
			{
				const MetadataEntry& entry = arr[i];
				const char* key = entry.key.AsChar();
				switch (entry.value.type)
				{
				case MetadataValue::kBool:   meta[key] = entry.value.boolVal;              break;
				case MetadataValue::kInt:    meta[key] = entry.value.intVal;               break;
				case MetadataValue::kFloat:  meta[key] = entry.value.floatVal;             break;
				case MetadataValue::kString: meta[key] = entry.value.stringVal.AsChar();   break;
				}
			}
			outJson["metadata"] = meta;
		}

		static bool LoadMetadataFromJson(const Json::Value& stateJson, MetadataArray& outArr)
		{
			if (!stateJson.isMember("metadata") || !stateJson["metadata"].isObject())
				return true;

			const Json::Value& meta = stateJson["metadata"];
			auto members = meta.getMemberNames();
			for (unsigned int m = 0; m < static_cast<unsigned int>(members.size()); ++m)
			{
				const std::string& key = members[m];
				const Json::Value& val = meta[key];

				MetadataEntry entry;
				entry.key = Dia::Core::StringCRC(key.c_str());

				if (val.isBool())
					entry.value = MetadataValue::FromBool(val.asBool());
				else if (val.isInt())
					entry.value = MetadataValue::FromInt(val.asInt());
				else if (val.isDouble())
					entry.value = MetadataValue::FromFloat(val.asFloat());
				else if (val.isString())
					entry.value = MetadataValue::FromString(val.asCString());
				else
					continue;

				outArr.Add(entry);
			}
			return true;
		}

		static bool WriteOutput(const Json::Value& root, char* outBuffer, unsigned int bufferSize)
		{
			Json::StyledWriter writer;
			std::string output = writer.write(root);
			DIA_ASSERT(output.size() + 1 <= bufferSize, "JsonStateMachineSerializer: output buffer too small");
			if (output.size() + 1 > bufferSize)
				return false;
			memcpy(outBuffer, output.c_str(), output.size() + 1);
			return true;
		}

		static bool ParseRoot(const char* data, Json::Value& outRoot, const char* expectedType)
		{
			Json::Reader reader;
			if (!reader.parse(data, outRoot))
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: failed to parse JSON — check for malformed input");
				return false;
			}

			if (!outRoot.isMember("version") || !outRoot["version"].isString())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'version' field");
				return false;
			}

			if (strcmp(outRoot["version"].asCString(), JsonStateMachineSerializer::kSchemaVersion) != 0)
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: schema version mismatch (expected '%s', got '%s')",
					JsonStateMachineSerializer::kSchemaVersion, outRoot["version"].asCString());
				return false;
			}

			if (!outRoot.isMember("type") || !outRoot["type"].isString())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'type' field");
				return false;
			}

			if (strcmp(outRoot["type"].asCString(), expectedType) != 0)
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: type mismatch (expected '%s', got '%s')",
					expectedType, outRoot["type"].asCString());
				return false;
			}

			return true;
		}

		// ---------------------------------------------------------------------------
		// FlatStateMachine
		// ---------------------------------------------------------------------------

		bool JsonStateMachineSerializer::Save(const StateMachineDefinition& def,
			char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["version"] = kSchemaVersion;
			root["type"] = "FlatStateMachine";
			root["initialState"] = def.GetInitialStateId().AsChar();

			SaveMetadataToJson(def.GetMetadata(), root);

			Json::Value statesArray(Json::arrayValue);
			const auto& states = def.GetStates();
			for (unsigned int i = 0; i < states.Size(); ++i)
			{
				const StateDef& s = states[i];
				Json::Value stateJson;
				stateJson["id"] = s.id.AsChar();
				if (s.onEnterName != Dia::Core::StringCRC())
					stateJson["onEnter"] = s.onEnterName.AsChar();
				if (s.onExitName != Dia::Core::StringCRC())
					stateJson["onExit"] = s.onExitName.AsChar();
				if (s.onUpdateName != Dia::Core::StringCRC())
					stateJson["onUpdate"] = s.onUpdateName.AsChar();
				SaveMetadataToJson(s.metadata, stateJson);
				statesArray.append(stateJson);
			}
			root["states"] = statesArray;

			Json::Value transitionsArray(Json::arrayValue);
			const auto& transitions = def.GetTransitions();
			for (unsigned int i = 0; i < transitions.Size(); ++i)
			{
				const TransitionDef& t = transitions[i];
				Json::Value transJson;
				transJson["source"] = t.sourceStateId.AsChar();
				transJson["target"] = t.targetStateId.AsChar();
				transJson["trigger"] = t.triggerId.AsChar();
				if (t.guardName != Dia::Core::StringCRC())
					transJson["guard"] = t.guardName.AsChar();
				transitionsArray.append(transJson);
			}
			root["transitions"] = transitionsArray;

			return WriteOutput(root, outBuffer, bufferSize);
		}

		bool JsonStateMachineSerializer::Load(StateMachineDefinition& outDef,
			const CallbackRegistry& registry, const char* data) const
		{
			Json::Value root;
			if (!ParseRoot(data, root, "FlatStateMachine"))
				return false;

			if (!root.isMember("initialState") || !root["initialState"].isString())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'initialState'");
				return false;
			}

			if (!root.isMember("states") || !root["states"].isArray())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'states' array");
				return false;
			}

			LoadMetadataFromJson(root, outDef.GetMetadata());

			const Json::Value& statesArray = root["states"];
			for (unsigned int i = 0; i < statesArray.size(); ++i)
			{
				const Json::Value& sj = statesArray[i];
				if (!sj.isMember("id") || !sj["id"].isString())
				{
					DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: state %u missing 'id'", i);
					return false;
				}

				StateDef state;
				state.id = Dia::Core::StringCRC(sj["id"].asCString());

				if (sj.isMember("onEnter") && sj["onEnter"].isString())
				{
					state.onEnterName = Dia::Core::StringCRC(sj["onEnter"].asCString());
					state.onEnter = registry.FindAction(state.onEnterName);
					if (!state.onEnter) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onEnter '%s' not found in registry", sj["onEnter"].asCString()); return false; }
				}
				if (sj.isMember("onExit") && sj["onExit"].isString())
				{
					state.onExitName = Dia::Core::StringCRC(sj["onExit"].asCString());
					state.onExit = registry.FindAction(state.onExitName);
					if (!state.onExit) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onExit '%s' not found in registry", sj["onExit"].asCString()); return false; }
				}
				if (sj.isMember("onUpdate") && sj["onUpdate"].isString())
				{
					state.onUpdateName = Dia::Core::StringCRC(sj["onUpdate"].asCString());
					state.onUpdate = registry.FindUpdate(state.onUpdateName);
					if (!state.onUpdate) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onUpdate '%s' not found in registry", sj["onUpdate"].asCString()); return false; }
				}

				LoadMetadataFromJson(sj, state.metadata);
				outDef.GetStates().Add(state);
			}

			if (root.isMember("transitions") && root["transitions"].isArray())
			{
				const Json::Value& transArray = root["transitions"];
				for (unsigned int i = 0; i < transArray.size(); ++i)
				{
					const Json::Value& tj = transArray[i];
					if (!tj.isMember("source") || !tj.isMember("target") || !tj.isMember("trigger"))
					{
						DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: transition %u missing required fields", i);
						return false;
					}

					TransitionDef trans;
					trans.sourceStateId = Dia::Core::StringCRC(tj["source"].asCString());
					trans.targetStateId = Dia::Core::StringCRC(tj["target"].asCString());
					trans.triggerId     = Dia::Core::StringCRC(tj["trigger"].asCString());

					if (tj.isMember("guard") && tj["guard"].isString())
					{
						trans.guardName = Dia::Core::StringCRC(tj["guard"].asCString());
						trans.guard = registry.FindGuard(trans.guardName);
						if (!trans.guard) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: guard '%s' not found in registry", tj["guard"].asCString()); return false; }
					}

					outDef.GetTransitions().Add(trans);
				}
			}

			outDef.SetInitialStateId(Dia::Core::StringCRC(root["initialState"].asCString()));

			Dia::Core::Containers::DynamicArrayC<const char*, 16> errors;
			bool valid = outDef.Validate(errors);
			if (!valid) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: loaded FlatStateMachine definition failed validation"); }
			return valid;
		}

		// ---------------------------------------------------------------------------
		// HierarchicalStateMachine
		// ---------------------------------------------------------------------------

		bool JsonStateMachineSerializer::Save(const HierarchicalStateMachineDefinition& def,
			char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["version"] = kSchemaVersion;
			root["type"] = "HierarchicalStateMachine";
			root["initialState"] = def.GetInitialStateId().AsChar();

			SaveMetadataToJson(def.GetMetadata(), root);

			Json::Value statesArray(Json::arrayValue);
			const auto& states = def.GetStates();
			for (unsigned int i = 0; i < states.Size(); ++i)
			{
				const HierarchicalStateDef& s = states[i];
				Json::Value sj;
				sj["id"] = s.id.AsChar();
				if (s.parentId != Dia::Core::StringCRC())
					sj["parent"] = s.parentId.AsChar();
				if (s.initialChildId != Dia::Core::StringCRC())
					sj["initialChild"] = s.initialChildId.AsChar();
				sj["hasHistory"] = s.hasHistory;
				if (s.onEnterName != Dia::Core::StringCRC())
					sj["onEnter"] = s.onEnterName.AsChar();
				if (s.onExitName != Dia::Core::StringCRC())
					sj["onExit"] = s.onExitName.AsChar();
				if (s.onUpdateName != Dia::Core::StringCRC())
					sj["onUpdate"] = s.onUpdateName.AsChar();
				SaveMetadataToJson(s.metadata, sj);
				statesArray.append(sj);
			}
			root["states"] = statesArray;

			Json::Value transitionsArray(Json::arrayValue);
			const auto& transitions = def.GetTransitions();
			for (unsigned int i = 0; i < transitions.Size(); ++i)
			{
				const HierarchicalTransitionDef& t = transitions[i];
				Json::Value tj;
				tj["source"] = t.sourceStateId.AsChar();
				tj["target"] = t.targetStateId.AsChar();
				tj["trigger"] = t.triggerId.AsChar();
				if (t.guardName != Dia::Core::StringCRC())
					tj["guard"] = t.guardName.AsChar();
				transitionsArray.append(tj);
			}
			root["transitions"] = transitionsArray;

			return WriteOutput(root, outBuffer, bufferSize);
		}

		bool JsonStateMachineSerializer::Load(HierarchicalStateMachineDefinition& outDef,
			const CallbackRegistry& registry, const char* data) const
		{
			Json::Value root;
			if (!ParseRoot(data, root, "HierarchicalStateMachine"))
				return false;

			if (!root.isMember("initialState") || !root["initialState"].isString())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'initialState'");
				return false;
			}

			if (!root.isMember("states") || !root["states"].isArray())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'states' array");
				return false;
			}

			LoadMetadataFromJson(root, outDef.GetMetadata());

			const Json::Value& statesArray = root["states"];
			for (unsigned int i = 0; i < statesArray.size(); ++i)
			{
				const Json::Value& sj = statesArray[i];
				if (!sj.isMember("id") || !sj["id"].isString())
				{
					DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: state %u missing 'id'", i);
					return false;
				}

				HierarchicalStateDef state;
				state.id = Dia::Core::StringCRC(sj["id"].asCString());

				if (sj.isMember("parent") && sj["parent"].isString())
					state.parentId = Dia::Core::StringCRC(sj["parent"].asCString());
				if (sj.isMember("initialChild") && sj["initialChild"].isString())
					state.initialChildId = Dia::Core::StringCRC(sj["initialChild"].asCString());
				if (sj.isMember("hasHistory") && sj["hasHistory"].isBool())
					state.hasHistory = sj["hasHistory"].asBool();

				if (sj.isMember("onEnter") && sj["onEnter"].isString())
				{
					state.onEnterName = Dia::Core::StringCRC(sj["onEnter"].asCString());
					state.onEnter = registry.FindAction(state.onEnterName);
					if (!state.onEnter) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onEnter '%s' not found in registry", sj["onEnter"].asCString()); return false; }
				}
				if (sj.isMember("onExit") && sj["onExit"].isString())
				{
					state.onExitName = Dia::Core::StringCRC(sj["onExit"].asCString());
					state.onExit = registry.FindAction(state.onExitName);
					if (!state.onExit) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onExit '%s' not found in registry", sj["onExit"].asCString()); return false; }
				}
				if (sj.isMember("onUpdate") && sj["onUpdate"].isString())
				{
					state.onUpdateName = Dia::Core::StringCRC(sj["onUpdate"].asCString());
					state.onUpdate = registry.FindUpdate(state.onUpdateName);
					if (!state.onUpdate) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onUpdate '%s' not found in registry", sj["onUpdate"].asCString()); return false; }
				}

				LoadMetadataFromJson(sj, state.metadata);
				outDef.GetStates().Add(state);
			}

			if (root.isMember("transitions") && root["transitions"].isArray())
			{
				const Json::Value& transArray = root["transitions"];
				for (unsigned int i = 0; i < transArray.size(); ++i)
				{
					const Json::Value& tj = transArray[i];
					if (!tj.isMember("source") || !tj.isMember("target") || !tj.isMember("trigger"))
					{
						DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: transition %u missing required fields", i);
						return false;
					}

					HierarchicalTransitionDef trans;
					trans.sourceStateId = Dia::Core::StringCRC(tj["source"].asCString());
					trans.targetStateId = Dia::Core::StringCRC(tj["target"].asCString());
					trans.triggerId     = Dia::Core::StringCRC(tj["trigger"].asCString());

					if (tj.isMember("guard") && tj["guard"].isString())
					{
						trans.guardName = Dia::Core::StringCRC(tj["guard"].asCString());
						trans.guard = registry.FindGuard(trans.guardName);
						if (!trans.guard) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: guard '%s' not found in registry", tj["guard"].asCString()); return false; }
					}

					outDef.GetTransitions().Add(trans);
				}
			}

			outDef.SetInitialStateId(Dia::Core::StringCRC(root["initialState"].asCString()));

			Dia::Core::Containers::DynamicArrayC<const char*, 16> errors;
			bool valid = outDef.Validate(errors);
			if (!valid) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: loaded HierarchicalStateMachine definition failed validation"); }
			return valid;
		}

		// ---------------------------------------------------------------------------
		// PushdownAutomaton
		// ---------------------------------------------------------------------------

		bool JsonStateMachineSerializer::Save(const PushdownAutomatonDefinition& def,
			char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["version"] = kSchemaVersion;
			root["type"] = "PushdownAutomaton";
			root["initialState"] = def.GetInitialStateId().AsChar();

			SaveMetadataToJson(def.GetMetadata(), root);

			Json::Value statesArray(Json::arrayValue);
			const auto& states = def.GetStates();
			for (unsigned int i = 0; i < states.Size(); ++i)
			{
				const PushdownStateDef& s = states[i];
				Json::Value sj;
				sj["id"] = s.id.AsChar();
				if (s.onEnterName  != Dia::Core::StringCRC()) sj["onEnter"]  = s.onEnterName.AsChar();
				if (s.onExitName   != Dia::Core::StringCRC()) sj["onExit"]   = s.onExitName.AsChar();
				if (s.onUpdateName != Dia::Core::StringCRC()) sj["onUpdate"] = s.onUpdateName.AsChar();
				if (s.onPauseName  != Dia::Core::StringCRC()) sj["onPause"]  = s.onPauseName.AsChar();
				if (s.onResumeName != Dia::Core::StringCRC()) sj["onResume"] = s.onResumeName.AsChar();
				SaveMetadataToJson(s.metadata, sj);
				statesArray.append(sj);
			}
			root["states"] = statesArray;

			return WriteOutput(root, outBuffer, bufferSize);
		}

		bool JsonStateMachineSerializer::Load(PushdownAutomatonDefinition& outDef,
			const CallbackRegistry& registry, const char* data) const
		{
			Json::Value root;
			if (!ParseRoot(data, root, "PushdownAutomaton"))
				return false;

			if (!root.isMember("initialState") || !root["initialState"].isString())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'initialState'");
				return false;
			}

			if (!root.isMember("states") || !root["states"].isArray())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'states' array");
				return false;
			}

			LoadMetadataFromJson(root, outDef.GetMetadata());

			const Json::Value& statesArray = root["states"];
			for (unsigned int i = 0; i < statesArray.size(); ++i)
			{
				const Json::Value& sj = statesArray[i];
				if (!sj.isMember("id") || !sj["id"].isString())
				{
					DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: state %u missing 'id'", i);
					return false;
				}

				PushdownStateDef state;
				state.id = Dia::Core::StringCRC(sj["id"].asCString());

				if (sj.isMember("onEnter") && sj["onEnter"].isString())
				{
					state.onEnterName = Dia::Core::StringCRC(sj["onEnter"].asCString());
					state.onEnter = registry.FindAction(state.onEnterName);
					if (!state.onEnter) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onEnter '%s' not found in registry", sj["onEnter"].asCString()); return false; }
				}
				if (sj.isMember("onExit") && sj["onExit"].isString())
				{
					state.onExitName = Dia::Core::StringCRC(sj["onExit"].asCString());
					state.onExit = registry.FindAction(state.onExitName);
					if (!state.onExit) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onExit '%s' not found in registry", sj["onExit"].asCString()); return false; }
				}
				if (sj.isMember("onUpdate") && sj["onUpdate"].isString())
				{
					state.onUpdateName = Dia::Core::StringCRC(sj["onUpdate"].asCString());
					state.onUpdate = registry.FindUpdate(state.onUpdateName);
					if (!state.onUpdate) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onUpdate '%s' not found in registry", sj["onUpdate"].asCString()); return false; }
				}
				if (sj.isMember("onPause") && sj["onPause"].isString())
				{
					state.onPauseName = Dia::Core::StringCRC(sj["onPause"].asCString());
					state.onPause = registry.FindAction(state.onPauseName);
					if (!state.onPause) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onPause '%s' not found in registry", sj["onPause"].asCString()); return false; }
				}
				if (sj.isMember("onResume") && sj["onResume"].isString())
				{
					state.onResumeName = Dia::Core::StringCRC(sj["onResume"].asCString());
					state.onResume = registry.FindAction(state.onResumeName);
					if (!state.onResume) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onResume '%s' not found in registry", sj["onResume"].asCString()); return false; }
				}

				LoadMetadataFromJson(sj, state.metadata);
				outDef.GetStates().Add(state);
			}

			outDef.SetInitialStateId(Dia::Core::StringCRC(root["initialState"].asCString()));

			Dia::Core::Containers::DynamicArrayC<const char*, 16> errors;
			bool valid = outDef.Validate(errors);
			if (!valid) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: loaded PushdownAutomaton definition failed validation"); }
			return valid;
		}
	}
}
