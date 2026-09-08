// EntityRouterBusWiringTests.cpp
//
// Integration test for wiring Domain's real EntityRouter (owned by
// Dia::Entity::Domain, auto-registered on Domain's own private Mailbox in the
// Domain constructor) onto a second, independent mailbox — the one owned by
// Dia::MessageBus::Bus — via Domain::GetEntityRouter() + Bus::RegisterRouter.
//
// This proves the SAME real EntityRouter instance (not the test-local
// MockEntityRouter used in EntityRouterRegistrationTest.cpp) resolves
// entity-addressed messages posted through the shared Bus against real
// Domain component/entity state, while leaving Domain's own private-mailbox
// EntityDestroyedMessage plumbing completely unaffected.
//
// The test fixture IS the composition root for this task (per the approved
// spec's Open Question #2): no real CluicheTest stage yet owns both a Domain
// and a Bus, so the wiring is exercised directly here rather than inside a
// not-yet-existing stage.
//
// IMPORTANT — router-id mismatch discovered while writing this test, FIXED
// same session: Dia::MessageBus::Bus::kEntityRouterId was StringCRC("entity"),
// but the real Dia::Entity::EntityRouter::GetRouterId() returns
// Dia::Entity::kEntityRouterId, StringCRC("dia.entity.router") (see
// EntityAddress.cpp). Mailbox routers are looked up by GetRouterId()
// (Mailbox::RegisterRouter / Mailbox::GetRouter — see Mailbox.cpp), so an
// Address built with the old Bus::kEntityRouterId value would never have
// found the real EntityRouter once registered — only the test-local
// MockEntityRouter in EntityRouterRegistrationTest.cpp matched it, because it
// hardcoded GetRouterId() to return whatever Bus::kEntityRouterId was.
// Bus::kEntityRouterId's VALUE is now "dia.entity.router" too (Bus.cpp) —
// DiaMessageBus still doesn't #include diaentitytemplate headers (the two
// constants remain independently defined, not a shared symbol), but their
// values now match. Tests below still build addresses via the
// Dia::Entity::Make*Address helpers (unaffected either way); see
// RouterIdConstants_ValuesMatch below for the regression guard on the fix.
//
// SubscriberId encoding reused as-is (EntityAddress.cpp):
//   MakeEntitySubscriberId(entity).value == (index << 24) | (generation & 0xFFFFFF)
// Bus::Subscribe<T> only accepts a StringCRC subscriber id (Bus.h), and
// internally narrows it via `mailboxSubscriberId.value =
// static_cast<uint64_t>(subscriberId.Value())` — StringCRC::Value() is a
// 32-bit unsigned int (DiaCore/CRC/CRC.h). ToBusSubscriberId() below builds a
// StringCRC whose Value() equals that same numeric encoding (using the same
// raw-value-assignment trick EntityAddress.cpp's file-local
// StringCRCFromValue() uses), so a Bus-side subscription lands on the exact
// Dia::Mailbox::SubscriberId that EntityRouter::ResolveEntity computes.

#include <gtest/gtest.h>

#include <DiaCore/CRC/CRC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/EntityAddress.h>
#include <DiaEntity/EntityRouter.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/Messages/EntityDestroyedMessage.h>
#include <DiaMailbox/IMailboxRouter.h>
#include <DiaMessageBus/Bus.h>
#include "TestComponent.h"

using namespace Dia::Entity;
using namespace Dia::Mailbox;
using namespace Dia::MessageBus;

namespace {

    // Test-local message type for Bus registration/subscription/posting.
    struct BusRouterTestMsg {
        static inline const Dia::Core::StringCRC kTypeId{ "BusRouterTestMsg" };
        uint32_t value = 0;
    };

