#include "DiaStateMachine/CallbackRegistry.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace StateMachine
	{
		void CallbackRegistry::RegisterAction(Dia::Core::StringCRC name, ActionFn fn)
		{
			DIA_ASSERT(fn != nullptr, "Cannot register null action");
			mActions.Add(name, fn);
		}

		void CallbackRegistry::RegisterGuard(Dia::Core::StringCRC name, GuardFn fn)
		{
			DIA_ASSERT(fn != nullptr, "Cannot register null guard");
			mGuards.Add(name, fn);
		}

		void CallbackRegistry::RegisterUpdate(Dia::Core::StringCRC name, UpdateFn fn)
		{
			DIA_ASSERT(fn != nullptr, "Cannot register null update");
			mUpdates.Add(name, fn);
		}

		CallbackRegistry::ActionFn CallbackRegistry::FindAction(Dia::Core::StringCRC name) const
		{
			const ActionFn* result = mActions.TryGetItemConst(name);
			return result ? *result : nullptr;
		}

		CallbackRegistry::GuardFn CallbackRegistry::FindGuard(Dia::Core::StringCRC name) const
		{
			const GuardFn* result = mGuards.TryGetItemConst(name);
			return result ? *result : nullptr;
		}

		CallbackRegistry::UpdateFn CallbackRegistry::FindUpdate(Dia::Core::StringCRC name) const
		{
			const UpdateFn* result = mUpdates.TryGetItemConst(name);
			return result ? *result : nullptr;
		}

		bool CallbackRegistry::HasAction(Dia::Core::StringCRC name) const
		{
			return mActions.ContainsKey(name);
		}

		bool CallbackRegistry::HasGuard(Dia::Core::StringCRC name) const
		{
			return mGuards.ContainsKey(name);
		}

		bool CallbackRegistry::HasUpdate(Dia::Core::StringCRC name) const
		{
			return mUpdates.ContainsKey(name);
		}
	}
}
