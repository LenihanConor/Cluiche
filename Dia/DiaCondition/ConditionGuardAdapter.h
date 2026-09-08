#pragma once

#include <DiaCondition/IConditionContext.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStateMachine/CallbackRegistry.h>

namespace Dia
{
	namespace Condition
	{
		//-------------------------------------------------------------------------------------------
		// ConditionGuardAdapter
		//
		// Wires a ConditionExpr + IConditionContext into a DiaStateMachine CallbackRegistry guard
		// slot.  The registered guard calls expr.Evaluate(ctx) when the FSM queries the guard.
		//
		// SD-007: Free function — guards are stateless once registered.
		// SD-008: DiaCondition depends on DiaStateMachine (not the reverse).
		// PD-001: guardName is StringCRC.
		// AD-003: Namespace Dia::Condition::
		//
		// Lifetime contract: expr and ctx must outlive all guard evaluations (i.e. the lifetime
		// of the CallbackRegistry).
		//-------------------------------------------------------------------------------------------
		void RegisterAsGuard(
			Dia::Core::StringCRC guardName,
			const ConditionExpr& expr,
			IConditionContext& ctx,
			Dia::StateMachine::CallbackRegistry& registry);

	} // namespace Condition
} // namespace Dia