    // See file header — builds a StringCRC whose Value() equals the exact
    // numeric encoding MakeEntitySubscriberId() already produces, so it
    // survives Bus::Subscribe<T>'s StringCRC->SubscriberId narrowing intact.
    Dia::Core::StringCRC ToBusSubscriberId(Dia::Mailbox::SubscriberId subId) {
        Dia::Core::StringCRC result;
        static_cast<Dia::Core::CRC&>(result) = static_cast<unsigned int>(subId.value);
        return result;
    }

    void RegisterTestComponentPool(Domain& domain) {
        domain.RegisterPool(new ComponentPool<DiaEntityTest::TestComponent>(
            DiaEntityTest::TestComponent::kTypeId));
    }

} // namespace

// =========================================================================
// RouterIdConstants_ValuesMatch
//
// Regression guard for the id mismatch documented in the file header.
// DiaMessageBus and diaentitytemplate are specced/built independently and
// must not #include each other's headers, so this can only be a value
// equality check (two separately-defined StringCRC constants), not a shared
// symbol — if either constant's literal ever drifts again, this fails.
// =========================================================================
TEST(EntityRouterBusWiring, RouterIdConstants_ValuesMatch) {
    EXPECT_EQ(Dia::MessageBus::Bus::kEntityRouterId, Dia::Entity::kEntityRouterId);
}

// =========================================================================
// GetEntityRouter_ReturnsSameInstanceAsPrivateMailboxRouter
// =========================================================================
TEST(EntityRouterBusWiring, GetEntityRouter_ReturnsSameInstanceAsPrivateMailboxRouter) {
    Domain domain;

    IMailboxRouter* registeredOnOwnMailbox = domain.GetMailbox().GetRouter(kEntityRouterId);
    ASSERT_NE(registeredOnOwnMailbox, nullptr);

    EXPECT_EQ(registeredOnOwnMailbox, &domain.GetEntityRouter());
    EXPECT_EQ(&domain.GetEntityRouter(), &const_cast<const Domain&>(domain).GetEntityRouter());
}

// =========================================================================
// RegisterRouterOnBus_DelegatesCleanlyAlongsideExistingRegistration
// =========================================================================
TEST(EntityRouterBusWiring, RegisterRouterOnBus_DelegatesCleanlyAlongsideExistingRegistration) {
    Domain domain;
    Bus bus;
    bus.Initialize();

    bus.RegisterRouter(&domain.GetEntityRouter());

    EXPECT_TRUE(bus.IsRouterRegistered(kEntityRouterId));

    // Domain's own registration on its private mailbox is undisturbed.
    IMailboxRouter* stillOnOwnMailbox = domain.GetMailbox().GetRouter(kEntityRouterId);
    EXPECT_EQ(stillOnOwnMailbox, &domain.GetEntityRouter());
}

// =========================================================================
// EntityAddressedPost_DeliversToSubscribedComponent
// =========================================================================
TEST(EntityRouterBusWiring, EntityAddressedPost_DeliversToSubscribedComponent) {
    Domain domain;
    Bus bus;
    bus.Initialize();
    bus.RegisterRouter(&domain.GetEntityRouter());
    ASSERT_TRUE((bus.RegisterType<BusRouterTestMsg, 8>()));

    Entity e = domain.CreateEntity();
    ASSERT_TRUE(e.IsValid());

    int callCount = 0;
    BusSubscriptionHandle handle = bus.Subscribe<BusRouterTestMsg>(
        ToBusSubscriberId(MakeEntitySubscriberId(e)),
        [&callCount](const BusRouterTestMsg&) { ++callCount; });
    ASSERT_TRUE(handle.IsValid());

    BusRouterTestMsg msg;
    msg.value = 7;
    EXPECT_TRUE(bus.Post<BusRouterTestMsg>(MakeEntityAddress(e), msg));

    EXPECT_EQ(callCount, 0) << "Resolve/dispatch must not run before Update()";
    bus.Update();
    EXPECT_EQ(callCount, 1);
}

