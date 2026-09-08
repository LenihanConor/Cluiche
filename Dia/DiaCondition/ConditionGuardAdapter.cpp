#include <DiaCondition/ConditionGuardAdapter.h>
#include <DiaCore/Core/Assert.h>

namespace
{
	//-------------------------------------------------------------------------------------------
	// CaptureData
	//
	// Holds the two pointers needed to evaluate a condition guard.  Both pointers are non-owning;
	// the caller is responsible for ensuring expr and ctx outlive all guard evaluations.
	//-------------------------------------------------------------------------------------------
	struct CaptureData
	{
		const Dia::Condition::ConditionExpr* expr;
		Dia::Condition::IConditionContext*   ctx;
	};

	// Maximum number of simultaneous condition guards that can be registered across all
	// CallbackRegistries in a single process lifetime.  Increase if the limit is hit.
	static const int kMaxGuardSlots = 16;

	static CaptureData gCaptures[kMaxGuardSlots] = {};
	static int         gCaptureCount = 0;

	//-------------------------------------------------------------------------------------------
	// Thunk<N>
	//
	// One static thunk function per slot.  Each thunk reads from gCaptures[N] and evaluates
	// the condition expression.  The FSM passes its own state pointer as the void* argument;
	// we deliberately ignore it — the capture already holds everything we need.
	//
	// Using template<int N> produces 16 distinct function addresses, satisfying the plain
	// bool(*)(const void*) requirement without closures or heap allocation per thunk.
	//-------------------------------------------------------------------------------------------
	template<int N>
	static bool Thunk(const void* /*fsmStateData*/)
	{
		return gCaptures[N].expr->Evaluate(*gCaptures[N].ctx);
	}

	// Table of all thunk function pointers, indexed by slot.
	using ThunkFn = bool(*)(const void*);
	static const ThunkFn gThunks[kMaxGuardSlots] = {
		Thunk<0>,  Thunk<1>,  Thunk<2>,  Thunk<3>,
		Thunk<4>,  Thunk<5>,  Thunk<6>,  Thunk<7>,
		Thunk<8>,  Thunk<9>,  Thunk<10>, Thunk<11>,
		Thunk<12>, Thunk<13>, Thunk<14>, Thunk<15>
	};

} // anonymous namespace

namespace Dia
{
	namespace Condition
	{
		void RegisterAsGuard(
			Dia::Core::StringCRC guardName,
			const ConditionExpr& expr,
			IConditionContext& ctx,
			Dia::StateMachine::CallbackRegistry& registry)
		{
			DIA_ASSERT(gCaptureCount < kMaxGuardSlots,
				"ConditionGuardAdapter: slot table exhausted (max %d). "
				"Increase kMaxGuardSlots in ConditionGuardAdapter.cpp.",
				kMaxGuardSlots);

			const int slot = gCaptureCount++;
			gCaptures[slot].expr = &expr;
			gCaptures[slot].ctx  = &ctx;

			registry.RegisterGuard(guardName, gThunks[slot]);
		}

	} // namespace Condition
} // namespace Dia
