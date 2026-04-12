# DiaPhysics API

**Last Updated:** 2026-04-01

**Status:** ⏳ **STUB** - Not yet implemented

Physics simulation API (planned).

---

## Overview

**DiaPhysics** is a planned physics simulation subsystem (not yet implemented).

**Location:** `Dia/DiaPhysics/` (future)

**Namespace:** `Dia::Physics::` (planned)

**Status:** ⏳ **STUB** - Placeholder only

**Planned Features:**
- Rigid body dynamics
- Collision detection
- Collision response
- Physics constraints (joints, springs)
- 2D and 3D physics

---

## Planned API

### IPhysicsWorld

**Purpose:** Abstract physics world interface

```cpp
// Planned API (not yet implemented)
class IPhysicsWorld
{
public:
    virtual ~IPhysicsWorld() = default;
    
    // Simulation
    virtual void Step(float deltaTime) = 0;
    
    // Bodies
    virtual IPhysicsBody* CreateBody(const BodyDef& def) = 0;
    virtual void DestroyBody(IPhysicsBody* body) = 0;
    
    // Collision
    virtual void SetCollisionListener(ICollisionListener* listener) = 0;
    
    // Gravity
    virtual void SetGravity(const Vector2D& gravity) = 0;
    virtual Vector2D GetGravity() const = 0;
};
```

---

### IPhysicsBody

**Purpose:** Rigid body interface

```cpp
// Planned API (not yet implemented)
class IPhysicsBody
{
public:
    enum BodyType
    {
        Static,     // Fixed, non-moving
        Kinematic,  // User-controlled
        Dynamic     // Physics-controlled
    };
    
    virtual ~IPhysicsBody() = default;
    
    // Transform
    virtual void SetPosition(const Vector2D& position) = 0;
    virtual Vector2D GetPosition() const = 0;
    
    virtual void SetRotation(float radians) = 0;
    virtual float GetRotation() const = 0;
    
    // Velocity
    virtual void SetVelocity(const Vector2D& velocity) = 0;
    virtual Vector2D GetVelocity() const = 0;
    
    virtual void SetAngularVelocity(float velocity) = 0;
    virtual float GetAngularVelocity() const = 0;
    
    // Forces
    virtual void ApplyForce(const Vector2D& force) = 0;
    virtual void ApplyImpulse(const Vector2D& impulse) = 0;
    virtual void ApplyTorque(float torque) = 0;
    
    // Properties
    virtual void SetMass(float mass) = 0;
    virtual float GetMass() const = 0;
    
    virtual void SetFriction(float friction) = 0;
    virtual float GetFriction() const = 0;
    
    virtual void SetRestitution(float restitution) = 0;
    virtual float GetRestitution() const = 0;
    
    // Type
    virtual void SetType(BodyType type) = 0;
    virtual BodyType GetType() const = 0;
};
```

---

### ICollisionShape

**Purpose:** Collision shape interface

```cpp
// Planned API (not yet implemented)
class ICollisionShape
{
public:
    enum ShapeType
    {
        Circle,
        Box,
        Polygon,
        Edge
    };
    
    virtual ~ICollisionShape() = default;
    
    virtual ShapeType GetType() const = 0;
};

class CircleShape : public ICollisionShape
{
public:
    float radius;
    Vector2D center;
};

class BoxShape : public ICollisionShape
{
public:
    Vector2D halfExtents;
};
```

---

### ICollisionListener

**Purpose:** Collision event callback interface

```cpp
// Planned API (not yet implemented)
class ICollisionListener
{
public:
    virtual ~ICollisionListener() = default;
    
    virtual void OnCollisionBegin(IPhysicsBody* bodyA, IPhysicsBody* bodyB) = 0;
    virtual void OnCollisionEnd(IPhysicsBody* bodyA, IPhysicsBody* bodyB) = 0;
};
```

---

## Planned Usage

### Creating Physics World

```cpp
// Planned usage (not yet implemented)
Dia::Physics::IPhysicsWorld* world = CreatePhysicsWorld();

// Set gravity
world->SetGravity(Dia::Maths::Vector2D(0.0f, 9.8f));

// Step simulation
world->Step(deltaTime);
```

---

### Creating Rigid Bodies

```cpp
// Planned usage (not yet implemented)
Dia::Physics::BodyDef bodyDef;
bodyDef.type = IPhysicsBody::Dynamic;
bodyDef.position = Dia::Maths::Vector2D(0.0f, 10.0f);
bodyDef.mass = 1.0f;

Dia::Physics::IPhysicsBody* body = world->CreateBody(bodyDef);

// Apply force
body->ApplyForce(Dia::Maths::Vector2D(10.0f, 0.0f));

// Get position
Dia::Maths::Vector2D position = body->GetPosition();
```

---

### Collision Handling

