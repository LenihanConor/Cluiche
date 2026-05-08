# DiaAI API

**Last Updated:** 2026-04-01

**Status:** ⏳ **STUB** - Not yet implemented

AI and pathfinding API (planned).

---

## Overview

**DiaAI** is a planned AI subsystem (not yet implemented).

**Location:** `Dia/DiaAI/` (future)

**Namespace:** `Dia::AI::` (planned)

**Status:** ⏳ **STUB** - Placeholder only

**Planned Features:**
- Pathfinding (A*, Dijkstra)
- Navigation meshes
- Steering behaviors
- Finite state machines (FSM)
- Behavior trees
- Goal-oriented action planning (GOAP)

---

## Planned API

### IPathfinder

**Purpose:** Pathfinding interface

```cpp
// Planned API (not yet implemented)
class IPathfinder
{
public:
    virtual ~IPathfinder() = default;
    
    // Find path
    virtual bool FindPath(
        const Vector2D& start,
        const Vector2D& goal,
        Path& outPath) = 0;
    
    // Query
    virtual bool IsWalkable(const Vector2D& position) const = 0;
    virtual float GetCost(const Vector2D& from, const Vector2D& to) const = 0;
};

struct Path
{
    DynamicArray<Vector2D> waypoints;
    float totalCost;
};
```

---

### INavigationMesh

**Purpose:** Navigation mesh for pathfinding

```cpp
// Planned API (not yet implemented)
class INavigationMesh
{
public:
    virtual ~INavigationMesh() = default;
    
    // Build
    virtual void Build(const Level* level) = 0;
    
    // Query
    virtual bool IsPointInMesh(const Vector2D& point) const = 0;
    virtual Vector2D GetNearestPoint(const Vector2D& point) const = 0;
    
    // Pathfinding
    virtual bool FindPath(
        const Vector2D& start,
        const Vector2D& goal,
        Path& outPath) = 0;
};
```

---

### ISteeringBehavior

**Purpose:** Steering behavior interface (movement AI)

```cpp
// Planned API (not yet implemented)
class ISteeringBehavior
{
public:
    virtual ~ISteeringBehavior() = default;
    
    // Calculate steering force
    virtual Vector2D CalculateSteering(
        const Vector2D& position,
        const Vector2D& velocity,
        float maxSpeed) = 0;
};

// Concrete behaviors
class SeekBehavior : public ISteeringBehavior
{
public:
    Vector2D target;
    Vector2D CalculateSteering(...) override;
};

class FleeBehavior : public ISteeringBehavior
{
public:
    Vector2D threat;
    Vector2D CalculateSteering(...) override;
};

class WanderBehavior : public ISteeringBehavior
{
public:
    float wanderRadius;
    Vector2D CalculateSteering(...) override;
};
```

---

### IFiniteStateMachine

**Purpose:** FSM for AI logic

```cpp
// Planned API (not yet implemented)
class IFiniteStateMachine
{
public:
    virtual ~IFiniteStateMachine() = default;
    
    // State management
    virtual void AddState(IState* state) = 0;
    virtual void SetInitialState(IState* state) = 0;
    virtual void TransitionTo(IState* state) = 0;
    
    // Update
    virtual void Update(float deltaTime) = 0;
    
    // Query
    virtual IState* GetCurrentState() const = 0;
};

class IState
{
public:
    virtual ~IState() = default;
    
    virtual void OnEnter() = 0;
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnExit() = 0;
    
    virtual IState* CheckTransitions() = 0;
};
```

---

### IBehaviorTree

**Purpose:** Behavior tree for complex AI

```cpp
// Planned API (not yet implemented)
class IBehaviorTree
{
public:
    enum NodeStatus
    {
        Success,
        Failure,
        Running
    };
    
    virtual ~IBehaviorTree() = default;
    
    // Build tree
    virtual void SetRoot(IBehaviorNode* root) = 0;
    
    // Execute
    virtual NodeStatus Tick(float deltaTime) = 0;
};

class IBehaviorNode
{
public:
    virtual ~IBehaviorNode() = default;
    
    virtual NodeStatus Execute(float deltaTime) = 0;
};

// Node types
class SequenceNode : public IBehaviorNode { /* ... */ };
class SelectorNode : public IBehaviorNode { /* ... */ };
class ActionNode : public IBehaviorNode { /* ... */ };
class ConditionNode : public IBehaviorNode { /* ... */ };
```

---

## Planned Usage

### Pathfinding

```cpp
// Planned usage (not yet implemented)
Dia::AI::IPathfinder* pathfinder = CreatePathfinder();

// Find path
Vector2D start(0.0f, 0.0f);
Vector2D goal(100.0f, 100.0f);
Dia::AI::Path path;

if (pathfinder->FindPath(start, goal, path))
{
    // Follow waypoints
    for (const Vector2D& waypoint : path.waypoints)
    {
        MoveTowards(waypoint);
    }
}
```

