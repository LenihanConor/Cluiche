#include <DiaCondition/ConditionExpr.h>

#include <vector>
#include <cstring>

namespace Dia
{
	namespace Condition
	{
		//-------------------------------------------------------------------------------------------
		// Internal node representation (file-scope, not nested in ConditionExpr)
		//-------------------------------------------------------------------------------------------
		struct Node
		{
			ConditionOp             op          = ConditionOp::kEq;

			// Leaf fields
			Dia::Core::StringCRC    slot;
			Dia::Core::StringCRC    field;
			bool                    isBoolValue = false;
			float                   floatValue  = 0.0f;
			bool                    boolValue   = false;

			// Composite children (AND/OR have N children; NOT has exactly 1)
			std::vector<Node>       children;
		};

		//-------------------------------------------------------------------------------------------
		// Impl — owns the root node
		//-------------------------------------------------------------------------------------------
		struct ConditionExpr::Impl
		{
			Node root;
		};

		//-------------------------------------------------------------------------------------------
		// Internal helpers
		//-------------------------------------------------------------------------------------------
		static bool ParseOp(const char* opStr, ConditionOp& outOp)
		{
			if (std::strcmp(opStr, "and") == 0) { outOp = ConditionOp::kAnd; return true; }
			if (std::strcmp(opStr, "or")  == 0) { outOp = ConditionOp::kOr;  return true; }
			if (std::strcmp(opStr, "not") == 0) { outOp = ConditionOp::kNot; return true; }
			if (std::strcmp(opStr, "==")  == 0) { outOp = ConditionOp::kEq;  return true; }
			if (std::strcmp(opStr, "!=")  == 0) { outOp = ConditionOp::kNeq; return true; }
			if (std::strcmp(opStr, "<")   == 0) { outOp = ConditionOp::kLt;  return true; }
			if (std::strcmp(opStr, "<=")  == 0) { outOp = ConditionOp::kLte; return true; }
			if (std::strcmp(opStr, ">")   == 0) { outOp = ConditionOp::kGt;  return true; }
			if (std::strcmp(opStr, ">=")  == 0) { outOp = ConditionOp::kGte; return true; }
			return false;
		}

		// Forward declaration for recursion
		static bool ParseNode(
			const Json::Value& json,
			Node& outNode,
			Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors);

