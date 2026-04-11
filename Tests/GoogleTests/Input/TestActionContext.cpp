////////////////////////////////////////////////////////////////////////////////
// Filename: TestActionContext.cpp - Google Test for ActionContext
////////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include <DiaInput/ActionContext.h>
#include <DiaInput/Event.h>
#include <DiaInput/EventData.h>
#include <DiaInput/EKey.h>

using namespace Dia::Input;

////////////////////////////////////////////////////////////////////////////////
// ActionContext Basic Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionContext, ConstructorSetsContextId)
{
	ContextID contextId("Gameplay");
	ActionContext context(contextId);

	EXPECT_EQ(context.GetContextId(), contextId);
}

TEST(ActionContext, DefaultBlockingBehavior)
{
	ActionContext context(ContextID("Menu"));

	EXPECT_FALSE(context.BlocksLowerContexts());
}

TEST(ActionContext, BlockingBehaviorCanBeSet)
{
	ActionContext context(ContextID("Menu"), true);

	EXPECT_TRUE(context.BlocksLowerContexts());

	context.SetBlockLowerContexts(false);
	EXPECT_FALSE(context.BlocksLowerContexts());
}

TEST(ActionContext, BindKeyConvenienceMethod)
{
	ActionContext context(ContextID("Gameplay"));
	ActionID jumpAction("Jump");

	context.BindKey(jumpAction, EKey::Space);

	// Access internal ActionMap
	const ActionMap& actionMap = context.GetActionMap();

	// Process event
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::Space);
	events.Add(evt);

	// Need to process on the action map directly
	ActionMap& mutableMap = context.GetActionMap();
	mutableMap.ProcessEvents(events);

	EXPECT_TRUE(mutableMap.IsActionActive(jumpAction));
}

////////////////////////////////////////////////////////////////////////////////
// ActionContextManager Basic Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionContextManager, CreateContext)
{
	ActionContextManager manager;
	ContextID gameplayId("Gameplay");

	ActionContext* context = manager.CreateContext(gameplayId);

	EXPECT_NE(context, nullptr);
	EXPECT_EQ(context->GetContextId(), gameplayId);
}

TEST(ActionContextManager, GetContextById)
{
	ActionContextManager manager;
	ContextID gameplayId("Gameplay");

	ActionContext* createdContext = manager.CreateContext(gameplayId);
	ActionContext* retrievedContext = manager.GetContext(gameplayId);

	EXPECT_EQ(createdContext, retrievedContext);
}

TEST(ActionContextManager, GetNonExistentContextReturnsNull)
{
	ActionContextManager manager;
	ActionContext* context = manager.GetContext(ContextID("NonExistent"));

	EXPECT_EQ(context, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// Context Stack Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionContextManager, PushContextOntoStack)
{
	ActionContextManager manager;
	ActionContext* context = manager.CreateContext(ContextID("Gameplay"));

	manager.PushContext(context);

	EXPECT_EQ(manager.GetStackSize(), 1);
	EXPECT_EQ(manager.GetActiveContext(), context);
}

TEST(ActionContextManager, PushMultipleContexts)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"));
	ActionContext* menu = manager.CreateContext(ContextID("Menu"));

	manager.PushContext(gameplay);
	manager.PushContext(menu);

	EXPECT_EQ(manager.GetStackSize(), 2);
	EXPECT_EQ(manager.GetActiveContext(), menu);  // Top of stack
}

TEST(ActionContextManager, PopContextFromStack)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"));
	ActionContext* menu = manager.CreateContext(ContextID("Menu"));

	manager.PushContext(gameplay);
	manager.PushContext(menu);

	ActionContext* popped = manager.PopContext();

	EXPECT_EQ(popped, menu);
	EXPECT_EQ(manager.GetStackSize(), 1);
	EXPECT_EQ(manager.GetActiveContext(), gameplay);
}

TEST(ActionContextManager, PopEmptyStackReturnsNull)
{
	ActionContextManager manager;
	ActionContext* popped = manager.PopContext();

	EXPECT_EQ(popped, nullptr);
}

