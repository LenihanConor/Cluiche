#pragma once
#include <DiaCondition/IConditionContext.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/Core/Assert.h>
#include <unordered_map>
#include <cstdint>
#include <gtest/gtest.h>

namespace Dia { namespace Condition { namespace Testing {

    // Helper: combine slot+field CRCs into a 64-bit key (same as ConditionRegistry)
    inline uint64_t MakeKey(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) {
        return (static_cast<uint64_t>(slot.Value()) << 32) | static_cast<uint32_t>(field.Value());
    }

    class MockConditionContext : public IConditionContext {
    public:
        void SetFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field, float value) {
            mFloats[MakeKey(slot, field)] = value;
        }
        void SetBool(Dia::Core::StringCRC slot, Dia::Core::StringCRC field, bool value) {
            mBools[MakeKey(slot, field)] = value;
        }
        float GetFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const override {
            auto it = mFloats.find(MakeKey(slot, field));
            DIA_ASSERT(it != mFloats.end(), "MockConditionContext: GetFloat called with unregistered slot/field");
            if (it == mFloats.end()) return 0.0f;
            return it->second;
        }
        bool GetBool(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const override {
            auto it = mBools.find(MakeKey(slot, field));
            DIA_ASSERT(it != mBools.end(), "MockConditionContext: GetBool called with unregistered slot/field");
            if (it == mBools.end()) return false;
            return it->second;
        }
    private:
        std::unordered_map<uint64_t, float> mFloats;
        std::unordered_map<uint64_t, bool>  mBools;
    };

    inline void AssertExprResult(const ConditionExpr& expr, IConditionContext& ctx, bool expected) {
        EXPECT_EQ(expr.Evaluate(ctx), expected);
    }

}}} // namespace Dia::Condition::Testing
