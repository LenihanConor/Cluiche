# DiaMaths API

**Last Updated:** 2026-04-01

Math library API providing vectors, matrices, transforms, shapes, and math utilities.

---

## Overview

**DiaMaths** provides comprehensive 2D/3D math operations for game development.

**Location:** `Dia/DiaMaths/`

**Namespace:** `Dia::Maths::`

**Key Components:**
- **Vectors** - Vector2D, Vector3D, Vector4D
- **Matrices** - Matrix22, Matrix33, Matrix44
- **Transforms** - Transform2D, Transform3D (hierarchical)
- **Shapes** - Circle, AABB, Line, Polygon
- **Utilities** - Random, interpolation, float math

---

## Vectors

### Vector2D

**Header:** `Dia/DiaMaths/Vector/Vector2D.h`

**Purpose:** 2D vector (x, y)

#### Key Members

```cpp
struct Vector2D
{
    float x, y;
    
    // Constructors
    Vector2D();
    Vector2D(float x, float y);
    
    // Arithmetic
    Vector2D operator+(const Vector2D& other) const;
    Vector2D operator-(const Vector2D& other) const;
    Vector2D operator*(float scalar) const;
    Vector2D operator/(float scalar) const;
    
    // Compound assignment
    Vector2D& operator+=(const Vector2D& other);
    Vector2D& operator-=(const Vector2D& other);
    Vector2D& operator*=(float scalar);
    Vector2D& operator/=(float scalar);
    
    // Comparison
    bool operator==(const Vector2D& other) const;
    bool operator!=(const Vector2D& other) const;
    
    // Operations
    float Magnitude() const;
    float MagnitudeSquared() const;
    Vector2D Normalize() const;
    void NormalizeInPlace();
};
```

#### Free Functions

```cpp
// Dot product
float Dot(const Vector2D& a, const Vector2D& b);

// Cross product (2D returns scalar)
float Cross(const Vector2D& a, const Vector2D& b);

// Distance
float Distance(const Vector2D& a, const Vector2D& b);
float DistanceSquared(const Vector2D& a, const Vector2D& b);

// Lerp
Vector2D Lerp(const Vector2D& a, const Vector2D& b, float t);

// Perpendicular
Vector2D Perpendicular(const Vector2D& v);
```

#### Usage Example

```cpp
using namespace Dia::Maths;

// Create vectors
Vector2D a(1.0f, 2.0f);
Vector2D b(3.0f, 4.0f);

// Arithmetic
Vector2D sum = a + b;           // (4, 6)
Vector2D diff = a - b;          // (-2, -2)
Vector2D scaled = a * 2.0f;     // (2, 4)

// Magnitude
float length = a.Magnitude();                    // ~2.236
float lengthSq = a.MagnitudeSquared();          // 5.0

// Normalize
Vector2D normalized = a.Normalize();             // (0.447, 0.894)
a.NormalizeInPlace();                           // Modifies a

// Dot product
float dot = Dot(a, b);

// Distance
float dist = Distance(a, b);

// Lerp (linear interpolation)
Vector2D mid = Lerp(a, b, 0.5f);  // Halfway between a and b
```

---

### Vector3D

**Header:** `Dia/DiaMaths/Vector/Vector3D.h`

**Purpose:** 3D vector (x, y, z)

#### Key Members

```cpp
struct Vector3D
{
    float x, y, z;
    
    Vector3D();
    Vector3D(float x, float y, float z);
    
    // Same operators as Vector2D
    // ...
    
    float Magnitude() const;
    Vector3D Normalize() const;
};
```

#### Free Functions

```cpp
float Dot(const Vector3D& a, const Vector3D& b);
Vector3D Cross(const Vector3D& a, const Vector3D& b);  // Returns vector
float Distance(const Vector3D& a, const Vector3D& b);
Vector3D Lerp(const Vector3D& a, const Vector3D& b, float t);
```

#### Usage Example

```cpp
Vector3D a(1.0f, 0.0f, 0.0f);
Vector3D b(0.0f, 1.0f, 0.0f);

// Cross product (perpendicular vector)
Vector3D perpendicular = Cross(a, b);  // (0, 0, 1)
```

---

### Vector4D

