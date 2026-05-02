#pragma once

namespace Dia
{
	namespace StateMachine
	{
		class StateMachineDefinition;
		class HierarchicalStateMachineDefinition;
		class PushdownAutomatonDefinition;
		class CallbackRegistry;

		class IStateMachineSerializer
		{
		public:
			virtual ~IStateMachineSerializer() {}

			virtual bool Save(const StateMachineDefinition& def, char* outBuffer, unsigned int bufferSize) const = 0;
			virtual bool Load(StateMachineDefinition& outDef, const CallbackRegistry& registry, const char* data) const = 0;

			virtual bool Save(const HierarchicalStateMachineDefinition& def, char* outBuffer, unsigned int bufferSize) const = 0;
			virtual bool Load(HierarchicalStateMachineDefinition& outDef, const CallbackRegistry& registry, const char* data) const = 0;

			virtual bool Save(const PushdownAutomatonDefinition& def, char* outBuffer, unsigned int bufferSize) const = 0;
			virtual bool Load(PushdownAutomatonDefinition& outDef, const CallbackRegistry& registry, const char* data) const = 0;
		};
	}
}
