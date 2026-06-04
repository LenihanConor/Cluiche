#include <gtest/gtest.h>
#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/EntityAddress.h>
#include <diaentitytemplate/EntityRouter.h>
#include <diaentitytemplate/ComponentPool.h>
#include <DiaMailbox/IMailboxRouter.h>
#include "TestComponent.h"

using namespace Dia::Entity;
using namespace Dia::Mailbox;

// Minimal message type for subscription tests.
struct TestMsg {
    int value = 0;
};

// =========================================================================
// Helper: register TestComponent pool with a domain
// =========================================================================
static void RegisterTestComponentPool(Domain& domain) {
    domain.RegisterPool(new ComponentPool<DiaEntityTest::TestComponent>(
        DiaEntityTest::TestComponent::kTypeId));
}

// =========================================================================
// 1. RouterIsRegisteredOnDomainConstruction
// =========================================================================
TEST(DiaEntityRouter, RouterIsRegisteredOnDomainConstruction) {
    Domain domain;
    IMailboxRouter* router = domain.GetMailbox().GetRouter(kEntityRouterId);
    EXPECT_NE(router, nullptr);
    EXPECT_EQ(router->GetRouterId(), kEntityRouterId);
}

// =========================================================================
// 2. MakeEntityAddressEncodesCorrectly
// =========================================================================
TEST(DiaEntityRouter, MakeEntityAddressEncodesCorrectly) {
    Domain domain;
    Entity e = domain.CreateEntity();

    Address addr = MakeEntityAddress(e);

    EXPECT_EQ(addr.routerId, kEntityRouterId);
    EXPECT_EQ(GetAddressKind(addr), AddressKind::Entity);

    Entity decoded = GetAddressEntity(addr);
    EXPECT_EQ(decoded.GetIndex(),      e.GetIndex());
    EXPECT_EQ(decoded.GetGeneration(), e.GetGeneration() & 0xFFFFFFu);
}

// =========================================================================
// 3. MakeAllAddressEncodesCorrectly
// =========================================================================
TEST(DiaEntityRouter, MakeAllAddressEncodesCorrectly) {
    Address addr = MakeAllAddress();
    EXPECT_EQ(addr.routerId, kEntityRouterId);
    EXPECT_EQ(GetAddressKind(addr), AddressKind::All);
}

// =========================================================================
// 4. MakeComponentTypeAddressEncodesCorrectly
// =========================================================================
TEST(DiaEntityRouter, MakeComponentTypeAddressEncodesCorrectly) {
    Dia::Core::StringCRC typeId("test-component");
    Address addr = MakeComponentTypeAddress(typeId);

    EXPECT_EQ(addr.routerId, kEntityRouterId);
    EXPECT_EQ(GetAddressKind(addr), AddressKind::ComponentType);

    Dia::Core::StringCRC decoded = GetAddressComponentType(addr);
    EXPECT_EQ(decoded, typeId);
}

// =========================================================================
// 5. MakeSelfAddressEncodesCorrectly
// =========================================================================
TEST(DiaEntityRouter, MakeSelfAddressEncodesCorrectly) {
    Domain domain;
    Entity e = domain.CreateEntity();

    Address addr = MakeSelfAddress(e);

    EXPECT_EQ(addr.routerId, kEntityRouterId);
    EXPECT_EQ(GetAddressKind(addr), AddressKind::Self);

    Entity decoded = GetAddressEntity(addr);
    EXPECT_EQ(decoded.GetIndex(),      e.GetIndex());
    EXPECT_EQ(decoded.GetGeneration(), e.GetGeneration() & 0xFFFFFFu);
}

// =========================================================================
// 6. ResolveEntityKind — subscriber receives message addressed to its entity
// =========================================================================
TEST(DiaEntityRouter, ResolveEntityKind) {
    Domain domain;
    domain.GetMailbox().RegisterType<TestMsg, 16>();

    Entity e = domain.CreateEntity();
    SubscriberId sub = MakeEntitySubscriberId(e);
    domain.GetMailbox().Subscribe<TestMsg>(sub);

    Address addr = MakeEntityAddress(e);
    SubscriberSet matched;
    bool resolved = domain.GetMailbox().Resolve<TestMsg>(addr, matched);

    EXPECT_TRUE(resolved);
    EXPECT_EQ(matched.Size(), 1u);
    EXPECT_EQ(matched[0], sub);
}

