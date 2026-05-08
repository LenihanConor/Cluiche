#include "DiaStateMachine/JsonStateMachineSerializer.h"
#include "DiaStateMachine/StateMachineDefinition.h"
#include "DiaStateMachine/HierarchicalStateMachineDefinition.h"
#include "DiaStateMachine/PushdownAutomatonDefinition.h"
#include "DiaStateMachine/CallbackRegistry.h"
#include <DiaSerializer/JsonMetadataHelpers.h>
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

		const char* JsonStateMachineSerializer::GetVersion() const
		{
			return kSchemaVersion;
		}

		// ---------------------------------------------------------------------------
		// Shared helpers
		// ---------------------------------------------------------------------------

		static Dia::Serializer::SerializeResult WriteOutput(const Json::Value& root, char* outBuffer, unsigned int bufferSize)
		{
			Json::StyledWriter writer;
			std::string output = writer.write(root);
			DIA_ASSERT(output.size() + 1 <= bufferSize, "JsonStateMachineSerializer: output buffer too small");
			if (output.size() + 1 > bufferSize)
				return Dia::Serializer::SerializeResult::Failure("output buffer too small");
			memcpy(outBuffer, output.c_str(), output.size() + 1);
			return Dia::Serializer::SerializeResult::Success();
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

		Dia::Serializer::SerializeResult JsonStateMachineSerializer::Save(const StateMachineDefinition& def,
			char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["version"] = kSchemaVersion;
			root["type"] = "FlatStateMachine";
			root["initialState"] = def.GetInitialStateId().AsChar();

			Dia::Serializer::WriteMetadataToJson(def.GetMetadata(), root);

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
				Dia::Serializer::WriteMetadataToJson(def.GetStateMetadata(i), stateJson);
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

		Dia::Serializer::SerializeResult JsonStateMachineSerializer::Load(StateMachineDefinition& outDef,
			const CallbackRegistry& registry, const char* data) const
		{
			DIA_ASSERT(registry.IsFinalized(), "CallbackRegistry must be Finalized() before Load()");

			Json::Value root;
			if (!ParseRoot(data, root, "FlatStateMachine"))
				return Dia::Serializer::SerializeResult::Failure("parse error");

			if (!root.isMember("initialState") || !root["initialState"].isString())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'initialState'");
				return Dia::Serializer::SerializeResult::Failure("missing initialState");
			}

			if (!root.isMember("states") || !root["states"].isArray())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'states' array");
				return Dia::Serializer::SerializeResult::Failure("missing states array");
			}

			Dia::Serializer::ReadMetadataFromJson(root, outDef.GetMetadata());

			const Json::Value& statesArray = root["states"];
			for (unsigned int i = 0; i < statesArray.size(); ++i)
			{
				const Json::Value& sj = statesArray[i];
				if (!sj.isMember("id") || !sj["id"].isString())
				{
					DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: state %u missing 'id'", i);
					return Dia::Serializer::SerializeResult::Failure("state missing id");
				}

				StateDef state;
				state.id = Dia::Core::StringCRC(sj["id"].asCString());

				if (sj.isMember("onEnter") && sj["onEnter"].isString())
				{
					state.onEnterName = Dia::Core::StringCRC(sj["onEnter"].asCString());
					state.onEnter = registry.FindAction(state.onEnterName);
					if (!state.onEnter) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onEnter '%s' not found in registry", sj["onEnter"].asCString()); return Dia::Serializer::SerializeResult::Failure("onEnter not found"); }
				}
				if (sj.isMember("onExit") && sj["onExit"].isString())
				{
					state.onExitName = Dia::Core::StringCRC(sj["onExit"].asCString());
					state.onExit = registry.FindAction(state.onExitName);
					if (!state.onExit) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onExit '%s' not found in registry", sj["onExit"].asCString()); return Dia::Serializer::SerializeResult::Failure("onExit not found"); }
				}
				if (sj.isMember("onUpdate") && sj["onUpdate"].isString())
				{
					state.onUpdateName = Dia::Core::StringCRC(sj["onUpdate"].asCString());
					state.onUpdate = registry.FindUpdate(state.onUpdateName);
					if (!state.onUpdate) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onUpdate '%s' not found in registry", sj["onUpdate"].asCString()); return Dia::Serializer::SerializeResult::Failure("onUpdate not found"); }
				}

				outDef.GetStates().Add(state);
				MetadataArray stateMeta;
				Dia::Serializer::ReadMetadataFromJson(sj, stateMeta);
				outDef.mStateMetadata.Add(stateMeta);
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
						return Dia::Serializer::SerializeResult::Failure("transition missing fields");
					}

					TransitionDef trans;
					trans.sourceStateId = Dia::Core::StringCRC(tj["source"].asCString());
					trans.targetStateId = Dia::Core::StringCRC(tj["target"].asCString());
					trans.triggerId     = Dia::Core::StringCRC(tj["trigger"].asCString());

					if (tj.isMember("guard") && tj["guard"].isString())
					{
						trans.guardName = Dia::Core::StringCRC(tj["guard"].asCString());
						trans.guard = registry.FindGuard(trans.guardName);
						if (!trans.guard) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: guard '%s' not found in registry", tj["guard"].asCString()); return Dia::Serializer::SerializeResult::Failure("guard not found"); }
					}

					outDef.GetTransitions().Add(trans);
				}
			}

			outDef.SetInitialStateId(Dia::Core::StringCRC(root["initialState"].asCString()));

			Dia::Core::Containers::DynamicArrayC<const char*, 16> errors;
			if (!outDef.Validate(errors))
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: loaded FlatStateMachine definition failed validation");
				return Dia::Serializer::SerializeResult::Failure("validation failed");
			}
			outDef.MarkValid();
			return Dia::Serializer::SerializeResult::Success();
		}

		// ---------------------------------------------------------------------------
		// HierarchicalStateMachine
		// ---------------------------------------------------------------------------

		Dia::Serializer::SerializeResult JsonStateMachineSerializer::Save(const HierarchicalStateMachineDefinition& def,
			char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["version"] = kSchemaVersion;
			root["type"] = "HierarchicalStateMachine";
			root["initialState"] = def.GetInitialStateId().AsChar();

			Dia::Serializer::WriteMetadataToJson(def.GetMetadata(), root);

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
				Dia::Serializer::WriteMetadataToJson(def.GetStateMetadata(i), sj);
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

		Dia::Serializer::SerializeResult JsonStateMachineSerializer::Load(HierarchicalStateMachineDefinition& outDef,
			const CallbackRegistry& registry, const char* data) const
		{
			DIA_ASSERT(registry.IsFinalized(), "CallbackRegistry must be Finalized() before Load()");

			Json::Value root;
			if (!ParseRoot(data, root, "HierarchicalStateMachine"))
				return Dia::Serializer::SerializeResult::Failure("parse error");

			if (!root.isMember("initialState") || !root["initialState"].isString())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'initialState'");
				return Dia::Serializer::SerializeResult::Failure("missing initialState");
			}

			if (!root.isMember("states") || !root["states"].isArray())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'states' array");
				return Dia::Serializer::SerializeResult::Failure("missing states array");
			}

			Dia::Serializer::ReadMetadataFromJson(root, outDef.GetMetadata());

			const Json::Value& statesArray = root["states"];
			for (unsigned int i = 0; i < statesArray.size(); ++i)
			{
				const Json::Value& sj = statesArray[i];
				if (!sj.isMember("id") || !sj["id"].isString())
				{
					DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: state %u missing 'id'", i);
					return Dia::Serializer::SerializeResult::Failure("state missing id");
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
					if (!state.onEnter) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onEnter '%s' not found in registry", sj["onEnter"].asCString()); return Dia::Serializer::SerializeResult::Failure("onEnter not found"); }
				}
				if (sj.isMember("onExit") && sj["onExit"].isString())
				{
					state.onExitName = Dia::Core::StringCRC(sj["onExit"].asCString());
					state.onExit = registry.FindAction(state.onExitName);
					if (!state.onExit) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onExit '%s' not found in registry", sj["onExit"].asCString()); return Dia::Serializer::SerializeResult::Failure("onExit not found"); }
				}
				if (sj.isMember("onUpdate") && sj["onUpdate"].isString())
				{
					state.onUpdateName = Dia::Core::StringCRC(sj["onUpdate"].asCString());
					state.onUpdate = registry.FindUpdate(state.onUpdateName);
					if (!state.onUpdate) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onUpdate '%s' not found in registry", sj["onUpdate"].asCString()); return Dia::Serializer::SerializeResult::Failure("onUpdate not found"); }
				}

				outDef.GetStates().Add(state);
				MetadataArray stateMeta;
				Dia::Serializer::ReadMetadataFromJson(sj, stateMeta);
				outDef.mStateMetadata.Add(stateMeta);
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
						return Dia::Serializer::SerializeResult::Failure("transition missing fields");
					}

					HierarchicalTransitionDef trans;
					trans.sourceStateId = Dia::Core::StringCRC(tj["source"].asCString());
					trans.targetStateId = Dia::Core::StringCRC(tj["target"].asCString());
					trans.triggerId     = Dia::Core::StringCRC(tj["trigger"].asCString());

					if (tj.isMember("guard") && tj["guard"].isString())
					{
						trans.guardName = Dia::Core::StringCRC(tj["guard"].asCString());
						trans.guard = registry.FindGuard(trans.guardName);
						if (!trans.guard) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: guard '%s' not found in registry", tj["guard"].asCString()); return Dia::Serializer::SerializeResult::Failure("guard not found"); }
					}

					outDef.GetTransitions().Add(trans);
				}
			}

			outDef.SetInitialStateId(Dia::Core::StringCRC(root["initialState"].asCString()));

			Dia::Core::Containers::DynamicArrayC<const char*, 16> errors;
			if (!outDef.Validate(errors))
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: loaded HierarchicalStateMachine definition failed validation");
				return Dia::Serializer::SerializeResult::Failure("validation failed");
			}
			outDef.MarkValid();
			return Dia::Serializer::SerializeResult::Success();
		}

		// ---------------------------------------------------------------------------
		// PushdownAutomaton
		// ---------------------------------------------------------------------------

		Dia::Serializer::SerializeResult JsonStateMachineSerializer::Save(const PushdownAutomatonDefinition& def,
			char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["version"] = kSchemaVersion;
			root["type"] = "PushdownAutomaton";
			root["initialState"] = def.GetInitialStateId().AsChar();

			Dia::Serializer::WriteMetadataToJson(def.GetMetadata(), root);

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
				Dia::Serializer::WriteMetadataToJson(def.GetStateMetadata(i), sj);
				statesArray.append(sj);
			}
			root["states"] = statesArray;

			return WriteOutput(root, outBuffer, bufferSize);
		}

		Dia::Serializer::SerializeResult JsonStateMachineSerializer::Load(PushdownAutomatonDefinition& outDef,
			const CallbackRegistry& registry, const char* data) const
		{
			DIA_ASSERT(registry.IsFinalized(), "CallbackRegistry must be Finalized() before Load()");

			Json::Value root;
			if (!ParseRoot(data, root, "PushdownAutomaton"))
				return Dia::Serializer::SerializeResult::Failure("parse error");

			if (!root.isMember("initialState") || !root["initialState"].isString())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'initialState'");
				return Dia::Serializer::SerializeResult::Failure("missing initialState");
			}

			if (!root.isMember("states") || !root["states"].isArray())
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: missing 'states' array");
				return Dia::Serializer::SerializeResult::Failure("missing states array");
			}

			Dia::Serializer::ReadMetadataFromJson(root, outDef.GetMetadata());

			const Json::Value& statesArray = root["states"];
			for (unsigned int i = 0; i < statesArray.size(); ++i)
			{
				const Json::Value& sj = statesArray[i];
				if (!sj.isMember("id") || !sj["id"].isString())
				{
					DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: state %u missing 'id'", i);
					return Dia::Serializer::SerializeResult::Failure("state missing id");
				}

				PushdownStateDef state;
				state.id = Dia::Core::StringCRC(sj["id"].asCString());

				if (sj.isMember("onEnter") && sj["onEnter"].isString())
				{
					state.onEnterName = Dia::Core::StringCRC(sj["onEnter"].asCString());
					state.onEnter = registry.FindAction(state.onEnterName);
					if (!state.onEnter) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onEnter '%s' not found in registry", sj["onEnter"].asCString()); return Dia::Serializer::SerializeResult::Failure("onEnter not found"); }
				}
				if (sj.isMember("onExit") && sj["onExit"].isString())
				{
					state.onExitName = Dia::Core::StringCRC(sj["onExit"].asCString());
					state.onExit = registry.FindAction(state.onExitName);
					if (!state.onExit) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onExit '%s' not found in registry", sj["onExit"].asCString()); return Dia::Serializer::SerializeResult::Failure("onExit not found"); }
				}
				if (sj.isMember("onUpdate") && sj["onUpdate"].isString())
				{
					state.onUpdateName = Dia::Core::StringCRC(sj["onUpdate"].asCString());
					state.onUpdate = registry.FindUpdate(state.onUpdateName);
					if (!state.onUpdate) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onUpdate '%s' not found in registry", sj["onUpdate"].asCString()); return Dia::Serializer::SerializeResult::Failure("onUpdate not found"); }
				}
				if (sj.isMember("onPause") && sj["onPause"].isString())
				{
					state.onPauseName = Dia::Core::StringCRC(sj["onPause"].asCString());
					state.onPause = registry.FindAction(state.onPauseName);
					if (!state.onPause) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onPause '%s' not found in registry", sj["onPause"].asCString()); return Dia::Serializer::SerializeResult::Failure("onPause not found"); }
				}
				if (sj.isMember("onResume") && sj["onResume"].isString())
				{
					state.onResumeName = Dia::Core::StringCRC(sj["onResume"].asCString());
					state.onResume = registry.FindAction(state.onResumeName);
					if (!state.onResume) { DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: onResume '%s' not found in registry", sj["onResume"].asCString()); return Dia::Serializer::SerializeResult::Failure("onResume not found"); }
				}

				outDef.GetStates().Add(state);
				MetadataArray stateMeta;
				Dia::Serializer::ReadMetadataFromJson(sj, stateMeta);
				outDef.mStateMetadata.Add(stateMeta);
			}

			outDef.SetInitialStateId(Dia::Core::StringCRC(root["initialState"].asCString()));

			Dia::Core::Containers::DynamicArrayC<const char*, 16> errors;
			if (!outDef.Validate(errors))
			{
				DIA_LOG_WARNING("StateMachine", "JsonStateMachineSerializer: loaded PushdownAutomaton definition failed validation");
				return Dia::Serializer::SerializeResult::Failure("validation failed");
			}
			outDef.MarkValid();
			return Dia::Serializer::SerializeResult::Success();
		}

		// ---------------------------------------------------------------------------
		// IStateMachineSerializer LoadFromFile / SaveToFile
		// ---------------------------------------------------------------------------

		Dia::Serializer::SerializeResult IStateMachineSerializer::LoadFromFile(
			const char* path, StateMachineDefinition& outDef, const CallbackRegistry& registry) const
		{
			char buffer[32768];
			if (!ReadFileToBuffer(path, buffer, sizeof(buffer)))
				return Dia::Serializer::SerializeResult::Failure("file read error");
			return Load(outDef, registry, buffer);
		}

		Dia::Serializer::SerializeResult IStateMachineSerializer::LoadFromFile(
			const char* path, HierarchicalStateMachineDefinition& outDef, const CallbackRegistry& registry) const
		{
			char buffer[32768];
			if (!ReadFileToBuffer(path, buffer, sizeof(buffer)))
				return Dia::Serializer::SerializeResult::Failure("file read error");
			return Load(outDef, registry, buffer);
		}

		Dia::Serializer::SerializeResult IStateMachineSerializer::LoadFromFile(
			const char* path, PushdownAutomatonDefinition& outDef, const CallbackRegistry& registry) const
		{
			char buffer[32768];
			if (!ReadFileToBuffer(path, buffer, sizeof(buffer)))
				return Dia::Serializer::SerializeResult::Failure("file read error");
			return Load(outDef, registry, buffer);
		}

		Dia::Serializer::SerializeResult IStateMachineSerializer::SaveToFile(
			const char* path, const StateMachineDefinition& def) const
		{
			char buffer[32768];
			auto result = Save(def, buffer, sizeof(buffer));
			if (!result) return result;
			if (!WriteBufferToFile(path, buffer, static_cast<unsigned int>(strlen(buffer))))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult IStateMachineSerializer::SaveToFile(
			const char* path, const HierarchicalStateMachineDefinition& def) const
		{
			char buffer[32768];
			auto result = Save(def, buffer, sizeof(buffer));
			if (!result) return result;
			if (!WriteBufferToFile(path, buffer, static_cast<unsigned int>(strlen(buffer))))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult IStateMachineSerializer::SaveToFile(
			const char* path, const PushdownAutomatonDefinition& def) const
		{
			char buffer[32768];
			auto result = Save(def, buffer, sizeof(buffer));
			if (!result) return result;
			if (!WriteBufferToFile(path, buffer, static_cast<unsigned int>(strlen(buffer))))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}
	}
}
