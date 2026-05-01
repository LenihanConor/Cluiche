# Bug and Performance Report - DiaMaths

## 🐛 CRITICAL BUGS

### 1. Interpolation.inl - InverseLerp Template Bug (LINE 41)
**Severity:** CRITICAL
**Location:** `Dia/DiaMaths/Core/Interpolation.inl:41`

```cpp
// BUGGY CODE:
if (diff * diff < static_cast<T>(FLOAT_EPSILON * FLOAT_EPSILON))
```

**Problem:** For Vector2D, `diff * diff` performs component-wise multiplication, NOT magnitude squared!
- For float: `diff * diff` = correct squared value
- For Vector2D: `diff * diff` = Vector2D(x*x, y*y) ❌ NOT what we want

**Impact:** InverseLerp will fail for Vector2D and other vector types.

**Fix:** Template specialization or use proper magnitude calculation.

---

### 2. Interpolation.inl - MoveTowards Template Bug (LINE 104)
**Severity:** CRITICAL
**Location:** `Dia/DiaMaths/Core/Interpolation.inl:104`

```cpp
// BUGGY CODE:
T diff = target - current;
float sqrDist = diff * diff;  // ❌ WRONG for Vector2D!
```

**Problem:** Same as above - `diff * diff` doesn't work for vectors.

**Impact:** MoveTowards will crash or produce incorrect results for Vector2D.

**Fix:** Need proper squared magnitude calculation.

---

## ⚠️ BUGS

### 3. IntersectionTests.cpp - Dead Code (LINES 620-623)
**Severity:** LOW
**Location:** `Dia/DiaMaths/Shape/Common/IntersectionTests.cpp:620-623`

```cpp
// UNUSED VARIABLES:
bool atLine1Endpoint = (Dia::Maths::Float::FEqual(t1, 0.0f, FLOAT_EPSILON) ||
                        Dia::Maths::Float::FEqual(t1, 1.0f, FLOAT_EPSILON));
bool atLine2Endpoint = (Dia::Maths::Float::FEqual(t2, 0.0f, FLOAT_EPSILON) ||
                        Dia::Maths::Float::FEqual(t2, 1.0f, FLOAT_EPSILON));
// Never used!
```

**Problem:** Variables calculated but never used. Wastes CPU cycles.

**Fix:** Either use them for classification or remove them.

---

## 🐌 PERFORMANCE ISSUES

### 4. Transform2D - Multiple Hierarchy Traversals
**Severity:** MEDIUM
**Location:** `Dia/DiaMaths/Transform/Transform2D.inl`

**Problem:** If you call GetWorldPosition(), GetWorldRotation(), and GetWorldScale() in sequence, each one traverses the parent hierarchy independently.

Example:
```cpp
Vector2D pos = transform.GetWorldPosition();    // Traverses hierarchy
Angle rot = transform.GetWorldRotation();       // Traverses hierarchy AGAIN
Vector2D scale = transform.GetWorldScale();     // Traverses hierarchy AGAIN!
```

**Impact:** 3x the work for deep hierarchies. O(3n) instead of O(n).

**Fix:** Add a method to get all three at once, or cache world transform.

---

### 5. IntersectionTests - Line vs AARect Creates Objects
**Severity:** LOW
**Location:** `Dia/DiaMaths/Shape/Common/IntersectionTests.cpp:650`

```cpp
// Creates 4 Line2D objects every call:
Line2D edges[4] = {
    Line2D(Vector2D(min.x, min.y), Vector2D(max.x, min.y)),
    Line2D(Vector2D(max.x, min.y), Vector2D(max.x, max.y)),
    Line2D(Vector2D(max.x, max.y), Vector2D(min.x, max.y)),
    Line2D(Vector2D(min.x, max.y), Vector2D(min.x, min.y))
};
```

**Impact:** Heap allocations or stack copies on every call.

**Fix:** Test line-edge intersection without creating objects, or use static const edges.

---

### 6. Easing - InOut Functions Duplicate Calculations
**Severity:** LOW
**Location:** Various easing InOut functions

**Problem:** Some InOut variants recalculate t values:
```cpp
if (t < 0.5f)
    return 2.0f * t * t;
else
{
    t = t * 2.0f - 1.0f;  // Recalculates
    return -0.5f * (t * (t - 2.0f) - 1.0f);
}
```

**Fix:** Minor optimization possible, but not critical.

---

### 7. Random - Uniform Distribution Could Use Lookup Table
**Severity:** LOW
**Location:** `Dia/DiaMaths/Core/Random.cpp`

**Problem:** RandomPointInCircle() calls Cos() and Sin() every time.

**Fix:** For games needing many random points, consider lookup table or faster approximations.

---

## 📊 SUMMARY

| Type | Count | Severity |
|------|-------|----------|
| Critical Bugs | 2 | 🔴 Must Fix |
| Medium Bugs | 1 | 🟡 Should Fix |
| Performance | 4 | 🟢 Nice to Have |

## 🔧 RECOMMENDED FIX ORDER

1. **IMMEDIATE:** Fix Interpolation template bugs (breaks Vector2D usage)
2. **SOON:** Remove dead code in IntersectionTests
3. **LATER:** Optimize Transform2D hierarchy traversal
4. **OPTIONAL:** Other performance improvements