---

### Steering Behaviors

```cpp
// Planned usage (not yet implemented)
Dia::AI::SeekBehavior seek;
seek.target = playerPosition;

Vector2D steering = seek.CalculateSteering(
    enemyPosition,
    enemyVelocity,
    enemyMaxSpeed);

// Apply steering force
enemyVelocity += steering * deltaTime;
enemyPosition += enemyVelocity * deltaTime;
```

---

### Finite State Machine

```cpp
// Planned usage (not yet implemented)
class IdleState : public Dia::AI::IState
{
public:
    void OnEnter() override { /* Start idle animation */ }
    void OnUpdate(float deltaTime) override { /* Check for threats */ }
    void OnExit() override { /* Stop idle animation */ }
    
    IState* CheckTransitions() override
    {
        if (PlayerNearby())
        {
            return mChaseState;
        }
        return nullptr;  // Stay in idle
    }
};

class ChaseState : public Dia::AI::IState
{
public:
    void OnEnter() override { /* Start chase animation */ }
    void OnUpdate(float deltaTime) override { /* Chase player */ }
    void OnExit() override { /* Stop chase animation */ }
    
    IState* CheckTransitions() override
    {
        if (!PlayerNearby())
        {
            return mIdleState;
        }
        if (PlayerInAttackRange())
        {
            return mAttackState;
        }
        return nullptr;  // Stay in chase
    }
};

// Create FSM
Dia::AI::IFiniteStateMachine* fsm = CreateFSM();
fsm->AddState(new IdleState());
fsm->AddState(new ChaseState());
fsm->SetInitialState(idleState);

// Update each frame
fsm->Update(deltaTime);
```

---

### Behavior Tree

```cpp
// Planned usage (not yet implemented)
// Build behavior tree
auto root = new SequenceNode();
root->AddChild(new ConditionNode("IsPlayerVisible"));
root->AddChild(new SelectorNode(
    new ActionNode("Attack"),
    new ActionNode("ChasePlayer")
));

Dia::AI::IBehaviorTree* tree = CreateBehaviorTree();
tree->SetRoot(root);

// Update each frame
Dia::AI::IBehaviorTree::NodeStatus status = tree->Tick(deltaTime);
```

---

## Backend Options

### Option 1: Custom Implementation

**Pros:**
- Full control
- Lightweight
- Tailored to needs

**Cons:**
- Time to implement
- More testing needed

---

### Option 2: Recast & Detour

**Pros:**
- Industry standard navigation
- Excellent navmesh generation
- Well-tested
- MIT license (free)

**Cons:**
- 3D focused (but works for 2D)
- C API requires wrapper

