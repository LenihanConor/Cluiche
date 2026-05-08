#pragma once

#include <DiaSerializer/SerializeResult.h>
#include <DiaSerializer/ISerializer.h>

namespace Dia
{
	namespace StateMachine
	{
		class StateMachineDefinition;
		class HierarchicalStateMachineDefinition;
		class PushdownAutomatonDefinition;
		class CallbackRegistry;

		class IStateMachineSerializer : public Dia::Serializer::ISerializer
		{
		public:
			virtual ~IStateMachineSerializer() {}

			virtual Dia::Serializer::SerializeResult Save(const StateMachineDefinition& def, char* outBuffer, unsigned int bufferSize) const = 0;
			virtual Dia::Serializer::SerializeResult Load(StateMachineDefinition& outDef, const CallbackRegistry& registry, const char* data) const = 0;

			virtual Dia::Serializer::SerializeResult Save(const HierarchicalStateMachineDefinition& def, char* outBuffer, unsigned int bufferSize) const = 0;
			virtual Dia::Serializer::SerializeResult Load(HierarchicalStateMachineDefinition& outDef, const CallbackRegistry& registry, const char* data) const = 0;

			virtual Dia::Serializer::SerializeResult Save(const PushdownAutomatonDefinition& def, char* outBuffer, unsigned int bufferSize) const = 0;
			virtual Dia::Serializer::SerializeResult Load(PushdownAutomatonDefinition& outDef, const CallbackRegistry& registry, const char* data) const = 0;

			Dia::Serializer::SerializeResult LoadFromFile(const char* path, StateMachineDefinition& outDef, const CallbackRegistry& registry) const;
			Dia::Serializer::SerializeResult LoadFromFile(const char* path, HierarchicalStateMachineDefinition& outDef, const CallbackRegistry& registry) const;
			Dia::Serializer::SerializeResult LoadFromFile(const char* path, PushdownAutomatonDefinition& outDef, const CallbackRegistry& registry) const;

			Dia::Serializer::SerializeResult SaveToFile(const char* path, const StateMachineDefinition& def) const;
			Dia::Serializer::SerializeResult SaveToFile(const char* path, const HierarchicalStateMachineDefinition& def) const;
			Dia::Serializer::SerializeResult SaveToFile(const char* path, const PushdownAutomatonDefinition& def) const;
		};
	}
}