// =========================================================================
// EntityAddressedPost_NoSubscribers_SilentNoOp
// =========================================================================
TEST(EntityRouterBusWiring, EntityAddressedPost_NoSubscribers_SilentNoOp) {
    Domain domain;
    Bus bus;
    bus.Initialize();
    bus.RegisterRouter(&domain.GetEntityRouter());
    ASSERT_TRUE((bus.RegisterType<BusRouterTestMsg, 8>()));

    Entity e = domain.CreateEntity();
    ASSERT_TRUE(e.IsValid());
    // Deliberately no Bus-side subscriber for e.

    BusRouterTestMsg msg;
    msg.value = 3;
    EXPECT_TRUE(bus.Post<BusRouterTestMsg>(MakeEntityAddress(e), msg));

    EXPECT_NO_FATAL_FAILURE(bus.Update());

    const LedgerSnapshot& ledger = bus.GetLastTickLedger();
    bool foundEntry = false;
    for (uint32_t i = 0; i < ledger.entries.Size(); ++i) {
        if (ledger.entries[i].typeId == BusRouterTestMsg::kTypeId) {
            foundEntry = true;
            EXPECT_EQ(ledger.entries[i].deliveries, 0u)
                << "Zero Bus-side subscribers must mean zero deliveries";
        }
    }
    EXPECT_TRUE(foundEntry) << "Ledger should still record the posted message, just with 0 deliveries";
}

// =========================================================================
// EntityAddressedPost_WrongEntityHandle_NotDelivered
// =========================================================================
TEST(EntityRouterBusWiring, EntityAddressedPost_WrongEntityHandle_NotDelivered) {
    Domain domain;
    Bus bus;
    bus.Initialize();
    bus.RegisterRouter(&domain.GetEntityRouter());
    ASSERT_TRUE((bus.RegisterType<BusRouterTestMsg, 8>()));

    Entity a = domain.CreateEntity();
    Entity b = domain.CreateEntity();
    ASSERT_TRUE(a.IsValid());
    ASSERT_TRUE(b.IsValid());

    int callCountForB = 0;
    BusSubscriptionHandle handleB = bus.Subscribe<BusRouterTestMsg>(
        ToBusSubscriberId(MakeEntitySubscriberId(b)),
        [&callCountForB](const BusRouterTestMsg&) { ++callCountForB; });
    ASSERT_TRUE(handleB.IsValid());

    // Address to A — B's subscriber must never fire.
    BusRouterTestMsg msg;
    EXPECT_TRUE(bus.Post<BusRouterTestMsg>(MakeEntityAddress(a), msg));
    bus.Update();

    EXPECT_EQ(callCountForB, 0);
}

// =========================================================================
// ComponentTypeAddress_StillResolvesViaExistingMechanism
// =========================================================================
TEST(EntityRouterBusWiring, ComponentTypeAddress_StillResolvesViaExistingMechanism) {
    Domain domain;
    RegisterTestComponentPool(domain);
    Bus bus;
    bus.Initialize();
    bus.RegisterRouter(&domain.GetEntityRouter());
    ASSERT_TRUE((bus.RegisterType<BusRouterTestMsg, 8>()));

    Entity withComponent    = domain.CreateEntity();
    Entity withoutComponent = domain.CreateEntity();
    domain.QueueAddComponent<DiaEntityTest::TestComponent>(withComponent, Json::Value());
    domain.EndOfFrame();
    ASSERT_TRUE(domain.HasComponent<DiaEntityTest::TestComponent>(withComponent));
    ASSERT_FALSE(domain.HasComponent<DiaEntityTest::TestComponent>(withoutComponent));

    int withCount    = 0;
    int withoutCount = 0;
    BusSubscriptionHandle h1 = bus.Subscribe<BusRouterTestMsg>(
        ToBusSubscriberId(MakeEntitySubscriberId(withComponent)),
        [&withCount](const BusRouterTestMsg&) { ++withCount; });
    BusSubscriptionHandle h2 = bus.Subscribe<BusRouterTestMsg>(
        ToBusSubscriberId(MakeEntitySubscriberId(withoutComponent)),
        [&withoutCount](const BusRouterTestMsg&) { ++withoutCount; });
    ASSERT_TRUE(h1.IsValid());
    ASSERT_TRUE(h2.IsValid());

    BusRouterTestMsg msg;
    EXPECT_TRUE(bus.Post<BusRouterTestMsg>(
        MakeComponentTypeAddress(DiaEntityTest::TestComponent::kTypeId), msg));
    bus.Update();

    EXPECT_EQ(withCount, 1);
    EXPECT_EQ(withoutCount, 0);
}