**[→ Recast & Detour](https://github.com/recastnavigation/recastnavigation)**

---

### Option 3: MicroPather

**Pros:**
- Lightweight A* implementation
- Easy integration
- MIT license

**Cons:**
- A* only (no navmesh, steering)
- Small community

**[→ MicroPather](http://www.grinninglizard.com/MicroPather/)**

---

## Implementation Plan

### Phase 1: Pathfinding
- Implement A* algorithm
- Grid-based pathfinding
- Path smoothing
- Dynamic obstacles

### Phase 2: Navigation Mesh
- Navmesh generation
- Navmesh pathfinding
- Dynamic navmesh updates

### Phase 3: Steering Behaviors
- Seek, flee, wander
- Arrival, pursuit, evasion
- Obstacle avoidance
- Flocking (separation, alignment, cohesion)

### Phase 4: FSM
- State management
- Transition system
- State stack (for push/pop behavior)

### Phase 5: Behavior Trees
- Node types (sequence, selector, parallel)
- Actions and conditions
- Blackboard (shared data)

### Phase 6: Advanced AI
- Goal-oriented action planning (GOAP)
- Utility-based AI
- Machine learning integration

---

## Requirements

**From requirements.md:**

**DE-011: AI API (P2 - Medium)**
- **Status:** ❌ Not Started
- **Description:** AI and pathfinding utilities
- **Priority:** P2 (Medium) - Not critical for initial release

**Target Features:**
- A* pathfinding (minimum)
- Navigation mesh (nice to have)
- Steering behaviors (seek, flee, wander)
- FSM or Behavior Tree (one or both)

**[→ Requirements](../../requirements-as-built/requirements.md)**

---

## Current Status

**Not Implemented:**
- No AI subsystem exists
- No pathfinding implementation
- No steering behaviors
- No FSM or behavior trees

**Workarounds:**
- Manual pathfinding (straight-line movement)
- Manual state management (switch statements)
- Manual steering (direct velocity manipulation)

**Example (Manual AI):**
```cpp
// Current workaround (manual AI)
enum EnemyState
{
    Idle,
    Chase,
    Attack
};

class Enemy
{
public:
    void Update(float deltaTime)
    {
        switch (mState)
        {
            case Idle:
                if (PlayerNearby())
                {
                    mState = Chase;
                }
                break;
                
            case Chase:
            {
                // Manual pathfinding (straight line)
                Vector2D direction = (playerPosition - mPosition).Normalize();
                mPosition += direction * mSpeed * deltaTime;
                
                if (PlayerInAttackRange())
                {
                    mState = Attack;
                }
                else if (!PlayerNearby())
                {
                    mState = Idle;
                }
                break;
            }
            
            case Attack:
                AttackPlayer();
                if (!PlayerInAttackRange())
                {
                    mState = Chase;
                }
                break;
        }
    }
    
private:
    EnemyState mState = Idle;
    Vector2D mPosition;
    float mSpeed = 100.0f;
};
```

---

## Common AI Patterns

### Pathfinding Pattern

```cpp
// Planned pattern (not yet implemented)
class AIController
{
public:
    void MoveTo(const Vector2D& destination)
    {
        // Find path
        Path path;
        if (mPathfinder->FindPath(mPosition, destination, path))
        {
            mCurrentPath = path;
            mCurrentWaypointIndex = 0;
        }
    }
    
    void Update(float deltaTime)
    {
        if (mCurrentPath.waypoints.IsEmpty())
        {
            return;
        }
        
        // Follow current waypoint
        Vector2D target = mCurrentPath.waypoints[mCurrentWaypointIndex];
        Vector2D direction = (target - mPosition).Normalize();
        mPosition += direction * mSpeed * deltaTime;
        
        // Check if reached waypoint
        if (Distance(mPosition, target) < mWaypointRadius)
        {
            mCurrentWaypointIndex++;
            
            if (mCurrentWaypointIndex >= mCurrentPath.waypoints.Size())
            {
                // Reached destination
                mCurrentPath.waypoints.Clear();
            }
        }
    }
    
private:
    IPathfinder* mPathfinder;
    Path mCurrentPath;
    int mCurrentWaypointIndex;
    Vector2D mPosition;
    float mSpeed;
    float mWaypointRadius = 5.0f;
};
```

---

### Steering Combination Pattern

```cpp
// Planned pattern (not yet implemented)
class SteeringController
{
public:
    void AddBehavior(ISteeringBehavior* behavior, float weight)
    {
        mBehaviors.push_back({behavior, weight});
    }
    
    Vector2D CalculateSteering()
    {
        Vector2D totalSteering(0.0f, 0.0f);
        
        for (const auto& pair : mBehaviors)
        {
            Vector2D steering = pair.behavior->CalculateSteering(
                mPosition,
                mVelocity,
                mMaxSpeed);
            
            totalSteering += steering * pair.weight;
        }
        
        // Clamp to max force
        if (totalSteering.Magnitude() > mMaxForce)
        {
            totalSteering = totalSteering.Normalize() * mMaxForce;
        }
        
        return totalSteering;
    }
    
private:
    struct BehaviorWeight
    {
        ISteeringBehavior* behavior;
        float weight;
    };
    
    std::vector<BehaviorWeight> mBehaviors;
    Vector2D mPosition;
    Vector2D mVelocity;
    float mMaxSpeed;
    float mMaxForce;
};
```

---

## Summary

**Status:**
- ⏳ **STUB** - Not yet implemented

**Planned Features:**
- Pathfinding (A*, Dijkstra)
- Navigation meshes
- Steering behaviors
- FSM and Behavior Trees
- GOAP (advanced)

**Backend Options:**
- Custom implementation
- Recast & Detour (navmesh)
- MicroPather (A*)

**Implementation Plan:**
- Phase 1: Pathfinding
- Phase 2: Navigation mesh
- Phase 3: Steering behaviors
- Phase 4: FSM
- Phase 5: Behavior trees
- Phase 6: Advanced AI

**Current Workaround:**
- Manual pathfinding (straight line)
- Manual state management (switch statements)
- Manual steering (direct velocity)

**Priority:**
- P2 (Medium) - Not critical for initial release
- Can implement incrementally as needed

**Recommended Starting Point:**
- Implement A* pathfinding first (most useful)
- Add basic steering behaviors (seek, flee)
- Add simple FSM
- Expand to navmesh and behavior trees later

**[→ API Overview](../api-overview.md)**  
**[→ DiaMaths API](maths-api.md)**  
**[→ Requirements](../../requirements-as-built/requirements.md)**  
**[→ Future Directions](../../design-rationale/future-directions.md)**