// =========================================================================
// 7. ResolveEntityKindStaleMissesIfNotAlive
// =========================================================================
TEST(DiaEntityRouter, ResolveEntityKindStaleMissesIfNotAlive) {
    Domain domain;
    domain.GetMailbox().RegisterType<TestMsg, 16>();

    Entity e = domain.CreateEntity();
    SubscriberId sub = MakeEntitySubscriberId(e);
    domain.GetMailbox().Subscribe<TestMsg>(sub);

    // Destroy entity — stale address should silently no-op
    domain.QueueDestroy(e);
    domain.EndOfFrame();

    Address addr = MakeEntityAddress(e);  // points to now-dead entity
    SubscriberSet matched;
    domain.GetMailbox().Resolve<TestMsg>(addr, matched);

    EXPECT_EQ(matched.Size(), 0u);
}

// =========================================================================
// 8. ResolveAllKind — all 3 subscribers are matched
// =========================================================================
TEST(DiaEntityRouter, ResolveAllKind) {
    Domain domain;
    domain.GetMailbox().RegisterType<TestMsg, 16>();

    Entity e1 = domain.CreateEntity();
    Entity e2 = domain.CreateEntity();
    Entity e3 = domain.CreateEntity();

    domain.GetMailbox().Subscribe<TestMsg>(MakeEntitySubscriberId(e1));
    domain.GetMailbox().Subscribe<TestMsg>(MakeEntitySubscriberId(e2));
    domain.GetMailbox().Subscribe<TestMsg>(MakeEntitySubscriberId(e3));

    Address addr = MakeAllAddress();
    SubscriberSet matched;
    bool resolved = domain.GetMailbox().Resolve<TestMsg>(addr, matched);

    EXPECT_TRUE(resolved);
    EXPECT_EQ(matched.Size(), 3u);
}

// =========================================================================
// 9. ResolveComponentTypeKind — only entity with TestComponent is matched
// =========================================================================
TEST(DiaEntityRouter, ResolveComponentTypeKind) {
    Domain domain;
    RegisterTestComponentPool(domain);
    domain.GetMailbox().RegisterType<TestMsg, 16>();

    Entity e1 = domain.CreateEntity();  // will have TestComponent
    Entity e2 = domain.CreateEntity();  // no component

    // Add TestComponent to e1
    domain.QueueAddComponent<DiaEntityTest::TestComponent>(e1, Json::Value());
    domain.EndOfFrame();

    // Subscribe both entities
    SubscriberId sub1 = MakeEntitySubscriberId(e1);
    SubscriberId sub2 = MakeEntitySubscriberId(e2);
    domain.GetMailbox().Subscribe<TestMsg>(sub1);
    domain.GetMailbox().Subscribe<TestMsg>(sub2);

    // Resolve by component type — should only match e1
    Address addr = MakeComponentTypeAddress(DiaEntityTest::TestComponent::kTypeId);
    SubscriberSet matched;
    bool resolved = domain.GetMailbox().Resolve<TestMsg>(addr, matched);

    EXPECT_TRUE(resolved);
    EXPECT_EQ(matched.Size(), 1u);
    EXPECT_EQ(matched[0], sub1);
}

// =========================================================================
// 10. ResolveSelfKind — resolves sender's own subscriber
// =========================================================================
TEST(DiaEntityRouter, ResolveSelfKind) {
    Domain domain;
    domain.GetMailbox().RegisterType<TestMsg, 16>();

    Entity sender = domain.CreateEntity();
    Entity other  = domain.CreateEntity();

    SubscriberId senderSub = MakeEntitySubscriberId(sender);
    SubscriberId otherSub  = MakeEntitySubscriberId(other);
    domain.GetMailbox().Subscribe<TestMsg>(senderSub);
    domain.GetMailbox().Subscribe<TestMsg>(otherSub);

    // Self address encodes the sender entity
    Address addr = MakeSelfAddress(sender);
    SubscriberSet matched;
    bool resolved = domain.GetMailbox().Resolve<TestMsg>(addr, matched);

    EXPECT_TRUE(resolved);
    EXPECT_EQ(matched.Size(), 1u);
    EXPECT_EQ(matched[0], senderSub);
}
