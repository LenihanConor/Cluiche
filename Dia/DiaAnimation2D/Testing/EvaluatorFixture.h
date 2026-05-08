#pragma once
#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaRig2D/Skeleton.h>

namespace Dia { namespace Animation2D { namespace Testing {

    // Fixture for AnimationEvaluator tests.
    // evaluator is a heap-allocated pointer owned by this struct.
    struct EvaluatorFixture {
        AnimationEvaluator* evaluator = nullptr;

        EvaluatorFixture() = default;

        EvaluatorFixture(EvaluatorFixture&& other) noexcept
            : evaluator(other.evaluator)
        {
            other.evaluator = nullptr;
        }

        ~EvaluatorFixture() {
            delete evaluator;
        }

        // Non-copyable
        EvaluatorFixture(const EvaluatorFixture&) = delete;
        EvaluatorFixture& operator=(const EvaluatorFixture&) = delete;
    };

    // Build a test evaluator fixture from an existing skeleton.
    // The returned fixture owns the AnimationEvaluator.
    inline EvaluatorFixture BuildTestEvaluator(Dia::Rig2D::Skeleton& skeleton) {
        EvaluatorFixture fixture;
        fixture.evaluator = new AnimationEvaluator(skeleton);
        return fixture;
    }

} } }