// =========================================================================
// DomainPrivateMailbox_EntityDestroyedMessage_StillDeliveredUnaffected
// (regression — mirrors IntegrationTests.cpp's MailboxAndHierarchyDestroy
// pattern: subscribe on Domain's own mailbox, destroy, Drain directly.)
// =========================================================================
TEST(EntityRouterBusWiring, DomainPrivateMailbox_EntityDestroyedMessage_StillDeliveredUnaffected) {
    Domain domain;
    Bus bus;
    bus.Initialize();
    bus.RegisterRouter(&domain.GetEntityRouter()); // registered on the Bus too

    Entity listener = domain.CreateEntity("listener");
    Dia::Mailbox::SubscriberId sid = MakeEntitySubscriberId(listener);
    domain.GetMailbox().Subscribe<EntityDestroyedMessage>(sid);

    Entity target = domain.CreateEntity("target");
    domain.QueueDestroy(target);
    domain.EndOfFrame();

    EXPECT_FALSE(domain.IsAlive(target));

    int destroyCount = 0;
    bool targetReceived = false;
    domain.GetMailbox().Drain<EntityDestroyedMessage>(
        [&](const Dia::Mailbox::Address& /*addr*/, const EntityDestroyedMessage& msg) {
            ++destroyCount;
            if (msg.destroyed.GetIndex() == target.GetIndex()) {
                targetReceived = true;
            }
        });

    EXPECT_EQ(destroyCount, 1);
    EXPECT_TRUE(targetReceived);
}

// =========================================================================
// DomainPrivateMailbox_And_Bus_AreIndependentQueues
// =========================================================================
TEST(EntityRouterBusWiring, DomainPrivateMailbox_And_Bus_AreIndependentQueues) {
    Domain domain;
    Bus bus;
    bus.Initialize();
    bus.RegisterRouter(&domain.GetEntityRouter());

    domain.GetMailbox().RegisterType<BusRouterTestMsg, 8>();
    ASSERT_TRUE((bus.RegisterType<BusRouterTestMsg, 8>()));

    Entity e = domain.CreateEntity();
    Dia::Mailbox::SubscriberId subId = MakeEntitySubscriberId(e);

    int busCallCount = 0;
    BusSubscriptionHandle busHandle = bus.Subscribe<BusRouterTestMsg>(
        ToBusSubscriberId(subId), [&busCallCount](const BusRouterTestMsg&) { ++busCallCount; });
    ASSERT_TRUE(busHandle.IsValid());

    // Send via Domain's OWN mailbox directly — the Bus subscriber must never see it,
    // and Bus::Update() must find nothing on its own (separate) mailbox.
    BusRouterTestMsg viaDomain;
    viaDomain.value = 1;
    EXPECT_TRUE(domain.GetMailbox().Send<BusRouterTestMsg>(MakeEntityAddress(e), viaDomain));

    bus.Update();
    EXPECT_EQ(busCallCount, 0) << "Bus subscriber must not see messages sent on Domain's own mailbox";

    int domainDrainCount = 0;
    domain.GetMailbox().Drain<BusRouterTestMsg>(
        [&](const Dia::Mailbox::Address&, const BusRouterTestMsg&) { ++domainDrainCount; });
    EXPECT_EQ(domainDrainCount, 1) << "The message sent on Domain's own mailbox is still there";

    // Now post via the Bus — Domain's own mailbox must stay empty (independent queues).
    BusRouterTestMsg viaBus;
    viaBus.value = 2;
    EXPECT_TRUE(bus.Post<BusRouterTestMsg>(MakeEntityAddress(e), viaBus));
    bus.Update();
    EXPECT_EQ(busCallCount, 1);

    int domainDrainCount2 = 0;
    domain.GetMailbox().Drain<BusRouterTestMsg>(
        [&](const Dia::Mailbox::Address&, const BusRouterTestMsg&) { ++domainDrainCount2; });
    EXPECT_EQ(domainDrainCount2, 0) << "Posting via the Bus must never land in Domain's own mailbox queue";
}