		static bool ParseNode(
			const Json::Value& json,
			Node& outNode,
			Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
		{
			if (!json.isObject())
			{
				outErrors.Add("ConditionExpr: expected JSON object node");
				return false;
			}

			// Determine op
			if (!json.isMember("op") || !json["op"].isString())
			{
				outErrors.Add("ConditionExpr: node missing 'op' field");
				return false;
			}

			const char* opStr = json["op"].asCString();
			ConditionOp op    = ConditionOp::kEq;
			if (!ParseOp(opStr, op))
			{
				outErrors.Add("ConditionExpr: unknown op string");
				return false;
			}

			outNode.op = op;

			// --- Interior node: AND / OR ---
			if (op == ConditionOp::kAnd || op == ConditionOp::kOr)
			{
				if (!json.isMember("conditions") || !json["conditions"].isArray())
				{
					outErrors.Add("ConditionExpr: 'and'/'or' node requires 'conditions' array");
					return false;
				}

				const Json::Value& arr = json["conditions"];
				for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
				{
					Node child;
					if (!ParseNode(arr[i], child, outErrors))
					{
						return false;
					}
					outNode.children.push_back(std::move(child));
				}

				if (outNode.children.empty())
				{
					outErrors.Add("ConditionExpr: 'and'/'or' node must have at least one child");
					return false;
				}

				return true;
			}

			// --- Interior node: NOT ---
			if (op == ConditionOp::kNot)
			{
				if (!json.isMember("condition") || !json["condition"].isObject())
				{
					outErrors.Add("ConditionExpr: 'not' node requires singular 'condition' object");
					return false;
				}

				Node child;
				if (!ParseNode(json["condition"], child, outErrors))
				{
					return false;
				}
				outNode.children.push_back(std::move(child));
				return true;
			}

			// --- Leaf node: comparison ---
			if (!json.isMember("slot") || !json["slot"].isString())
			{
				outErrors.Add("ConditionExpr: leaf node missing 'slot' field");
				return false;
			}
			if (!json.isMember("field") || !json["field"].isString())
			{
				outErrors.Add("ConditionExpr: leaf node missing 'field' field");
				return false;
			}
			if (!json.isMember("value"))
			{
				outErrors.Add("ConditionExpr: leaf node missing 'value' field");
				return false;
			}

			outNode.slot  = Dia::Core::StringCRC(json["slot"].asCString());
			outNode.field = Dia::Core::StringCRC(json["field"].asCString());

			const Json::Value& val = json["value"];
			if (val.isBool())
			{
				outNode.isBoolValue = true;
				outNode.boolValue   = val.asBool();
			}
			else if (val.isDouble() || val.isInt() || val.isUInt())
			{
				outNode.isBoolValue = false;
				outNode.floatValue  = static_cast<float>(val.asDouble());
			}
			else
			{
				outErrors.Add("ConditionExpr: leaf 'value' must be bool or numeric (float)");
				return false;
			}

			return true;
		}

		//-------------------------------------------------------------------------------------------
		// Evaluate helpers
		//-------------------------------------------------------------------------------------------
		static bool EvaluateNode(const Node& node, IConditionContext& ctx)
		{
			switch (node.op)
			{
			case ConditionOp::kAnd:
			{
				for (const Node& child : node.children)
				{
					if (!EvaluateNode(child, ctx))
					{
						return false;
					}
				}
				return true;
			}
			case ConditionOp::kOr:
			{
				for (const Node& child : node.children)
				{
					if (EvaluateNode(child, ctx))
					{
						return true;
					}
				}
				return false;
			}
			case ConditionOp::kNot:
			{
				return !EvaluateNode(node.children[0], ctx);
			}
			default:
				break;
			}

			// Leaf comparison
			if (node.isBoolValue)
			{
				const bool lhs = ctx.GetBool(node.slot, node.field);
				const bool rhs = node.boolValue;
				switch (node.op)
				{
				case ConditionOp::kEq:  return lhs == rhs;
				case ConditionOp::kNeq: return lhs != rhs;
				case ConditionOp::kLt:  return (!lhs && rhs);   // false < true
				case ConditionOp::kLte: return (lhs == rhs) || (!lhs && rhs);
				case ConditionOp::kGt:  return (lhs && !rhs);   // true > false
				case ConditionOp::kGte: return (lhs == rhs) || (lhs && !rhs);
				default:                return false;
				}
			}
			else
			{
				const float lhs = ctx.GetFloat(node.slot, node.field);
				const float rhs = node.floatValue;
				switch (node.op)
				{
				case ConditionOp::kEq:  return lhs == rhs;
				case ConditionOp::kNeq: return lhs != rhs;
				case ConditionOp::kLt:  return lhs <  rhs;
				case ConditionOp::kLte: return lhs <= rhs;
				case ConditionOp::kGt:  return lhs >  rhs;
				case ConditionOp::kGte: return lhs >= rhs;
				default:                return false;
				}
			}
		}

		//-------------------------------------------------------------------------------------------
		// Validate helper
		//-------------------------------------------------------------------------------------------
		static bool ValidateNode(
			const Node& node,
			const ConditionRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
		{
			// Interior nodes — recurse
			if (node.op == ConditionOp::kAnd ||
			    node.op == ConditionOp::kOr  ||
			    node.op == ConditionOp::kNot)
			{
				bool ok = true;
				for (const Node& child : node.children)
				{
					if (!ValidateNode(child, registry, outErrors))
					{
						ok = false;
					}
				}
				return ok;
			}

			// Leaf — check registry
			if (!registry.HasFloat(node.slot, node.field) &&
			    !registry.HasBool(node.slot, node.field))
			{
				outErrors.Add("ConditionExpr: leaf slot/field not found in registry");
				return false;
			}

			return true;
		}

		//-------------------------------------------------------------------------------------------
		// ConditionExpr — public implementation
		//-------------------------------------------------------------------------------------------
		ConditionExpr::ConditionExpr()
			: mImpl(nullptr)
			, mValid(false)
		{
		}

		ConditionExpr::~ConditionExpr()
		{
			delete mImpl;
			mImpl = nullptr;
		}

		ConditionExpr::ConditionExpr(ConditionExpr&& other) noexcept
			: mImpl(other.mImpl)
			, mValid(other.mValid)
		{
			other.mImpl  = nullptr;
			other.mValid = false;
		}

		ConditionExpr& ConditionExpr::operator=(ConditionExpr&& other) noexcept
		{
			if (this != &other)
			{
				delete mImpl;
				mImpl        = other.mImpl;
				mValid       = other.mValid;
				other.mImpl  = nullptr;
				other.mValid = false;
			}
			return *this;
		}

		bool ConditionExpr::IsValid() const
		{
			return mValid && mImpl != nullptr;
		}

		bool ConditionExpr::Evaluate(IConditionContext& ctx) const
		{
			if (!IsValid())
			{
				return false;
			}
			return EvaluateNode(mImpl->root, ctx);
		}

		/*static*/
		ConditionExpr ConditionExpr::LoadFromJson(
			const Json::Value& node,
			Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
		{
			ConditionExpr expr;
			expr.mImpl = new Impl();

			if (!ParseNode(node, expr.mImpl->root, outErrors))
			{
				delete expr.mImpl;
				expr.mImpl  = nullptr;
				expr.mValid = false;
				return expr;
			}

			expr.mValid = true;
			return expr;
		}

		bool ConditionExpr::Validate(
			const ConditionRegistry& registry,
			Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const
		{
			if (!IsValid())
			{
				outErrors.Add("ConditionExpr: cannot validate an invalid expression");
				return false;
			}
			return ValidateNode(mImpl->root, registry, outErrors);
		}

	} // namespace Condition
} // namespace Dia