**Header:** `Dia/DiaMaths/Vector/Vector4D.h`

**Purpose:** 4D vector (x, y, z, w) - for homogeneous coordinates

---

## Matrices

### Matrix33

**Header:** `Dia/DiaMaths/Matrix/Matrix33.h`

**Purpose:** 3x3 matrix (2D transformations)

#### Key Methods

```cpp
class Matrix33
{
public:
    Matrix33();
    
    // Access
    float& operator()(unsigned int row, unsigned int col);
    float operator()(unsigned int row, unsigned int col) const;
    
    // Operations
    Matrix33 operator*(const Matrix33& other) const;
    Vector2D operator*(const Vector2D& vec) const;
    
    // Transpose, Inverse
    Matrix33 Transpose() const;
    Matrix33 Inverse() const;
    
    // Factory methods
    static Matrix33 Identity();
    static Matrix33 Translation(float x, float y);
    static Matrix33 Rotation(float radians);
    static Matrix33 Scale(float sx, float sy);
};
```

#### Usage Example

```cpp
using namespace Dia::Maths;

// Identity matrix
Matrix33 identity = Matrix33::Identity();

// Translation matrix
Matrix33 translation = Matrix33::Translation(10.0f, 20.0f);

// Rotation matrix (90 degrees = π/2 radians)
Matrix33 rotation = Matrix33::Rotation(3.14159f / 2.0f);

// Scale matrix
Matrix33 scale = Matrix33::Scale(2.0f, 2.0f);

// Combine transformations (right-to-left)
Matrix33 transform = translation * rotation * scale;

// Transform vector
Vector2D point(1.0f, 0.0f);
Vector2D transformed = transform * point;
```

---

### Matrix44

**Header:** `Dia/DiaMaths/Matrix/Matrix44.h`

**Purpose:** 4x4 matrix (3D transformations)

#### Key Methods

```cpp
class Matrix44
{
public:
    Matrix44();
    
    // Operations
    Matrix44 operator*(const Matrix44& other) const;
    Vector3D operator*(const Vector3D& vec) const;
    
    // Factory methods
    static Matrix44 Identity();
    static Matrix44 Translation(float x, float y, float z);
    static Matrix44 RotationX(float radians);
    static Matrix44 RotationY(float radians);
    static Matrix44 RotationZ(float radians);
    static Matrix44 Scale(float sx, float sy, float sz);
    static Matrix44 LookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& up);
    static Matrix44 Perspective(float fovY, float aspect, float near, float far);
};
```

---

### Matrix22

**Header:** `Dia/DiaMaths/Matrix/Matrix22.h`

**Purpose:** 2x2 matrix (simple 2D rotations)

---

## Transforms

### Transform2D

**Header:** `Dia/DiaMaths/Transform/Transform2D.h`

**Purpose:** 2D transform with parent/child hierarchy

#### Key Methods

```cpp
class Transform2D
{
public:
    Transform2D();
    
    // Position
    void SetPosition(const Vector2D& position);
    Vector2D GetPosition() const;
    Vector2D GetWorldPosition() const;  // Includes parents
    
    // Rotation
    void SetRotation(float radians);
    float GetRotation() const;
    float GetWorldRotation() const;
    
    // Scale
    void SetScale(const Vector2D& scale);
    Vector2D GetScale() const;
    Vector2D GetWorldScale() const;
    
    // Hierarchy
    void SetParent(Transform2D* parent);
    Transform2D* GetParent() const;
    
    // Matrices
    Matrix33 GetLocalMatrix() const;
    Matrix33 GetWorldMatrix() const;  // Traverses hierarchy
};
```

#### Usage Example

```cpp
using namespace Dia::Maths;

// Create transforms
Transform2D parent;
Transform2D child;

// Set up parent
parent.SetPosition(Vector2D(10.0f, 10.0f));
parent.SetRotation(0.785f);  // 45 degrees

// Set up child
child.SetPosition(Vector2D(5.0f, 0.0f));  // 5 units to right of parent
child.SetParent(&parent);

// Get world position (parent + child)
Vector2D worldPos = child.GetWorldPosition();  // ~(10+3.5, 10+3.5)

// Get world matrix (parent * child)
Matrix33 worldMatrix = child.GetWorldMatrix();
```

