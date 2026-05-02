#pragma once

#include "DiaStateMachine/IStateMachineSerializer.h"

namespace Dia
{
	namespace StateMachine
	{
		class JsonStateMachineSerializer : public IStateMachineSerializer
		{
		public:
			static const char* kSchemaVersion;

			bool Save(const StateMachineDefinition& def, char* outBuffer, unsigned int bufferSize) const override;
			bool Load(StateMachineDefinition& outDef, const CallbackRegistry& registry, const char* data) const override;

			bool Save(const HierarchicalStateMachineDefinition& def, char* outBuffer, unsigned int bufferSize) const override;
			bool Load(HierarchicalStateMachineDefinition& outDef, const CallbackRegistry& registry, const char* data) const override;

			bool Save(const PushdownAutomatonDefinition& def, char* outBuffer, unsigned int bufferSize) const override;
			bool Load(PushdownAutomatonDefinition& outDef, const CallbackRegistry& registry, const char* data) const override;
		};
	}
}