// =========================================================================
// RouterRegisteredOnBothMailboxes_FunctionsIndependently
// =========================================================================
TEST(EntityRouterBusWiring, RouterRegisteredOnBothMailboxes_FunctionsIndependently) {
    Domain domain;
    Bus bus;
    bus.Initialize();
    bus.RegisterRouter(&domain.GetEntityRouter());

    domain.GetMailbox().RegisterType<BusRouterTestMsg, 8>();
    ASSERT_TRUE((bus.RegisterType<BusRouterTestMsg, 8>()));

    Entity e = domain.CreateEntity();
    Dia::Mailbox::SubscriberId subId = MakeEntitySubscriberId(e);

    // Resolve directly against Domain's own mailbox.
    domain.GetMailbox().Subscribe<BusRouterTestMsg>(subId);
    Dia::Mailbox::SubscriberSet matchedOnDomain;
    const bool resolvedOnDomain =
        domain.GetMailbox().Resolve<BusRouterTestMsg>(MakeEntityAddress(e), matchedOnDomain);
    EXPECT_TRUE(resolvedOnDomain);
    ASSERT_EQ(matchedOnDomain.Size(), 1u);
    EXPECT_EQ(matchedOnDomain[0], subId);

    // Same router, resolving via the Bus's mailbox in the same run.
    int callCount = 0;
    BusSubscriptionHandle handle = bus.Subscribe<BusRouterTestMsg>(
        ToBusSubscriberId(subId), [&callCount](const BusRouterTestMsg&) { ++callCount; });
    ASSERT_TRUE(handle.IsValid());

    BusRouterTestMsg msg;
    EXPECT_TRUE(bus.Post<BusRouterTestMsg>(MakeEntityAddress(e), msg));
    bus.Update();
    EXPECT_EQ(callCount, 1);
}

// =========================================================================
// StaleEntityHandle_GenerationMismatch_SilentNoOpOnBusPath
// =========================================================================
TEST(EntityRouterBusWiring, StaleEntityHandle_GenerationMismatch_SilentNoOpOnBusPath) {
    Domain domain;
    Bus bus;
    bus.Initialize();
    bus.RegisterRouter(&domain.GetEntityRouter());
    ASSERT_TRUE((bus.RegisterType<BusRouterTestMsg, 8>()));

    Entity e = domain.CreateEntity();
    ASSERT_TRUE(e.IsValid());

    int callCount = 0;
    BusSubscriptionHandle handle = bus.Subscribe<BusRouterTestMsg>(
        ToBusSubscriberId(MakeEntitySubscriberId(e)),
        [&callCount](const BusRouterTestMsg&) { ++callCount; });
    ASSERT_TRUE(handle.IsValid());

    // Destroy the entity — the previously-captured handle is now stale
    // (its generation no longer matches the live slot's generation),
    // mirroring EntityRouterTests.cpp's ResolveEntityKindStaleMissesIfNotAlive.
    domain.QueueDestroy(e);
    domain.EndOfFrame();
    ASSERT_FALSE(domain.IsAlive(e));

    BusRouterTestMsg msg;
    EXPECT_TRUE(bus.Post<BusRouterTestMsg>(MakeEntityAddress(e), msg));
    EXPECT_NO_FATAL_FAILURE(bus.Update());

    EXPECT_EQ(callCount, 0) << "A stale entity address must silently resolve to zero matches";
}