#### Known Issue

⚠️ **Performance:** `GetWorldMatrix()` traverses hierarchy multiple times. No caching implemented.

**Workaround:** Cache world matrix if calling frequently, or optimize hierarchy traversal.

---

### Transform3D

**Header:** `Dia/DiaMaths/Transform/Transform3D.h`

**Purpose:** 3D transform with hierarchy (similar to Transform2D)

---

## Shapes

### Circle

**Header:** `Dia/DiaMaths/Shape/Circle.h`

**Purpose:** Circle shape for collision/intersection

#### Key Members

```cpp
class Circle
{
public:
    Vector2D center;
    float radius;
    
    Circle();
    Circle(const Vector2D& center, float radius);
    
    // Intersection
    bool Intersects(const Circle& other) const;
    bool Contains(const Vector2D& point) const;
};
```

#### Usage Example

```cpp
Circle a(Vector2D(0.0f, 0.0f), 5.0f);
Circle b(Vector2D(8.0f, 0.0f), 5.0f);

if (a.Intersects(b))
{
    // Circles overlap
}

Vector2D point(2.0f, 2.0f);
if (a.Contains(point))
{
    // Point inside circle
}
```

---

### AABB

**Header:** `Dia/DiaMaths/Shape/AABB.h`

**Purpose:** Axis-aligned bounding box

#### Key Members

```cpp
class AABB
{
public:
    Vector2D min;
    Vector2D max;
    
    AABB();
    AABB(const Vector2D& min, const Vector2D& max);
    
    // Intersection
    bool Intersects(const AABB& other) const;
    bool Contains(const Vector2D& point) const;
    
    // Properties
    Vector2D GetCenter() const;
    Vector2D GetSize() const;
};
```

#### Usage Example

```cpp
AABB box1(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
AABB box2(Vector2D(5.0f, 5.0f), Vector2D(15.0f, 15.0f));

if (box1.Intersects(box2))
{
    // Boxes overlap
}

Vector2D center = box1.GetCenter();  // (5, 5)
Vector2D size = box1.GetSize();      // (10, 10)
```

---

### Line

**Header:** `Dia/DiaMaths/Shape/Line.h`

**Purpose:** Line segment

#### Key Members

```cpp
class Line
{
public:
    Vector2D start;
    Vector2D end;
    
    Line();
    Line(const Vector2D& start, const Vector2D& end);
    
    float Length() const;
    Vector2D GetDirection() const;
};
```

---

### Polygon

**Header:** `Dia/DiaMaths/Shape/Polygon.h`

**Purpose:** Convex polygon

---

## Math Utilities

### Random

**Header:** `Dia/DiaMaths/Core/Random.h`

**Purpose:** Thread-safe random number generation

#### Key Methods

```cpp
class Random
{
public:
    Random();
    Random(unsigned int seed);
    
    // Random values
    float RandomFloat();                      // [0, 1)
    float RandomFloat(float min, float max);  // [min, max)
    int RandomInt(int min, int max);          // [min, max]
    bool RandomBool();
    
    // Seed
    void SetSeed(unsigned int seed);
};
```

#### Usage Example

```cpp
Dia::Maths::Random rng;

// Random float [0, 1)
float value = rng.RandomFloat();

// Random float [10, 20)
float value = rng.RandomFloat(10.0f, 20.0f);

// Random int [1, 6] (dice roll)
int roll = rng.RandomInt(1, 6);

// Random bool (coin flip)
bool heads = rng.RandomBool();

// Custom seed
rng.SetSeed(12345);
```

#### Thread Safety

✅ **Thread-safe** (uses `std::mutex` internally)

---

### Interpolation

**Header:** `Dia/DiaMaths/Core/Interpolation.h`

**Purpose:** Interpolation and easing functions

#### Key Functions

```cpp
// Linear interpolation
float Lerp(float a, float b, float t);
Vector2D Lerp(const Vector2D& a, const Vector2D& b, float t);
Vector3D Lerp(const Vector3D& a, const Vector3D& b, float t);

// Inverse lerp (find t given value)
float InverseLerp(float a, float b, float value);

// Move towards
float MoveTowards(float current, float target, float maxDelta);
Vector2D MoveTowards(const Vector2D& current, const Vector2D& target, float maxDelta);

// Clamp
float Clamp(float value, float min, float max);
Vector2D Clamp(const Vector2D& value, const Vector2D& min, const Vector2D& max);

// Smooth step
float SmoothStep(float t);  // Cubic Hermite
```

