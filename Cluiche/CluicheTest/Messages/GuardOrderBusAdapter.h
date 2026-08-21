#pragma once

#include <DiaOrder/IOrderQueueObserver.h>

namespace Dia { namespace MessageBus { class Bus; } }

namespace CluicheTest {

// Forward-declared only — GuardOrderContext is a plain data struct (defined
// in Modules/TestStages/BehaviourTreeTestStageModule.h) with a single raw
// GuardAgent* member, so an incomplete type is sufficient for the
// IOrderQueueObserver<GuardOrderContext> template instantiation and for the
// by-reference IOrder<GuardOrderContext>& parameters below. The .cpp
// includes the real header for the GuardMoveOrder dynamic_cast.
struct GuardOrderContext;

// -----------------------------------------------------------------------
// GuardOrderBusAdapter
//
// One more IOrderQueueObserver<GuardOrderContext> — registered on a
// Dia::Order::OrderQueue<GuardOrderContext> instance (one per GuardAgent,
// see BehaviourTreeTestStageModule.h) alongside any other direct observer
// (e.g. GuardOrderObserver), it does not replace them. Forwards each
// synchronous OnOrderStarted/OnOrderFinished/OnOrderCancelled/OnQueueEmpty
// callback onto the shared DiaMessageBus::Bus via Bus::Broadcast<T>() —
// never forwarding the IOrder<GuardOrderContext>& itself into the payload.
//
// GuardAgent (declared in BehaviourTreeTestStageModule.h) is plain
// per-guard state owned directly by the stage module — it is NOT a
// Dia::Entity::Entity — so there is no entity handle to Post() against.
// Broadcast is therefore the correct router here, for the same reason
// DiaEconomy's EconomyBusAdapter broadcasts rather than posts: an
// EconomyInstance is likewise not entity-scoped. (Contrast with
// DiaBehaviourTree's BehaviourTreeBusAdapter, which IS entity-scoped and
// uses Post + MakeEntityAddress.)
//
// GuardMoveOrder is the only concrete IOrder<GuardOrderContext> subtype in
// the codebase today, and it exposes `target`/`speed` beyond the base
// IOrder<TContext> interface's GetOrderId(). OnOrderStarted dynamic_casts
// the incoming IOrder<GuardOrderContext>& to const GuardMoveOrder* to
// recover those two fields for OrderStartedEvent; if a future order type
// is not a GuardMoveOrder, target/speed are left zero-initialized rather
// than asserting — orderId is always correct regardless of subtype.
//
// This adapter lives in CluicheTest (application layer), not DiaOrder,
// because .diagamemessages-declared message structs are concrete,
// non-template types, and GuardOrderContext is application-layer content.
// See docs/specs/applications/dia/systems/diaorder/order-observer-bus-adapter.md.
// -----------------------------------------------------------------------
class GuardOrderBusAdapter : public Dia::Order::IOrderQueueObserver<GuardOrderContext>
{
public:
    explicit GuardOrderBusAdapter(Dia::MessageBus::Bus& bus);

    void OnOrderStarted(const Dia::Order::IOrder<GuardOrderContext>& order)   override;
    void OnOrderFinished(const Dia::Order::IOrder<GuardOrderContext>& order)  override;
    void OnOrderCancelled(const Dia::Order::IOrder<GuardOrderContext>& order) override;
    void OnQueueEmpty() override;

private:
    Dia::MessageBus::Bus& mBus;
};

} // namespace CluicheTest