```cpp
// Planned usage (not yet implemented)
class GameCollisionListener : public Dia::Physics::ICollisionListener
{
public:
    void OnCollisionBegin(IPhysicsBody* bodyA, IPhysicsBody* bodyB) override
    {
        // Handle collision start
        OnPlayerHit(bodyA, bodyB);
    }
    
    void OnCollisionEnd(IPhysicsBody* bodyA, IPhysicsBody* bodyB) override
    {
        // Handle collision end
    }
};

// Register listener
world->SetCollisionListener(new GameCollisionListener());
```

---

## Backend Options

### Option 1: Box2D

**Pros:**
- Mature, well-tested
- 2D physics
- Good performance
- MIT license (free)

**Cons:**
- 2D only
- C API requires wrapper

**[→ Box2D](https://box2d.org/)**

---

### Option 2: Bullet Physics

**Pros:**
- 3D physics
- Feature-rich
- Open source (zlib license)
- Used in many games

**Cons:**
- Heavier than Box2D
- More complex API
- 2D support limited

**[→ Bullet Physics](https://pybullet.org/wordpress/)**

---

### Option 3: PhysX

**Pros:**
- Industry standard (NVIDIA)
- 3D physics
- GPU acceleration
- High performance

**Cons:**
- Large dependency
- Complex integration
- BSD license (restrictions)

**[→ PhysX](https://developer.nvidia.com/physx-sdk)**

---

### Option 4: Chipmunk2D

**Pros:**
- Lightweight
- 2D focused
- Good performance
- MIT license

**Cons:**
- 2D only
- Smaller community than Box2D

**[→ Chipmunk2D](https://chipmunk-physics.net/)**

---

## Implementation Plan

### Phase 1: Interface Design
- Define IPhysicsWorld, IPhysicsBody, ICollisionShape
- Define collision callbacks
- Review API with team

### Phase 2: Backend Selection
- Evaluate Box2D vs Bullet vs PhysX
- Prototype integration
- Performance testing

### Phase 3: Backend Integration
- Implement Dia wrapper around chosen backend
- Conversion functions (Dia types ↔ backend types)
- Unit tests for physics operations

### Phase 4: Features
- Rigid body dynamics
- Collision detection
- Collision response
- Constraints (joints, springs, motors)

### Phase 5: Optimization
- Spatial partitioning (broadphase)
- Sleep/wake system
- Multi-threading (if supported)

### Phase 6: Tools
- Physics debug rendering
- Performance profiling
- Editor integration

---

## Requirements

**From requirements.md:**

**DE-010: Physics API (P2 - Medium)**
- **Status:** ❌ Not Started
- **Description:** Physics simulation with collision detection
- **Priority:** P2 (Medium) - Not critical for initial release

**Target Features:**
- 2D rigid body physics (minimum)
- 3D physics (nice to have)
- Collision detection (circles, boxes, polygons)
- Collision response (elastic, inelastic)
- Constraints (distance, revolute, prismatic)
- Physics debug rendering

**[→ Requirements](../../03-requirements/requirements.md)**

---

## Current Status

**Not Implemented:**
- No physics subsystem exists
- No physics backend integrated
- No physics module in build system

**Workarounds:**
- Manual collision detection using DiaMaths shapes (Circle, AABB)
- Manual collision response
- No automatic physics simulation

**Example (Manual Collision):**
```cpp
// Current workaround (manual collision)
Dia::Maths::Circle circleA(posA, radiusA);
Dia::Maths::Circle circleB(posB, radiusB);

if (circleA.Intersects(circleB))
{
    // Manual collision response
    Dia::Maths::Vector2D direction = (posB - posA).Normalize();
    float overlap = (radiusA + radiusB) - Dia::Maths::Distance(posA, posB);
    
    posA -= direction * overlap * 0.5f;
    posB += direction * overlap * 0.5f;
}
```

---

## Summary

**Status:**
- ⏳ **STUB** - Not yet implemented

**Planned Features:**
- Rigid body dynamics
- Collision detection and response
- Physics constraints
- 2D/3D support

**Backend Options:**
- Box2D (2D, lightweight)
- Bullet Physics (3D, feature-rich)
- PhysX (3D, high-performance)
- Chipmunk2D (2D, lightweight)

**Implementation Plan:**
- Phase 1: Interface design
- Phase 2: Backend selection
- Phase 3: Integration
- Phase 4: Features
- Phase 5: Optimization
- Phase 6: Tools

**Current Workaround:**
- Manual collision detection with DiaMaths
- Manual collision response

**Priority:**
- P2 (Medium) - Not critical for initial release
- Can implement later when needed

**[→ API Overview](../api-overview.md)**  
**[→ DiaMaths API](maths-api.md)**  
**[→ Requirements](../../03-requirements/requirements.md)**  
**[→ Future Directions](../../02-design/future-directions.md)**