#### Usage Example

```cpp
// Linear interpolation
float a = 0.0f, b = 10.0f;
float mid = Lerp(a, b, 0.5f);  // 5.0

// Inverse lerp
float t = InverseLerp(0.0f, 10.0f, 5.0f);  // 0.5

// Move towards (max speed)
float current = 0.0f;
float target = 10.0f;
float result = MoveTowards(current, target, 1.0f);  // 1.0 (moved 1 unit)

// Clamp
float value = Clamp(15.0f, 0.0f, 10.0f);  // 10.0

// Smooth step (ease in/out)
float eased = SmoothStep(0.5f);  // ~0.5 (smooth curve)
```

#### Known Issue

⚠️ **Template Bug:** `InverseLerp(Vector2D)` missing, `MoveTowards(Vector2D)` returns wrong type

**Status:** Documented in BUG_REPORT.md, fix planned

---

### FloatMaths

**Header:** `Dia/DiaMaths/Core/FloatMaths.h`

**Purpose:** Float utilities

#### Key Functions

```cpp
// Constants
constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kHalfPi = 1.57079632679f;

// Conversions
float DegreesToRadians(float degrees);
float RadiansToDegrees(float radians);

// Comparison
bool FloatEquals(float a, float b, float epsilon = 0.0001f);

// Math
float Abs(float value);
float Min(float a, float b);
float Max(float a, float b);
float Sqrt(float value);
float Sin(float radians);
float Cos(float radians);
float Tan(float radians);
float Atan2(float y, float x);
```

---

## Common Patterns

### Transform Hierarchy

```cpp
// Setup hierarchy
Transform2D root;
Transform2D child1;
Transform2D child2;

child1.SetParent(&root);
child2.SetParent(&child1);

// Set local transforms
root.SetPosition(Vector2D(10.0f, 10.0f));
child1.SetPosition(Vector2D(5.0f, 0.0f));
child2.SetPosition(Vector2D(0.0f, 5.0f));

// Get world position (root + child1 + child2)
Vector2D worldPos = child2.GetWorldPosition();
```

---

### Matrix Transformations

```cpp
// Build transformation matrix
Matrix33 transform = 
    Matrix33::Translation(10.0f, 20.0f) *
    Matrix33::Rotation(0.785f) *
    Matrix33::Scale(2.0f, 2.0f);

// Apply to vector
Vector2D point(1.0f, 0.0f);
Vector2D transformed = transform * point;
```

---

### Circle-Circle Collision

```cpp
Circle a(Vector2D(0.0f, 0.0f), 5.0f);
Circle b(Vector2D(8.0f, 0.0f), 5.0f);

if (a.Intersects(b))
{
    // Collision response
    Vector2D direction = (b.center - a.center).Normalize();
    float overlap = (a.radius + b.radius) - Distance(a.center, b.center);
    
    // Separate circles
    a.center -= direction * overlap * 0.5f;
    b.center += direction * overlap * 0.5f;
}
```

---

### Interpolation Over Time

```cpp
float startTime = 0.0f;
float duration = 2.0f;
Vector2D startPos(0.0f, 0.0f);
Vector2D endPos(100.0f, 100.0f);

void Update(float currentTime)
{
    float t = (currentTime - startTime) / duration;
    t = Clamp(t, 0.0f, 1.0f);
    
    Vector2D currentPos = Lerp(startPos, endPos, t);
    
    // Smooth easing
    float smoothT = SmoothStep(t);
    Vector2D smoothPos = Lerp(startPos, endPos, smoothT);
}
```

---

## Dependencies

**Required:**
- `Dia/DiaCore/` - Assert utilities

**Standard Library:**
- `<cmath>` - sin, cos, sqrt, etc.

---

## Thread Safety