TEST(ActionContextManager, ClearStack)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"));
	ActionContext* menu = manager.CreateContext(ContextID("Menu"));

	manager.PushContext(gameplay);
	manager.PushContext(menu);

	manager.ClearStack();

	EXPECT_EQ(manager.GetStackSize(), 0);
	EXPECT_EQ(manager.GetActiveContext(), nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// Input Processing Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionContextManager, ProcessEventsThroughSingleContext)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"));
	ActionID jumpAction("Jump");

	gameplay->BindKey(jumpAction, EKey::Space);
	manager.PushContext(gameplay);

	// Create event
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::Space);
	events.Add(evt);

	manager.ProcessEvents(events);

	EXPECT_TRUE(manager.IsActionActive(jumpAction));
	EXPECT_FLOAT_EQ(manager.GetActionValue(jumpAction), 1.0f);
}

TEST(ActionContextManager, TopContextProcessedFirst)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"));
	ActionContext* menu = manager.CreateContext(ContextID("Menu"));

	ActionID actionInGameplay("GameplayAction");
	ActionID actionInMenu("MenuAction");

	gameplay->BindKey(actionInGameplay, EKey::Space);
	menu->BindKey(actionInMenu, EKey::Enter);

	manager.PushContext(gameplay);
	manager.PushContext(menu);

	// Create events for both actions
	EventData events;

	Event spaceEvt;
	spaceEvt.type = Event::EType::kKeyPressed;
	spaceEvt.key = static_cast<int>(EKey::Space);
	events.Add(spaceEvt);

	Event enterEvt;
	enterEvt.type = Event::EType::kKeyPressed;
	enterEvt.key = static_cast<int>(EKey::Enter);
	events.Add(enterEvt);

	manager.ProcessEvents(events);

	// Both contexts should process events
	EXPECT_TRUE(manager.IsActionActive(actionInGameplay));
	EXPECT_TRUE(manager.IsActionActive(actionInMenu));
}

////////////////////////////////////////////////////////////////////////////////
// Blocking Behavior Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionContextManager, BlockingContextPreventsLowerContexts)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
	ActionContext* menu = manager.CreateContext(ContextID("Menu"), true);  // Blocks lower

	ActionID jumpAction("Jump");
	gameplay->BindKey(jumpAction, EKey::Space);
	menu->BindKey(ActionID("MenuAction"), EKey::Enter);

	manager.PushContext(gameplay);
	manager.PushContext(menu);

	// Create event for gameplay action
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::Space);
	events.Add(evt);

	manager.ProcessEvents(events);

	// Gameplay action should NOT be active (blocked by menu)
	EXPECT_FALSE(manager.IsActionActive(jumpAction));
}

TEST(ActionContextManager, NonBlockingContextAllowsLowerContexts)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
	ActionContext* overlay = manager.CreateContext(ContextID("Overlay"), false);  // Non-blocking

	ActionID jumpAction("Jump");
	gameplay->BindKey(jumpAction, EKey::Space);

	manager.PushContext(gameplay);
	manager.PushContext(overlay);

	// Create event for gameplay action
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::Space);
	events.Add(evt);

	manager.ProcessEvents(events);

	// Gameplay action SHOULD be active (not blocked)
	EXPECT_TRUE(manager.IsActionActive(jumpAction));
}

TEST(ActionContextManager, BlockingAppliesFromTopToBottom)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
	ActionContext* inventory = manager.CreateContext(ContextID("Inventory"), false);
	ActionContext* dialog = manager.CreateContext(ContextID("Dialog"), true);  // Blocks lower

	ActionID gameplayAction("Gameplay");
	ActionID inventoryAction("Inventory");
	ActionID dialogAction("Dialog");

	gameplay->BindKey(gameplayAction, EKey::W);
	inventory->BindKey(inventoryAction, EKey::I);
	dialog->BindKey(dialogAction, EKey::Enter);

	manager.PushContext(gameplay);
	manager.PushContext(inventory);
	manager.PushContext(dialog);

	// Create events
	EventData events;

	Event wEvt;
	wEvt.type = Event::EType::kKeyPressed;
	wEvt.key = static_cast<int>(EKey::W);
	events.Add(wEvt);

	Event iEvt;
	iEvt.type = Event::EType::kKeyPressed;
	iEvt.key = static_cast<int>(EKey::I);
	events.Add(iEvt);

	Event enterEvt;
	enterEvt.type = Event::EType::kKeyPressed;
	enterEvt.key = static_cast<int>(EKey::Enter);
	events.Add(enterEvt);

	manager.ProcessEvents(events);

	// Only dialog action should be active (dialog blocks inventory and gameplay)
	EXPECT_TRUE(manager.IsActionActive(dialogAction));
	EXPECT_FALSE(manager.IsActionActive(inventoryAction));
	EXPECT_FALSE(manager.IsActionActive(gameplayAction));
}

