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

			const char* GetVersion() const override;

			Dia::Serializer::SerializeResult Save(const StateMachineDefinition& def, char* outBuffer, unsigned int bufferSize) const override;
			Dia::Serializer::SerializeResult Load(StateMachineDefinition& outDef, const CallbackRegistry& registry, const char* data) const override;

			Dia::Serializer::SerializeResult Save(const HierarchicalStateMachineDefinition& def, char* outBuffer, unsigned int bufferSize) const override;
			Dia::Serializer::SerializeResult Load(HierarchicalStateMachineDefinition& outDef, const CallbackRegistry& registry, const char* data) const override;

			Dia::Serializer::SerializeResult Save(const PushdownAutomatonDefinition& def, char* outBuffer, unsigned int bufferSize) const override;
			Dia::Serializer::SerializeResult Load(PushdownAutomatonDefinition& outDef, const CallbackRegistry& registry, const char* data) const override;
		};
	}
}