| Class/Function | Thread Safety |
|----------------|---------------|
| `Vector2D/3D/4D` | ✅ Safe if immutable |
| `Matrix33/44` | ✅ Safe if immutable |
| `Transform2D` | ❌ Hierarchy not thread-safe |
| `Random` | ✅ Thread-safe (mutex-protected) |
| Free functions | ✅ Thread-safe (pure functions) |

---

## Best Practices

### 1. Use Const References for Large Types

```cpp
// ✅ Good
void ProcessTransform(const Matrix44& matrix);

// ❌ Bad: Unnecessary copy
void ProcessTransform(Matrix44 matrix);
```

---

### 2. Prefer Squared Distance for Comparisons

```cpp
// ✅ Good: Avoids sqrt
float distSq = DistanceSquared(a, b);
if (distSq < radiusSq)
{
    // In range
}

// ❌ Bad: Unnecessary sqrt
float dist = Distance(a, b);
if (dist < radius)
{
    // In range
}
```

---

### 3. Cache World Transforms if Queried Frequently

```cpp
// ✅ Good: Cache if querying multiple times per frame
Matrix33 cachedWorld = transform.GetWorldMatrix();
for (int i = 0; i < 1000; ++i)
{
    Vector2D transformed = cachedWorld * points[i];
}

// ❌ Bad: Traverses hierarchy 1000 times
for (int i = 0; i < 1000; ++i)
{
    Vector2D transformed = transform.GetWorldMatrix() * points[i];
}
```

---

### 4. Normalize Before Use

```cpp
// ✅ Good: Check before normalize
Vector2D direction = target - current;
float length = direction.Magnitude();
if (length > 0.0f)
{
    direction = direction.Normalize();
}

// ❌ Bad: Division by zero if length == 0
Vector2D direction = (target - current).Normalize();
```

---

## Gotchas

### Gotcha 1: Transform Hierarchy Performance

`GetWorldMatrix()` traverses hierarchy **multiple times** (no caching). Cache result if calling repeatedly.

---

### Gotcha 2: Matrix Multiplication Order

Matrix multiplication is **right-to-left**:

```cpp
// Applied in order: scale → rotate → translate
Matrix33 m = translation * rotation * scale;
```

---

### Gotcha 3: Rotation Units

All rotations use **radians**, not degrees:

```cpp
// ✅ Good: Use radians
float radians = DegreesToRadians(90.0f);
transform.SetRotation(radians);

// ❌ Bad: Using degrees directly
transform.SetRotation(90.0f);  // Incorrect!
```

---

### Gotcha 4: Template Bugs

`InverseLerp(Vector2D)` and `MoveTowards(Vector2D)` have known bugs. Use float versions or implement custom specializations.

---

## Known Issues

**From BUG_REPORT.md:**

1. **InverseLerp template missing for Vector2D**
   - Status: Known bug, fix planned
   - Workaround: Use float version

2. **MoveTowards returns wrong type for Vector2D**
   - Status: Known bug, fix planned
   - Workaround: Implement custom version

3. **Transform2D hierarchy traversal performance**
   - Status: Known issue, optimization planned
   - Workaround: Cache world matrix

**[→ Known Issues](../../07-subsystems/dia-maths/known-issues.md)**

---

## Summary

**Vectors:**
- `Vector2D`, `Vector3D`, `Vector4D` - Position, direction, velocity

**Matrices:**
- `Matrix33` - 2D transformations
- `Matrix44` - 3D transformations

**Transforms:**
- `Transform2D` - Hierarchical 2D transforms (⚠️ performance issue)
- `Transform3D` - Hierarchical 3D transforms

**Shapes:**
- `Circle`, `AABB`, `Line`, `Polygon` - Collision/intersection

**Utilities:**
- `Random` - Thread-safe RNG
- Interpolation - Lerp, MoveTowards, SmoothStep
- FloatMaths - Trig, conversions, constants

**Thread Safety:**
- Random: ✅ Thread-safe
- Transforms: ❌ Not thread-safe
- Vectors/Matrices: ✅ Safe if immutable

**Known Issues:**
- Template bugs (InverseLerp, MoveTowards)
- Transform hierarchy performance (no caching)

**[→ API Overview](../api-overview.md)**  
**[→ DiaCore API](core-api.md)**  
**[→ DiaGraphics API](graphics-api.md)**
