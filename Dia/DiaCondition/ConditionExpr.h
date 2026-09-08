#pragma once

#include <DiaCondition/IConditionContext.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Condition
	{
		//-------------------------------------------------------------------------------------------
		// ConditionOp
		//
		// Operators for both interior (AND/OR/NOT) and leaf (comparison) nodes.
		//-------------------------------------------------------------------------------------------
		enum class ConditionOp
		{
			kAnd,
			kOr,
			kNot,
			kEq,
			kNeq,
			kLt,
			kLte,
			kGt,
			kGte
		};

		//-------------------------------------------------------------------------------------------
		// ConditionExpr
		//
		// Immutable JSON-loadable boolean expression tree.  Supports AND/OR/NOT interior nodes and
		// leaf comparisons against float/bool slot+field pairs resolved via IConditionContext.
		//
		// SD-002: Float and bool value types only (no int/string/vector).
		// SD-004: Immutable after LoadFromJson — no mutable state.
		// SD-005: AND/OR/NOT composition only.
		// SD-006: Leaf ops: ==, !=, <, <=, >, >=.
		// SD-010: Caller owns JSON parsing; LoadFromJson takes Json::Value& reference.
		// PD-001: Slot/field keys are StringCRC internally.
		// PD-004: No STL in public API — DynamicArrayC for outErrors.
		// AD-003: Namespace Dia::Condition::
		//-------------------------------------------------------------------------------------------
		class ConditionExpr
		{
		public:
			ConditionExpr();
			~ConditionExpr();

			ConditionExpr(ConditionExpr&&) noexcept;
			ConditionExpr& operator=(ConditionExpr&&) noexcept;

			// Non-copyable — move only.
			ConditionExpr(const ConditionExpr&) = delete;
			ConditionExpr& operator=(const ConditionExpr&) = delete;

			// Evaluate the expression against the given context.
			// Returns false immediately if !IsValid().
			bool Evaluate(IConditionContext& ctx) const;

			// Parse a JSON node into an expression tree.
			// Returns a default-false expr and populates outErrors on failure.
			static ConditionExpr LoadFromJson(
				const Json::Value& node,
				Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors);

			// Validate all leaf slot/field pairs are resolvable in the given registry.
			// Returns false and populates outErrors if any leaf is unresolvable.
			bool Validate(
				const ConditionRegistry& registry,
				Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const;

			bool IsValid() const;

		private:
			// Impl owns the root node tree — fully defined in the .cpp.
			struct Impl;
			Impl* mImpl;
			bool  mValid;
		};

	} // namespace Condition
} // namespace Dia
