#include <gtest/gtest.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/IDecoratorNode.h>
#include <DiaBehaviourTree/DecoratorRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::BehaviourTree;
using namespace Dia::Core;

// ============================================================================
// DiaBehaviourTree_ActionRegistry
// ============================================================================

class DiaBehaviourTree_ActionRegistry : public ::testing::Test {
protected:
    ActionRegistry registry;
};

TEST_F(DiaBehaviourTree_ActionRegistry, Register_ThenFind_ReturnsRegisteredFunction) {
    // Arrange
    auto testAction = [](void* ctx, const Containers::DynamicArrayC<StringCRC, 8>& params) {
        return NodeResult::kSuccess;
    };
    StringCRC actionId("TestAction1");

    // Act
    registry.Register(actionId, testAction);
    ActionFn result = registry.Find(actionId);

    // Assert
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result, testAction);
}

TEST_F(DiaBehaviourTree_ActionRegistry, Has_AfterRegister_ReturnsTrue) {
    // Arrange
    auto testAction = [](void* ctx, const Containers::DynamicArrayC<StringCRC, 8>& params) {
        return NodeResult::kSuccess;
    };
    StringCRC actionId("TestAction2");

    // Act
    registry.Register(actionId, testAction);
    bool result = registry.Has(actionId);

    // Assert
    EXPECT_TRUE(result);
}

TEST_F(DiaBehaviourTree_ActionRegistry, Has_BeforeRegister_ReturnsFalse) {
    // Arrange
    StringCRC unknownId("UnknownAction");

    // Act
    bool result = registry.Has(unknownId);

    // Assert
    EXPECT_FALSE(result);
}

TEST_F(DiaBehaviourTree_ActionRegistry, Find_UnknownCRC_ReturnsNullptr) {
    // Arrange
    StringCRC unknownId("UnknownAction2");

    // Act
    ActionFn result = registry.Find(unknownId);

    // Assert
    EXPECT_EQ(result, nullptr);
}

TEST_F(DiaBehaviourTree_ActionRegistry, Register_Overwrite_ReplacesPreviousEntry) {
    // Arrange
    auto action1 = [](void* ctx, const Containers::DynamicArrayC<StringCRC, 8>& params) {
        return NodeResult::kSuccess;
    };
    auto action2 = [](void* ctx, const Containers::DynamicArrayC<StringCRC, 8>& params) {
        return NodeResult::kFailure;
    };
    StringCRC actionId("OverwriteTest");

    registry.Register(actionId, action1);
    EXPECT_EQ(registry.Find(actionId), action1);

    // Act
    registry.Register(actionId, action2);
    ActionFn result = registry.Find(actionId);

    // Assert
    EXPECT_EQ(result, action2);
}

// ============================================================================
// DiaBehaviourTree_DecoratorRegistry
// ============================================================================

// Mock decorator for testing — in anonymous namespace to avoid ODR conflict with other test files
namespace {
class MockDecorator : public IDecoratorNode {
public:
    bool ShouldTickChild(const DecoratorContext& ctx) const override {
        return true;
    }

    NodeResult Evaluate(NodeResult childResult, DecoratorContext& ctx) const override {
        return childResult;
    }

    void OnReset(DecoratorContext& ctx) const override {
    }

    StringCRC GetTypeId() const override {
        return StringCRC("MockDecorator");
    }
};
} // anonymous namespace

class DiaBehaviourTree_DecoratorRegistry : public ::testing::Test {
protected:
    DecoratorRegistry registry;
};

TEST_F(DiaBehaviourTree_DecoratorRegistry, Register_ThenFind_ReturnsRegisteredDecorator) {
    // Arrange
    MockDecorator decorator;
    StringCRC decoratorId("TestDecorator");

    // Act
    registry.Register(decoratorId, &decorator);
    const IDecoratorNode* result = registry.Find(decoratorId);

    // Assert
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result, &decorator);
}

TEST_F(DiaBehaviourTree_DecoratorRegistry, Find_UnknownCRC_ReturnsNullptr) {
    // Arrange
    StringCRC unknownId("UnknownDecorator");

    // Act
    const IDecoratorNode* result = registry.Find(unknownId);

    // Assert
    EXPECT_EQ(result, nullptr);
}