////////////////////////////////////////////////////////////////////////////////
// Action Query Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionContextManager, IsActionActiveChecksAllNonBlockedContexts)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
	ActionContext* overlay = manager.CreateContext(ContextID("Overlay"), false);

	ActionID moveAction("Move");

	// Only bind in gameplay context
	gameplay->BindKey(moveAction, EKey::W);

	manager.PushContext(gameplay);
	manager.PushContext(overlay);

	// Create event
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::W);
	events.Add(evt);

	manager.ProcessEvents(events);

	// Should find action in lower (gameplay) context
	EXPECT_TRUE(manager.IsActionActive(moveAction));
}

TEST(ActionContextManager, GetActionValueReturnsFirstNonZeroValue)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
	ActionContext* overlay = manager.CreateContext(ContextID("Overlay"), false);

	ActionID moveAction("Move");

	// Bind in gameplay context
	gameplay->BindKey(moveAction, EKey::W);

	manager.PushContext(gameplay);
	manager.PushContext(overlay);

	// Create event
	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::W);
	events.Add(evt);

	manager.ProcessEvents(events);

	EXPECT_FLOAT_EQ(manager.GetActionValue(moveAction), 1.0f);
}

////////////////////////////////////////////////////////////////////////////////
// Edge Cases
////////////////////////////////////////////////////////////////////////////////

TEST(ActionContextManager, PushNullContextDoesNothing)
{
	ActionContextManager manager;
	manager.PushContext(nullptr);

	EXPECT_EQ(manager.GetStackSize(), 0);
}

TEST(ActionContextManager, ProcessEventsOnEmptyStackDoesNotCrash)
{
	ActionContextManager manager;

	EventData events;
	Event evt;
	evt.type = Event::EType::kKeyPressed;
	evt.key = static_cast<int>(EKey::Space);
	events.Add(evt);

	// Should not crash
	manager.ProcessEvents(events);

	EXPECT_FALSE(manager.IsActionActive(ActionID("Any")));
}

TEST(ActionContextManager, IsActionActiveOnEmptyStackReturnsFalse)
{
	ActionContextManager manager;

	EXPECT_FALSE(manager.IsActionActive(ActionID("Any")));
	EXPECT_FLOAT_EQ(manager.GetActionValue(ActionID("Any")), 0.0f);
}

////////////////////////////////////////////////////////////////////////////////
// Real-World Scenario Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ActionContextManager, GameplayToMenuTransition)
{
	ActionContextManager manager;
	ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
	ActionContext* menu = manager.CreateContext(ContextID("Menu"), true);

	ActionID jumpAction("Jump");
	ActionID pauseAction("Pause");
	ActionID selectAction("Select");

	gameplay->BindKey(jumpAction, EKey::Space);
	gameplay->BindKey(pauseAction, EKey::Escape);
	menu->BindKey(selectAction, EKey::Enter);

	// Start in gameplay
	manager.PushContext(gameplay);

	// Simulate Escape press (pause game)
	EventData pauseEvents;
	Event escapeEvt;
	escapeEvt.type = Event::EType::kKeyPressed;
	escapeEvt.key = static_cast<int>(EKey::Escape);
	pauseEvents.Add(escapeEvt);

	manager.ProcessEvents(pauseEvents);
	EXPECT_TRUE(manager.IsActionActive(pauseAction));

	// Open menu (blocks gameplay)
	manager.PushContext(menu);

	// Try to jump while in menu
	EventData menuEvents;
	Event spaceEvt;
	spaceEvt.type = Event::EType::kKeyPressed;
	spaceEvt.key = static_cast<int>(EKey::Space);
	menuEvents.Add(spaceEvt);

	manager.ProcessEvents(menuEvents);

	// Jump should be blocked
	EXPECT_FALSE(manager.IsActionActive(jumpAction));

	// Close menu
	manager.PopContext();

	// Now jump should work
	manager.ProcessEvents(menuEvents);
	EXPECT_TRUE(manager.IsActionActive(jumpAction));
}
